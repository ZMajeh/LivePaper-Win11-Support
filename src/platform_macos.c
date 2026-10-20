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