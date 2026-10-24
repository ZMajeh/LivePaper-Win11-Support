# LivePaper Implementation Plan

This document contains all fixes that need to be applied to the codebase in the correct order, with step-by-step instructions for each fix.

## Prerequisites
- Clean git state (all source files at original HEAD)
- Linux build environment with SDL2, FFmpeg development headers
- Cross-compilation toolchain for Windows (mingw-w64)

---

## Fix 1: Dynamic Max Rows Based on Screen Size (Task 33 related)

**Files:** `src/dock.h`, `src/dock.c`

**Problem:** `DOCK_POPUP_MAX_ROWS` was hardcoded to 12, causing popup height to exceed screen on small screens and leaving empty space on large screens.

**Changes:**

### src/dock.h
1. Remove `#define DOCK_POPUP_MAX_ROWS 12`
2. Add new function declaration after `DOCK_POPUP_MAX_COLUMNS`:
```c
// Computes the maximum rows per column that fit on screen.
// The dock bar is at the top, popup hangs below it.
int DockMaxPopupRows(const struct Dock *dock);
```

### src/dock.c
1. Add new function implementation (before `DockVisibleRows`):
```c
// Computes the maximum rows per column that fit on screen.
// The dock bar is at the top, popup hangs below it.
int DockMaxPopupRows(struct Dock *dock) {
    int available = dock->screen_height - dock->height;
    if (available < 0) available = 0;
    // Leave some margin at bottom (e.g., 40px for taskbar, etc.)
    available -= 40;
    if (available < 0) available = 0;
    int rows = available / DOCK_POPUP_ROW_HEIGHT;
    // Cap at a reasonable maximum for performance/UI
    if (rows > 48) rows = 48;
    return rows;
}
```

2. Update `DockVisibleRows`:
```c
static int DockVisibleRows(const struct Dock *dock) {
    int rows = dock->row_count;
    int max_rows = DockMaxPopupRows(dock);
    if (rows > max_rows) rows = max_rows;
    return rows;
}
```

3. Update `DockPopupColumns`:
```c
static int DockPopupColumns(const struct Dock *dock) {
    if (dock->popup_mode == DOCK_POPUP_ADD_MENU) return 1;
    int visible = DockFilteredRowCount(dock);
    int max_rows = DockMaxPopupRows(dock);
    int cols = (visible + max_rows - 1) / max_rows;
    if (cols > DOCK_POPUP_MAX_COLUMNS) cols = DOCK_POPUP_MAX_COLUMNS;
    return cols > 0 ? cols : 1;
}
```

4. Update `DockPopupSize`:
```c
static void DockPopupSize(const struct Dock *dock, int *width, int *height) {
    int row_height = DockRowHeight(dock);
    int rows_per_col = DockRowsPerColumn(dock);
    int cols = DockPopupColumns(dock);
    int max_rows = DockMaxPopupRows(dock);

    // The drawing is capped at max_rows per column, so height must match.
    if (rows_per_col > max_rows) rows_per_col = max_rows;
    *height = rows_per_col * row_height;
    // ... rest unchanged
```

---

## Fix 2: Save Path Before DockClosePopup (Path Corruption Fix)

**File:** `src/dock.c` - `DockOpenFolderPopup`

**Problem:** `path` pointer pointed to `dock->rows` which was freed by `DockClosePopup`, causing path corruption.

**Changes to `DockOpenFolderPopup`:**
1. Save path before `DockClosePopup`:
```c
void DockOpenFolderPopup(struct Dock *dock, const char *path, enum DockPopupMode mode) {
    // Save the path before DockClosePopup frees dock->rows
    char *saved_path = strdup(path);
    DockClosePopup(dock);

    struct DirEntry *entries = NULL;
    int count = DirListRead(saved_path, &entries);
    if (count < 0) {
        LOG_WARN("the files of the folder '%s' could not be shown", saved_path);
        free(saved_path);
        return;
    }
    
    // Use saved_path everywhere instead of path
    // ... rest of function uses saved_path
    
    // At end:
    free(saved_path);
```

---

## Fix 3: Remove Search Field from Popup

**Files:** `src/dock.h`, `src/dock.c`, `src/dock_ui.c`

### src/dock.h
Remove from `struct Dock`:
```c
// REMOVE these two lines:
char *popup_search;           // the search text for filtering
bool popup_search_active;     // whether the search field has focus
```

### src/dock.c
1. Remove search field initialization in `DockOpenFolderPopup`:
```c
// REMOVE these lines:
dock->popup_search = strdup("");
dock->popup_search_active = false;
```

2. Remove search field cleanup in `DockClosePopup`:
```c
// REMOVE these lines:
if (dock->popup_search) {
    free(dock->popup_search);
    dock->popup_search = NULL;
}
dock->popup_search_active = false;
```

3. Remove search field handling from `DockHandleEvent`:
- Remove `handle_keyboard` variable and related logic
- Remove `SDL_KEYDOWN` case
- Remove `SDL_TEXTINPUT` case
- Remove search field click handling in `SDL_MOUSEBUTTONDOWN`

### src/dock_ui.c
1. Remove `has_search` and `search_height` variables
2. Remove search field drawing code (the entire `if (has_search)` block)
3. Fix row_y calculation: `int row_y = row_in_col * row_height;` (remove `search_height +`)
3. Fix column calculation to use `DockMaxPopupRows(dock)` instead of `DOCK_POPUP_MAX_ROWS`

---

## Fix 4: Fix ".." Row Alignment in Popup

**File:** `src/dock_ui.c`

**Problem:** ".." row text was indented like other rows, making it look misaligned.

**Change in `DockUiDrawPopup`:**
```c
// For ".." row, keep text_x = 8 (left-aligned), otherwise indent
if (!row->is_up) {
    text_x = 22;
}
```

---

## Fix 5: Fix Path Corruption in Multi-Column Hit Testing

**File:** `src/dock.c` - `DockRowAt`

**Problem:** `visible_index = col * rows_per_col + row_in_col` was wrong when last column has fewer items.

**Fix:**
```c
// Change from:
int visible_index = col * rows_per_col + row_in_col;
// To:
int visible_index = col_start + row_in_col;
```
Where `col_start = col * rows_per_col;` (already calculated correctly)

---

## Fix 5b: Add DirIsFolder Check for .rpf and .url Files

**File:** `src/dirlist.c` - `DirIsFolder`

Add exclusions for files that have directory attribute but aren't directories:
```c
bool DirIsFolder(const char *path) {
    if (!path || !*path) return false;
    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) return false;
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) return false;
    
    // Exclude known archive/file types that incorrectly have directory attribute
    const char *ext = strrchr(path, '.');
    if (ext) {
        if (_stricmp(ext, ".rpf") == 0) return false;  // Rockstar archives
        if (_stricmp(ext, ".url") == 0) return false;  // Internet shortcuts
    }
    return true;
}
```

Also update `DockOpenFolderPopup` to check `DirIsFolder(full)` before treating as directory.

---

## Fix 6: Fix DockClickRow to Verify Directory Before Opening

**File:** `src/dock.c` - `DockClickRow`

```c
if (row->is_dir) {
    // A folder of the list: it is shown.
    // Double-check it's actually a directory (some files like .rpf archives
    // may have the directory attribute on Windows).
    if (DirIsFolder(row->path)) {
        DockOpenFolderPopup(dock, row->path, dock->popup_mode);
    } else {
        // It's a file with directory attribute - launch it
        DockClosePopup(dock);
        LauncherOpenFile(row->path);
    }
    return;
}
```

---

## Fix 7: Fix Hit Testing Row Index Calculation

**File:** `src/dock.c` - `DockRowAt`

Change:
```c
// Before:
int visible_index = col * rows_per_col + row_in_col;
// To:
int visible_index = col_start + row_in_col;
```

---

## Fix 8: Fix ".." Row Alignment in Popup

**File:** `src/dock_ui.c` - `DockUiDrawPopup`

For ".." row, keep text left-aligned:
```c
// For ".." row, keep text_x = 8 (left-aligned), otherwise indent
if (!row->is_up) {
    text_x = 22;
}
```

---

## Fix 9: Clean Up Dead Code

**Files to clean:**
1. Remove `DOCK_POPUP_MAX_ROWS` define from `dock.h`
2. Remove `popup_search` and `popup_search_active` from `struct Dock`
3. Remove search field handling from `DockHandleEvent` (SDL_KEYDOWN, SDL_TEXTINPUT cases)
4. Remove search field drawing from `DockUiDrawPopup`
5. Remove search field logs
4. Remove debug LOG_INFO statements added during debugging

---

## Fix 10: Icon Support for Dock Items

**Files:** `src/dock.h`, `src/dock.c`, `src/dock_ui.c`, `src/platform_icon.h`, `src/platform_icon_windows.c`, `src/platform_icon_linux.c`, `CMakeLists.txt`

This is a larger feature - adds real executable icons to dock items. See separate detailed spec.

---

## Build Verification Steps

After applying all fixes:

```bash
# Linux build
cmake -B build && cmake --build build

# Windows cross-compile
./makeLinuxToWindow.sh
```

---

## Testing Checklist

- [ ] Linux build compiles without errors/warnings
- [ ] Windows cross-compile succeeds
- [ ] Popup height adapts to screen size
- [ ] ".." row visible at top of first column
- [ ] ".." row text left-aligned
- [ ] Clicking ".." navigates to parent folder
- [ ] Clicking .rpf files launches them (doesn't try to open as folder)
- [ ] Clicking .url files opens in browser
- [ ] Clicking BattlEye folder opens it (not "PlayGTAV.exe")
- [ ] Clicking GTA5.exe launches game
- [ ] Multi-column popup has correct column count
- [ ] No empty rows at bottom of shorter columns
- [ ] Hover highlighting works correctly in all columns
- [ ] Search field removed from popup
- [ ] Windows build succeeds
- [ ] Linux build succeeds

---

## Order of Application

1. Fix 1: Dynamic max rows (dock.h, dock.c)
2. Fix 2: Path corruption fix in DockOpenFolderPopup
3. Fix 3: Remove search field (dock.h, dock.c, dock_ui.c)
4. Fix 4: ".." row alignment (dock_ui.c)
5. Fix 5: Hit testing fix (dock.c)
5b. DirIsFolder .rpf/.url exclusion (dirlist.c)
6. Fix 6: DockClickRow verification (dock.c)
7. Fix 7: Hit testing row index (dock.c)
8. Fix 8: ".." row alignment (dock_ui.c)
9. Clean up dead code
10. Icon support (if desired)

---

## Notes

- Apply fixes in order - later fixes may depend on earlier ones
- Test after each major fix
- Keep debug logs minimal (only essential INFO logs)