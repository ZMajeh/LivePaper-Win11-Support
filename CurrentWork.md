# Current work log

Project: **LivePaper** — live wallpaper for Windows and Linux
Repository: `/home/majeh/program/LivePaper`
Last updated: 2026-10-21

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
| Windows desktop icons | 🚫 **switched off on purpose** — the dock bar replaces them, see task 29 |
| Top dock bar (hover bar) | ⚠️ **the bar works, the add button does not show anything on Windows** — see task 31 |
| The add button (the file picker) | 🚧 **not working on Windows** — the state machine is right, the custom popup window is not visible |

---

## Task log

| # | Task | Status |
| --- | --- | --- |
| 1 | Build fix (GCC 14, `SDL_LockTexture`) | ✅ done |
| 2 | Diagnostics subsystem (log, sysinfo, video stats, wallpaper status) | ✅ done |
| 3 | Show the diagnostics in the terminal on Windows | ✅ done |
| 4 | Cross-compile for Windows from Linux (`makeLinuxToWindow.sh`) | ✅ done |
| 5 | Fix the memory leak (one packet buffer per decoded frame) | ✅ done |
| 6 | Fix the Windows wallpaper (WorkerW host + real child window) | ✅ done |
| 7 | Add the working rules to this document | ✅ done |
| 8 | Fix the wallpaper host on Windows 11 24H2 (icons + clicks broken) | ✅ done |
| 9 | Add `--dump-windows` and the PowerShell script to collect the desktop structure | ✅ done |
| 10 | Diagnose why the wallpaper window composites transparently | ✅ done — the renderer produced nothing |
| 11 | Fix the texture upload (SDL_UpdateTexture instead of LockTexture) | ✅ done — the renderer shows the video |
| 12 | Support Windows 11 virtual desktops (one Progman per workspace) | ✅ done — the right Progman is found |
| 13 | Verify the wallpaper is really on the screen (BitBlt check) + auto fallback | ✅ done — the window is not composited |
| 14 | Search the attachment strategies automatically until the wallpaper shows | ✅ done — strategy 2 works |
| 15 | Fix the icons and the mouse clicks (z-order + unreliable screen check) | ⏸ paused — see task 23 |
| 16 | Reliable screen check + `--wallpaper-strategy` to pick one manually | ✅ done |
| 17 | Deploy the build to the Windows desktop folder automatically | ✅ done |
| 18 | New strategy: below `Progman`; fix the placement of the child strategies | ✅ done |
| 19 | Research: the mechanism of Windows 11 24H2 (the transparent icon layer) | ✅ done |
| 20 | The `0x052c` message, the layered window, and the order of the strategies | ✅ done |
| 21 | The capture of the screen: the only reliable way to see the desktop | ✅ done — the screenshots confirm strategy 6 |
| 22 | Strategy 6 works — make it the default and deploy the build | ✅ done |
| 23 | The desktop icons are not painted (the video and the clicks work) | 🚫 dropped — the icons are switched off on purpose, see task 29 |
| 24 | The session of 2026-10-20: deploy, analyse the screenshots, plan | ✅ done |
| 25 | Fork the project and copy the changes into the new repository | ✅ done — pushed as `17fdb35` |
| 26 | Fix the CI: the release fails with 403, and the MSVC warnings | ✅ done — the release is created |
| 27 | Publish the program for Linux as well | ✅ done — the release has all three files |
| 28 | The instructions for the automatic start on Windows 11 (scheduled task) | ✅ done — documented in the README |
| 29 | The top dock bar (the hover bar, the add button, the system details) | ✅ done — the bar works, the icons are off |
| 30 | The add button does nothing: the menu of the add button is not shown | ✅ done — the popup has a window of its own, see task 30 |
| 31 | The add button still shows nothing on Windows: use a native file dialog | ✅ done — `LauncherPickFile()` opens `IFileOpenDialog` on a worker thread, result via SDL event; Linux/macOS keep the custom popup |
| 32 | Add button: two separate buttons (Add file / Add folder), folder popup navigation with search, icons | ✅ done |

---

## Detailed entries

### Task 1 — Build fix (GCC 14) ✅ done
`src/video.c` — `SDL_LockTexture()` expects `void **`, but the code passed
`uint8_t **`. GCC 14 turned that warning into a hard error.

```c
void *pixels = NULL;
if (SDL_LockTexture(v->tex, NULL, &pixels, &pitch) != 0)
    FAIL_WITH("can't lock SDL texture: %s", SDL_GetError());
pix = (uint8_t *)pixels;
```

### Task 2 — Diagnostics subsystem ✅ done
New files: `src/log.h`, `src/log.c`, `src/sysinfo.h`, `src/sysinfo.c`

* **Logging** — levels `ERROR`/`WARN`/`INFO`/`DEBUG`, timestamps, output to
  stderr, optionally to a file, plus `OutputDebugStringA` on Windows.
* **System info** — RSS, peak RSS, virtual size and total system RAM for
  Linux (`/proc/self/status`), Windows (`psapi`) and macOS (`mach`).
* **Video info** — `VideoGetStats()` (fps, frames decoded/rendered, loops) and
  `VideoGetFileInfo()` (format, codec, resolution, duration, fps, file size).
* **Wallpaper status** — `enum WallpaperStatus` in `struct Context`:
  `ACTIVE` / `INACTIVE` / `FAILED`, plus a `wallpaper_detail` string.
* **CLI flags** — `-v/--verbose`, `--stats`, `--log-file=<path>`.
* Errors are now logged too, so they are visible even without a console.

### Task 3 — Console output on Windows ✅ done
The Windows build is a GUI application, so its stdout went nowhere and no
diagnostics were visible from a terminal.

* `LogAttachParentConsole()` calls `AttachConsole(ATTACH_PARENT_PROCESS)` and
  reopens `stdout`/`stderr` on `CONOUT$`.
* A second executable `live-paper-console.exe` is built with the console
  subsystem (`WIN32_EXECUTABLE FALSE`, `SDL_MAIN_HANDLED`).

Verified under Wine: both executables now print to the terminal.

### Task 4 — Cross-compilation from Linux ✅ done
* `cmake/windows-mingw.cmake` — MinGW-w64 toolchain file.
* `makeLinuxToWindow.sh` — one-command build (downloads SDL2 + FFmpeg, builds,
  packages `dist-windows/` and `LivePaper-win64.zip`).
* `CMakeLists.txt` — `MINGW` branch for the FFmpeg/SDL2 dependencies,
  `LIVEPAPER_SDL2_DIR` / `LIVEPAPER_FFMPEG_DIR` options.

Bugs found while cross-compiling (all MSVC-only assumptions):

* `VersionHelpers.h` is MSVC-only → guarded with `#ifdef _MSC_VER`.
* `OpenVideoFile()` had a non-static forward declaration → made `static`.
* `SetProcessDPIAware()` needs `_WIN32_WINNT >= 0x0600` → set via
  `target_compile_definitions` (windows.h is pulled in by SDL.h first).
* `PROCESS_MEMORY_COUNTERS.cb` was never set → now set, plus a
  `VirtualQuery` fallback for the working set.

### Task 5 — Memory leak ✅ done — FIXED
**Symptom:** RAM grew continuously in Task Manager.

**Measured** with a harness that reports `mallinfo2().uordblks` (heap actually
in use) after every loop of the video:

```
before fix:  loop 0: 0.86 MiB   loop 1: 1.24 MiB   loop 2: 1.62 MiB   (+0.39 MiB per loop)
after  fix:  loop 0: 0.43 MiB   loop 1: 0.44 MiB   loop 2: 0.44 MiB   (+0.00 MiB per loop)
```

**Root cause** — `src/video.c`, `DecodeNextFrame()`: the packet was handed to
`avcodec_send_packet()` but the reference to its buffer was never released.
`avcodec_send_packet()` keeps its *own* reference to the same buffer
(`av_buffer_ref`), so the buffer's refcount stayed at 2 forever and the
memory was never freed. The leak only became visible after the first
`av_seek_frame()` (the seek stops the demuxer from recycling the packet).

**Fix** — release our reference after handing the packet to the decoder
(this is the pattern used by the official FFmpeg examples):

```c
int send_ret = avcodec_send_packet(v->decoder_ctx, &packet);
// The decoder keeps its own reference to the data of the packet
// (it is shared with the one of this packet), so the reference of
// this packet can be released. Without this the buffer of the
// packet is leaked for every decoded frame.
av_packet_unref(&packet);
if (send_ret < 0) FAIL_WITH("%s", av_err2str(send_ret));
```

**Verified** leak-free for `test.gif`, `test.mp4` and `screencap.gif`.


---

### Task 6 — Windows wallpaper is not visible ✅ done

The user reported `wallpaper: not visible (Windows WorkerW)`.

**Diagnosis from the user's log:**

```
found 16 WorkerW window(s) that can host the wallpaper
the WorkerW window is class='WorkerW' visible=0 WS_VISIBLE=0
the wallpaper window is class='SDL_app' visible=0 WS_VISIBLE=1
```

* The wallpaper window itself has `WS_VISIBLE=1`, but
  `IsWindowVisible()` is 0 because **its parent WorkerW is hidden**
  (`WS_VISIBLE=0`). A child is only painted when the whole ancestor chain
  is visible.
* The old code compared `GetParent(sdl_hwnd) == sdl_parent_hwnd`. That is
  **always false**: `GetParent()` returns the *owner* of a popup window, and
  `SetParent()` does not change the `WS_POPUP` style. The old check was
  therefore meaningless.

### Iteration 1 — showed the WorkerW window (REGRESSION, reverted)

First attempt: call `ShowWindow(workerw, SW_SHOW)` on the hidden WorkerW and
re-check. The wallpaper then reported "set", but the user saw **a small white
window in the top-left corner of the screen that could be dragged around**.

Two things went wrong:

1. `ShowWindow()` on a *hidden Explorer WorkerW* made an ordinary helper
   window visible — that was the small white rectangle. Explorer's windows
   must never be modified.
2. The wallpaper window was still a **popup** window (`WS_POPUP`), because
   `SetParent()` does not change the window styles. A popup stays an
   independent top-level window: it has a title bar, can be dragged and is
   not clipped to the desktop. That is what the user was seeing.

### Iteration 2 — the wallpaper window becomes a real child ✅ (current)

`src/platform_windows.c`:

1. `EnumWindows()` collects **all** top-level `WorkerW` windows
   (Windows 11 24H2 creates many; only one is the right host).
   The limit was raised from 16 to 64 — the user was hitting the cap.
2. After `SetParent()`, the styles are corrected as MSDN prescribes:
   `WS_POPUP` is cleared and `WS_CHILD` is set. The window is now painted as
   part of the desktop and can no longer be dragged around.
   `WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW` are added and `WS_EX_APPWINDOW`
   removed, so it does not steal the focus and does not appear in the task bar.
3. The window is resized to the client area of its new parent with
   `SetWindowPos()` — the coordinates of a child window are relative to the
   parent.
4. The candidates are tried **one after another** and the first one that
   becomes visible wins. Two passes: the first one only accepts parents that
   are large enough to hold the wallpaper (some WorkerW windows are tiny
   helper windows), the second one accepts any of them. `Progman` is the last
   resort.
5. Explorer's windows are never modified any more.
6. Every candidate is logged with its class, `visible`, `WS_VISIBLE`,
   `WS_CHILD` and `WS_POPUP`, so the log shows exactly which windows exist.

### Task 8 — Fix the wallpaper host on Windows 11 24H2 ✅ done

The user reported, after iteration 2: **the desktop icons are invisible, the
old background is still shown and the clicks on the desktop do nothing.**

**Diagnosis from the user's log** — this is the decisive data:

```
candidate  0..11: class='WorkerW' visible=0 WS_VISIBLE=0 WS_CHILD=0 WS_POPUP=0
candidate 12: class='Progman' visible=1 WS_VISIBLE=1 WS_CHILD=0 WS_POPUP=1
attached the wallpaper window to the WorkerW window 0000000000110ef0 (class='Progman' ...)
```

* **All twelve WorkerW windows are hidden** (`WS_VISIBLE=0`). The classic
  WorkerW trick cannot work on this system — there is no visible WorkerW to
  host the wallpaper.
* The fallback (`Progman`) was used instead. `Progman` is the window that
  paints the wallpaper **and** hosts the desktop icons, so a child window that
  covers it hides the icons and swallows the mouse clicks.
* The old background was still visible because the window ended up below the
  wallpaper layer instead of above it.

**Changes written** (`src/platform_windows.c`):

1. `FindDesktopIconsHost()` finds the top-level window that contains the
   `SHELLDLL_DefView` window (the desktop icons). That window is the one that
   paints the wallpaper on Windows 11 24H2, so it is the correct host.
2. The window is now placed with `SetWindowPos(..., HWND_BOTTOM, ...)` — at the
   **bottom of the children of the parent**. The desktop icons are a child of
   the same window, so they stay on top of the wallpaper and keep receiving
   the mouse clicks.
3. The candidate order is: the icons host → the classic WorkerW → the other
   WorkerW windows → `Progman`.

**Status:** compiled and ready; the exact behaviour has to be confirmed on the
user's Windows machine.

---

### Task 9 — `--dump-windows` and the PowerShell script ✅ done

The user asked for a way to list the details of all the WorkerW windows.

Two ways are provided:

* **`live-paper-console.exe --dump-windows`** — built into the application
  (`PlatformDumpWindows()` in `src/platform_windows.c`). It prints the screen
  metrics, every top-level window in z-order, the WorkerW windows, `Progman`,
  the window that hosts the desktop icons and the children of that host, with
  the class, the title, the visibility, the decoded style bits and the
  geometry of each one. It needs no file and no window, and it exits
  afterwards.
* **`tools/dump-windows.ps1`** — the same information from PowerShell, for the
  cases where the application cannot be started. It is shipped in
  `dist-windows/`.

**Note:** a `.ps1` file is not executed by typing its path into PowerShell;
it has to be run with
`powershell -ExecutionPolicy Bypass -File dump-windows.ps1`.

**Status:** the C implementation was verified to run (under Wine, which is
headless, so the window lists are empty there but the format is correct). The
PowerShell script could not be executed in this container (no PowerShell
available); it was checked for structural correctness instead.

---

### Task 11 — Fix the texture upload (SDL_UpdateTexture) ✅ done

**Diagnosis from the user's log (23:38):**

```
INFO : the wallpaper window is at (0,0)-(1920,1080), its client area is 1920x1080
INFO : its parent is at (0,0)-(1920,1080), its client area is 1920x1080
INFO : the children of the parent (z-order, topmost first):
INFO :   [ 0] class='SHELLDLL_DefView'  visible=1 rect=(0,0)-(1920,1080)
INFO :   [ 1] class='SysListView32'     visible=1 rect=(0,0)-(1920,1080)
INFO :   [ 2] class='SysHeader32'       visible=0
INFO :   [ 3] class='SDL_app'           visible=1 rect=(0,0)-(1920,1080)   ← our window
INFO :   [ 4] class='WorkerW'           visible=1 rect=(0,0)-(1920,1080)   ← the wallpaper layer
INFO : the pixel in the middle of the screen is R=0 G=0 B=0 A=255
```

**What this proves:**

* The **placement is correct**: our window is a `WS_CHILD` of `Progman`, it
  covers the whole screen, and it is directly above the `WorkerW` child that
  paints the wallpaper. The icons (`SHELLDLL_DefView`) are above it, which is
  why they are visible and the clicks work.
* The **renderer produces nothing**: the read-back is black. The window is in
  the right place, but it is empty, so the old wallpaper shows through.
* On Windows 11 24H2 the wallpaper layer is a **`WorkerW` child of `Progman`**,
  not a top-level `WorkerW` — that is why the classic trick (which looks for
  top-level `WorkerW` windows) cannot work here.

**The bug:** the pixels were uploaded with `SDL_LockTexture()` /
`SDL_UnlockTexture()`. The Direct3D renderers of SDL do not upload the data of
a locked streaming texture, so the texture stayed empty.

**The fix:** `SDL_UpdateTexture()`, which is the path that every renderer
supports:

```c
if (SDL_UpdateTexture(v->tex, NULL, v->sws_frame->data[0],
                      v->sws_frame->linesize[0]) != 0)
    FAIL_WITH("can't update the texture: %s", SDL_GetError());
```

It also removes the manual `memcpy()` of every row.

**Also changed:**

* The window is created without `SDL_WINDOW_OPENGL` — the flag asks for an
  OpenGL window, but the renderer that is used is Direct3D.
* The diagnostic now samples **nine** pixels and also reports the middle
  pixel of the **decoded frame**, so it is unambiguous whether the frame is
  empty, the upload fails or the renderer is at fault. Verified on Linux:
  `the decoded frame is 1000x562, its middle pixel is R=57 G=91 B=113` and
  `the renderer shows the video`.
* `--renderer=software` can be used to check whether the Direct3D renderer is
  the problem.

### Task 12 — Support Windows 11 virtual desktops ✅ done

The user reported that the video is still not visible, and mentioned: *"I am
using Windows 11, and have 2 workspaces, each with separate wallpaper."*

**The good news in that log** — the texture fix worked:

```
INFO : the decoded frame is 1280x720, its middle pixel is R=0 G=0 B=0
INFO :   pixel 0: R=0 G=0 B=0 A=255
INFO :   pixel 1: R=0 G=0 B=85 A=255
...
INFO :   pixel 8: R=144 G=72 B=85 A=255
INFO : the renderer shows the video
```

The pixels have real colours, so **the decoding and the rendering work**. The
window is in the right place too (a `WS_CHILD` of `Progman`, above the
`WorkerW` that paints the wallpaper, below the desktop icons).

**The remaining problem is the virtual desktop.** Every virtual desktop of
Windows 11 has **its own `Progman` window**, and the wallpaper of a virtual
desktop is painted by the `Progman` of *that* desktop. The application used
`FindWindow("Progman", NULL)`, which returns an arbitrary one of them — very
likely the one of the *other* workspace, which is not displayed at all.

**Changes written** (`src/platform_windows.c`):

1. `IVirtualDesktopManager` is used (through `CoCreateInstance`, the interface
   is declared in the source because the SDK doesn't provide it for C) to find
   out **which virtual desktop a window is on**.
2. `FindCurrentDesktopProgman()` enumerates all the `Progman` windows and
   returns the one that is on the virtual desktop that is currently shown.
3. The window follows the user: `PlatformUpdate()` watches the identifier of
   the current virtual desktop and re-attaches the wallpaper when it changes.
4. The read-back of the pixels is now **opt-in** (`--readback`), because it
   disturbs the state of the renderer and must not influence the normal run.
5. `ole32` was added to the libraries that are linked on Windows.

**Note:** the log also shows **two** `SDL_app` windows, so two instances of the
application were running at the same time. The older one was below the
wallpaper layer and could never be seen.

**Status:** the virtual desktop support works — the log shows the `Progman` of the
current virtual desktop.

---

### Task 13 — Verify the wallpaper is really on the screen ✅ done

The video is still not visible, although everything that could be checked from
the inside is correct:

* the window is a `WS_CHILD` of the `Progman` of the **current** virtual
  desktop (the virtual desktop fix works — the log shows the right `Progman`),
* it covers the whole screen and is directly above the `WorkerW` that paints
  the wallpaper,
* the renderer produces the correct pixels,
* and only **one** instance of the application is running now.

**The missing piece:** everything so far was checked *inside* the process. None
of it proves that the window is actually **composited onto the screen**.

**The change:** the screen itself is now read back. `PlatformScreenShowsColor()`
uses `BitBlt()` from the screen DC, which returns the **composited** desktop,
and compares the pixel with the colour that was drawn. This is the only check
that can prove that the wallpaper is really visible.

It is used automatically:

1. After the renderer is created, a magenta colour is drawn over the whole
   window and presented.
2. The middle of the screen is read back. If it is magenta, the wallpaper is
   on the screen — the log says so.
3. If it is not, the **software renderer** is tried instead (it draws with GDI
   instead of Direct3D) and the check is repeated.
4. If it is still not magenta, the log says that the wallpaper is not visible
   on the screen, and the exact colour that was found there is reported.

### Task 14 — Search the attachment strategies automatically ✅ done

**The decisive measurement from the user's log:**

```
DEBUG: the screen shows R=12 G=12 B=12 at (960,540), expected R=255 G=0 B=255
DEBUG: the screen shows R=12 G=12 B=12 at (960,540), expected R=255 G=0 B=255
```

The magenta colour that was drawn over the whole window is **not** on the
screen — with the Direct3D renderer *and* with the software renderer. So it is
not the renderer: **the window is not composited at all.**

(The `R=12 G=12 B=12` at the middle of the screen is the pixel of the terminal
window that the application was started from — the terminal covers the middle
of the screen, which is also why the wallpaper could never be seen there.)

**The change — the application finds a working setup by itself.** The window is
attached with one of several strategies, and after each one the whole screen is
searched for the magenta colour:

| Strategy | Description |
| --- | --- |
| 0 | a `WS_CHILD` of `Progman`, below the desktop icons |
| 1 | a popup window directly below `Progman` in the z-order |
| 2 | a popup window at the very bottom of the z-order |

* The **whole screen** is searched now (`PlatformScreenHasColor()`), not only
  the middle of it, so that windows that cover parts of the screen do not
  hide the wallpaper from the check.
* The first strategy that makes the colour appear is kept and reported:
  `the wallpaper is visible on the screen (strategy N)`.
* If none of them works, the **software renderer** is tried with all of the
  strategies, and the log says that the wallpaper is not visible.

### Task 15 — Fix the icons and the mouse clicks ⏸ paused

**What went wrong:** my own change broke the window. The log showed it
plainly:

```
the wallpaper window itself: class='SDL_app' visible=0 WS_VISIBLE=0 WS_CHILD=1
```

The window was **hidden**. The cause: the window style was read *before* the
window was shown, and then written back *after* it — which removed the
`WS_VISIBLE` bit that `ShowWindow()` had just set. Because of that, all the
strategies were rejected and the desktop was gone.

**The fix:**

* The style is read again for every strategy, so that the `WS_VISIBLE` bit is
  never lost.
* `ShowWindow(hwnd, SW_SHOWNOACTIVATE)` is used instead of `SW_SHOW`:
  `SW_SHOW` **activates** the window, which raises it above `Progman`, hides
  the desktop icons and swallows the mouse clicks of the desktop.
* **The order of the operations was changed**, because showing a window can
  change the z-order and the requested z-order was therefore not the one that
  was in effect:
  1. the parent and the styles are changed,
  2. the window is shown (`SW_SHOWNOACTIVATE`),
  3. **then** the z-order is set with `SetWindowPos()`.
* Strategy 1 does not use `SetParent()` any more: it is a top-level popup that
  is placed directly below `Progman`, so that the desktop icons (which are a
  child of `Progman`) stay on top of the wallpaper.
* **A new strategy was added:** a child of the `WorkerW` window that paints
  the wallpaper. It is below the desktop icons (they are in `Progman`) and
  above the wallpaper itself — which is exactly where a live wallpaper belongs.

There are now four strategies:

| # | Description |
| --- | --- |
| 0 | a `WS_CHILD` of `Progman`, below the desktop icons |
| 1 | a `WS_CHILD` of the `WorkerW` that paints the wallpaper |
| 2 | a top-level popup directly below `Progman` |
| 3 | a top-level popup at the bottom of the z-order |

The state of the window **and its position in the z-order** are logged after
every strategy, so it is visible whether it was hidden, covered, or whether the
z-order was not the one that was requested:

```
DEBUG: the wallpaper window after the strategy 2: class='SDL_app' visible=1 ...
DEBUG:   it is at the position 419 of the z-order, Progman is at the position 418
```

### Task 16 — Reliable screen check + manual strategy selection ✅ done

**Two findings from the last log:**

1. The z-order shows that the wallpaper window ends up **above** `Progman`:
   ```
   DEBUG:   it is at the position 418 of the z-order, Progman is at the position 419
   ```
   That is why the desktop icons are hidden and the mouse clicks are eaten:
   the icons are a child of `Progman`, so they are below the wallpaper window.

2. **The screen check is unreliable.** In the verbose run the strategy was
   reported as *not* showing the wallpaper, although the user could clearly
   see the video. The magenta frame is simply not always on the screen when
   the screen is read back — the window may not have been composed yet, or the
   screen may be covered by other windows.

**The changes:**

* The screen check is now **repeated up to five times** with a delay, so that
  the window has time to be composed.
* The strategies have **names**, which are printed in the log.
* **A new option `--wallpaper-strategy=<n>`** forces one strategy instead of
  trying them all, so that every variant can be tried without waiting for
  the search.

| # | Strategy |
| --- | --- |
| 0 | a `WS_CHILD` of `Progman`, below the desktop icons |
| 1 | a `WS_CHILD` of the `WorkerW` that paints the wallpaper |
| 2 | a top-level popup directly below `Progman` |
| 3 | a top-level popup at the bottom of the z-order |

**The next step is to find out which one of them shows the icons and lets the
clicks through.** Strategy 1 is the most promising one: it is a child of the
`WorkerW` that paints the wallpaper, so it is *below* the desktop icons (they
are in `Progman`) and *above* the wallpaper.

### Task 17 — Deploy the build to the Windows desktop ✅ done

The build is now copied automatically to
`/mnt/c/Users/mundh/OneDrive/Desktop/LivePaper-win64`, so the new version is
always in the folder that is used on the Windows side.

* `makeLinuxToWindow.sh` copies the result there after every build. The
  folder is used by default when it exists, and it can be changed with
  `--deploy <dir>` (the option can be given more than once).
* The directory itself is not removed — it may be locked by the
  synchronisation of the cloud storage — only the files in it are replaced.
* A file that can't be overwritten (because a program is still running from it,
  or because OneDrive is syncing it) is retried a few times, and a warning is
  printed if it still fails.

**Note for the user:** `live-paper-console.exe` was locked during the first
attempt, because it was probably still running. The programs have to be closed
before a new build can be copied over them.

### Task 18 — The placement of the strategies ✅ done

**Important:** the verbose log that the user sent came from the **older build**
in the folder `LivePaper-win64verbbose working` — it doesn't have the names of
the strategies, so it was made before the last change. The normal log is from
the current build.

**A bug in the placement of the child strategies was found.** The children of
`Progman` are, in z-order:

```
[ 0] SHELLDLL_DefView    (the desktop icons view)
[ 1] SysListView32       (the list view that actually draws the icons)
[ 2] SysHeader32
[ 3] SDL_app             (the wallpaper window)
[ 4] WorkerW             (the window that paints the wallpaper)
```

Strategy 0 placed the window directly below the `SHELLDLL_DefView` — but the
`SysListView32`, which draws the icons, is a **child** of the `DefView` and is
therefore below it. So the wallpaper window ended up **above** the icon view,
which would have hidden the icons even with that strategy.

**The fix:** the wallpaper window is now placed directly **above the `WorkerW`
that paints the wallpaper** (the window just below it in the z-order is used
as the reference). It then covers the wallpaper, but stays below the icons.

**A fifth strategy was added:** a popup window that is placed **below
`Progman` itself**. The z-order data showed that `Progman` is one of the last
windows (`Progman is at the position 422`), and the wallpaper window ended up
*above* it, which is why the icons were hidden and the mouse clicks were eaten.
This strategy puts the window below `Progman`, so that the icons are on top of
the wallpaper and the clicks reach the desktop.

| # | Strategy |
| --- | --- |
| 0 | a `WS_CHILD` of `Progman`, directly above the wallpaper window |
| 1 | a `WS_CHILD` of the `WorkerW` that paints the wallpaper |
| 2 | a top-level popup directly below `Progman` |
| 3 | a top-level popup at the bottom of the z-order |
| 4 | a top-level popup **below `Progman`** *(new)* |

**Status:** compiled and ready; the strategies 0, 1 and 4 have to be tested.

### Task 19 — The mechanism of Windows 11 24H2 ✅ done

**The research that was done on the web:**

* Lively Wallpaper, issue #2116: *"it works animates on desktop but the problem
  is makes my desktop icons taskbar hides everything just wallpaper is
  showing"* — the same problem, on the same version of Windows.
* <https://blog.cast1e.top/posts/windeskchange/wdc/> — the analysis of the
  desktop switching mechanism of Windows 11 24H2.

**What the research says:**

* Before 24H2, `Program Manager` was split into two `WorkerW` windows and
  `Progman` itself when the desktop is switched (the message `0x052c`). One of
  the `WorkerW` windows was the `SHELLDLL_DefView` of the icons.
* **In 24H2, the `WorkerW` is a child of `Progman`**, the `SHELLDLL_DefView`
  does not move, and **the layer of the desktop icons is transparent**.
* But Windows keeps **a snapshot of the desktop over the icon layer**, to make
  the transition smooth. This snapshot is what hides the wallpaper and what
  swallows the mouse clicks. The snapshot can be removed by making the shell
  repaint the icon layer:

  ```c
  ShowWindow(hDefView, SW_HIDE);
  Sleep(0);
  ShowWindow(hDefView, SW_SHOWNORMAL);
  ```

* The window of the wallpaper is then a child of the `WorkerW`:

  ```c
  HWND hDefView = FindWindowEx(hProgman, 0, L"SHELLDLL_DefView", 0);
  HWND hWorkerW = FindWindowEx(hProgman, 0, L"WorkerW", 0);
  SetParent(hPlayer, hWorkerW);
  ShowWindow(hDefView, SW_HIDE); Sleep(0); ShowWindow(hDefView, SW_SHOWNORMAL);
  ```

**What was changed in the code:**

1. `MakeDesktopIconsTransparent()` was added: it hides and shows the
   `SHELLDLL_DefView` of `Progman`, which removes the snapshot of the desktop.
   It is called when the wallpaper is attached, and again every two seconds
   during the first seconds, because the shell can create the snapshot again.
2. The window is enabled explicitly (`EnableWindow(TRUE)`) after `SetParent()`:
   the `WorkerW` window is disabled, and a disabled parent disables its
   children.
3. **The software renderer is now the default on Windows.** The log of the
   user showed that the strategy 0 only shows the wallpaper with the software
   renderer: the Direct3D renderers of SDL don't composite a window that is a
   child of the desktop window. The accelerated renderer can still be asked for
   with `--renderer=accelerated`.
4. The strategies 0 and 1 are the ones that are worth testing now: they are
   children of the desktop, so the icons stay on top of the wallpaper.

**Status:** compiled, deployed and verified. The behaviour has to be tested by
the user: the video, the icons, the left click and the right click.

### Task 20 — The `0x052c` message, the layered window ✅ done

**The second round of research on the web:**

* <https://www.autoitscript.com/forum/topic/212981-move-window-behind-desktop-icons/>
  — the thread "Move window behind the desktop icons", with a working code
  for Windows 11 24H2. It uses three things that were missing:
  1. **the message `0x052c`** is sent to `Progman` to force the creation of the
     `WorkerW` window of the desktop:

     ```c
     SendMessageTimeout(hProgman, 0x052C, 0, 0, 3000, SMTO_NORMAL);
     ```

  2. the window is made a child of the `WorkerW` whose parent is `Progman`,
  3. the window is a **layered** window (`WS_EX_LAYERED` and
     `SetLayeredWindowAttributes`).
* <https://github.com/lively-community/lively/issues/2464> — the discussion
  "Not working after updating to 24h2". The known problem of Windows 11 24H2:
  **the desktop icons have to be enabled** for the wallpaper to be shown at
  all. The workaround of the other live wallpapers is to switch the icons off
  and on again (right click on the desktop, *View*, *Show desktop icons*), which
  is the same thing as the `ShowWindow(SW_HIDE)` / `ShowWindow(SW_SHOWNORMAL)`
  trick of the task 19.
* <https://github.com/lively-community/lively/issues/683> — the same input
  problem: the clicks do not reach the desktop when the wallpaper is a window
  that covers the whole screen.

**A bug of our own code was found at the same time:** the renderer was always
created with `SDL_RENDERER_ACCELERATED`, so the default of the task 19 (the
software renderer on Windows) had no effect at all. The log of the user showed
`renderer: direct3d` even though the software renderer had been requested.
**This is why the strategy 0 was never tried with the software renderer, and
why the program always ended up with a popup window that covers the icons.**

**What was changed:**

1. The renderer now follows the mode: `software`, `accelerated` or `auto`.
2. The message `0x052c` is sent to `Progman` before the window is attached
   (`PrepareDesktopLayer()`), which makes Windows create the `WorkerW` and turn
   the layer of the icons into a transparent layer.
3. A new strategy 5: a **layered** child of the `WorkerW`.
4. The strategies are not tried in the numerical order any more: the ones that
   keep the icons and the clicks working (0, 1, 5) come first, the popups (2,
   3, 4) are the last resort (`PlatformWallpaperStrategyOrder()`).
5. The children of the parent are now printed after **every** strategy, so the
   log shows exactly where the wallpaper window ended up.

| # | Strategy |
| --- | --- |
| 0 | a `WS_CHILD` of `Progman`, directly above the wallpaper window |
| 1 | a `WS_CHILD` of the `WorkerW` that paints the wallpaper |
| 2 | a top-level popup directly below `Progman` |
| 3 | a top-level popup at the bottom of the z-order |
| 4 | a top-level popup below `Progman` |
| 5 | a **layered** `WS_CHILD` of the `WorkerW` *(new)* |

**Status:** compiled, deployed and verified. The behaviour has to be tested by
the user.

### Task 21 — The capture of the screen ✅ done

**What the tests of the user showed:**

| Run | The strategy that was used | The video | The icons | The clicks |
| --- | --- | --- | --- | --- |
| 1 and 2 | 2 (a popup) | visible | missing | don't work |
| 3 | 0 (a child of `Progman`) | **not visible** | there | **work** |

**So the two mechanisms are complementary:** the child of `Progman` has the
right z-order (the icons and the clicks work) but its content is not composed
by Windows, and the popup is composed but it covers the icons.

**The check of the colour is unreliable:** in the run 3 the check said that the
wallpaper was visible, but the user saw no video. The reason is that the
compositor of Windows can show an old frame, and the magenta pixel can come
from anywhere on the screen.

**What was added:**

1. **`--screenshot=<path>`**: the screen is captured into a BMP file three
   seconds after the wallpaper is running (`PlatformCaptureScreen()`). The
   file is in the folder of the program, so it can be analysed on Linux with
   `tools/analyze-screenshot.py` — and it shows exactly what the user sees.
2. **Strategy 6: a popup that the mouse clicks pass through**
   (`WS_EX_LAYERED | WS_EX_TRANSPARENT`). Such a window is skipped when the
   system looks for the window under the mouse pointer, so the clicks reach
   the desktop. It is the standard way to make an overlay click-through.
3. The strategy 6 is tried before the popups that cover the icons.

**Status:** compiled, deployed and verified. The user has to run the program
with `--screenshot=shot.bmp`, and the file has to be analysed.

---

### Task 7 — Working rules for this document ✅ done

The user asked for a permanent rule: **log every task in this document before
starting it, and update the entry when it is done.**

Implemented as:

* A **Working rules** section at the top of the document with the four rules.
* A **Task log** table listing every task with its status.
* A detailed entry per task (this section).

**Result:** the document now has the structure `Working rules` → `Status` →
`Task log` → the detailed entries → `Next steps` → `Useful commands` →
`Notes and caveats`. From now on every task starts with an entry here and
ends with this entry being updated.

### Task 22 — Strategy 6 works: make it the default and deploy ✅ done

**What the user reported:** the strategy 6 (a popup that the mouse clicks pass
through) works — the video is shown, the icons are visible and the clicks reach
the desktop. The screenshots that the user captured with `--screenshot` confirm
it: the three captures (`shot0.bmp`, `shot2.bmp`, `shot6.bmp`) all show the
video, and the analysis of the corner of the screen (where the icons are) shows
that the icons are painted over the video.

**The problem that was found:** the change that makes the strategy 6 the
default was written to the source, but it was **never built and never
deployed**. The timestamps show it clearly:

| What | Timestamp |
| --- | --- |
| `src/main.c`, `src/platform_windows.c` (the change) | 19:47–19:49 |
| `live-paper.exe` in `LivePaper-win64` (the deployed build) | 19:29 |

So the build that the user was testing was the **old** one, without the
default. The program still tried the strategies one after the other, and the
unreliable colour check could stop it at the wrong one.

**What was done:**

1. The strategy 6 is the default on Windows (`src/main.c`): the window is a
   popup with `WS_EX_LAYERED | WS_EX_TRANSPARENT`, so the video is composited
   by the system and the mouse clicks pass through it to the desktop.
2. The build was made again and deployed to the Windows desktop folder.

**The result of the user test of this build:** the video is shown, the left and
the right click work, the desktop items can be opened — **but the desktop
icons are not painted** (they can be clicked, they are just invisible). See
task 23.

---

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
(`WALLPAPER_STRATEGY_LAYERED`) already do the `SetParent()` — but they were
tested **before** the renderer produced anything and they were rejected by the
(unreliable) colour check. **They have to be tested again now**, because:

* the renderer works now (the texture upload was fixed in task 11),
* the software renderer is the default on Windows now (task 19/20),
* the check of the screen is more reliable now (task 16).

**The next session should therefore:**

1. Test the strategies 1 and 5 again (`--wallpaper-strategy=1` and
   `--wallpaper-strategy=5`) — they are the ones that can show the icons.
2. Check that the `WorkerW` that is used is the one **inside** `Progman`
   (the log prints the children of `Progman` for every strategy).
3. Only if that fails, try the variant of the AutoIt thread, which uses
   `SetWindowLong(GWL_HWNDPARENT, SysListView32)` and `WS_EX_LAYERED` with
   `SetLayeredWindowAttributes`.
4. Make the strategy 6 the default again only if the icons cannot be saved
   — the user has to decide which is more important.

---

### Task 24 — The session of 2026-10-20: deploy, analyse, plan ✅ done

**What was done in this session:**

1. **The missing deployment was found and fixed.** The change that makes the
   strategy 6 the default was in the source since 19:47, but the deployed
   build was from 19:29 — it was never built. It was built and deployed, and
   the user confirmed: the video is shown, the left and the right click work,
   the desktop items open.
2. **The three screenshots of the user were analysed** with
   `tools/analyze-screenshot.py` (`shot0.bmp`, `shot2.bmp`, `shot6.bmp`): all
   three show the video, so the renderer and the compositing work.
3. **The mechanism of the 24H2 icon layer was researched** (task 23): the
   icons are drawn by the `SysListView32` list, and the layer is covered by a
   snapshot of the desktop.
4. **The fix of task 23 was written, built and deployed** — and it did not
   work. The reason is now understood and written down in task 23: the
   strategy 6 is a top-level popup, and the icons can only be painted over a
   window that is a **child of `Progman`**.

**The state of the code at the end of the session:**

| File | State |
| --- | --- |
| `src/main.c` | the strategy 6 is the default on Windows (`#ifdef _WIN32`) |
| `src/platform_windows.c` | 7 strategies, the refresh of the icon layer, the diagnostics of the z-order |
| `src/video.c`, `src/log.c`, `src/sysinfo.c` | unchanged since the earlier tasks |
| `CurrentWork.md` | up to date |

**Both builds are clean** (no warnings): the Linux build and the
cross-compiled Windows build. The Windows build is deployed in
`LivePaper-win64` on the Windows desktop.

**What is *not* solved:** the desktop icons are not painted over the video.
The user has to decide which is more important — the icons or the video.

---

### Task 25 — Fork the project and copy the changes ✅ done

**What the user asked for:** the original project
(<https://github.com/zuranthus/LivePaper>) is **archived and read-only** — a
push to it is refused with `403`. The user forked it themselves to
<https://github.com/ZMajeh/LivePaper-Win11-Support> and asked for the changes
to be copied into a clone of the new repository, so that they can commit and
push it themselves.

**What was done:**

1. The upstream repository was checked with the GitHub API: it is archived
   (`fork: False`, `default_branch: main`, MIT licence, 8 forks).
2. The fork was cloned to `~/git/LivePaper-Win11-Support`. **The base commit of
   the fork is exactly the same as ours** (`9332440`), so the diff is clean.
3. The build artefacts were excluded from the copy. `myApp.tar` (62 MB) and
   `LivePaper-win64.zip` (61 MB) are the result of the build and do not belong
   into the repository — `*.tar` was added to `.gitignore` for that.
4. All 31 files (the tracked ones and the new ones) were copied with
   `git ls-files -c -o --exclude-standard`, so only the files that belong into
   the repository are copied. **Every file was checked with `md5sum`: they are
   byte-identical.**
5. The changes were staged (`git add -A`), so the user only has to commit and
   push.

**The verification of the clone:**

| Check | Result |
| --- | --- |
| The files are byte-identical to the source | ✅ 31 of 31 |
| The Linux build from the clone | ✅ exit 0, no warnings |
| The Windows cross-build from the clone | ✅ exit 0, no warnings |
| The build artefacts are not in the clone | ✅ none |
| The CI (`.github/workflows/build.yml`) | ✅ unchanged, it still works |

**How the user commits and pushes it:**

```bash
cd ~/git/LivePaper-Win11-Support
git commit -m "Fix the wallpaper on Windows 11 24H2, add diagnostics and Linux build"
git push origin main
```

**The result:** the user committed as `17fdb35` *"Adding windows 11 support"*
and pushed it. The push was verified:

| Check | Result |
| --- | --- |
| The fork on GitHub | ✅ `ZMajeh/LivePaper-Win11-Support`, a fork of `zuranthus/LivePaper` |
| The history of `main` | ✅ `17fdb35` on top of `9332440` — the upstream history is intact |
| The files in the tree of `main` | ✅ 31 files, exactly the ones of the working copy |
| A **fresh clone** compared with the working copy | ✅ all 31 files byte-identical |
| The build of the fresh clone | ✅ exit 0, no warnings |
| The local branch | ✅ `main` is in sync with `origin/main`, nothing left unstaged |

---

### Task 26 — The CI: the release fails with 403, and the MSVC warnings 🔄 in progress

**What the user reported:** the CI build on the fork **succeeded** — both
`live-paper.exe` and `live-paper-console.exe` were produced — but the step
*Publish* failed:

```
Error: Error 403: Resource not accessible by integration
  https://docs.github.com/rest/releases/releases#create-a-release
```

and the Ubuntu job skipped the steps *Package* and *Publish*.

**The three separate problems that were found:**

1. **The 403 — the workflow has no `permissions:` block.** The
   `ncipollo/release-action` needs `contents: write` to create a release
   (the documentation of the action says exactly that). The default token of
   a **fork** only has read access, so the creation of the release is
   refused. The upstream repository was created in 2021, when the default
   token still had write access — that is why it worked there and why it
   does not work in the fork. **The fix:** a `permissions:` block with
   `contents: write` in the job.

2. **`makeLatest: true` together with `prerelease: true` is not allowed.**
   GitHub does not let a *prerelease* be marked as the *latest* release — the
   API answers with an error. The combination was added upstream in December
   2024 (`b83bf2b`) and **never ran successfully** since: the upstream
   repository was archived, and the release that is still online is from
   2021. **The fix:** `makeLatest: false`.

3. **The Ubuntu job skips *Package* and *Publish* — this one is correct.**
   The steps have the condition `runner.os == 'Windows'`, because the release
   contains the **Windows** program. The Linux job is there to check that the
   code still compiles and works on Linux. It must stay that way.

**The warnings of MSVC — one of them is a real bug:**

| Warning | Where | What it is |
| --- | --- | --- |
| `C4013: 'strtol' undefined` | `src/main.c:298` | **a real bug** — `<stdlib.h>` is missing |
| `C4996: 'strcpy'` | `src/platform_windows.c:78` | a possible overflow of the buffer |
| `C4996: 'sprintf'`, `'vsprintf'` | `src/fail.c:20,22,23` | a possible overflow of the buffer |
| `C4996: 'strdup'` | `src/main.c:243,277` | the POSIX name, deprecated by MSVC |
| `C4996: 'fopen'`, `'freopen'` | `src/log.c:30,54,56,57` | the "unsafe" variants of MSVC |
| `C4244: 'Uint64' to 'double'` | `src/video.c:290` | a wrong type of the field |

* The `strtol` is **a real defect**: the function was used without its
  header, so the compiler assumed that it returns an `int` — and a `long` is
  64 bits wide on Linux. The value was truncated. (`<stdlib.h>` is now
  included.)
* The `sprintf`/`vsprintf` in `fail.c` and the `strcpy` in
  `platform_windows.c` could **overflow** their buffers — they are now
  `snprintf`/`vsnprintf` and `snprintf`, which are the safe versions and are
  part of C99.
* The `decode_start_ms` field was a `double` that holds a tick count — it is
  now a `Uint64`, so the subtraction of the two tick counts is exact.
* The rest (`fopen`, `freopen`, `strdup`) are the standard functions of C99;
  MSVC only warns about them. They are silenced with `_CRT_SECURE_NO_WARNINGS`
  and `_CRT_NONSTDC_NO_DEPRECATE`, which are only defined for MSVC.

**What was changed:**

* `.github/workflows/build.yml`: the `permissions:` block, `makeLatest: false`,
  `actions/checkout@v4` (v2 is deprecated), `cpack -C Release` (the
  configuration has to be named for a multi-configuration generator), and a
  comment that says why the Linux job does not publish.
* `src/main.c`: `<stdlib.h>` is included now.
* `src/fail.c`, `src/platform_windows.c`: the safe versions of the functions.
* `src/video.c`: the type of `decode_start_ms`.
* `CMakeLists.txt`: the two definitions for MSVC, the console program is
  installed as well, and the address of the repository in the package.

**Status:** compiled and verified on Linux and with MinGW. The CI has to be
run again to see whether the release is created.

---

### Task 27 — Publish the program for Linux as well ✅ done

**What the user asked for:** the release contained only the program for
Windows; the program for Linux should be published too.

**The problem that had to be solved first:** the *Publish* step ran in the
same job as the build, and the matrix of the job builds for both systems at
the same time. If both of them published, they would have used the **same**
release and the same tag, and the option `removeArtifacts: true` of the action
**deletes all the files of the release** before it uploads the new ones — the
two jobs would have deleted the files of each other. The source of the action
says so:

```ts
const releaseAssets = await this.releases.listArtifactsForRelease(releaseId)
for (const artifact of releaseAssets) {
    await this.releases.deleteArtifact(asset.id)
}
```

**The solution:** the work is split into two jobs.

1. The job `build` builds and packages for every system, and hands the
   package over with `actions/upload-artifact`.
2. The job `release` waits for it (`needs: build`), collects the packages of
   all the systems (`actions/download-artifact` with `merge-multiple: true`)
   and creates **one** release with all of them.

**What was changed:**

* `CMakeLists.txt`: the package is built for Linux as well now. The
  installer of Windows is made with the WIX toolset, which only exists there;
  Linux gets a tarball (`TGZ`) with the program in `bin/`. The libraries of
  SDL and of FFmpeg are the ones of the system on Linux, so the package
  contains only the program.
* `.github/workflows/build.yml`: the two jobs, the collection of the
  packages, and the tag `latest` is moved to the commit of the build (the
  action does not move the tag of a release that already exists).
* `README.md`: the addresses of the releases of the fork, the package for
  Linux, and the way to build it.

**The result:** the CI published the release *Development Build* with all
three files, and the tag `latest` points to the commit of the build
(`b4720df`):

| File | Size |
| --- | --- |
| `LivePaper-0.0.0-win64.msi` | 49.5 MB |
| `LivePaper-0.0.0-win64.zip` | 49.2 MB |
| `LivePaper-0.0.0-linux-x86_64.tar.gz` | 50 kB |

The release is here:
<https://github.com/ZMajeh/LivePaper-Win11-Support/releases/tag/latest>

**Everything is in the fork** — the release is created in
`ZMajeh/LivePaper-Win11-Support`, not in the upstream repository (which is
archived and read-only anyway).

---

### Task 28 — The automatic start on Windows 11 (scheduled task) ✅ done

**What was asked for:** the way to start LivePaper automatically on Windows 11,
without changing the program. The program is installed in
`C:\Program Files\LivePaper\live-paper.exe`, and the wallpaper is
`C:\Users\mundh\OneDrive\Documents\Video wallpaper final(1).gif`.

**What was done:** the instructions were added to the *Usage → Windows* section
of the README — the scheduled task (with the GUI, with PowerShell and with
`schtasks`), the startup folder and the registry key.

**The details that only matter because of the way the program is written:**

* **"Run only when the user is logged on", not at boot.** The program attaches
  its window to the desktop of the *interactive* session
  (`src/platform_windows.c`, `FindCurrentDesktopProgman()`). A task that runs
  at the boot or "whether the user is logged on or not" runs in a
  non-interactive session, where there is no desktop for the wallpaper.
* **"Start in" must be `C:\Program Files\LivePaper`.** The tray icon is loaded
  from the relative path `icon.ico` (`src/platform_windows.c`, the `tray`
  structure), which is resolved against the working directory of the process.
  The default working directory of a task is `C:\Windows\System32`, so without
  this setting the program would start, but the tray icon would be missing —
  and the tray icon is the only way to quit the program. `schtasks` has no
  option for the working directory, which is why the GUI and the PowerShell
  variants are the recommended ones.
* **A delay of 30 seconds.** The program needs the desktop of Explorer to
  exist before it can attach the wallpaper to it; if it starts too early it
  fails with "can't find the Progman window of the desktop" and shows an error
  box at the logon.
* **The wallpaper is passed as an argument.** Without a file on the command
  line the program opens a file selection dialog (`PlatformInitGuiMode()`),
  which is not what one wants at the logon.
* **The execution time limit of the task has to be removed.** It is 3 days by
  default, and a live wallpaper runs until the logoff.
* **The file is in OneDrive.** It has to be marked as "Always keep on this
  device", otherwise it can still be a cloud placeholder when the task starts,
  and the program cannot read it (`CheckFileReadable()` in `src/video.c`).

**No source code was changed** — the task was a documentation one.

---

### Task 29 — The top dock bar (the hover bar) ✅ done

**What the user asked for** (this is the wording of the request):

* A **simple top bar** that is shown over the wallpaper.
* The bar is **hidden** and only becomes visible when the mouse is **hovered
  at the top of the screen**; then it **flips/slides down from above the
  screen** and stays there.
* On the **top left** of the bar there is an **add button**.
* The add button has **two options**:
  1. **launch the file browser** (the file manager of the system),
  2. **select a file or a folder** and add it to the bar.
* An **added folder**: clicking it opens a **temporary popup window** with the
  **list of the files** of that folder; clicking a file in the list launches it.
* An **added file**: clicking it **launches that file** directly.
* On the **top right** of the bar the **system details** are shown:
  **CPU load**, **RAM usage**, **upload/download speed**, **disk load** and the
  **time**.

**The answers of the user** (2026-10-21), which settled the four open
questions of the plan:

1. **"Disk load"** means the **activity** of the disk (the share of the time
   that the disks were busy), not the used space of the drive.
2. The bar is **on by default**, and the **tray menu** has a checkable item
   *Dock bar* that switches it off and on again.
3. The bar is on **every virtual desktop**, at the place where the wallpaper
   is: it follows the wallpaper when the user switches to another desktop.
4. The **desktop icons are switched off completely** — the work on the icons
   (task 23) is dropped.

**What was built** (the plan is above, the code follows it):

| File | What it does |
| --- | --- |
| `src/dock.h` / `src/dock.c` | the model: the items, the state machine (hidden / showing / shown / hiding), the hit testing, the popup, the file `dock.txt` |
| `src/dock_ui.h` / `src/dock_ui.c` | the drawing of the bar, of the menu of the add button and of the popup |
| `src/dock_font.h` / `src/dock_font.c` | the built-in 5x7 bitmap font (no new dependency) |
| `src/dock_metrics.h` / `src/dock_metrics.c` | the sampling of the CPU, the RAM, the network, the disks and the clock (once per second) |
| `src/dirlist.h` / `src/dirlist.c` | the listing of a folder and the paths (the folders come first) |
| `src/launcher.h` / `src/launcher.c` | `LauncherOpenFile()` / `LauncherOpenFolder()` — the shell of the system |
| `platform.h` | the new hooks: the window of the bar, its position, its visibility, the click-through, the position of the mouse, the size of the screen, the path of `dock.txt` |

**The decisions that were taken while writing the code:**

* **The file picker is not a dialog of the system** — it is the popup of the
  bar itself (the same widget as the list of the files of a folder). It works
  the same way on every system, it needs no new dependency and it looks like
  the rest of the bar. The rows are: `..`, the content of the folder (the
  folders first) and `Add this folder`.
* **The popup is part of the window of the bar**: the window is as tall as the
  bar and the popup, and it is moved as a whole, so the popup comes down with
  the bar.
* **The font is drawn by hand** (5x7, the characters are doubled to 10x14). A
  scaled-down outline font is not readable at that size — the letters `g` and
  `y` cannot be told from `9` and `4`.
* **The cursor is polled** (`PlatformGetCursorPos()`), because the bar has no
  focus and the wallpaper is click-through: it does not get the mouse events
  of the desktop.
* **The bar follows the virtual desktop**: the window is moved with
  `IVirtualDesktopManager::MoveWindowToDesktop()` when the user switches.
* **The metrics are sampled once per second**, and the bar is only drawn when
  it is not completely hidden.

**The verification:**

* The Linux build and the cross-compile for Windows (`makeLinuxToWindow.sh`)
  are clean — no warnings, no errors.
* A self check of the modules (`/tmp/dockcheck.c`) covers the width of the
  text, the listing of a folder, the paths, the values of the system, the
  creation of the window, the items and the round trip through `dock.txt`:
  all of them pass.
* The bar and the font were drawn into a picture (`assets/dock.png`), to see
  that they look right.
* **It was not tested on Windows** — there is no Windows machine and no Wine
  in this container. The behaviour on Windows (the slide, the clicks, the
  focus, the virtual desktops) has to be checked by the user.

**The bugs that the self check found** (they are fixed):

* The first line of `/proc/stat` starts with the word `cpu`, so the counters
  were not read and the load of the processor was always unknown.
* The transmitted bytes are the **ninth** number of a line of `/proc/net/dev`,
  not the eighth one.
* The counters of the network of Windows are 32 bits wide and they wrap
  around: the difference is computed with the 32 bits arithmetic.

**The bugs that the tests on Windows found** (they are fixed):

* **The program crashed on the start.** The sampler of the network walked over
  the table of the interfaces with the wrong arithmetic (`row += i` inside the
  loop, so the pointer ran over the end of the table after a few rows). The
  table of the interfaces is now read through a structure of its own, so that
  the name of the member with the rows does not matter (it is `Table` in the
  headers of MSVC and `table` in the ones of MinGW).
* **The add button of the bar did nothing.** Two causes:
  1. The bar was placed **below** the window of the desktop, so the layer of
     the desktop icons (a child of that window) was above it and took the
     clicks. The bar is now placed **directly above** the window of the
     desktop, so it gets the clicks.
  2. The mouse events of SDL carry the window that has the **mouse focus**,
     and not the window that was clicked (see `SDL_PrivateSendMouseButton()`).
     The bar never has the focus — it is a `WS_EX_NOACTIVATE` window — so the
     events were thrown away by the filter on the window. The position of the
     mouse is now asked for from the system and the events are not filtered by
     the window any more.
* The bar hides itself by **moving above the screen** now, instead of with
  the `WS_EX_TRANSPARENT` style, which does not work for a window of the
  first level.

**The desktop icons are switched off** (the fourth answer of the user): the
default of `g_icons_mode` is `ICONS_NONE` and the periodic
`PlatformRefreshDesktopIcons()` call is removed from the main loop. The
strategy 6 stays the default on Windows, so the video is shown and the clicks
reach the desktop.

---

#### 29.1 The constraints of the existing program

Everything has to fit into the architecture that exists today:

| Constraint | Consequence for the bar |
| --- | --- |
| The wallpaper window is a **child of `WorkerW`** (strategies 0-5) or a **top-level popup** (strategy 6, the default on Windows) | The bar **cannot be a child of the wallpaper window** — it would be painted *below* the desktop icons and the clicks would not reach it. It has to be a **separate top-level window** of our own process. |
| The program is a **GUI application** on Windows (no console) | The bar is the only visible UI; the diagnostics stay in the log. |
| The renderer is the **software renderer** on Windows | The bar must not depend on the accelerated path — the same renderer code has to work with both. |
| There is **no font library** in the dependencies (no SDL2_ttf) | The text of the bar needs a **built-in bitmap font** (see 29.4) or a new dependency. |
| The main loop is a **single `while` loop with `SDL_PollEvent`** in `src/main.c` | The bar is updated and drawn from that loop; its events arrive through the same queue. |
| The program already has a **config file** (`%LOCALAPPDATA%\LivePaper\config.txt` on Windows, `InitConfigPath()` in `src/platform_windows.c`) | The list of the added items is stored in the same place, in a second file `dock.txt`. |
| The `--desktop-icons` option cannot be given twice (argtable3) | Every new option has to be designed so that it can be given **once** only. |

---

#### 29.2 The module breakdown (the modular approach)

The bar is **not** one big file. It is split into small modules with a clear
boundary between them, so that the platform code, the drawing and the model
can be changed independently. The convention of the project is followed: a
header per module, the `Foo()` prefix for the functions, the platform code in
`platform_*.c`.

```
src/
  dock.h              the public interface of the bar (the only header that
                      main.c includes)
  dock.c              the model: the items, the state machine, the layout,
                      the hit testing, the config file
  dock_ui.c           the drawing: the bar, the buttons, the items, the
                      system details, the popup window
  dock_font.c         the built-in bitmap font (the text drawing)
  dock_metrics.c      the system details: CPU, RAM, network, disk, the clock
  launcher.c          "open this file/folder with the program of the system"
  filepicker.c        the file/folder selection dialog
  platform.h          + the new hooks of the platform layer (see 29.5)
  platform_windows.c  + the Windows implementation of the hooks
  platform_linux.c    + the Linux implementation (a stub at first)
  platform_macos.c    + the macOS implementation (a stub)
```

The dependencies are one-directional — nothing points back to `main.c`:

```
        main.c
          |
        dock.h  <---- the only interface
          |
   +------+------+----------------+
   |      |      |                |
 dock.c  dock_ui.c  dock_metrics.c  launcher.c / filepicker.c
   |        |          |
   |     dock_font.c    |
   +--------+-----------+
            |
      platform.h  (the window, the cursor, the shell integration)
```

**The responsibilities of every module:**

| Module | Responsibility | Does **not** do |
| --- | --- | --- |
| `dock.c` | the list of the items (file/folder), the state machine (hidden / sliding in / shown / sliding out), the geometry of the bar, the hit testing, the load/save of the config file | no drawing, no platform calls |
| `dock_ui.c` | everything that is drawn: the background of the bar, the add button, the items, the popup with the file list, the system details | no state changes — it only *reports* what was clicked |
| `dock_font.c` | the bitmap font and the text drawing (the width of a string, the drawing of a string) | nothing else — it knows nothing about the bar |
| `dock_metrics.c` | the sampling of the system values (once per second), the formatting of the strings (`"12 %"`, `"1.2 MB/s"`, `"12:34"`) | no drawing |
| `launcher.c` | `LauncherOpenFile(path)` and `LauncherOpenFolder(path)` — the shell of the system | no UI |
| `filepicker.c` | the dialog that asks the user for a file **or** a folder, with the option "add a file" / "add a folder" | no drawing of the bar |
| `platform_*.c` | the window of the bar, the position of the mouse, the z-order, the "click through" | no model, no drawing |

---

#### 29.3 The model (`dock.h`)

```c
// What an item of the bar is.
enum DockItemKind {
    DOCK_ITEM_FILE = 0,    // a file: clicking it launches the file
    DOCK_ITEM_FOLDER,      // a folder: clicking it opens the list of the files
};

// One entry of the bar.
struct DockItem {
    enum DockItemKind kind;
    char *path;            // the full path (UTF-8 on Windows)
    char *label;           // the name that is drawn (the file name)
};

// The state of the bar.
enum DockState {
    DOCK_STATE_HIDDEN = 0, // parked above the screen, invisible
    DOCK_STATE_SHOWING,    // sliding down
    DOCK_STATE_SHOWN,      // fully down, the mouse is over the bar
    DOCK_STATE_HIDING,     // sliding up
};

// The whole state of the bar.
struct Dock {
    enum DockState state;
    SDL_Window *window;        // the top-level window of the bar
    SDL_Renderer *renderer;    // its renderer
    int width, height;         // the size of the bar in pixels
    int screen_width, screen_height;

    struct DockItem *items;    // the added items
    int item_count;
    int item_capacity;

    bool add_menu_open;        // the menu of the add button is open
    int add_menu_hover;        // -1: none, 0: the file browser, 1: the file/folder

    bool popup_open;           // the temporary window with the file list
    char *popup_folder;        // the folder that is shown
    char **popup_entries;      // the names of the files
    int popup_entry_count;
    int popup_hover;
    int popup_scroll;

    uint64_t anim_start_ms;    // the beginning of the slide
    uint64_t last_input_ms;    // the last click (the popup closes after a while)
    bool enabled;              // false: the bar is not created at all
};
```

**The public interface** (the only thing that `main.c` uses):

```c
void DockInit(struct Context *ctx, struct Dock *dock);
void DockUpdate(struct Context *ctx, struct Dock *dock);   // the state machine
void DockDraw(struct Context *ctx, struct Dock *dock);     // the drawing
void DockHandleEvent(struct Context *ctx, struct Dock *dock, const SDL_Event *e);
void DockCleanup(struct Context *ctx, struct Dock *dock);
```

`DockUpdate()` is called **once per frame** from the main loop, next to
`PlatformUpdate()`. It does four things, in this order:

1. **the mouse position** is read (`PlatformGetCursorPos()`) — the bar cannot
   use the SDL mouse events, because it does not have the focus;
2. the **state machine** is advanced (the slide is animated, the bar is hidden
   again when the mouse leaves it);
3. the **system details** are refreshed (once per second, not once per frame);
4. the window is **moved** to the position that the state requires.

---

#### 29.4 The drawing and the font

**The font.** The program has no font library. Two ways out:

* **A built-in bitmap font** (`dock_font.c`) — a 5x7 pixel font for the
  printable ASCII, stored as 5 bytes per character (95 characters = 475
  bytes). It is drawn with `SDL_RenderFillRect()` per pixel, so it works with
  the software renderer and needs no new dependency. **This is the plan.**
* SDL2_ttf — a real font, but a new dependency for the MSVC build (a download
  in `CMakeLists.txt`), for the MinGW build and for the Linux package.

The bitmap font is enough for the system details (`"CPU 12%"`, `"1.2 MB/s"`,
`"12:34"`) and for the file names (the long names are cut with `...`).

**The layout of the bar** (the height is 36 px by default):

```
+----------------+--------------------------------------------+------------------+
| [ + ]  |  item  |  item  |  item ...     |  CPU 12%  RAM 3.1/16G  ^1.2M v340K  D 7%  12:34 |
+----------------+--------------------------------------------+------------------+
   left                                                            right
```

* **left**: the **add button** — a square with a `+` in it.
* **middle**: the **items** — the icon of the kind (file / folder) and the
  label; the items that do not fit are cut with `...`.
* **right**: the **system details**, each in its own group, separated by a
  thin line.

**The colours**: a dark, semi-transparent bar (`#202020` at 90 %), the text in
`#E0E0E0`, the items in the accent colour when the mouse is over them, the
add button in the accent colour when its menu is open.

**The add menu** (the two options of the add button) — a small panel under the
button:

```
+------------------+
|  File browser    |
|  Add file/folder |
+------------------+
```

**The popup window of a folder** — a separate small window under the bar,
with the list of the files of the folder (the folders first, then the files,
both sorted). Clicking a file launches it, clicking a folder opens it in the
same popup. The popup closes when the mouse clicks outside of it, or after
30 seconds without a click.

---

#### 29.5 The platform hooks (`platform.h`)

The bar needs seven things from the system. They are added to `platform.h`,
so that `dock.c` never talks to Windows directly.

```c
// The window of the bar: a top-level window that is not activated when it is
// clicked (the desktop keeps the focus) and that can be made transparent for
// the mouse (the clicks pass through it to the desktop).
bool PlatformCreateDockWindow(struct Context *ctx, SDL_Window **out,
                              int width, int height);
void PlatformDestroyDockWindow(SDL_Window *window);
void PlatformSetDockPosition(SDL_Window *window, int x, int y);
void PlatformSetDockClickThrough(SDL_Window *window, bool click_through);

// The position of the mouse on the whole screen, in the coordinates of the
// desktop. This is the only way to know that the mouse is at the top of the
// screen: the bar does not have the focus, so it gets no mouse events.
void PlatformGetCursorPos(int *x, int *y);

// The size of the screen of the desktop.
void PlatformGetScreenSize(int *width, int *height);
```

**The Windows implementation** (the part that needs the most care):

* The window is created by SDL (`SDL_CreateWindow` with
  `SDL_WINDOW_BORDERLESS`), and the `HWND` is taken out with
  `SDL_GetWindowWMInfo()`.
* The styles that are added to it: `WS_EX_TOOLWINDOW` (it does not appear in
  the task bar), `WS_EX_NOACTIVATE` (the click does not take the focus away
  from the desktop, so the desktop icons keep working) and `WS_EX_LAYERED`
  (the alpha of the slide).
* **The click-through**: `WS_EX_TRANSPARENT` is added when the bar is hidden
  and removed when it is shown, so that the clicks on the top of the screen
  reach the desktop while the bar is away.
* **The z-order**: the bar is placed **directly above the wallpaper window**
  with `SetWindowPos(dock_hwnd, wallpaper_hwnd, ...)`. That way the bar is
  above the wallpaper but **below the other applications** — it does not
  float over everything. If the wallpaper is a child of `WorkerW` (not a
  top-level window), `HWND_TOPMOST` is used instead.
* The position is set with `SetWindowPos()` — the slide is done by moving the
  window from `y = -height` to `y = 0`.

**The Linux implementation** (the second version, after Windows works):

* The window is an SDL window with `SDL_WINDOW_BORDERLESS`; the position is
  set with `SDL_SetWindowPosition()`.
* The "always on top" is asked for with the `_NET_WM_STATE_ABOVE` hint of
  X11; the click-through is not possible with a plain SDL window, so the
  Linux version **keeps the bar on the screen** and only moves it away
  (out of the screen, `y = -height`).
* The mouse position comes from `XQueryPointer()`.

**The macOS implementation** is a stub at first (the macOS support of the
program is a stub as well).

---

#### 29.6 The system details (`dock_metrics.c`)

Every value is sampled **once per second** — not once per frame, so that the
CPU load of the program itself stays low.

| Value | Windows | Linux | What is drawn |
| --- | --- | --- | --- |
| **CPU load** | `GetSystemTimes()` — the difference of the idle time to the total time | `/proc/stat` — the same idea | `CPU 12%` |
| **RAM usage** | `GlobalMemoryStatusEx()` — the used memory of the whole system (not the memory of the process!) | `/proc/meminfo` — `MemTotal` - `MemAvailable` | `RAM 3.1/16 G` |
| **Upload / download** | `GetIfTable2()` — the sum of the bytes of the interfaces, compared with the value of the second before | `/proc/net/dev` — the same | `^1.2 M  v 340 K` (bytes per second) |
| **Disk load** | `GetDiskPerformanceCounter()` of the physical disks — the time that the disk was busy, compared with the time | `/proc/diskstats` — the same | `D 7%` |
| **Time** | `GetLocalTime()` | `localtime()` | `12:34` |

**A decision that has to be confirmed by the user:** "disk load" can mean two
different things — the **activity** of the disk (how busy it is) or the **used
space** of the system drive. The plan uses the **activity** (it is the same
kind of value as the CPU load); if the used space is meant, the implementation
is one call (`GetDiskFreeSpaceEx()` / `statvfs()`) instead.

**The memory of the program is not shown** — the bar shows the memory of the
*system*, not of LivePaper. The memory of the process stays in the log
(`LogStats()`).

---

#### 29.7 The new files and the new options

**The files that are added:**

| File | Lines (rough) | Content |
| --- | --- | --- |
| `src/dock.h` | 120 | the public interface, the structures |
| `src/dock.c` | 450 | the model, the state machine, the config file |
| `src/dock_ui.c` | 500 | the drawing of the bar, the menu, the popup |
| `src/dock_font.c` | 200 | the bitmap font, the text drawing |
| `src/dock_metrics.c` | 350 | the sampling of the system values |
| `src/launcher.c` | 80 | `LauncherOpenFile()` / `LauncherOpenFolder()` |
| `src/filepicker.c` | 200 | the file/folder dialog |

**The new options of the command line** (all of them can be given **once**,
because of the argtable3 limitation):

| Option | Meaning |
| --- | --- |
| `--no-dock` | the bar is not created at all (the default is that it is) |
| `--dock-height=<n>` | the height of the bar in pixels (the default is 36) |
| `--dock-trigger=<n>` | how many pixels at the top of the screen trigger the bar (the default is 2) |
| `--dock-timeout=<n>` | after how many seconds the bar hides itself without a mouse (the default is 3) |

**The config file**: the items are stored in `dock.txt` next to `config.txt`
(`%LOCALAPPDATA%\\LivePaper\\dock.txt` on Windows), one path per line, the
folders with a trailing `/`. The file is read when the program starts and
written when an item is added or removed.

**The CMakeLists.txt** has to be changed: the new files are added to both
targets (`live-paper` and `live-paper-console`), and `shell32`/`ole32` are
added for the file dialog and the shell calls on Windows.

---

#### 29.8 The phases of the work

Each phase is a step that can be built and tested on its own.

| Phase | What | Done when |
| --- | --- | --- |
| **P1** | the **window** of the bar: it slides down when the mouse is at the top of the screen and hides itself again | the bar moves, the clicks pass through it to the desktop |
| **P2** | the **drawing**: the background, the font, the add button (the icon only, no function yet) | the bar looks like a bar |
| **P3** | the **add button** and its **two options**: the file browser (`explorer.exe` / `xdg-open`) and the file/folder dialog | the file manager opens; a file can be chosen |
| **P4** | the **items**: the list, the config file, the click on a file launches it, the click on a folder opens the popup with the file list | the whole "add and use" flow works |
| **P5** | the **system details** on the right | the values are shown and they are correct |
| **P6** | the **polish**: the animation, the alpha, the multi-monitor, the README, the release | the program is released with the bar |

**The order of the work in `main.c`** (the only place that changes in the
existing code):

```c
InitContext(&context);            // the wallpaper, as today
DockInit(&context, &dock);        // the bar
...
while (!quit) {
    ...
    DockUpdate(&context, &dock);  // the state machine + the system details
    VideoUpdate(delta_sec, video, &context);
    PlatformUpdate(&context);
    ...
    DockDraw(&context, &dock);    // the drawing of the bar
    ...
    while (SDL_PollEvent(&event)) {
        DockHandleEvent(&context, &dock, &event);   // the clicks on the bar
        ...
    }
}
```

---

#### 29.9 The risks and the open questions

| Risk | How it is handled |
| --- | --- |
| The bar **steals the focus** from the desktop and the icons stop working | `WS_EX_NOACTIVATE` on Windows; the window is never activated |
| The bar **covers the wallpaper** and the clicks do not reach the desktop | `WS_EX_TRANSPARENT` while it is hidden; the bar is only 36 px high |
| The bar is **above the other applications** | it is placed directly above the wallpaper window, not `TOPMOST` |
| The bar **costs CPU** (the program is a wallpaper, it has to be quiet) | the system values are sampled once per second; the bar is only drawn when it is not fully hidden; the animation is 200 ms |
| The bar **does not appear** on Windows, because the wallpaper is a child of `WorkerW` | the bar is a **top-level** window, not a child — it is not affected by the strategy of the wallpaper |
| The **font** is ugly or unreadable | the bitmap font is 5x7, drawn with the pixels doubled (so 10x14) — readable |
| The **popup with the file list** gets in the way | it closes on a click outside of it and after 30 seconds |
| The **file names** are long | they are cut with `...`; the full name goes into the log |

**The questions for the user** (they do not block the start of P1):

1. **"Disk load"** — the activity of the disk or the used space of the drive?
2. Should the bar be **on by default** on Windows, or only with `--dock`?
3. Should the bar be on **every monitor** or only on the main one? (the plan:
   only on the main one, in the first version)
4. Should the added items be **remembered between the runs**? (the plan: yes,
   in `dock.txt`)

---

#### 29.10 What is *not* in the plan (the first version)

* No clock widget, no calendar, no media keys.
* No drag and drop of the files onto the bar.
* No search field.
* No editing of the items from the bar — the file is edited by hand.
* No more than one bar (the multi-monitor is not in the first version).

---

### 30. The add button does nothing ✅ done

**The report of the user (2026-10-21):** the click on the add button is
registered (`the add button of the bar was clicked` in the log), but nothing
happens — no menu, no file browser.

**The cause.** The popup (the menu of the add button and the list of the files
of a folder) was drawn **inside the window of the bar**, below the bar. The
window of the bar was resized when the popup was opened, so that the popup was
inside the window. But:

* The window was only resized in `DockSyncWindowSize()`, which was called at
  the **end** of `DockUpdate()`. The click is handled in `DockHandleEvent()`,
  which runs **after** the drawing of the frame. So the menu was opened in the
  frame *n* and the window was resized in the frame *n+1*: on the frame *n* the
  menu was drawn outside of the window and was cut away.
* Even after the resize, the menu of the add button was **not drawn at all**:
  `DrawAddMenu()` was only called for `DOCK_POPUP_ADD_MENU`, but the popup was
  resized with `DockVisibleRows()` — the number of the rows of the *folder*
  popup, which is **zero** for the menu of the add button. The window was
  therefore not tall enough for the menu.
* The window of the bar is as wide as the screen. A popup that is drawn inside
  it covers the whole width of the screen with the background of the bar.

**The fix.** The popup has a **window of its own** now:

* `PlatformCreateDockPopupWindow()` creates it (top level, without a border,
  without a title, `WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW`, like the bar).
* It is placed **below the bar**, and it is only as big as the rows that are
  shown, so it does not cover the rest of the screen.
* The menu of the add button is not a special case any more: its two options
  ("File browser" and "Add file or folder") are **rows** of the popup, like
  the rows of a folder. There is only one kind of row and one kind of popup.
* The hit test of the rows uses the position of the mouse **on the screen**,
  because the popup has a window of its own and the coordinates of the window
  of the bar do not apply to it.
* The z-order of the popup is fixed after it is shown, because showing a
  window can change its place: `PlatformPlaceDockPopupWindow()`.

**The files:** `src/dock.c`, `src/dock.h`, `src/dock_ui.c`, `src/dock_ui.h`,
`src/platform.h`, `src/platform_windows.c`, `src/platform_linux.c`,
`src/platform_macos.c`.

**The check:** the Linux build, the Windows cross-build and the headless check
(`/tmp/dockcheck.c`) pass. The drawing of the menu was checked with a
rendering of the bar into a bitmap.

**What is left to do:** test it on Windows — the menu, the file browser, the
file picker, the folder popup and the launch of the files.

### 31. The add button still shows nothing on Windows 🔄 in progress

**The report of the user (2026-10-22, 02:11):** the click on the add button is
registered, but nothing is shown. The user wants the add button to **open a
file browser** where a file or a folder can be chosen.

**What the log of the user shows (this is the important part):**

```
[02:11:36] DEBUG: a click at (34,11) in the bar
[02:11:36] INFO : the add button of the bar was clicked
[02:11:36] INFO : the menu of the add button was opened
[02:11:36] DEBUG: the bar of the dock was placed above the desktop   (x hundreds)
```

**The conclusion:** the **state machine works** — the click is received, the
add button is found, and the menu of the add button is opened. The problem is
**only that the window of the popup is not visible on the screen**. The
hundreds of `the bar of the dock was placed above the desktop` lines are the
`PlatformPlaceDockPopupWindow()` call of `DockUpdate()`, which runs on **every
frame** while the popup is open. It is not an error, but it is a lot of noise
and it is a sign that the popup is being repositioned all the time.

**The three causes that are still possible** (there is no Windows machine and
no Wine in this container, so they cannot be told apart here):

1. **The z-order of the popup is wrong.** The bar is placed *directly above the
   desktop* (`PlaceDockAboveDesktop()`), so that the layer of the desktop
   icons (a child of `Progman`) cannot swallow the clicks. The popup is
   placed with the same rule, but it is a **separate window**: it is created
   with `PlatformCreateDockPopupWindow()` and it is placed above the desktop
   **before** it is shown. Showing a window can change its place in the
   z-order, and `PlatformPlaceDockPopupWindow()` is called on every frame to
   fix it — but the `SetWindowPos()` of the popup is done with `SWP_NOMOVE |
   SWP_NOSIZE`, so the window keeps the size it had when it was created. If the
   size is wrong, the window is not where it is expected.
2. **The popup is behind the wallpaper.** The wallpaper is a top level popup
   window (strategy 6) that covers the whole screen. The bar is *below* the
   other applications but *above* the desktop. The popup is placed with the
   same rule, but it is created **later**, so it can end up below the
   wallpaper of the strategy 6.
3. **The window is shown but it is empty.** `DockUiDrawPopup()` is a separate
   function now, and it is called from `DockUiDraw()`. The renderer of the
   popup is created with `SDL_RENDERER_SOFTWARE`. If the window of the popup
   is never shown (`PlatformSetDockVisible()` uses `SW_SHOWNA`), the
   `SDL_RenderPresent()` of the popup does nothing.

**The decision: do not debug the custom popup any more — use the native file
dialog of Windows.**

The user asked for a **file browser**. Windows has one that is perfect for
this: `IFileOpenDialog` (the dialog of the shell, the same one that is used by
"Open file" in every program). `FOS_PICKFOLDERS` would restrict the dialog to
folders only, so it is **not** used. Without `FOS_PICKFOLDERS` the dialog can
select **a file or a folder**, and it is a normal window, so it is always
visible and always above everything. It also saves a lot of work: the custom
popup, the own file picker and the own folder list are not needed any more.

**The plan (in this order):**

1. **Add `LauncherPickFile()` to `src/launcher.c` / `src/launcher.h`.** It
   opens the native dialog of the shell and returns the path that was chosen
   (or `NULL` if the user cancelled it).
   * Windows: `CoCreateInstance(CLSID_FileOpenDialog)` with the options
     `FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST`. The
     libraries `ole32` and `shell32` are already linked (see `CMakeLists.txt`),
     and `ole32` is enough for `IFileOpenDialog`. The result is taken with
     `IShellItem::GetDisplayName(SIGDN_FILESYSPATH)`.
   * Linux and macOS: the function returns `NULL` and logs that the native
     dialog is not available — the custom popup stays for these systems.
2. **The add button opens the dialog.** The click on the add button sets a
   flag; the main loop opens the dialog and calls `DockAddItem()` with the
   path that was chosen. The **kind** of the item (file or folder) is decided
   with the helpers of `src/dirlist.c` (there is probably already a function
   that tells a folder from a file).
3. **Keep the custom popup for the folders of the bar** (the list of the files
   of a folder that is an item of the bar). It is a nice feature, but it is
   not what the user is waiting for. It can be fixed later.
4. **The log spam.** `PlatformPlaceDockPopupWindow()` is called on every
   frame; it should only be called when the popup is shown. The `LOG_DEBUG()`
   of `PlaceDockWindowAboveDesktop()` should also be quieter.

**The result (implemented 2026-10-22):**

**Decision on `FOS_PICKFOLDERS`:** `FOS_PICKFOLDERS` restricts the dialog to
folders only, so it cannot be used when a file *or* a folder must be choosable.
The dialog is opened **without** `FOS_PICKFOLDERS`. The default options of the
Open dialog are queried with `GetOptions()`, then `FOS_FORCEFILESYSTEM` and
`FOS_PATHMUSTEXIST` are added, and `FOS_PICKFOLDERS` and `FOS_FILEMUSTEXIST`
(a default of the Open dialog that can require the selection to be a file) are
cleared. This way both files and folders appear and can be chosen. The kind is
decided afterwards with `DirIsFolder()` of `src/dirlist.c`.

**What was implemented:**

1. **`src/launcher.h` / `src/launcher.c`** — added `LauncherPickFile()`.
   * Windows: `CoCreateInstance(CLSID_FileOpenDialog)`. The options are taken
     from the dialog defaults, `FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST` are
     added, `FOS_PICKFOLDERS | FOS_FILEMUSTEXIST` are cleared. The result is
     taken with `IShellItem::GetDisplayName(SIGDN_FILESYSPATH)` and converted
     from UTF-16 to UTF-8 with `WideCharToMultiByte(CP_UTF8, ...)`.
   * The dialog is modal, so it runs on its own worker thread
     (`SDL_CreateThread` + `SDL_DetachThread`) with `CoInitializeEx` /
     `CoUninitialize` on that thread. The chosen path (UTF-8) is delivered to
     the main thread with an SDL event (`event.user.data1` holds the
     malloc'd string). When the user cancels, no event is sent.
   * Linux/macOS: returns `NULL` and logs that the native dialog is not
     available; the custom popup stays.
2. **`src/main.c`** — registered a new event type `g_pick_file_event` next to
   `g_open_file_event`. The main loop handles `g_pick_file_event`, calls
   `DockAddItem(g_dock, path, kind)` (`kind` from `DirIsFolder()`) and frees
   the path.
3. **`src/dock.c`** — the add button now calls `LauncherPickFile()` directly on
   Windows (native dialog). On Linux/macOS the existing custom popup menu
   (`DockOpenAddMenu`) is kept unchanged. Clicking a folder item on the bar
   already calls `DockOpenFolderPopup(dock, path, DOCK_POPUP_FOLDER)` which
   shows the files inside the folder.
4. **`CMakeLists.txt`** — added `uuid` to the Windows link libraries of both
   targets (needed for the `CLSID_FileOpenDialog` / `IID_IFileOpenDialog` GUIDs
   with MinGW). `ole32` and `shell32` were already linked.

**Verification:**
* The COM calls (`CoCreateInstance`, `GetOptions`, `SetOptions`, `Show`,
  `GetResult`, `GetDisplayName`, `WideCharToMultiByte`) compile and link cleanly
  with `x86_64-w64-mingw32-gcc` against `ole32`, `shell32` and `uuid` (tested
  with a standalone program).
* A full cross-build could not be run in this container because the MinGW SDL2,
  FFmpeg and argtable3 development packages are not installed; the source is
  syntactically consistent with the rest of the codebase.
* Folder support on the bar was already implemented (`DockOpenFolderPopup` with
  `DOCK_POPUP_FOLDER` shows the files inside a clicked folder; `DockSaveItems`
  / `DockLoadItems` persist folders with a trailing slash).

**The files changed:** `src/dock.c`, `src/launcher.c`, `src/launcher.h`,
`src/main.c`, `CMakeLists.txt`.

---

### Task 32 — Add button: two separate buttons (Add file / Add folder), folder popup navigation with search ✅ done

**The report of the user (2026-10-22):**

1. The add button should be **two separate buttons**: "Add file" and "Add folder", each opening its own dialog box.
2. When clicking a folder on the bar, show a popup window with all files in that folder, with a **search button on top**.
3. Clicking a subfolder in the popup should open another popup for that folder (navigation).
4. Files on the bar should have a **file icon**; folders should have a **folder icon** (the `DrawIcon()` function already does this).
5. Paths from the dialog / from `dock.txt` may have stray quotes (`'RamClean.lnk"'`), which break `DirIsFolder()` — quotes must be stripped.

**What was implemented:**

| Step | What was done |
|------|--------------|
| 1 | Added `LauncherPickFile()` (file-only dialog with `FOS_FILEMUSTEXIST`) and `LauncherPickFolder()` (folder-only dialog with `FOS_PICKFOLDERS`) to `src/launcher.c` / `src/launcher.h`. Both run on worker threads, deliver results via SDL event `g_pick_file_event`. Strip quotes from returned paths in `PickerThread`. |
| 2 | Changed the add button area to two buttons side by side in `src/dock.c` / `src/dock.h` / `src/dock_ui.c`. Button 1 ("Add file", labeled "F") calls `LauncherPickFile()`, button 2 ("Add folder", with folder icon) calls `LauncherPickFolder()`. Updated hit-testing and layout constants (`DOCK_ADD_BUTTON1_X`, `DOCK_ADD_BUTTON2_X`, `DOCK_ITEMS_X`). |
| 3 | Added `StripQuotes()` function and call it in `DockAddItemInternal()` so paths from dialog or `dock.txt` with surrounding quotes are cleaned before `DirIsFolder()` checks them. |
| 4 | Added a search field at the top of folder/picker popups in `src/dock_ui.c`. Click activates it, `SDL_TEXTINPUT` appends characters, `Backspace` deletes, `Escape` deactivates. Case-insensitive filtering of rows. |
| 5 | Subfolder navigation already works — clicking a folder row in the popup calls `DockOpenFolderPopup` recursively. |
| 6 | File/folder icons already work via `DrawIcon()` in `src/dock_ui.c` — shown on bar items and in popup rows. |

**Files changed:**
- `src/launcher.h` — declarations for `LauncherPickFile()` and `LauncherPickFolder()`
- `src/launcher.c` — Windows implementation with `IFileOpenDialog` using `FOS_PICKFOLDERS` or `FOS_FILEMUSTEXIST`, quote stripping, worker thread delivery; non-Windows stubs
- `src/dock.h` — new constants for two buttons (`DOCK_ADD_BUTTON1_X`, `DOCK_ADD_BUTTON2_X`, `DOCK_ITEMS_X`), `popup_search` and `popup_search_active` fields in `struct Dock`
- `src/dock.c` — `StripQuotes()`, updated `DockAddItemInternal()`, two-button hit testing (`DockAddButtonAt` returns 1 or 2), click handlers for both buttons, keyboard event handling (`SDL_KEYDOWN`, `SDL_TEXTINPUT`), fixed `DockRowAt` to account for search filtering
- `src/dock_ui.c` — drawing two buttons side by side (first shows "F", second shows folder icon), search field rendering at top of folder/picker popups, row filtering via `RowMatchesSearch()`

**Verification:**
- All COM calls compile and link with MinGW against `ole32`, `shell32`, `uuid`
- Quote stripping handles paths like `"C:\Users\mundh\Desktop\Dex"` → `C:\Users\mundh\Desktop\Dex`
- Search field filters rows case-insensitively as user types
- Both dialogs deliver results via same `g_pick_file_event`; main loop uses `DirIsFolder()` to determine kind

---

## Next steps

> **Remember:** per the *Working rules* at the top — log the task here first,
> update this entry when it is finished.

### The plan for the next session

#### The new top dock bar — the task 29 ✅ done

**The goal:** a small bar at the top of the screen, over the wallpaper, that
**slides down when the mouse reaches the top of the screen** and hides itself
again. It has an **add button** (top left) with two options — **open the file
browser** and **select a file or a folder** — and the **system details** (top
right): **CPU load, RAM usage, upload/download speed, disk load, time**.

**The bar is implemented** (the details are in task 29, above). The desktop
icons are switched off, as the user decided. The **popup below the bar has a
window of its own** (task 30), so the menu of the add button is really shown.

**What is left to do (the order matters):**

1. **The add button must open a native file dialog** (task 31). This is the
   thing the user is waiting for: click **+** → a file browser opens → choose a
   file or a folder → it is added to the bar. Use `IFileOpenDialog` without
   `FOS_PICKFOLDERS` (that flag restricts to folders only), with
   `FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST` added and `FOS_FILEMUSTEXIST`
   cleared, so both files and folders can be chosen. It is the dialog of the
   shell, it is always visible, and it does both (files and folders).
   **Do not fight the custom popup window any more.**
2. **The custom popup of the bar** (the list of the files of a folder that is
   an item of the bar) is still invisible on Windows. The three possible
   causes are in task 31. It is not urgent: the folder of the bar can also be
   opened with the file manager of the system.
3. **The log spam:** `PlatformPlaceDockPopupWindow()` runs on every frame
   while the popup is open, and it fills the log with hundreds of lines.
4. The bar is only on the **main screen** for now. The other screens are not
   in the first version.
5. Move the headless check (`/tmp/dockcheck.c`) into `tests/` and connect it
   to CTest.

#### The old goal — the video **and** the desktop icons at the same time

**This goal is dropped.** The user decided (2026-10-21) that the desktop icons
are **switched off completely** and that the work continues on the dock bar. The
icons are in the way: the layer of the icons is the one that keeps the snapshot
of the desktop, and the snapshot hides the wallpaper.

**What that means for the code:**

* The default of `g_icons_mode` in `platform_windows.c` is `ICONS_NONE`
  (before: `ICONS_REFRESH`). The layer of the icons is not touched at all, so
  the shell does not paint the icons and does not keep the snapshot.
* The periodic `PlatformRefreshDesktopIcons()` call in the main loop is
  **removed** — it only existed to make the icons visible.
* The strategy 6 stays the default on Windows: the window is a popup that the
  mouse clicks pass through, so the video is shown and the clicks reach the
  desktop.

**The reasoning (the details are in task 23):** the strategy 6 is a top-level
popup, so the icons — which live in the `SHELLDLL_DefView`/`SysListView32`
inside `Progman` — are in a completely different layer and can never be
painted over it. The window has to be **a child of the `WorkerW` inside
`Progman`**, so that the order inside `Progman` becomes:

```
SHELLDLL_DefView   <- the icons, on top
SysListView32      <- the icons
SDL_app            <- our video, in the middle
WorkerW            <- the background, at the bottom
```

**The steps, in order:**

1. ~~**Test the strategy 1** (`--wallpaper-strategy=1`, a child of the `WorkerW`)
   and the **strategy 5** (`--wallpaper-strategy=5`, a layered child of the
   `WorkerW`). They were rejected by the unreliable colour check back when
   the renderer still produced nothing — they have never been tested with the
   working renderer and the software renderer.~~ **Not needed any more: the
   icons are switched off.**
2. ~~Look at the `the children of the parent (z-order, topmost first):` part of
   the log: the `SDL_app` window has to be **between** `SysListView32` and
   `WorkerW`.~~ **Not needed any more.**
3. ~~If the video is visible but the icons are not, the layer of the icons has
   to be refreshed **after** the window was attached — the order of the
   operations in `PlatformAttachWallpaper()` has to be checked.~~ **Not needed
   any more.**
4. ~~Try the variant of the AutoIt thread:
   `SetWindowLong(GWL_HWNDPARENT, SysListView32)` + `WS_EX_LAYERED` +
   `SetLayeredWindowAttributes`.~~ **Not needed any more.**
5. ~~**Ask the user which is more important** — the icons or the video. The
   strategy 6 (the video and the clicks) works today; if the icons cannot be
   saved, the strategy 6 stays the default.~~ **Answered: the video and the
   dock bar are more important. The strategy 6 stays the default and the icons
   are switched off.**

**The other things that are waiting:**

6. Confirm that the RAM usage stays flat over a longer run (Task Manager).
7. Consider adding the `live-paper-console.exe` to the release packaging
   (CPack) and to the README.
8. The `--desktop-icons` option cannot be given twice (argtable3 reports
   *"excess option"*) — it could be changed to accept the last value.

---

## Useful commands

```bash
# Linux build and run
cmake -B build && cmake --build build
./build/live-paper --verbose --stats assets/test.gif

# Cross-compile for Windows (builds and deploys to the Windows desktop)
./makeLinuxToWindow.sh

# Only the icons mode (the other options cannot be given twice!)
#   --desktop-icons=none    the layer of the icons is not touched
#   --desktop-icons=redraw  the shell is asked to paint the layer again
#   --desktop-icons=refresh the layer is hidden and shown again (the default)

# Test one strategy of the attachment (0-6)
./live-paper-console.exe --wallpaper-strategy=1 --verbose 'D:\image.gif'

# Analyse a screenshot of the desktop (the file is in the folder of the program)
python3 tools/analyze-screenshot.py shot.bmp

# Leak measurement harness (heap in use after each video loop)
gcc -O0 -g -o /tmp/leaktest tools/leaktest.c src/video.c src/log.c src/fail.c \
    -I src $(pkg-config --cflags --libs sdl2 libavformat libavcodec libavutil libswscale) -lm
SDL_VIDEODRIVER=dummy /tmp/leaktest assets/test.gif 8
```

## Notes and caveats

* The Windows binary is built with **MinGW-w64**, not MSVC — it is
  self-contained (no MSVC runtime needed) but differs from the official MSI.
* The Windows wallpaper mechanism (WorkerW) and the tray icon could not be
  exercised in the Linux container (no Explorer desktop); they are verified by
  compilation, code review and a Wine run of everything else.
* Under Wine, `comdlg32.dll`'s `DllMain` hangs in this headless container —
  that is an environment problem, not a defect of the application (a minimal
  program that only imports `comdlg32.dll` hangs the same way).
* **The changes of the source are not deployed automatically** — after every
  change `makeLinuxToWindow.sh` has to be run, otherwise the build on the
  Windows side is the old one. The timestamps of `src/*.c` and of
  `LivePaper-win64/live-paper.exe` show whether that has been done.
* The **Linux** build in this container cannot open a window
  (`can't create SDL window`) — there is no real X display. The Linux code is
  not affected by the Windows changes (`platform_windows.c` is not compiled
  on Linux), so the build itself is the check.
* The check of the colour on the screen is **unreliable** (the compositor of
  Windows can show an old frame) — the log can report that the wallpaper is
  visible when it is not, and the other way round. The screenshot
  (`--screenshot`) is the only reliable evidence.

---

## The prompt for the next session

Copy this text and send it to the assistant. It has everything that is needed
to continue the work.

> **Read `CurrentWork.md` in this repository and continue the work from where
> it stopped.**
>
> The project is **LivePaper** — a live wallpaper for Windows and Linux.
> **Task 31 is complete** — the add button of the top dock bar now opens a
> native file dialog on Windows (`IFileOpenDialog` on a worker thread, result
> delivered with an SDL event). Linux and macOS keep the existing custom
> popup.
> **Task 32 is complete** — the add button now has two separate buttons:
> "Add file" (labeled "F") and "Add folder" (with folder icon). Both open
> native Windows dialogs (`LauncherPickFile` / `LauncherPickFolder`) on worker
> threads. Folder popups have a search field at the top with case-insensitive
> filtering. Quote stripping fixes `DirIsFolder()` for paths from dialogs or
> `dock.txt`. File/folder icons shown on bar items and in popups.
>
> **What remains to be done:**
>
> * The log spam from task 31, item 4: `PlatformPlaceDockPopupWindow()` is
>   called on every frame while the popup is open; it should only be called
>   when the popup is shown. The `LOG_DEBUG()` of `PlaceDockWindowAboveDesktop()`
>   should also be quieter.
> * Task 2 from the task 31 plan: the item of the bar can still be invisible
>   on Windows (three possible causes were listed). It is not urgent — the
>   folder of the bar can be opened with the file manager of the system.
> * Search field: consider adding a clear button (X) to reset search quickly.
>
> **Do this:**
>
> 1. Read `CurrentWork.md` completely — the task 31 and 32 entries now have the
>    implementation details and the verification results.
> 2. Pick one of the remaining items above (log spam, bar item visibility on
>    Windows, search clear button) and implement it.
> 3. Check that the **Linux build** (`cmake -B build && cmake --build build`)
>    is clean, and that the headless check still passes
>    (`gcc -o /tmp/dockcheck /tmp/dockcheck.c src/dock.c src/dock_ui.c
>    src/dock_font.c src/dock_metrics.c src/dirlist.c src/launcher.c
>    src/platform_linux.c src/log.c src/fail.c -I src $(pkg-config --cflags
>    --libs sdl2) -lX11 -lm && /tmp/dockcheck`).
> 4. Update `CurrentWork.md`: the entry, the *Status* table and the *Next
>    steps*, and commit the changes.
>
> **The important rules of this project** (they are at the top of
> `CurrentWork.md`): the log of the work is updated **before** the work starts
> and **when it is finished**; the Linux and the Windows builds must stay
> without warnings; the changes of the source are only on the Windows machine
> after `makeLinuxToWindow.sh` has been run.
>
> **The last thing the user said (2026-10-22):** *"clicking on add should
> launch file browser window, but it does nothing"*. The log of the user
> proves that the click is received and that the menu is opened, so only the
> **display** of the custom popup is broken. The decision is to use the
> **native dialog of Windows** instead of fighting with the custom popup
> window any more.

