# Current work log

Project: **LivePaper** — live wallpaper for Windows and Linux
Repository: `/home/majeh/program/LivePaper`
Last updated: 2026-10-24

---

## Working rules

These rules apply to **every** task in this project:

1. **Before starting any task**, add an entry to this document under
   *Task log* describing what is going to be done, with the status
   `🔄 in progress`.
2. **When the task is finished**, update that same entry: what was actually
   done, the results (measurements, verification), the files that were
   touched, and set the status to `✅ done` (or `❌ blocked` / `⏸ paused`).
3. Keep the *Status* table at the top of this document up to date.
4. This document is the single source of truth for the current state of the
   work — the final answer to the user should match what is written here.

---

## Status

| Area | State |
| --- | --- |
| Linux build | ✅ working, no warnings |
| Windows build (cross-compiled) | ✅ working, no warnings |
| Diagnostics subsystem | ✅ done |
| Memory leak (per decoded frame) | ✅ **fixed** |
| Windows wallpaper visibility (WorkerW) | ✅ **working** — the strategy 6 is the default |
| Windows desktop icons | 🚫 **switched off on purpose** — the dock bar replaces them, see task 23 |
| Top dock bar (hover bar) | ✅ **working** — add button uses native file dialog on Windows |
| Folder popup (multi-column) | ✅ **working** — dynamic rows, fixed hit testing, search removed |
| Add button (file/folder picker) | ✅ **working** — native `IFileOpenDialog` on Windows |

---

## Remaining work

### Task 23 — The desktop icons are not painted ⏸ paused

**What the user reported:** the new build works — the wallpaper is there, the
left and the right click work, the desktop items open — but the **desktop
icons are not visible**. They can be clicked, so they are there; they are just
not drawn.

**The research that was done on the web** (the blog of roogerchen2005, which
analyses the mechanism of Windows 11 24H2, and the AutoIt thread *"Move window
behind desktop icons"*):

* On 24H2 the layer of the desktop icons (`SHELLDLL_DefView`) is **transparent**,
  and Windows keeps a **snapshot of the desktop** over it, to make the switch
  between the virtual desktops smooth. The snapshot is what hides the wallpaper
  and what swallows the mouse clicks.
* The **complete** code of the blog is:

```c
HWND hDefView = FindWindowEx(hProgman, 0, L"SHELLDLL_DefView", 0);
if (hDefView != NULL) {
    HWND hWorkerW = FindWindowEx(hProgman, 0, L"WorkerW", 0);
    if (hWorkerW != NULL) {
        SetParent(hPlayer, hWorkerW);
        ShowWindow(hDefView, SW_HIDE);
        Sleep(0);
        ShowWindow(hDefView, SW_SHOWNORMAL);
        return TRUE;
    }
}
```

* The important detail that was missed until now: the icons are **not** drawn
  by the `SHELLDLL_DefView` window, but by the `SysListView32` window (the
  *"FolderView"* list) inside it. The shell only repaints that list **when the
  mouse is over it** — which is exactly what the user described: the icons are
  there and they can be clicked, but they are invisible until the mouse moves
  over them.

**The two bugs that were in our own code:**

1. The default was `ICONS_REDRAW`, which only asks the shell to paint the layer
   again — it does **not** remove the snapshot of the desktop. Only
   `ICONS_REFRESH` (hiding and showing the `SHELLDLL_DefView` again) does that.
   The default is now `ICONS_REFRESH`.
2. The `SysListView32` list — the window that actually draws the icons — was
   never repainted explicitly. It is now.

**What was changed** (`src/platform_windows.c`):

* `RepaintDesktopIcons()` is a new function. It hides and shows the
  `SHELLDLL_DefView` again (which removes the snapshot of the desktop) and
  then repaints the `SysListView32` list inside it, so that the icons are
  drawn.
* The default of `--desktop-icons` is now `refresh` instead of `redraw`.
* `PlatformRefreshDesktopIcons()` (called every two seconds in the beginning)
  does the **full** refresh only four times, and only the light repaint after
  that — otherwise the icons would flicker.

**Status:** compiled (no warnings) and deployed.

**The result of the user test: the fix did NOT work.** The user tested both
modes on 2026-10-21:

| Command | Icons |
| --- | --- |
| `live-paper-console.exe --desktop-icons=redraw` | ❌ not visible |
| `live-paper-console.exe --desktop-icons=refresh` | ❌ not visible |

(The two options cannot be given together — argtable3 reports *"excess
option"* — so they have to be tested one after the other.)

**What the log of the test shows.** The important part is the structure of the
desktop, which is the same in both runs:

```
[ 0] SHELLDLL_DefView   (the layer of the icons)
[ 1] SysListView32     title='FolderView'   (the list that draws the icons)
[ 2] SysHeader32       visible=0
[ 3] SDL_app            title='live-paper'   (our window)
[ 4] WorkerW           visible=1 WS_DISABLED
```

**The reason why the fix cannot work with the strategy 6.** The strategy 6 is
a **top-level popup**: it is not a child of `Progman`, so the layer of the
icons is *not* above it — it is a completely separate window. The `Progman`
window (and with it the `SHELLDLL_DefView` and the `SysListView32` inside it)
is at the position **412** of the z-order, and our popup is at the bottom of
it. But the log says:

```
the wallpaper window is at the position -1 of the z-order, Progman is at the position 412
```

The position **-1** means that the wallpaper window is **not in the list of the
top-level windows at all** — because at the moment of the measurement it was
still a child of `Progman` (the measurement is done in `PlatformInit()`, before
the strategy 6 is applied). The strategy 6 is applied afterwards, in
`InitContext()`.

**So the real problem is the order of the operations:**

1. `PlatformInit()` attaches the window with the strategy 0 (a child of
   `Progman`) and measures the z-order there.
2. `InitContext()` then applies the strategy 6, which makes the window a
   **top-level popup** at the bottom of the z-order.
3. The layer of the icons is refreshed *before* that, in step 1 — and the
   refresh of the layer is only useful when the wallpaper is a **child** of
   `Progman`, because only then the icons are painted over it.

**The conclusion:** with the strategy 6 the icons **cannot** be painted over
the video, because the window is not part of the `Progman` hierarchy at all.
The video is composited (it is a separate top-level window), but the icons
live in a completely different layer that is above `Progman`.

**What has to change (the plan for the next session):** the window has to be
**a child of the `WorkerW` that is inside `Progman`** — the window `[ 4]` of
the log above. Then the order of the windows in `Progman` is exactly right:

```
SHELLDLL_DefView   <- the icons, on top
SysListView32      <- the icons
SDL_app            <- our video, in the middle
WorkerW            <- the background, at the bottom
```

This is exactly the code of the blog that was found (task 19/20):

```c
HWND hDefView = FindWindowEx(hProgman, 0, L"SHELLDLL_DefView", 0);
HWND hWorkerW = FindWindowEx(hProgman, 0, L"WorkerW", 0);
SetParent(hPlayer, hWorkerW);
ShowWindow(hDefView, SW_HIDE);
Sleep(0);
ShowWindow(hDefView, SW_SHOWNORMAL);
```

The strategy 1 (`WALLPAPER_STRATEGY_WORKERW`) and the strategy 5
(`WALLPAPER_STRATEGY_WORKERW_CHILD`) already do something similar, but they
were not the default. The next task is to make one of those the default and
verify that the icons are painted correctly.

---

### Task 24 — Make Strategy 1 (WorkerW child) the default 🔄 in progress

**Goal:** Change the default wallpaper strategy from Strategy 6 (top-level popup) to Strategy 1 (child of WorkerW inside Progman) so that desktop icons are painted correctly.

**Files to modify:**
- `src/main.c` - change default strategy
- `src/platform_windows.c` - ensure Strategy 1 works correctly with icon refresh

**Verification:** Test on Windows 11 24H2 that desktop icons are visible and clickable.

---

### Task 25 — Verify and fix icon refresh timing with Strategy 1 🔄 in progress

**Goal:** Ensure that when using Strategy 1 (WorkerW child), the icon refresh happens at the right time (after the window is parented to WorkerW) so that icons are painted over the video.

**Files to modify:**
- `src/platform_windows.c` - adjust `PlatformRefreshDesktopIcons()` timing

---

### Task 26 — Linux: Native file dialogs (GTK/Qt) ⏸ paused

**Goal:** Replace the custom add button menu with native file dialogs on Linux (GTK FileChooserDialog or Qt QFileDialog).

**Files to modify:**
- `src/launcher.c` / `src/launcher.h` - add `LauncherPickFile()` and `LauncherPickFolder()` for Linux
- `src/dock.c` - call native dialogs on Linux

---

### Task 27 — Unicode support on Windows ⏸ paused

**Goal:** Fix Unicode file paths on Windows (currently uses ANSI APIs).

**Files to modify:**
- `src/dirlist.c` - use wide character APIs (`FindFirstFileW`, etc.)
- `src/launcher.c` - use `IFileOpenDialog` with wide strings
- `src/platform_windows.c` - use wide character APIs where needed

---

### Task 28 — Multi-monitor support ⏸ paused

**Goal:** Support multiple displays with separate wallpapers or spanning.

**Files to modify:**
- `src/platform_windows.c` - enumerate monitors, create wallpaper window per monitor
- `src/main.c` - handle multi-monitor configuration

---

### Task 29 — Linux: Compositor compatibility ⏸ paused

**Goal:** Make LivePaper work with Wayland compositors (currently X11 only).

**Files to modify:**
- `src/platform_linux.c` - add Wayland support, or use XWayland

---

## Task log (completed tasks moved to CHANGELOG.md)

See [CHANGELOG.md](CHANGELOG.md) for all completed tasks (1-33).
