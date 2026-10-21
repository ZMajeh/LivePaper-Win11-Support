#include "platform.h"

#include <SDL.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "fail.h"
#include "log.h"

void PlatformInitGuiMode(struct Context *context) {}

const char *PlatformWallpaperMechanism(void) {
    return "X11 root window";
}

void PlatformDumpWindows(void) {
    // Only Windows has a window hierarchy that is worth dumping.
}

bool PlatformScreenShowsColor(int x, int y, uint32_t rgb) {
    return false;
}

bool PlatformCanCheckScreen(void) {
    return false;
}

const char *PlatformWallpaperStrategyName(int strategy) {
    return "the default";
}

int PlatformWallpaperStrategyCount(void) {
    return 1;
}

int PlatformWallpaperStrategyOrder(int index) {
    return index;
}

bool PlatformAttachWallpaper(struct Context *context, int strategy) {
    return true;
}

bool PlatformScreenHasColor(uint32_t rgb) {
    return false;
}

bool PlatformCaptureScreen(const char *path) {
    return false;
}

void PlatformSetDesktopIconsMode(int mode) {
}

void PlatformSetPrepareDesktop(bool enable) {
}

void PlatformRefreshDesktopIcons(void) {
}

// The window of the bar of the dock. On Linux it is an ordinary window of
// the window manager, so it is above the wallpaper (the root window of X11).
SDL_Window *PlatformCreateDockWindow(int width, int height) {
    SDL_Window *window = SDL_CreateWindow("live-paper-dock", 0, -height, width, height,
                                          SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!window) {
        LOG_WARN("the window of the dock bar could not be created: %s", SDL_GetError());
        return NULL;
    }
    return window;
}

SDL_Window *PlatformCreateDockPopupWindow(int width, int height) {
    return PlatformCreateDockWindow(width, height);
}

void PlatformDestroyDockWindow(SDL_Window *window) {
    if (window) SDL_DestroyWindow(window);
}

void PlatformSetDockPosition(SDL_Window *window, int x, int y) {
    if (!window) return;
    SDL_SetWindowPosition(window, x, y);
}

void PlatformSetDockVisible(SDL_Window *window, bool visible) {
    if (!window) return;
    if (visible) {
        SDL_ShowWindow(window);
    } else {
        SDL_HideWindow(window);
    }
}

void PlatformPlaceDockPopupWindow(SDL_Window *window) {
    (void)window;
}

void PlatformGetCursorPos(int *x, int *y) {
    // The position of the mouse is asked from SDL: it works with the window
    // manager of the screen.
    int state = SDL_GetGlobalMouseState(x, y);
    (void)state;
}

void PlatformGetScreenSize(int *width, int *height) {
    SDL_DisplayMode mode;
    if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
        *width = mode.w;
        *height = mode.h;
        return;
    }
    *width = 1920;
    *height = 1080;
}

bool PlatformGetDockConfigPath(char *buf, size_t buf_size) {
    const char *home = getenv("HOME");
    if (!home || !*home) return false;
    // The folder of the settings is created, if it does not exist yet.
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.config/live-paper", home);
    mkdir(dir, 0755);
    snprintf(buf, buf_size, "%s/dock.txt", dir);
    return true;
}

void PlatformInit(struct Context *context) {
    Display *x_display = XOpenDisplay(NULL);
    if (x_display == NULL) FAIL_WITH("can't open X11 display");
    context->platform_data = x_display;   // keep it for PlatformCleanup

    const char *display_name = XDisplayName(NULL);
    int screen_num = DefaultScreen(x_display);
    int screen_w = DisplayWidth(x_display, screen_num);
    int screen_h = DisplayHeight(x_display, screen_num);
    LOG_INFO("X11 display '%s' opened, screen %d is %dx%d",
             display_name ? display_name : "(unknown)", screen_num, screen_w, screen_h);

    // A wallpaper is set by drawing directly onto the root window. Check
    // whether the root window is actually viewable; with a running compositor
    // it usually is not, and nothing would be visible.
    Window x_window = XDefaultRootWindow(x_display);
    if (!x_window) FAIL();

    XWindowAttributes attrs;
    if (XGetWindowAttributes(x_display, x_window, &attrs) &&
        attrs.map_state != IsViewable) {
        context->wallpaper_status = WALLPAPER_INACTIVE;
        snprintf(context->wallpaper_detail, sizeof(context->wallpaper_detail),
                 "root window is not viewable (map_state=%d); the wallpaper may be hidden by a compositor",
                 (int)attrs.map_state);
        LOG_WARN("%s", context->wallpaper_detail);
    } else {
        context->wallpaper_status = WALLPAPER_ACTIVE;
        snprintf(context->wallpaper_detail, sizeof(context->wallpaper_detail),
                 "drawing onto the root window of '%s' (%dx%d)",
                 display_name ? display_name : "display", screen_w, screen_h);
        LOG_INFO("%s", context->wallpaper_detail);
    }

    context->window = SDL_CreateWindowFrom((void*)x_window);
    if (context->window == NULL) FAIL_WITH("can't create SDL window");
}

void PlatformUpdate(struct Context *context) {}

void PlatformCleanup(struct Context *context) {
    if (context->window)
        SDL_DestroyWindow(context->window);
    context->window = NULL;
    Display *x_display = context->platform_data;
    if (x_display) XCloseDisplay(x_display);
    context->platform_data = NULL;
}