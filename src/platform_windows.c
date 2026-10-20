#include "platform.h"

#define WIN32_LEAN_AND_MEAN
// SetProcessDPIAware() and the other APIs used here require Windows Vista or
// later; MinGW defaults to an older version otherwise.
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objbase.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_syswm.h>
#ifdef _MSC_VER
#include <VersionHelpers.h>
#endif
#define TRAY_WINAPI 1
#include <tray.h>

#include "fail.h"
#include "log.h"

extern Uint32 g_open_file_event;

static char* OpenVideoFile();

static void TrayQuit(struct tray_menu *item) {
    SDL_QuitEvent event;
    event.type = SDL_QUIT;
    event.timestamp = SDL_GetTicks();
    SDL_PushEvent((SDL_Event*)&event);
}

static void TraySetWallpaper(struct tray_menu *item) {
    char *file = OpenVideoFile();
    if (!file) return;
    
    SDL_Event event;
    SDL_zero(event);
    event.type = g_open_file_event;
    event.user.data1 = file;
    SDL_PushEvent(&event);
}

static struct tray tray = {
    .icon = "icon.ico",
    .menu = (struct tray_menu[]){{.text = "Live Paper", .disabled = true},
                                {.text = "-"},
                                {.text = "Set wallpaper...", .cb = &TraySetWallpaper},
                                {.text = "Quit", .cb = &TrayQuit},
                                {.text = NULL}},
};

static char config_dir[MAX_PATH];
static char config_file[MAX_PATH];

static void InitConfigPath() {
    if (config_file[0]) return; // already initialized
    char appdata[MAX_PATH] = {0};
    HRESULT hr = SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata);
    if (FAILED(hr)) FAIL();
    PathCombine(config_dir, appdata, "LivePaper\\");
    PathCombine(config_file, config_dir, "config.txt");
}

static void SaveConfig(const struct Context *context) {
    if (!context->file) return;
    InitConfigPath();
    char file[MAX_PATH] = {0};
    if (PathIsRelativeA(context->file)) {
        _fullpath(file, context->file, MAX_PATH);
    } else {
        strcpy(file, context->file);
    }
    SHCreateDirectoryExA(NULL, config_dir, NULL);
    FILE *f = fopen(config_file, "wt");
    fputs(file, f);
    fclose(f);
}

static void LoadConfig(struct Context *context) {
    InitConfigPath();
    if (!PathFileExistsA(config_file)) return;
    FILE *f = fopen(config_file, "rt");
    char *file = malloc(MAX_PATH);
    if (fgets(file, MAX_PATH, f) >= 0) {
        assert(!context->file);
        context->file = file;
    }
    fclose(f);
    DeleteFileA(config_file);
}

static char* OpenVideoFile() {
    char *file = malloc(MAX_PATH);
    file[0] = 0;
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "All Files\0*.*\0\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (!GetOpenFileNameA(&ofn)) {
        free(file);
        file = 0;
    }
    return file;
}

void PlatformInitGuiMode(struct Context *context) {
    if (context->file) FAIL();
    LoadConfig(context);
    if (!context->file)
        context->file = OpenVideoFile();
}

const char *PlatformWallpaperMechanism(void) {
    return "Windows WorkerW";
}

// The maximum number of WorkerW windows that are examined.
#define MAX_WORKER_WINDOWS 64

// The handles of the top-level WorkerW windows. EnumWindows() visits the
// windows in z-order, from the topmost one down to the bottom one.
struct WorkerWList {
    HWND handles[MAX_WORKER_WINDOWS];
    int count;
};

static BOOL CALLBACK EnumWorkerWProc(HWND hwnd, LPARAM lParam) {
    struct WorkerWList *list = (struct WorkerWList *)lParam;
    char classname[32] = {0};
    if (GetClassName(hwnd, classname, sizeof(classname)) > 0 &&
        strcmp(classname, "WorkerW") == 0) {
        if (list->count < MAX_WORKER_WINDOWS)
            list->handles[list->count++] = hwnd;
    }
    return TRUE;
}

static bool IsWorkerW(HWND hwnd) {
    if (!hwnd) return false;
    char classname[32] = {0};
    return GetClassName(hwnd, classname, sizeof(classname)) > 0 &&
           strcmp(classname, "WorkerW") == 0;
}

// Finds the top-level window that contains the desktop icons (the
// SHELLDLL_DefView window). On Windows 11 24H2 this is the window that also
// paints the wallpaper, so it is the one the wallpaper window has to be
// placed in.
static BOOL CALLBACK EnumIconsHostProc(HWND top, LPARAM lParam) {
    HWND *found = (HWND *)lParam;
    if (*found) return FALSE;   // already found
    if (FindWindowEx(top, NULL, "SHELLDLL_DefView", NULL)) {
        *found = top;
        return FALSE;
    }
    return TRUE;
}

static HWND FindDesktopIconsHost(void) {
    HWND found = NULL;
    EnumWindows(EnumIconsHostProc, (LPARAM)&found);
    return found;
}

// The IVirtualDesktopManager interface of Windows 10 and later. It is used to
// find out which virtual desktop a window is on, and to move a window to
// another one. The SDK doesn't declare it in a way that can be used from C,
// so the vtable is declared here.
typedef struct {
    // IUnknown
    HRESULT (WINAPI *QueryInterface)(void *self, REFIID iid, void **out);
    ULONG   (WINAPI *AddRef)(void *self);
    ULONG   (WINAPI *Release)(void *self);
    // IVirtualDesktopManager
    HRESULT (WINAPI *IsWindowOnCurrentVirtualDesktop)(void *self, HWND hwnd, BOOL *is_on_current);
    HRESULT (WINAPI *GetWindowDesktopId)(void *self, HWND hwnd, GUID *desktop_id);
    HRESULT (WINAPI *MoveWindowToDesktop)(void *self, HWND hwnd, GUID desktop_id);
} LpVirtualDesktopManagerVtbl;

typedef struct {
    const LpVirtualDesktopManagerVtbl *vtbl;
} LpVirtualDesktopManager;

static const GUID g_clsid_virtual_desktop_manager = {
    0xaa509086, 0x5ca1, 0x4f1b, {0x9f, 0x22, 0x62, 0x87, 0xa2, 0xb7, 0xa3, 0x4d}
};
static const GUID g_iid_virtual_desktop_manager = {
    0xa5cd92ff, 0x29be, 0x454c, {0x8d, 0x04, 0xd8, 0x28, 0x79, 0xfb, 0x3f, 0x1b}
};

// Returns the virtual desktop manager, or NULL if it is not available
// (Windows 8 and older, or the COM initialization failed).
static LpVirtualDesktopManager *GetVirtualDesktopManager(void) {
    static LpVirtualDesktopManager *manager = NULL;
    static bool tried = false;
    if (tried) return manager;
    tried = true;

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
        return NULL;

    void *obj = NULL;
    hr = CoCreateInstance(&g_clsid_virtual_desktop_manager, NULL, CLSCTX_INPROC_SERVER,
                          &g_iid_virtual_desktop_manager, &obj);
    if (FAILED(hr) || !obj) return NULL;
    manager = (LpVirtualDesktopManager *)obj;
    LOG_DEBUG("the virtual desktop manager is available");
    return manager;
}

// Tells whether the window is on the virtual desktop that is currently shown.
// If the virtual desktop manager is not available, every window is assumed to
// be on the current one.
static bool IsOnCurrentVirtualDesktop(HWND hwnd) {
    LpVirtualDesktopManager *manager = GetVirtualDesktopManager();
    if (!manager) return true;
    BOOL on_current = FALSE;
    HRESULT hr = manager->vtbl->IsWindowOnCurrentVirtualDesktop(manager, hwnd, &on_current);
    if (FAILED(hr)) return true;
    return on_current != FALSE;
}

// Returns the identifier of the virtual desktop that is currently shown.
static bool GetCurrentVirtualDesktopId(GUID *desktop_id) {
    LpVirtualDesktopManager *manager = GetVirtualDesktopManager();
    if (!manager) return false;
    HWND desktop = GetDesktopWindow();
    if (!desktop) return false;
    HRESULT hr = manager->vtbl->GetWindowDesktopId(manager, desktop, desktop_id);
    return SUCCEEDED(hr);
}

// Collects the top-level Progman windows.
struct ProgmanList {
    HWND handles[MAX_WORKER_WINDOWS];
    int count;
};

static BOOL CALLBACK EnumProgmanProc(HWND hwnd, LPARAM lParam) {
    struct ProgmanList *list = (struct ProgmanList *)lParam;
    char classname[32] = {0};
    if (GetClassName(hwnd, classname, sizeof(classname)) > 0 &&
        strcmp(classname, "Progman") == 0) {
        if (list->count < MAX_WORKER_WINDOWS)
            list->handles[list->count++] = hwnd;
    }
    return TRUE;
}

// Returns the Progman window of the virtual desktop that is currently shown.
// Every virtual desktop of Windows 11 has its own Progman window, so the
// wallpaper has to be attached to the right one.
static HWND FindCurrentDesktopProgman(void) {
    struct ProgmanList list = {0};
    EnumWindows(EnumProgmanProc, (LPARAM)&list);
    if (list.count == 0) return NULL;

    HWND fallback = list.handles[0];
    for (int i = 0; i < list.count; i++) {
        if (IsOnCurrentVirtualDesktop(list.handles[i]))
            return list.handles[i];
    }
    LOG_WARN("none of the %d Progman windows is on the current virtual desktop", list.count);
    return fallback;
}

// Describes the state of a window, for the diagnostics.
static void DescribeWindow(HWND hwnd, char *buf, size_t buf_size) {
    char classname[32] = {0};
    GetClassName(hwnd, classname, sizeof(classname));
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    snprintf(buf, buf_size,
             "class='%s' visible=%d WS_VISIBLE=%d WS_CHILD=%d WS_POPUP=%d",
             classname, IsWindowVisible(hwnd) ? 1 : 0,
             (style & WS_VISIBLE) ? 1 : 0,
             (style & WS_CHILD) ? 1 : 0,
             (style & WS_POPUP) ? 1 : 0);
}

// The result of attaching the wallpaper window to a host window.
enum AttachResult {
    ATTACH_FAILED = 0,   // the window couldn't be attached at all
    ATTACH_HIDDEN,       // attached, but the wallpaper is not visible
    ATTACH_OK,           // attached and visible
};

// Attaches the wallpaper window to the given parent window.
// require_full_screen rejects the parents that are too small to hold the
// wallpaper; some of the WorkerW windows are only small helper windows.
static enum AttachResult AttachToWallpaperHost(HWND sdl_hwnd, HWND parent, bool require_full_screen) {
    if (SetParent(sdl_hwnd, parent) == NULL)
        return ATTACH_FAILED;

    // SetParent() doesn't change the window styles, so the window would still
    // be a popup window: it would be shown as an ordinary window on top of the
    // desktop, which could be moved around. A child window is painted as a
    // part of the desktop instead.
    LONG style = GetWindowLong(sdl_hwnd, GWL_STYLE);
    style = (style & ~WS_POPUP) | WS_CHILD;
    SetWindowLong(sdl_hwnd, GWL_STYLE, style);
    // Don't steal the focus and don't show up in the task bar.
    LONG ex_style = GetWindowLong(sdl_hwnd, GWL_EXSTYLE);
    ex_style |= WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    ex_style &= ~WS_EX_APPWINDOW;
    SetWindowLong(sdl_hwnd, GWL_EXSTYLE, ex_style);

    // The coordinates of a child window are relative to the client area of its
    // parent, so the window is placed over the whole of it.
    RECT client;
    if (GetClientRect(parent, &client)) {
        int w = client.right - client.left;
        int h = client.bottom - client.top;
        if (require_full_screen &&
            (w < GetSystemMetrics(SM_CXSCREEN) || h < GetSystemMetrics(SM_CYSCREEN)))
            return ATTACH_HIDDEN;

        // The window is placed directly below the view that shows the desktop
        // icons, so that the icons stay on top of the wallpaper and keep
        // receiving the mouse clicks, while the wallpaper covers everything
        // that is below them.
        HWND icons_view = FindWindowEx(parent, NULL, "SHELLDLL_DefView", NULL);
        HWND insert_after = icons_view ? icons_view : HWND_BOTTOM;
        SetWindowPos(sdl_hwnd, insert_after, 0, 0, w, h, SWP_NOACTIVATE);
    }
    ShowWindow(sdl_hwnd, SW_SHOW);
    return IsWindowVisible(sdl_hwnd) ? ATTACH_OK : ATTACH_HIDDEN;
}

// Formats the window style bits into a readable string.
static void DescribeStyles(LONG_PTR style, char *buf, size_t buf_size) {
    static const struct { LONG_PTR bit; const char *name; } known[] = {
        { WS_POPUP,        "WS_POPUP" },
        { WS_CHILD,        "WS_CHILD" },
        { WS_MINIMIZE,     "WS_MINIMIZE" },
        { WS_VISIBLE,      "WS_VISIBLE" },
        { WS_DISABLED,     "WS_DISABLED" },
        { WS_CLIPSIBLINGS, "WS_CLIPSIBLINGS" },
        { WS_CLIPCHILDREN, "WS_CLIPCHILDREN" },
        { WS_MAXIMIZE,     "WS_MAXIMIZE" },
        { WS_CAPTION,      "WS_CAPTION" },
        { WS_BORDER,       "WS_BORDER" },
        { WS_DLGFRAME,     "WS_DLGFRAME" },
        { WS_VSCROLL,      "WS_VSCROLL" },
        { WS_HSCROLL,      "WS_HSCROLL" },
        { WS_SYSMENU,      "WS_SYSMENU" },
        { WS_THICKFRAME,   "WS_THICKFRAME" },
        { WS_MINIMIZEBOX,  "WS_MINIMIZEBOX" },
        { WS_MAXIMIZEBOX,  "WS_MAXIMIZEBOX" },
    };
    buf[0] = '\0';
    size_t pos = 0;
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if (!(style & known[i].bit)) continue;
        int written = snprintf(buf + pos, buf_size - pos, "%s%s", pos ? "|" : "", known[i].name);
        if (written < 0) break;
        pos += (size_t)written;
        if (pos >= buf_size) break;
    }
    if (pos == 0)
        snprintf(buf, buf_size, "0x%llx", (unsigned long long)style);
}

// Prints the class, the styles and the geometry of a window.
static void DescribeWindowFull(HWND hwnd, char *buf, size_t buf_size) {
    char classname[64] = {0};
    char title[128] = {0};
    GetClassName(hwnd, classname, sizeof(classname));
    GetWindowText(hwnd, title, sizeof(title));
    char styles[128] = {0};
    DescribeStyles(GetWindowLongPtr(hwnd, GWL_STYLE), styles, sizeof(styles));
    RECT wr = {0}, cr = {0};
    GetWindowRect(hwnd, &wr);
    GetClientRect(hwnd, &cr);
    snprintf(buf, buf_size,
             "class='%s' title='%s' visible=%d style=%s rect=(%ld,%ld)-(%ld,%ld) client=%ldx%ld",
             classname, title, IsWindowVisible(hwnd) ? 1 : 0, styles,
             (long)wr.left, (long)wr.top, (long)wr.right, (long)wr.bottom,
             (long)(cr.right - cr.left), (long)(cr.bottom - cr.top));
}

// The state of the enumeration of the top-level windows.
struct WindowDump {
    int index;
};

// Collects the top-level windows. EnumWindows() visits them in z-order,
// from the topmost one to the bottom one.
static BOOL CALLBACK DumpWindowProc(HWND hwnd, LPARAM lParam) {
    struct WindowDump *dump = (struct WindowDump *)lParam;
    char info[512] = {0};
    DescribeWindowFull(hwnd, info, sizeof(info));
    LOG_INFO("  [%2d] hwnd=%p %s", dump->index++, (void *)hwnd, info);
    return TRUE;
}

// Collects the child windows of a parent, in z-order.
static BOOL CALLBACK DumpChildProc(HWND hwnd, LPARAM lParam) {
    struct WindowDump *dump = (struct WindowDump *)lParam;
    char info[512] = {0};
    DescribeWindowFull(hwnd, info, sizeof(info));
    LOG_INFO("  [%2d] hwnd=%p %s", dump->index++, (void *)hwnd, info);
    return TRUE;
}

// Checks whether the given colour is really shown on the screen at the given
// position. The screen is read back with BitBlt(), which returns the
// composited desktop, so this tells whether the wallpaper window is shown at
// all.
bool PlatformScreenShowsColor(int x, int y, uint32_t rgb) {
    HDC screen = GetDC(NULL);
    if (!screen) return false;

    bool matches = false;
    HDC mem_dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = mem_dc ? CreateCompatibleBitmap(screen, 1, 1) : NULL;
    HGDIOBJ old = NULL;
    if (mem_dc && bitmap) {
        old = SelectObject(mem_dc, bitmap);
        if (BitBlt(mem_dc, 0, 0, 1, 1, screen, x, y, SRCCOPY)) {
            COLORREF pixel = GetPixel(mem_dc, 0, 0);
            uint32_t r = (uint32_t)GetRValue(pixel);
            uint32_t g = (uint32_t)GetGValue(pixel);
            uint32_t b = (uint32_t)GetBValue(pixel);
            LOG_DEBUG("the screen shows R=%u G=%u B=%u at (%d,%d), expected R=%u G=%u B=%u",
                      r, g, b, x, y, (rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
            // A small difference is allowed, the screen may be scaled.
            int diff = (int)r - (int)((rgb >> 16) & 0xFF);
            diff += (int)g - (int)((rgb >> 8) & 0xFF);
            diff += (int)b - (int)(rgb & 0xFF);
            if (diff < 0) diff = -diff;
            matches = diff <= 24;
        }
        SelectObject(mem_dc, old);
    }
    if (bitmap) DeleteObject(bitmap);
    if (mem_dc) DeleteDC(mem_dc);
    ReleaseDC(NULL, screen);
    return matches;
}

bool PlatformCanCheckScreen(void) {
    return true;
}

// Returns the position of a top-level window in the z-order (0 is the
// topmost one), or -1 if it is not found.
static int TopLevelZIndex(HWND hwnd) {
    int index = 0;
    HWND h = GetTopWindow(NULL);
    while (h) {
        if (h == hwnd) return index;
        index++;
        h = GetWindow(h, GW_HWNDNEXT);
    }
    return -1;
}

// The ways to prepare the layer of the desktop icons.
enum {
    ICONS_NONE = 0,     // the layer is not touched at all
    ICONS_REDRAW,       // the shell is asked to paint the layer again
    ICONS_REFRESH,      // the layer is hidden and shown again
};

// The way the layer of the desktop icons is prepared, and whether the message
// 0x052c is sent to the Progman window. They are set from the command line.
//
// The default is ICONS_REFRESH: it is the only mode that removes the snapshot
// of the desktop, which is what keeps the icons from being painted.
static int g_icons_mode = ICONS_REFRESH;
static bool g_prepare_desktop = true;

void PlatformSetDesktopIconsMode(int mode) {
    g_icons_mode = mode;
}

void PlatformSetPrepareDesktop(bool enable) {
    g_prepare_desktop = enable;
}

// Sends the message 0x052c to the Progman window. It is the message that the
// shell sends to itself when the desktop is switched (Win + Tab, the key
// combination of Win + Ctrl + D, ...). It makes Windows create the WorkerW
// window that paints the wallpaper, and it turns the layer of the desktop
// icons into a transparent layer.
//
// Without it, the WorkerW window of the desktop can be in a state where the
// wallpaper is never shown, which is a known problem of Windows 11 24H2.
//
// This is the technique that is used by the other live wallpapers:
// https://www.autoitscript.com/forum/topic/212981-move-window-behind-desktop-icons/
static void PrepareDesktopLayer(HWND progman) {
    static bool done = false;
    if (done || !progman) return;
    done = true;

    if (!g_prepare_desktop) {
        LOG_DEBUG("the message 0x052c was not sent to Progman");
        return;
    }
    DWORD_PTR result = 0;
    SendMessageTimeout(progman, 0x052c, 0, 0, SMTO_NORMAL, 3000, &result);
    LOG_DEBUG("the message 0x052c was sent to Progman, to prepare the desktop layer");
}


// The layer of the desktop icons is drawn on a transparent layer (the
// SHELLDLL_DefView window of Progman), but Windows keeps a snapshot of the
// desktop over that layer to make the switch between the desktops smooth. The
// snapshot is what hides the wallpaper, and it also swallows the mouse clicks.
//
// Hiding the SHELLDLL_DefView window and showing it again makes the shell
// repaint the layer, which removes the snapshot: the wallpaper becomes visible
// below the icons, and the clicks keep working.
//
// The icons themselves are not drawn by the SHELLDLL_DefView window, but by
// the SysListView32 window (the "FolderView" list) inside it. The shell only
// repaints that list when the mouse is over it, so it has to be repainted
// explicitly: otherwise the icons stay invisible, even though they are still
// there and they can be clicked.
//
// This is the trick documented for the dynamic wallpaper of Windows 11 24H2:
// https://blog.cast1e.top/posts/windeskchange/wdc/
static void RepaintDesktopIcons(HWND defview, bool refresh_layer) {
    if (!defview) return;

    if (refresh_layer) {
        // The layer of the icons is hidden and shown again: the shell then
        // creates it from scratch, without the snapshot of the desktop.
        ShowWindow(defview, SW_HIDE);
        Sleep(0);
        ShowWindow(defview, SW_SHOWNORMAL);
        LOG_DEBUG("the layer of the desktop icons was hidden and shown again");
    }

    // The view that holds the icons is the one that draws them, so it is
    // repainted explicitly. The erase flag is not used, because the view is
    // transparent: it must not paint a background over the wallpaper.
    HWND listview = FindWindowEx(defview, NULL, "SysListView32", NULL);
    if (listview) {
        RedrawWindow(listview, NULL, NULL,
                     RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        LOG_DEBUG("the view with the desktop icons was painted again");
    }
    RedrawWindow(defview, NULL, NULL,
                 RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

// The way the layer of the desktop icons is prepared when the wallpaper is
// attached.
static void PrepareDesktopIcons(HWND progman) {
    if (g_icons_mode == ICONS_NONE) return;
    HWND defview = FindWindowEx(progman, NULL, "SHELLDLL_DefView", NULL);
    if (!defview) {
        LOG_DEBUG("the window with the desktop icons was not found");
        return;
    }

    switch (g_icons_mode) {
        case ICONS_REDRAW:
            // The shell is asked to paint the layer again, without hiding it.
            RepaintDesktopIcons(defview, false);
            break;
        case ICONS_REFRESH:
            RepaintDesktopIcons(defview, true);
            break;
        default:
            break;
    }
}

// Finds the WorkerW window that paints the wallpaper. It is a child of the
// Progman window and it covers the whole screen.
static HWND FindWallpaperWorker(HWND progman) {
    HWND child = GetWindow(progman, GW_CHILD);
    for (; child; child = GetWindow(child, GW_HWNDNEXT)) {
        char classname[32] = {0};
        if (GetClassName(child, classname, sizeof(classname)) > 0 &&
            strcmp(classname, "WorkerW") == 0) {
            return child;
        }
    }
    return NULL;
}

// The strategies that are used to attach the wallpaper window. Windows 11
// composes the desktop in a way that no single method works on every
// version, so all of them are tried until the wallpaper shows up.
enum {
    WALLPAPER_STRATEGY_CHILD = 0,     // a child of Progman, below the icons
    WALLPAPER_STRATEGY_WORKERW,       // a child of the WorkerW that paints the wallpaper
    WALLPAPER_STRATEGY_POPUP_PARENT, // a popup directly below Progman
    WALLPAPER_STRATEGY_POPUP_BOTTOM, // a popup at the bottom of the z-order
    WALLPAPER_STRATEGY_POPUP_UNDER,  // a popup below Progman
    WALLPAPER_STRATEGY_LAYERED,      // a layered child of the WorkerW
    WALLPAPER_STRATEGY_CLICK_THROUGH, // a popup that the mouse clicks pass through itself
    WALLPAPER_STRATEGY_COUNT
};

int PlatformWallpaperStrategyCount(void) {
    return WALLPAPER_STRATEGY_COUNT;
}

// The order in which the strategies are tried. The ones that keep the desktop
// icons and the mouse clicks working are tried first: a popup window always
// covers the icons, so it is only the last resort.
int PlatformWallpaperStrategyOrder(int index) {
    static const int order[] = {
        WALLPAPER_STRATEGY_CHILD,
        WALLPAPER_STRATEGY_WORKERW,
        WALLPAPER_STRATEGY_LAYERED,
        WALLPAPER_STRATEGY_CLICK_THROUGH,
        WALLPAPER_STRATEGY_POPUP_PARENT,
        WALLPAPER_STRATEGY_POPUP_UNDER,
        WALLPAPER_STRATEGY_POPUP_BOTTOM,
    };
    int count = (int)(sizeof(order) / sizeof(order[0]));
    if (index < 0 || index >= count) return -1;
    return order[index];
}

const char *PlatformWallpaperStrategyName(int strategy) {
    switch (strategy) {
        case WALLPAPER_STRATEGY_CHILD:         return "a child of Progman, below the icons";
        case WALLPAPER_STRATEGY_WORKERW:       return "a child of the WorkerW that paints the wallpaper";
        case WALLPAPER_STRATEGY_POPUP_PARENT: return "a popup directly below Progman";
        case WALLPAPER_STRATEGY_POPUP_BOTTOM: return "a popup at the bottom of the z-order";
        case WALLPAPER_STRATEGY_POPUP_UNDER:  return "a popup below Progman";
        case WALLPAPER_STRATEGY_LAYERED:      return "a layered child of the WorkerW";
        case WALLPAPER_STRATEGY_CLICK_THROUGH: return "a popup that the clicks pass through";
        default: return "unknown";
    }
}

// Prints the children of a window, in z-order, to see what is drawn over the
// wallpaper.
static void DumpChildWindows(HWND parent) {
    LOG_INFO("the children of the parent (z-order, topmost first):");
    struct WindowDump child_dump = {0};
    if (parent) EnumChildWindows(parent, DumpChildProc, (LPARAM)&child_dump);
    LOG_INFO("  (%d child window(s))", child_dump.index);
}

// Searches the whole screen for the given colour.
bool PlatformScreenHasColor(uint32_t rgb) {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    if (w <= 0 || h <= 0) return false;

    HDC screen = GetDC(NULL);
    if (!screen) return false;

    bool found = false;
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;   // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC mem_dc = CreateCompatibleDC(screen);
    void *bits = NULL;
    HBITMAP bitmap = mem_dc ? CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0) : NULL;
    HGDIOBJ old = NULL;
    if (mem_dc && bitmap && bits) {
        old = SelectObject(mem_dc, bitmap);
        if (BitBlt(mem_dc, 0, 0, w, h, screen, 0, 0, SRCCOPY)) {
            uint32_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
            uint8_t *pixels = (uint8_t *)bits;
            for (int y = 0; y < h && !found; y++) {
                uint32_t *row = (uint32_t *)(pixels + (size_t)y * (size_t)w * 4);
                for (int x = 0; x < w; x++) {
                    uint32_t p = row[x];
                    int dr = (int)((p >> 16) & 0xFF) - (int)r;
                    int dg = (int)((p >> 8) & 0xFF) - (int)g;
                    int db = (int)(p & 0xFF) - (int)b;
                    if (dr < 0) dr = -dr;
                    if (dg < 0) dg = -dg;
                    if (db < 0) db = -db;
                    if (dr <= 24 && dg <= 24 && db <= 24) {
                        found = true;
                        break;
                    }
                }
            }
        }
        SelectObject(mem_dc, old);
    }
    if (bitmap) DeleteObject(bitmap);
    if (mem_dc) DeleteDC(mem_dc);
    ReleaseDC(NULL, screen);
    return found;
}

// Captures the screen into a BMP file. It is the only reliable way to see
// what the desktop really looks like: the check of the colour is unreliable,
// because the compositor of Windows can show an old frame.
bool PlatformCaptureScreen(const char *path) {
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    if (w <= 0 || h <= 0 || !path) return false;

    HDC screen = GetDC(NULL);
    if (!screen) return false;

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;   // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    bool ok = false;
    HDC mem_dc = CreateCompatibleDC(screen);
    void *bits = NULL;
    HBITMAP bitmap = mem_dc ? CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0) : NULL;
    HGDIOBJ old = NULL;
    if (mem_dc && bitmap && bits) {
        old = SelectObject(mem_dc, bitmap);
        ok = BitBlt(mem_dc, 0, 0, w, h, screen, 0, 0, SRCCOPY) != 0;
        SelectObject(mem_dc, old);
    }

    if (ok) {
        // The header of a BMP file: 14 bytes, then the header of the bitmap.
        // The offset of the pixels is 14 + 40 = 54: if it is wrong, every
        // program that reads the file is off by a few bytes, and the colours
        // are completely wrong.
        unsigned char file_header[14] = {0};
        unsigned int pixel_size = (unsigned int)w * (unsigned int)h * 4;
        unsigned int file_size = 14 + (unsigned int)sizeof(BITMAPINFOHEADER) + pixel_size;
        unsigned int pixel_offset = 14 + (unsigned int)sizeof(BITMAPINFOHEADER);
        file_header[0] = 'B'; file_header[1] = 'M';
        file_header[2] = (unsigned char)(file_size & 0xFF);
        file_header[3] = (unsigned char)((file_size >> 8) & 0xFF);
        file_header[4] = (unsigned char)((file_size >> 16) & 0xFF);
        file_header[5] = (unsigned char)((file_size >> 24) & 0xFF);
        file_header[10] = (unsigned char)(pixel_offset & 0xFF);
        file_header[11] = (unsigned char)((pixel_offset >> 8) & 0xFF);
        file_header[12] = (unsigned char)((pixel_offset >> 16) & 0xFF);
        file_header[13] = (unsigned char)((pixel_offset >> 24) & 0xFF);

        FILE *file = fopen(path, "wb");
        if (file) {
            fwrite(file_header, 1, sizeof(file_header), file);
            fwrite(&bmi.bmiHeader, 1, sizeof(BITMAPINFOHEADER), file);
            fwrite(bits, 1, pixel_size, file);
            fclose(file);
            LOG_INFO("the screen was captured in '%s'", path);
        } else {
            LOG_WARN("can't create the file '%s' for the capture of the screen", path);
            ok = false;
        }
    }

    if (bitmap) DeleteObject(bitmap);
    if (mem_dc) DeleteDC(mem_dc);
    ReleaseDC(NULL, screen);
    return ok;
}

// Attaches the wallpaper window with the given strategy.
bool PlatformAttachWallpaper(struct Context *context, int strategy) {
    if (!context->window) return false;

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(context->window, &info)) return false;
    HWND sdl_hwnd = info.info.win.window;

    HWND progman = FindCurrentDesktopProgman();
    if (!progman) {
        LOG_WARN("can't find the Progman window of the current virtual desktop");
        return false;
    }

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    // Makes Windows create the WorkerW window of the desktop and turn the layer
    // of the desktop icons into a transparent layer.
    PrepareDesktopLayer(progman);

    bool layered = false;
    switch (strategy) {
        case WALLPAPER_STRATEGY_CHILD: {
            // A child of Progman, below the view that shows the desktop icons.
            if (SetParent(sdl_hwnd, progman) == NULL) return false;
            LONG style = GetWindowLong(sdl_hwnd, GWL_STYLE);
            SetWindowLong(sdl_hwnd, GWL_STYLE, (style & ~WS_POPUP) | WS_CHILD);
            EnableWindow(sdl_hwnd, TRUE);
            break;
        }
        case WALLPAPER_STRATEGY_WORKERW: {
            // A child of the WorkerW window that paints the wallpaper: it is
            // below the desktop icons (which are in Progman) and above the
            // wallpaper itself.
            HWND wallpaper_worker = FindWallpaperWorker(progman);
            if (!wallpaper_worker) return false;
            if (SetParent(sdl_hwnd, wallpaper_worker) == NULL) return false;
            LONG style = GetWindowLong(sdl_hwnd, GWL_STYLE);
            SetWindowLong(sdl_hwnd, GWL_STYLE, (style & ~WS_POPUP) | WS_CHILD);
            // The WorkerW window is disabled, and a disabled parent disables
            // its children as well.
            EnableWindow(sdl_hwnd, TRUE);
            break;
        }
        case WALLPAPER_STRATEGY_LAYERED: {
            // The same as the strategy 1, but the window is a layered window:
            // the compositor of Windows then draws it over the desktop, and the
            // layer of the desktop icons stays on top of it.
            HWND wallpaper_worker = FindWallpaperWorker(progman);
            if (!wallpaper_worker) return false;
            if (SetParent(sdl_hwnd, wallpaper_worker) == NULL) return false;
            LONG style = GetWindowLong(sdl_hwnd, GWL_STYLE);
            SetWindowLong(sdl_hwnd, GWL_STYLE, (style & ~WS_POPUP) | WS_CHILD);
            EnableWindow(sdl_hwnd, TRUE);
            layered = true;
            break;
        }
        case WALLPAPER_STRATEGY_POPUP_PARENT:
        case WALLPAPER_STRATEGY_POPUP_BOTTOM:
        case WALLPAPER_STRATEGY_POPUP_UNDER:
        case WALLPAPER_STRATEGY_CLICK_THROUGH: {
            // A top-level popup window: the only kind of window that the
            // system composites reliably.
            SetParent(sdl_hwnd, NULL);
            LONG style = GetWindowLong(sdl_hwnd, GWL_STYLE);
            SetWindowLong(sdl_hwnd, GWL_STYLE, (style | WS_POPUP) & ~WS_CHILD);
            break;
        }
        default:
            return false;
    }

    LONG ex_style = GetWindowLong(sdl_hwnd, GWL_EXSTYLE);
    ex_style = (ex_style | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW) & ~WS_EX_APPWINDOW;
    if (layered) ex_style |= WS_EX_LAYERED;
    if (strategy == WALLPAPER_STRATEGY_CLICK_THROUGH) {
        // A window with WS_EX_TRANSPARENT is skipped when the system looks for
        // the window under the mouse pointer: the clicks reach the desktop.
        ex_style |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
    }
    SetWindowLong(sdl_hwnd, GWL_EXSTYLE, ex_style);
    if (layered) SetLayeredWindowAttributes(sdl_hwnd, 0, 255, LWA_ALPHA);
    if (strategy == WALLPAPER_STRATEGY_CLICK_THROUGH)
        SetLayeredWindowAttributes(sdl_hwnd, 0, 255, LWA_ALPHA);

    // The window is shown without being activated: SW_SHOW would raise it
    // above the desktop icons, which would hide them and swallow the mouse
    // clicks of the desktop.
    ShowWindow(sdl_hwnd, SW_SHOWNOACTIVATE);

    // The z-order is set after the window is shown, because showing a window
    // can change it.
    HWND insert_after;
    if (strategy == WALLPAPER_STRATEGY_CHILD) {
        // The window is placed directly above the WorkerW window that paints
        // the wallpaper, so that it covers the wallpaper, but stays below the
        // desktop icons (which are drawn by the SysListView32 inside the
        // SHELLDLL_DefView window).
        HWND wallpaper_worker = FindWallpaperWorker(progman);
        HWND icons_view = FindWindowEx(progman, NULL, "SHELLDLL_DefView", NULL);
        if (wallpaper_worker) {
            // The window that is just above the wallpaper window.
            HWND above = GetWindow(wallpaper_worker, GW_HWNDPREV);
            insert_after = above ? above : HWND_TOP;
        } else if (icons_view) {
            insert_after = icons_view;
        } else {
            insert_after = HWND_BOTTOM;
        }
    } else if (strategy == WALLPAPER_STRATEGY_WORKERW ||
               strategy == WALLPAPER_STRATEGY_LAYERED) {
        insert_after = HWND_TOP;
    } else if (strategy == WALLPAPER_STRATEGY_POPUP_PARENT) {
        insert_after = progman;      // directly below Progman
    } else if (strategy == WALLPAPER_STRATEGY_POPUP_UNDER) {
        // Below Progman itself: the window is inserted after the window that
        // is below Progman, so that the desktop icons (which are a child of
        // Progman) stay on top of the wallpaper and keep the mouse clicks.
        HWND below = GetWindow(progman, GW_HWNDNEXT);
        insert_after = below ? below : HWND_BOTTOM;
    } else {
        insert_after = HWND_BOTTOM;
    }
    SetWindowPos(sdl_hwnd, insert_after, 0, 0, screen_w, screen_h,
                 SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // The wallpaper is a child of the desktop: the layer of the desktop icons
    // has to be made transparent, otherwise the snapshot that Windows keeps
    // over it hides the wallpaper.
    PrepareDesktopIcons(progman);

    char own_info[128] = {0};
    DescribeWindow(sdl_hwnd, own_info, sizeof(own_info));
    LOG_DEBUG("the wallpaper window after the strategy %d: %s", strategy, own_info);
    LOG_DEBUG("  it is at the position %d of the z-order, Progman is at the position %d",
              TopLevelZIndex(sdl_hwnd), TopLevelZIndex(progman));
    // The children of the parent are printed after every strategy: it shows
    // exactly where the wallpaper window ended up, and what is drawn over it.
    if (LogGetLevel() >= LOG_LEVEL_DEBUG) DumpChildWindows(GetParent(sdl_hwnd));
    return true;
}

// The number of times the layer of the desktop icons is hidden and shown again
// after the wallpaper was attached. The shell creates the snapshot of the
// desktop again when the wallpaper window is shown, so the layer has to be
// refreshed a few times in the beginning. After that the icons are only
// painted again, which doesn't make them flicker.
#define ICON_REFRESH_ROUNDS 4

// Makes the layer of the desktop icons transparent again. It is called once
// the first frames are drawn, because the shell can create the snapshot of the
// desktop again when the wallpaper window is shown.
void PlatformRefreshDesktopIcons(void) {
    static int rounds_left = ICON_REFRESH_ROUNDS;

    HWND progman = FindCurrentDesktopProgman();
    if (!progman) return;
    if (g_icons_mode == ICONS_NONE) return;
    HWND defview = FindWindowEx(progman, NULL, "SHELLDLL_DefView", NULL);
    if (!defview) return;

    bool refresh = (g_icons_mode == ICONS_REFRESH) && rounds_left > 0;
    if (refresh) rounds_left--;
    RepaintDesktopIcons(defview, refresh);
}

// Prints the structure of the desktop windows. It is used to find out how the
// desktop is composed on the different versions of Windows.
void PlatformDumpWindows(void) {
    LOG_INFO("=== screen ===");
    LOG_INFO("  primary screen: %dx%d at (%ld,%ld)",
             GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
             (long)GetSystemMetrics(SM_XVIRTUALSCREEN),
             (long)GetSystemMetrics(SM_YVIRTUALSCREEN));
    LOG_INFO("  virtual screen: %dx%d at (%ld,%ld)",
             GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN),
             (long)GetSystemMetrics(SM_XVIRTUALSCREEN),
             (long)GetSystemMetrics(SM_YVIRTUALSCREEN));

    LOG_INFO("=== top-level windows (in z-order, topmost first) ===");
    struct WindowDump dump = {0};
    EnumWindows(DumpWindowProc, (LPARAM)&dump);

    HWND progman = FindWindow("Progman", NULL);
    LOG_INFO("=== Progman ===");
    if (progman) {
        char info[512] = {0};
        DescribeWindowFull(progman, info, sizeof(info));
        LOG_INFO("  hwnd=%p %s", (void *)progman, info);
    } else {
        LOG_INFO("  not found");
    }

    LOG_INFO("=== the desktop icons (SHELLDLL_DefView) ===");
    HWND icons_host = FindDesktopIconsHost();
    if (icons_host) {
        char info[512] = {0};
        DescribeWindowFull(icons_host, info, sizeof(info));
        LOG_INFO("  host: hwnd=%p %s", (void *)icons_host, info);
        HWND defview = FindWindowEx(icons_host, NULL, "SHELLDLL_DefView", NULL);
        if (defview) {
            char dv[512] = {0};
            DescribeWindowFull(defview, dv, sizeof(dv));
            LOG_INFO("  SHELLDLL_DefView: hwnd=%p %s", (void *)defview, dv);
        }
        LOG_INFO("  children of the host (in z-order, topmost first):");
        struct WindowDump child_dump = {0};
        EnumChildWindows(icons_host, DumpChildProc, (LPARAM)&child_dump);
        LOG_INFO("  (%d child window(s))", child_dump.index);
    } else {
        LOG_INFO("  not found");
    }

    LOG_INFO("=== WorkerW windows ===");
    struct WorkerWList list = {0};
    EnumWindows(EnumWorkerWProc, (LPARAM)&list);
    for (int i = 0; i < list.count; i++) {
        char info[512] = {0};
        DescribeWindowFull(list.handles[i], info, sizeof(info));
        LOG_INFO("  hwnd=%p %s", (void *)list.handles[i], info);
    }
    LOG_INFO("  (%d WorkerW window(s))", list.count);
}

void PlatformInit(struct Context *context) {
    SetProcessDPIAware();
    if (tray_init(&tray) == -1)
        FAIL_WITH("can't create tray with icon '%s'", tray.icon);
    LOG_INFO("tray icon initialized");

    HWND progman = FindCurrentDesktopProgman();
    if (progman == NULL) FAIL_WITH("can't find the Progman window of the desktop");
    LOG_INFO("the Progman window of the current virtual desktop is %p", (void *)progman);
    // Send the (undocumented) message to trigger the creation of WorkerW in required position
    if (SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, NULL) == 0)
        FAIL();

    // Collect the WorkerW windows that can host the wallpaper. Windows 11
    // 24H2 creates more than one of them and only one of them is suitable, so
    // all of them are tried below.
    struct WorkerWList list = {0};
    EnumWindows(EnumWorkerWProc, (LPARAM)&list);

    HWND candidates[MAX_WORKER_WINDOWS + 3];
    int candidate_count = 0;
    // The window that holds the desktop icons is the one that paints the
    // wallpaper on Windows 11 24H2, so it is the preferred host. The wallpaper
    // window is placed at the bottom of its children, so the icons stay on top
    // of it.
    HWND icons_host = NULL;
    if (FindWindowEx(progman, NULL, "SHELLDLL_DefView", NULL))
        icons_host = progman;      // the desktop icons of the current virtual desktop
    else
        icons_host = FindDesktopIconsHost();
    if (icons_host && icons_host != progman)
        candidates[candidate_count++] = icons_host;
    // The classic choice: the previous sibling of Progman.
    HWND classic = GetWindow(progman, GW_HWNDPREV);
    if (IsWorkerW(classic))
        candidates[candidate_count++] = classic;
    // The rest, starting from the bottom of the z-order, because the wallpaper
    // has to be drawn below the desktop icons.
    for (int i = list.count - 1; i >= 0; i--) {
        bool already_added = false;
        for (int j = 0; j < candidate_count; j++)
            if (candidates[j] == list.handles[i]) { already_added = true; break; }
        if (already_added) continue;
        if (candidate_count >= (int)(sizeof(candidates) / sizeof(candidates[0])) - 1) break;
        candidates[candidate_count++] = list.handles[i];
    }
    // Progman is always visible, so it is the last resort.
    bool progman_added = false;
    for (int j = 0; j < candidate_count; j++)
        if (candidates[j] == progman) { progman_added = true; break; }
    if (!progman_added)
        candidates[candidate_count++] = progman;

    if (candidate_count == 0) {
        context->wallpaper_status = WALLPAPER_FAILED;
        snprintf(context->wallpaper_detail, sizeof(context->wallpaper_detail),
                 "couldn't find the WorkerW window to host the wallpaper");
        FAIL_WITH("%s", context->wallpaper_detail);
    }
    LOG_INFO("found %d window(s) that can host the wallpaper", candidate_count);
    for (int i = 0; i < candidate_count; i++) {
        char info[128] = {0};
        DescribeWindow(candidates[i], info, sizeof(info));
        LOG_INFO("  candidate %d: hwnd=%p %s", i, (void *)candidates[i], info);
    }

    SDL_Window *window = SDL_CreateWindow("live-paper",
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (window == NULL) FAIL();
    context->window = window;

    // The window is attached to the desktop with the first strategy; the
    // others are tried later, if the wallpaper doesn't show up.
    if (!PlatformAttachWallpaper(context, 0))
        FAIL_WITH("can't attach the wallpaper window to the desktop");

    // Log where the wallpaper window ended up, to make it possible to see
    // whether it is on the screen at all.
    {
        HWND sdl_hwnd = NULL;
        SDL_SysWMinfo info;
        SDL_VERSION(&info.version);
        if (SDL_GetWindowWMInfo(window, &info))
            sdl_hwnd = info.info.win.window;
        if (sdl_hwnd) {
            RECT wr = {0}, cr = {0}, pr = {0}, pcr = {0};
            GetWindowRect(sdl_hwnd, &wr);
            GetClientRect(sdl_hwnd, &cr);
            HWND parent = GetAncestor(sdl_hwnd, GA_PARENT);
            if (parent) {
                GetWindowRect(parent, &pr);
                GetClientRect(parent, &pcr);
            }
            LOG_INFO("the wallpaper window is at (%ld,%ld)-(%ld,%ld), its client area is %ldx%ld",
                     (long)wr.left, (long)wr.top, (long)wr.right, (long)wr.bottom,
                     (long)(cr.right - cr.left), (long)(cr.bottom - cr.top));
            LOG_INFO("its parent is at (%ld,%ld)-(%ld,%ld), its client area is %ldx%ld",
                     (long)pr.left, (long)pr.top, (long)pr.right, (long)pr.bottom,
                     (long)(pcr.right - pcr.left), (long)(pcr.bottom - pcr.top));
            char own_info[128] = {0};
            DescribeWindow(sdl_hwnd, own_info, sizeof(own_info));
            LOG_INFO("the wallpaper window itself: %s", own_info);
            LOG_INFO("the wallpaper window is at the position %d of the z-order, "
                     "Progman is at the position %d",
                     TopLevelZIndex(sdl_hwnd), TopLevelZIndex(progman));

            // The children of the parent, in z-order, to see what is drawn over
            // the wallpaper.
            LOG_INFO("the children of the parent (z-order, topmost first):");
            struct WindowDump child_dump = {0};
            if (parent) EnumChildWindows(parent, DumpChildProc, (LPARAM)&child_dump);
            LOG_INFO("  (%d child window(s))", child_dump.index);
        }
    }

    context->wallpaper_status = WALLPAPER_ACTIVE;
    snprintf(context->wallpaper_detail, sizeof(context->wallpaper_detail),
             "the wallpaper window is attached to the desktop (%dx%d)",
             GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    LOG_INFO("%s", context->wallpaper_detail);
}

// Attaches the wallpaper window to the Progman window of the virtual desktop
// that is currently shown. It is used when the user switches to another
// virtual desktop.
static void AttachWallpaperToCurrentDesktop(struct Context *context) {
    if (!context->window) return;

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(context->window, &info)) return;
    HWND sdl_hwnd = info.info.win.window;

    HWND progman = FindCurrentDesktopProgman();
    if (!progman) {
        LOG_WARN("can't find the Progman window of the current virtual desktop");
        return;
    }
    enum AttachResult result = AttachToWallpaperHost(sdl_hwnd, progman, false);
    if (result == ATTACH_OK)
        LOG_INFO("the wallpaper is attached to the Progman window %p of the new virtual desktop",
                 (void *)progman);
    else
        LOG_WARN("the wallpaper is not visible after attaching it to %p", (void *)progman);
}

void PlatformUpdate(struct Context *context) {
    tray_loop(0);

    // Follow the user when they switch to another virtual desktop: every
    // virtual desktop of Windows 11 has its own Progman window, and the
    // wallpaper has to be attached to the one of the current desktop.
    static GUID last_desktop;
    static bool have_last = false;
    GUID current;
    if (GetCurrentVirtualDesktopId(&current)) {
        if (!have_last) {
            last_desktop = current;
            have_last = true;
        } else if (memcmp(&current, &last_desktop, sizeof(current)) != 0) {
            LOG_INFO("the virtual desktop has changed, attaching the wallpaper again");
            last_desktop = current;
            AttachWallpaperToCurrentDesktop(context);
        }
    }
}

void PlatformCleanup(struct Context *context) {
    SaveConfig(context);
    if (context->window)
        SDL_DestroyWindow(context->window);
    context->window = NULL;
    tray_exit();
}