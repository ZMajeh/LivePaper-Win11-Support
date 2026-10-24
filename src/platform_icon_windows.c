#include "platform_icon.h"

#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

#include "log.h"
#include "fail.h"

// Simple cache entry
typedef struct IconCacheEntry {
    char *path;
    int size;
    HICON icon;
    struct IconCacheEntry *next;
} IconCacheEntry;

static IconCacheEntry *g_icon_cache = NULL;
static CRITICAL_SECTION g_cache_lock;
static bool g_cache_initialized = false;

struct PlatformIcon {
    HICON icon;
    int size;
    char *path;  // for debugging
};

static void CacheLock(void) {
    if (!g_cache_initialized) {
        InitializeCriticalSection(&g_cache_lock);
        g_cache_initialized = true;
    }
    EnterCriticalSection(&g_cache_lock);
}

static void CacheUnlock(void) {
    LeaveCriticalSection(&g_cache_lock);
}

static IconCacheEntry *CacheFind(const char *path, int size) {
    for (IconCacheEntry *e = g_icon_cache; e; e = e->next) {
        if (e->size == size && _stricmp(e->path, path) == 0) {
            return e;
        }
    }
    return NULL;
}

static void CacheAdd(const char *path, int size, HICON icon) {
    IconCacheEntry *e = malloc(sizeof(IconCacheEntry));
    if (!e) return;
    e->path = _strdup(path);
    e->size = size;
    e->icon = icon;
    e->next = g_icon_cache;
    g_icon_cache = e;
}

static HICON ExtractIconForPath(const char *path, int size) {
    SHFILEINFO sfi = {0};
    UINT flags = SHGFI_ICON | SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES;
    
    if (size <= 16) {
        flags |= SHGFI_SMALLICON;
    } else {
        flags |= SHGFI_LARGEICON;
    }

    DWORD attrs = FILE_ATTRIBUTE_NORMAL;
    if (PathFileExistsA(path)) {
        attrs = GetFileAttributesA(path);
    }

    HIMAGELIST hImageList = (HIMAGELIST)SHGetFileInfoA(
        path, attrs, &sfi, sizeof(sfi), flags);
    
    if (!hImageList || !sfi.hIcon) {
        return NULL;
    }

    // Copy the icon so we own it
    ICONINFO ii = {0};
    if (!GetIconInfo(sfi.hIcon, &ii)) {
        return NULL;
    }

    HICON hIconCopy = CopyIcon(sfi.hIcon);
    
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);

    return hIconCopy;
}

bool PlatformIconInit(void) {
    // Nothing special needed for Windows
    return true;
}

void PlatformIconCleanup(void) {
    CacheLock();
    for (IconCacheEntry *e = g_icon_cache; e; ) {
        IconCacheEntry *next = e->next;
        if (e->icon) DestroyIcon(e->icon);
        free(e->path);
        free(e);
        e = next;
    }
    g_icon_cache = NULL;
    CacheUnlock();
    if (g_cache_initialized) {
        DeleteCriticalSection(&g_cache_lock);
        g_cache_initialized = false;
    }
}

PlatformIcon *PlatformIconGet(const char *path, int size) {
    if (!path || !*path) return NULL;

    CacheLock();
    
    // Check cache first
    IconCacheEntry *cached = CacheFind(path, size);
    if (cached && cached->icon) {
        PlatformIcon *icon = malloc(sizeof(PlatformIcon));
        if (icon) {
            icon->icon = CopyIcon(cached->icon);
            icon->size = size;
            icon->path = _strdup(path);
        }
        CacheUnlock();
        return icon;
    }

    // Extract icon
    HICON hIcon = ExtractIconForPath(path, size);
    if (!hIcon) {
        CacheUnlock();
        return NULL;
    }

    // Add to cache
    CacheAdd(path, size, hIcon);

    PlatformIcon *icon = malloc(sizeof(PlatformIcon));
    if (icon) {
        icon->icon = CopyIcon(hIcon);
        icon->size = size;
        icon->path = _strdup(path);
    }

    CacheUnlock();
    return icon;
}

SDL_Texture *PlatformIconCreateTexture(SDL_Renderer *renderer, PlatformIcon *icon) {
    if (!renderer || !icon || !icon->icon) return NULL;

    ICONINFO ii = {0};
    if (!GetIconInfo(icon->icon, &ii)) {
        return NULL;
    }

    BITMAP bmp = {0};
    GetObject(ii.hbmColor ? ii.hbmColor : ii.hbmMask, sizeof(BITMAP), &bmp);

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;

    // Create a DIB section to get pixel data
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, NULL, NULL, 0);
    HBITMAP hOldBitmap = SelectObject(hdcMem, hBitmap);

    // Draw icon to the bitmap
    DrawIconEx(hdcMem, 0, 0, icon->icon, width, height, 0, NULL, DI_NORMAL);

    SelectObject(hdcMem, hOldBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);

    if (!hBitmap) {
        return NULL;
    }

    // Get pixel data
    BITMAP bmp2 = {0};
    GetObject(hBitmap, sizeof(BITMAP), &bmp2);

    void *pixels = bmp2.bmBits;
    if (!pixels) {
        DeleteObject(hBitmap);
        return NULL;
    }

    // Create SDL texture
    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STATIC,
        width, height
    );

    if (texture) {
        SDL_UpdateTexture(texture, NULL, pixels, width * 4);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }

    DeleteObject(hBitmap);
    return texture;
}

void PlatformIconFree(PlatformIcon *icon) {
    if (!icon) return;
    if (icon->icon) DestroyIcon(icon->icon);
    free(icon->path);
    free(icon);
}

bool PlatformIconIsExecutable(const char *path) {
    if (!path || !*path) return false;
    
    const char *ext = PathFindExtensionA(path);
    if (!ext) return false;
    
    // Check for executable extensions
    if (_stricmp(ext, ".exe") == 0) return true;
    if (_stricmp(ext, ".lnk") == 0) return true;  // Shortcuts
    if (_stricmp(ext, ".bat") == 0) return true;
    if (_stricmp(ext, ".cmd") == 0) return true;
    if (_stricmp(ext, ".com") == 0) return true;
    if (_stricmp(ext, ".msi") == 0) return true;
    
    return false;
}
