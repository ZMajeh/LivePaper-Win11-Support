# Changelog

All notable changes to LivePaper will be documented in this file.

## [Unreleased]

### Added
- Task 33: Fixed multi-column folder popup with dynamic rows, hit testing, path corruption fixes, removed search field, fixed ".." row alignment
- Task 32: Two separate add buttons ("Add file" / "Add folder") with native Windows dialogs, folder popup navigation with search, file/folder icons
- Task 31: Native Windows file dialog (`IFileOpenDialog`) for add button on worker thread
- Task 30: Add button popup window (separate window) with file browser and file/folder options
- Task 29: Dock bar architecture - hover bar, add button, system details, folder items with popup
- Task 22: Strategy 6 as default - wallpaper window as top-level popup with WS_EX_LAYERED | WS_EX_TRANSPARENT
- Task 21: Capture of screen - reliable way to see desktop, screenshots confirm strategy 6
- Task 20: The 0x052c message, layered window, and order of strategies
- Task 19: Research Windows 11 24H2 mechanism (transparent icon layer)
- Task 18: New strategy below Progman; fix placement of child strategies
- Task 17: Deploy build to Windows desktop folder automatically
- Task 16: Reliable screen check + `--wallpaper-strategy` to pick manually
- Task 15: Fix icons and mouse clicks (z-order + unreliable screen check) - paused
- Task 14: Search attachment strategies automatically until wallpaper shows - strategy 2 works
- Task 13: Verify wallpaper is on screen (BitBlt check) + auto fallback
- Task 12: Support Windows 11 virtual desktops (one Progman per workspace)
- Task 11: Fix texture upload (SDL_UpdateTexture instead of LockTexture)
- Task 10: Diagnose wallpaper window composites transparently - renderer produced nothing
- Task 9: Add `--dump-windows` and PowerShell script to collect desktop structure
- Task 8: Fix wallpaper host on Windows 11 24H2 (icons + clicks broken)
- Task 7: Add working rules to document
- Task 6: Fix Windows wallpaper (WorkerW host + real child window)
- Task 5: Fix memory leak (one packet buffer per decoded frame)
- Task 4: Cross-compile for Windows from Linux (`makeLinuxToWindow.sh`)
- Task 3: Show diagnostics in terminal on Windows
- Task 2: Diagnostics subsystem (log, sysinfo, video stats, wallpaper status)
- Task 1: Build fix (GCC 14, `SDL_LockTexture`)

### Fixed
- Windows 11 24H2 desktop icons visibility issue (partially - switched off on purpose, dock bar replaces them)
- Memory leak per decoded frame
- Texture upload using SDL_UpdateTexture
- Wallpaper strategy detection and fallback
- Multi-column folder popup hit testing and dynamic sizing
- Path corruption when navigating folders from popup
- Search field removed from folder popup

### Changed
- Default wallpaper strategy changed to Strategy 6 (top-level popup with layered window)
- Desktop icons handling: default changed from `ICONS_REDRAW` to `ICONS_REFRESH` (hiding/showing SHELLDLL_DefView)
- Add button now uses two separate buttons ("Add file" / "Add folder") instead of single menu
- Native Windows file dialogs (`IFileOpenDialog`) replace custom popup on Windows
- Folder popup now uses multi-column layout with dynamic row calculation based on screen size
- Removed search field from folder popup
- ".." row and "Add this folder" row now left-aligned in popup

## [0.0.0] - 2026-10-21

### Added
- Initial LivePaper live wallpaper for Windows and Linux
- Video/GIF wallpaper support via FFmpeg
- SDL2-based rendering
- System diagnostics (CPU, RAM, network, disk, time)
- Dock bar with file/folder shortcuts
- Tray icon with menu (Dock bar toggle, Set wallpaper, Quit)
- Cross-compilation from Linux to Windows
- Automated CI/CD with GitHub Actions

### Known Issues
- Desktop icons not visible on Windows 11 24H2 with Strategy 6 (wallpaper is top-level popup, icons live in Progman hierarchy)
- Unicode not fully supported on Windows
- Only one display supported
- App quits with error on input file issues
- Linux compositor compatibility issues