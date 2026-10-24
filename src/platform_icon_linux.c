#include "platform_icon.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <SDL.h>

#ifdef HAVE_SDL_IMAGE
#include <SDL_image.h>
#endif

#include "log.h"
#include "fail.h"

// Simple cache entry
typedef struct IconCacheEntry {
    char *path;
    int size;
    SDL_Texture *texture;  // On Linux we cache the texture directly
    struct IconCacheEntry *next;
} IconCacheEntry;

static IconCacheEntry *g_icon_cache = NULL;

struct PlatformIcon {
    char *path;
    int size;
    char *icon_path;  // Path to the actual icon file
};

static IconCacheEntry *CacheFind(const char *path, int size) {
    for (IconCacheEntry *e = g_icon_cache; e; e = e->next) {
        if (e->size == size && strcmp(e->path, path) == 0) {
            return e;
        }
    }
    return NULL;
}

static void CacheAdd(const char *path, int size, SDL_Texture *texture) {
    IconCacheEntry *e = malloc(sizeof(IconCacheEntry));
    if (!e) return;
    e->path = strdup(path);
    e->size = size;
    e->texture = texture;
    e->next = g_icon_cache;
    g_icon_cache = e;
}

// Get icon theme directories
static void GetIconDirs(char **dirs, int *count, int max_dirs) {
    *count = 0;
    
    // User theme
    const char *home = getenv("HOME");
    if (home && *count < max_dirs) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/.icons", home);
        dirs[(*count)++] = strdup(buf);
        snprintf(buf, sizeof(buf), "%s/.local/share/icons", home);
        dirs[(*count)++] = strdup(buf);
    }
    
    // System themes
    const char *xdg_data_dirs = getenv("XDG_DATA_DIRS");
    if (!xdg_data_dirs) xdg_data_dirs = "/usr/local/share:/usr/share";
    
    char *copy = strdup(xdg_data_dirs);
    char *token = strtok(copy, ":");
    while (token && *count < max_dirs) {
        char buf[1024];
        snprintf(buf, sizeof(buf), "%s/icons", token);
        dirs[(*count)++] = strdup(buf);
        token = strtok(NULL, ":");
    }
    free(copy);
    
    // Fallback
    if (*count < max_dirs) dirs[(*count)++] = strdup("/usr/share/icons");
    if (*count < max_dirs) dirs[(*count)++] = strdup("/usr/share/pixmaps");
}

// Find icon file using freedesktop icon theme spec
static char *FindIconFile(const char *icon_name, int size) {
    if (!icon_name || !*icon_name) return NULL;
    
    char *dirs[32];
    int dir_count = 0;
    GetIconDirs(dirs, &dir_count, 32);
    
    // Try exact size first, then scalable, then any size
    const char *size_dirs[] = {
        "",  // exact match
        "scalable",
        NULL
    };
    
    const char *extensions[] = {".png", ".svg", ".xpm", NULL};
    
    for (int s = 0; size_dirs[s]; s++) {
        for (int d = 0; d < dir_count; d++) {
            char base[1024];
            if (size_dirs[s][0]) {
                snprintf(base, sizeof(base), "%s/%s/apps", dirs[d], size_dirs[s]);
            } else {
                snprintf(base, sizeof(base), "%s/%dx%d/apps", dirs[d], size, size);
            }
            
            DIR *dir = opendir(base);
            if (!dir) continue;
            
            struct dirent *entry;
            while ((entry = readdir(dir))) {
                for (int e = 0; extensions[e]; e++) {
                    char expected[256];
                    snprintf(expected, sizeof(expected), "%s%s", icon_name, extensions[e]);
                    if (strcmp(entry->d_name, expected) == 0) {
                        char *result = malloc(strlen(base) + strlen(entry->d_name) + 2);
                        sprintf(result, "%s/%s", base, entry->d_name);
                        closedir(dir);
                        // Free dirs
                        for (int i = 0; i < dir_count; i++) free(dirs[i]);
                        return result;
                    }
                }
            }
            closedir(dir);
        }
    }
    
    // Free dirs
    for (int i = 0; i < dir_count; i++) free(dirs[i]);
    return NULL;
}

// Parse .desktop file to get Icon= entry
static char *ParseDesktopFile(const char *desktop_path) {
    FILE *f = fopen(desktop_path, "r");
    if (!f) return NULL;
    
    char line[1024];
    char *icon_name = NULL;
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "Icon=", 5) == 0) {
            char *val = line + 5;
            // Trim newline
            char *nl = strchr(val, '\n');
            if (nl) *nl = '\0';
            nl = strchr(val, '\r');
            if (nl) *nl = '\0';
            
            if (*val) {
                icon_name = strdup(val);
            }
            break;
        }
    }
    
    fclose(f);
    return icon_name;
}

// Get icon name from .desktop file for an executable
static char *GetIconFromDesktopFile(const char *exe_path) {
    // Try to find .desktop file in standard locations
    const char *dirs[] = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        NULL
    };
    
    const char *exe_name = strrchr(exe_path, '/');
    exe_name = exe_name ? exe_name + 1 : exe_path;
    
    for (int i = 0; dirs[i]; i++) {
        DIR *dir = opendir(dirs[i]);
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir))) {
            if (strstr(entry->d_name, ".desktop")) {
                char path[1024];
                snprintf(path, sizeof(path), "%s/%s", dirs[i], entry->d_name);
                
                char *icon = ParseDesktopFile(path);
                if (icon) {
                    // Check if this desktop file matches our executable
                    // (simplified: just return first match for now)
                    closedir(dir);
                    return icon;
                }
            }
        }
        closedir(dir);
    }
    
    return NULL;
}

bool PlatformIconInit(void) {
#ifdef HAVE_SDL_IMAGE
    // Initialize SDL_image for PNG/SVG support
    int flags = IMG_INIT_PNG;
    if ((IMG_Init(flags) & flags) != flags) {
        LOG_WARN("Failed to initialize SDL_image: %s", IMG_GetError());
    }
#endif
    return true;
}

void PlatformIconCleanup(void) {
    for (IconCacheEntry *e = g_icon_cache; e; ) {
        IconCacheEntry *next = e->next;
        if (e->texture) SDL_DestroyTexture(e->texture);
        free(e->path);
        free(e);
        e = next;
    }
    g_icon_cache = NULL;
#ifdef HAVE_SDL_IMAGE
    IMG_Quit();
#endif
}

PlatformIcon *PlatformIconGet(const char *path, int size) {
    if (!path || !*path) return NULL;
    
    // Check cache first
    IconCacheEntry *cached = CacheFind(path, size);
    if (cached && cached->texture) {
        PlatformIcon *icon = malloc(sizeof(PlatformIcon));
        if (icon) {
            icon->path = strdup(path);
            icon->size = size;
            icon->icon_path = NULL;  // Using cached texture
        }
        return icon;
    }
    
    // For Linux, we need to find the icon file
    // First check if it's a .desktop file
    char *icon_path = NULL;
    
    const char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".desktop") == 0) {
        char *icon_name = ParseDesktopFile(path);
        if (icon_name) {
            icon_path = FindIconFile(icon_name, size);
            free(icon_name);
        }
    } else {
        // Try to get icon from executable name
        const char *exe_name = strrchr(path, '/');
        exe_name = exe_name ? exe_name + 1 : path;
        
        // Remove extension for icon lookup
        char *name_copy = strdup(exe_name);
        char *dot = strrchr(name_copy, '.');
        if (dot) *dot = '\0';
        
        icon_path = FindIconFile(name_copy, size);
        free(name_copy);
        
        // If not found, try .desktop file lookup
        if (!icon_path) {
            char *icon_name = GetIconFromDesktopFile(path);
            if (icon_name) {
                icon_path = FindIconFile(icon_name, size);
                free(icon_name);
            }
        }
    }
    
    PlatformIcon *icon = malloc(sizeof(PlatformIcon));
    if (icon) {
        icon->path = strdup(path);
        icon->size = size;
        icon->icon_path = icon_path;  // May be NULL
    }
    
    return icon;
}

SDL_Texture *PlatformIconCreateTexture(SDL_Renderer *renderer, PlatformIcon *icon) {
    if (!renderer || !icon) return NULL;
    
    // Check cache
    IconCacheEntry *cached = CacheFind(icon->path, icon->size);
    if (cached && cached->texture) {
        // Return a reference (caller doesn't own this)
        return cached->texture;
    }
    
    SDL_Texture *texture = NULL;
    
    if (icon->icon_path) {
#ifdef HAVE_SDL_IMAGE
        // Load from icon file
        SDL_Surface *surface = IMG_Load(icon->icon_path);
        if (surface) {
            // Scale to desired size
            SDL_Surface *scaled = SDL_CreateRGBSurfaceWithFormat(0,
                icon->size, icon->size, 32, SDL_PIXELFORMAT_ARGB8888);
            if (scaled) {
                SDL_BlitScaled(surface, NULL, scaled, NULL);
                texture = SDL_CreateTextureFromSurface(renderer, scaled);
                SDL_FreeSurface(scaled);
            }
            SDL_FreeSurface(surface);
        }
#endif
    }
    
    if (!texture) {
        // Create a fallback generic icon
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0,
            icon->size, icon->size, 32, SDL_PIXELFORMAT_ARGB8888);
        if (surface) {
            // Draw a simple generic document icon
            Uint32 *pixels = surface->pixels;
            int w = surface->w, h = surface->h;
            Uint32 color = 0xFF888888;  // Gray
            Uint32 dark = 0xFF666666;
            
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    // Document shape with folded corner
                    if (x < w - h/4 && y < h) {
                        pixels[y * w + x] = color;
                    }
                    if (x >= w - h/4 && y < h/4) {
                        pixels[y * w + x] = dark;
                    }
                }
            }
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
        }
    }
    
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        CacheAdd(icon->path, icon->size, texture);
    }
    
    return texture;
}

void PlatformIconFree(PlatformIcon *icon) {
    if (!icon) return;
    free(icon->path);
    free(icon->icon_path);
    free(icon);
}

bool PlatformIconIsExecutable(const char *path) {
    if (!path || !*path) return false;
    
    struct stat st;
    if (stat(path, &st) != 0) return false;
    
    // Check executable bit
    if (st.st_mode & S_IXUSR) return true;
    
    // Check for .desktop extension
    const char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".desktop") == 0) return true;
    
    return false;
}
