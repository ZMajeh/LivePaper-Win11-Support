#include "platform.h"

#include <SDL.h>

#include <stdio.h>

#include "fail.h"
#include "log.h"

void PlatformInitGuiMode(struct Context *context) {}

const char *PlatformWallpaperMechanism(void) {
    return "macOS test window";
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

// The window of the bar of the dock.
SDL_Window *PlatformCreateDockWindow(int width, int height) {
    SDL_Window *window = SDL_CreateWindow("live-paper-dock", 0, -height, width, height,
                                          SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!window) {
        LOG_WARN("the window of the dock bar could not be created: %s", SDL_GetError());
        return NULL;
    }
    return window;
}

void PlatformDestroyDockWindow(SDL_Window *window) {
    if (window) SDL_DestroyWindow(window);
}

void PlatformSetDockPosition(SDL_Window *window, int x, int y) {
    if (!window) return;
    SDL_SetWindowPosition(window, x, y);
}

void PlatformGetCursorPos(int *x, int *y) {
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
    snprintf(buf, buf_size, "%s/.config/live-paper/dock.txt", home);
    return true;
}

void PlatformInit(struct Context *context)
{
    context->window = SDL_CreateWindow("LivePaper TEST", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1000, 500, SDL_WINDOW_SHOWN);
    if (context->window == NULL)
        FAIL_WITH("can't create SDL window");

    context->wallpaper_status = WALLPAPER_INACTIVE;
    snprintf(context->wallpaper_detail, sizeof(context->wallpaper_detail),
             "macOS support is not implemented; showing a test window instead");
    LOG_WARN("%s", context->wallpaper_detail);
}

void PlatformUpdate(struct Context *context) {}

void PlatformCleanup(struct Context *context)
{
    if (context->window)
        SDL_DestroyWindow(context->window);
    context->window = NULL;
}