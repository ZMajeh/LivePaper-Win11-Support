#pragma once

#include <stdbool.h>
#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle for cached icon
typedef struct PlatformIcon PlatformIcon;

// Initializes the icon subsystem. Returns true on success.
bool PlatformIconInit(void);

// Cleans up the icon subsystem (frees cache).
void PlatformIconCleanup(void);

// Gets an icon for the given file path. The icon is cached.
// Returns a PlatformIcon handle (must be freed with PlatformIconFree when done).
// The icon is extracted at the requested size (typically 16x16 or 32x32).
PlatformIcon *PlatformIconGet(const char *path, int size);

// Creates an SDL_Texture from the platform icon.
// Returns the texture (owned by caller, must be destroyed with SDL_DestroyTexture)
// or NULL on failure.
SDL_Texture *PlatformIconCreateTexture(SDL_Renderer *renderer, PlatformIcon *icon);

// Frees the platform icon handle.
void PlatformIconFree(PlatformIcon *icon);

// Checks if the given path is an executable (Windows: .exe, .lnk; Linux: executable bit or .desktop)
bool PlatformIconIsExecutable(const char *path);

#ifdef __cplusplus
}
#endif