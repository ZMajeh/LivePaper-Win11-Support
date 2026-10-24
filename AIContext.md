# AI Context — LivePaper Win11 Support

This document provides context for continuing work on the LivePaper project.

## Project Overview

**LivePaper** — a live wallpaper for Windows and Linux. A small bar at the top of the screen slides down when the mouse reaches the top, with an add button (two options: "Add file" and "Add folder"), system details (CPU, RAM, network, disk, time), and folder items that show their contents in a popup.

## Current State (as of 2026-10-24)

### Completed Tasks

| Task | Description | Status |
|------|-------------|--------|
| 29 | Top dock bar (hover bar, add button, system details) | ✅ done |
| 30 | Add button popup window (separate window) | ✅ done |
| 31 | Native Windows file dialog (`IFileOpenDialog`) for add button | ✅ done |
| 32 | Two separate add buttons ("Add file"/"Add folder"), search field in folder popup, quote stripping, file/folder icons | ✅ done |
| 33 | Fix multi-column folder popup: dynamic max rows, path corruption, hit testing, remove search field, ".." alignment, clean up dead code | ✅ **done** |

### Today's Work (2026-10-24)

**Task 33 completed:** Applied all fixes from ImplementationPlan.md for the folder popup.

**Fixes applied:**

1. **Dynamic Max Rows** (`src/dock.h`, `src/dock.c`): Removed hardcoded `DOCK_POPUP_MAX_ROWS=12`, added `DockMaxPopupRows()` that computes max rows from screen height (capped at 48, 40px margin).

2. **Path Corruption Fix** (`src/dock.c`): `DockOpenFolderPopup()` now saves path with `strdup()` before `DockClosePopup()` frees `dock->rows`, preventing use-after-free.

3. **Remove Search Field** (`src/dock.h`, `src/dock.c`, `src/dock_ui.c`): Removed `popup_search`/`popup_search_active` fields, `RowMatchesSearch()` function, all keyboard/mouse handling, and drawing code for search field.

4. **".." Row Alignment** (`src/dock_ui.c`): ".." row (is_up) and "Add this folder" row (is_add_dir) now left-aligned (text_x=8); regular rows keep icon+indented text (text_x=22).

5. **Hit Testing Fixes** (`src/dock.c`, `src/dock_ui.c`): Fixed multi-column logic for uneven columns. When items don't divide evenly, first `remainder` columns get `rows_per_col` items, rest get `rows_per_col-1`. `DockRowAt()` and `DockUiDrawPopup()` calculate actual `col_start`/`col_end` per column. Returns -1 for empty space clicks. Added `DirIsFolder()` check in `DockClickRow` for .rpf/.url exclusion.

6. **Clean Up Dead Code**: Removed `DOCK_POPUP_MAX_ROWS`, search fields, `RowMatchesSearch()`, unused `DockUiVisibleRows()`, debug `LOG_DEBUG` spam from `PlaceDockWindowAboveDesktop()` and `PlatformPlaceDockPopupWindow()`. Added `popup_placed` flag to call `PlatformPlaceDockPopupWindow()` only once per popup show.

**Files changed:** `src/dock.h`, `src/dock.c`, `src/dock_ui.c`, `src/platform_windows.c`

**Build verification:**
- Linux build: ✅ compiles without errors/warnings
- Windows cross-compile: ✅ succeeds (`makeLinuxToWindow.sh`)

### Remaining Work

1. **Bar item visibility on Windows** (task 31, item 2): The item of the bar can still be invisible on Windows (three possible causes listed in task 31). Not urgent — the folder of the bar can be opened with the file manager.

2. **Log spam** (from task 31, item 4): ✅ **FIXED** - `PlatformPlaceDockPopupWindow()` now called only once per popup show, `LOG_DEBUG` removed.

### Key Files

| File | Purpose |
|------|---------|
| `src/dock.c` | Dock bar model, hit testing, popup logic |
| `src/dock_ui.c` | Dock bar drawing, multi-column popup rendering |
| `src/dock.h` | Dock data structures and constants |
| `src/launcher.c` | Windows native file/folder picker (`IFileOpenDialog`) |
| `src/launcher.h` | Launcher declarations |
| `src/main.c` | Main loop, SDL event handling |
| `src/platform_windows.c` | Windows-specific platform code |
| `src/dock_ui.c` | Multi-column popup drawing (`DockUiDrawPopup`) |

### Build Commands

```bash
# Linux build
cmake -B build && cmake --build build

# Headless check (no X11 display needed)
gcc -o /tmp/dockcheck /tmp/dockcheck.c src/dock.c src/dock_ui.c src/dock_font.c src/dock_metrics.c src/dirlist.c src/launcher.c src/platform_linux.c src/log.c src/fail.c -I src $(pkg-config --cflags --libs sdl2) -lX11 -lm && /tmp/dockcheck

# Windows cross-compile (run on Linux, deploys to Windows)
./makeLinuxToWindow.sh
```

### Key Constants (in `src/dock.h`)

```c
#define DOCK_POPUP_MAX_ROWS 12          // Max rows per column
#define DOCK_POPUP_MIN_WIDTH 240        // Minimum popup width
#define DOCK_POPUP_MAX_WIDTH 600        // Maximum popup width
#define DOCK_POPUP_MAX_COLUMNS 4        // Maximum columns
#define DOCK_POPUP_ROW_HEIGHT 20        // Row height in pixels
```

### Multi-Column Logic Summary

- **Columns:** Up to 4, based on filtered item count
- **Rows per column:** `ceil(filtered_count / cols)`, max 12
- **Width:** Dynamic based on longest label, clamped 240-600px per column
- **Search:** Case-insensitive, spans all columns at top
- **Hit testing:** `DockRowAt()` maps (x,y) → column → row → filtered index → original index

### Important Rules

1. **Log work** in `CurrentWork.md` before starting and when finished
2. **Linux and Windows builds** must stay clean (no warnings)
3. **Source changes** only on Windows machine after `makeLinuxToWindow.sh`
4. Update `CurrentWork.md` with status table, task entries, and next steps

### Next Session Plan

1. Read `CurrentWork.md` completely
2. Pick one remaining item (log spam, bar item visibility, search clear button)
3. Verify Linux build and headless check pass
4. Update `CurrentWork.md` and commit
