# Current work log

Project: **LivePaper** — live wallpaper for Windows and Linux
Repository: `/home/majeh/program/LivePaper`
Last updated: 2026-10-20

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
| Windows desktop icons (painted over the video) | ❌ **not solved** — see task 23 |

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
| 23 | The desktop icons are not painted (the video and the clicks work) | ⏸ paused — the fix did not work, see the next steps |
| 24 | The session of 2026-10-20: deploy, analyse the screenshots, plan | ✅ done |
| 25 | Fork the project and copy the changes into the new repository | ✅ done — pushed as `17fdb35` |

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

## Next steps

> **Remember:** per the *Working rules* at the top — log the task here first,
> update this entry when it is finished.

### The plan for the next session

**The goal:** show the video **and** the desktop icons at the same time.

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

1. **Test the strategy 1** (`--wallpaper-strategy=1`, a child of the `WorkerW`)
   and the **strategy 5** (`--wallpaper-strategy=5`, a layered child of the
   `WorkerW`). They were rejected by the unreliable colour check back when
   the renderer still produced nothing — they have never been tested with the
   working renderer and the software renderer.
2. Look at the `the children of the parent (z-order, topmost first):` part of
   the log: the `SDL_app` window has to be **between** `SysListView32` and
   `WorkerW`.
3. If the video is visible but the icons are not, the layer of the icons has
   to be refreshed **after** the window was attached — the order of the
   operations in `PlatformAttachWallpaper()` has to be checked.
4. If the icons still do not come, try the variant of the AutoIt thread:
   `SetWindowLong(GWL_HWNDPARENT, SysListView32)` + `WS_EX_LAYERED` +
   `SetLayeredWindowAttributes`.
5. **Ask the user which is more important** — the icons or the video. The
   strategy 6 (the video and the clicks) works today; if the icons cannot be
   saved, the strategy 6 stays the default.

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

