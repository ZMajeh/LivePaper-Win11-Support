#pragma once

#include <stdbool.h>
#include <SDL.h>

enum {
    FIT_FIT,
    FIT_FILL,
    FIT_CENTER
};

// The renderer that is used to draw the wallpaper.
enum {
    RENDERER_AUTO = 0,      // accelerated, with a fallback to software
    RENDERER_ACCELERATED,   // accelerated only
    RENDERER_SOFTWARE,      // software only
};
// Describes how the wallpaper was attached to the desktop.
enum WallpaperStatus {
    WALLPAPER_UNKNOWN = 0,
    WALLPAPER_ACTIVE,     // wallpaper is attached and visible
    WALLPAPER_INACTIVE,   // attached, but not visible on the desktop
    WALLPAPER_FAILED,     // could not attach the wallpaper
};

struct Context {
    SDL_Window *window;
    SDL_Renderer *renderer;
    void *platform_data;

    char *file;
    char *screenshot;       // the file for the capture of the screen, if any
    int fit;
    int renderer_mode;
    int wallpaper_strategy;   // -1: try them all, >= 0: use this one

    // diagnostics
    enum WallpaperStatus wallpaper_status;
    char wallpaper_detail[256];
};

void PlatformInitGuiMode(struct Context *context);
void PlatformInit(struct Context *context);
void PlatformUpdate(struct Context *context);
void PlatformCleanup(struct Context *context);

// Returns a human readable description of the platform-specific wallpaper
// mechanism used (e.g. "Windows WorkerW" or "X11 root window").
const char *PlatformWallpaperMechanism(void);

// Checks whether the given colour is really shown on the screen at the given
// position. The screen is read back with BitBlt(), which returns the
// composited desktop, so this tells whether the wallpaper window is shown at
// all.
// It is used to find out whether the window is composited by the system and
// to fall back to another renderer if it is not.
bool PlatformScreenShowsColor(int x, int y, uint32_t rgb);

// Tells whether PlatformScreenShowsColor() can be used on this platform.
bool PlatformCanCheckScreen(void);

// Searches the whole screen for the given colour. It is used to find out
// whether the wallpaper is really shown, no matter which part of the screen
// is not covered by other windows.
bool PlatformScreenHasColor(uint32_t rgb);

// The way the layer of the desktop icons is prepared (Windows only):
// 0: it is not touched, 1: it is painted again, 2: it is hidden and shown.
void PlatformSetDesktopIconsMode(int mode);

// Whether the message 0x052c is sent to the Progman window, to make Windows
// create the WorkerW window of the desktop (Windows only).
void PlatformSetPrepareDesktop(bool enable);

// Captures the screen into a bitmap file (BMP), to see what the desktop
// really looks like. It does nothing on the other systems.
bool PlatformCaptureScreen(const char *path);

// The number of the strategies that are tried to attach the wallpaper window.
int PlatformWallpaperStrategyCount(void);

// The order in which the strategies are tried. The strategies that keep the
// desktop icons and the mouse clicks working come first.
int PlatformWallpaperStrategyOrder(int index);

// Attaches the wallpaper window with the given strategy (0 based).
// Returns true if the window was attached.
bool PlatformAttachWallpaper(struct Context *context, int strategy);

// The names of the strategies, for the diagnostics.
const char *PlatformWallpaperStrategyName(int strategy);

// Prints the structure of the desktop windows. It is used to find out how the
// desktop is composed on the different versions of Windows.
void PlatformDumpWindows(void);

// Makes the layer of the desktop icons transparent again, so that the
// wallpaper is visible below the icons. It does nothing on the other systems.
void PlatformRefreshDesktopIcons(void);

// ---------------------------------------------------------------------------
// The dock bar
// ---------------------------------------------------------------------------

// The bar of the dock is a separate top level window: it has to be above the
// wallpaper, but it must not steal the focus from the desktop and it must
// not take the clicks of the desktop away.
//
// The bar hides itself by moving above the screen, so that it cannot get the
// clicks of the desktop: no special style is needed for that.

// Creates the window of the bar. It is a borderless window at the top of
// the screen. Returns NULL if the window could not be created.
SDL_Window *PlatformCreateDockWindow(int width, int height);

// Creates the window of the popup below the bar. It is a window of its own,
// so that it covers only the part of the screen where it is. Returns NULL if
// the window could not be created.
SDL_Window *PlatformCreateDockPopupWindow(int width, int height);

// Destroys the window of the bar.
void PlatformDestroyDockWindow(SDL_Window *window);

// Moves the window of the bar. The y coordinate is negative when the bar is
// hidden above the screen.
void PlatformSetDockPosition(SDL_Window *window, int x, int y);

// Shows or hides a window of the bar. A hidden window does not get the
// clicks of the desktop.
void PlatformSetDockVisible(SDL_Window *window, bool visible);

// Puts the window of the popup again in the right place of the z-order, after
// it was shown.
void PlatformPlaceDockPopupWindow(SDL_Window *window);

// The position of the mouse on the whole screen. The bar has no focus, so
// it does not get the mouse events of the desktop: the position has to be
// asked for.
void PlatformGetCursorPos(int *x, int *y);

// The size of the screen of the desktop.
void PlatformGetScreenSize(int *width, int *height);

// The path of the file with the items of the bar. It is next to the other
// settings of the program.
bool PlatformGetDockConfigPath(char *buf, size_t buf_size);
