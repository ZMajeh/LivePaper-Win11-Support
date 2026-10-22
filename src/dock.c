#include "dock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dock_metrics.h"
#include "dock_ui.h"
#include "dirlist.h"
#include "launcher.h"
#include "log.h"
#include "platform.h"

// The bar of the program. It is a global, so that the tray menu of the
// platform layer can switch it on and off.
struct Dock *g_dock = NULL;

// Hides the window of the popup and frees it. It is defined below.
static void DockHidePopupWindow(struct Dock *dock);

// Shows the popup below the bar. It is defined below.
static void DockShowPopup(struct Dock *dock);

// The row of the popup that is under the mouse. It is defined below.
static int DockRowAt(const struct Dock *dock, int screen_x, int screen_y);

// The time that the slide of the bar takes, in milliseconds.
#define SLIDE_MS 200

// The time that the popup of a folder stays open without a click.
#define POPUP_IDLE_MS 30000

// The width of the bar. It covers the whole screen, so that the system
// details can be shown at the right of it.
static int BarWidth(struct Dock *dock) {
    return dock->screen_width;
}

// Adds an item to the bar. The label is the name of the file or of the
// folder, without the path.
static void DockAddItemInternal(struct Dock *dock, const char *path, enum DockItemKind kind) {
    if (dock->item_count >= dock->item_capacity) {
        int capacity = dock->item_capacity ? dock->item_capacity * 2 : 8;
        struct DockItem *items = realloc(dock->items, sizeof(struct DockItem) * capacity);
        if (!items) return;
        dock->items = items;
        dock->item_capacity = capacity;
    }
    struct DockItem *item = &dock->items[dock->item_count];
    item->kind = kind;
    item->path = strdup(path);
    item->label = strdup(DirBaseName(path));
    if (!item->path || !item->label) {
        free(item->path);
        free(item->label);
        return;
    }
    dock->item_count++;
    LOG_INFO("the item '%s' was added to the bar", item->label);
}

// Closes the popup and frees the rows of it.
static void DockClosePopup(struct Dock *dock) {
    for (int i = 0; i < dock->row_count; i++) {
        free(dock->rows[i].label);
        free(dock->rows[i].path);
    }
    free(dock->rows);
    dock->rows = NULL;
    dock->row_count = 0;
    dock->popup_mode = DOCK_POPUP_NONE;
    dock->row_hover = -1;
    dock->row_scroll = 0;
    dock->popup_height = 0;
    if (dock->popup_folder) {
        free(dock->popup_folder);
        dock->popup_folder = NULL;
    }
    // The window of the popup is not needed any more.
    DockHidePopupWindow(dock);
}

// Adds a row to the popup.
static void DockAddRow(struct Dock *dock, const char *label, const char *path,
                       bool is_dir, bool is_up, bool is_add_dir) {
    struct DockRow *rows = realloc(dock->rows, sizeof(struct DockRow) * (dock->row_count + 1));
    if (!rows) return;
    dock->rows = rows;
    struct DockRow *row = &dock->rows[dock->row_count];
    memset(row, 0, sizeof(*row));
    row->label = label ? strdup(label) : NULL;
    row->path = path ? strdup(path) : NULL;
    row->is_dir = is_dir;
    row->is_up = is_up;
    row->is_add_dir = is_add_dir;
    dock->row_count++;
}

// Shows the content of the given folder in the popup. The first row goes to
// the parent folder, the last one adds the folder itself.
void DockOpenFolderPopup(struct Dock *dock, const char *path, enum DockPopupMode mode) {
    DockClosePopup(dock);

    struct DirEntry *entries = NULL;
    int count = DirListRead(path, &entries);
    if (count < 0) {
        LOG_WARN("the files of the folder '%s' could not be shown", path);
        return;
    }

    // The row that goes to the parent folder.
    char *parent = DirParentPath(path);
    if (parent && strcmp(parent, path) != 0) {
        DockAddRow(dock, "..", parent, true, true, false);
    }
    free(parent);

    for (int i = 0; i < count; i++) {
        char *full = DirJoinPath(path, entries[i].name);
        DockAddRow(dock, entries[i].name, full, entries[i].is_dir, false, false);
        free(full);
    }
    DirListFree(entries, count);

    // The row that adds the folder itself.
    DockAddRow(dock, "Add this folder", path, true, false, true);

    dock->popup_folder = strdup(path);
    dock->popup_mode = mode;
    dock->popup_x = DOCK_ADD_BUTTON_X;
    dock->popup_width = DOCK_POPUP_WIDTH;
    dock->row_hover = -1;
    dock->row_scroll = 0;
    DockShowPopup(dock);
    LOG_INFO("the popup of the folder '%s' was opened", path);
}

// Shows the menu of the add button. The two options are rows of the popup, so
// that the popup needs only one kind of row.
void DockOpenAddMenu(struct Dock *dock) {
    DockClosePopup(dock);
    DockAddRow(dock, "File browser", NULL, false, false, false);
    DockAddRow(dock, "Add file or folder", NULL, false, false, false);
    dock->popup_mode = DOCK_POPUP_ADD_MENU;
    dock->popup_x = DOCK_ADD_BUTTON_X;
    dock->popup_width = DOCK_ADD_MENU_WIDTH;
    dock->row_hover = -1;
    dock->row_scroll = 0;
    DockShowPopup(dock);
    LOG_INFO("the menu of the add button was opened");
}

// Adds an item to the bar.
void DockAddItem(struct Dock *dock, const char *path, enum DockItemKind kind) {
    DockAddItemInternal(dock, path, kind);
    DockSaveItems(dock);
}

// The path of the file with the items of the bar.
static bool DockConfigPath(char *buf, size_t buf_size) {
    return PlatformGetDockConfigPath(buf, buf_size);
}

void DockLoadItems(struct Dock *dock) {
    char path[512] = {0};
    if (!DockConfigPath(path, sizeof(path))) return;
    FILE *file = fopen(path, "rt");
    if (!file) {
        LOG_INFO("there is no file with the items of the bar yet ('%s')", path);
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        // The line break is removed.
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
            line[--length] = '\0';
        }
        if (!length) continue;
        // The folders end with a slash, the files do not.
        bool is_dir = line[length - 1] == '/' || line[length - 1] == '\\';
        if (is_dir) line[length - 1] = '\0';
        DockAddItemInternal(dock, line, is_dir ? DOCK_ITEM_FOLDER : DOCK_ITEM_FILE);
    }
    fclose(file);
    LOG_INFO("%d item(s) of the bar were read from '%s'", dock->item_count, path);
}

void DockSaveItems(const struct Dock *dock) {
    char path[512] = {0};
    if (!DockConfigPath(path, sizeof(path))) return;
    FILE *file = fopen(path, "wt");
    if (!file) {
        LOG_WARN("the file with the items of the bar could not be written ('%s')", path);
        return;
    }
    for (int i = 0; i < dock->item_count; i++) {
        const struct DockItem *item = &dock->items[i];
        if (item->kind == DOCK_ITEM_FOLDER) {
            fprintf(file, "%s/\n", item->path);
        } else {
            fprintf(file, "%s\n", item->path);
        }
    }
    fclose(file);
    LOG_INFO("the items of the bar were written to '%s'", path);
}

// ---------------------------------------------------------------------------
// The file picker
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// The life cycle of the bar
// ---------------------------------------------------------------------------

void DockInit(struct Context *context, struct Dock *dock, const struct DockOptions *options) {
    memset(dock, 0, sizeof(*dock));
    dock->context = context;
    dock->enabled = options ? options->enabled : true;
    dock->height = options && options->height > 0 ? options->height : 36;
    dock->trigger = options ? options->trigger : 2;
    dock->timeout_ms = (options ? options->timeout : 3) * 1000;
    dock->state = DOCK_STATE_HIDDEN;
    dock->row_hover = -1;
    g_dock = dock;

    PlatformGetScreenSize(&dock->screen_width, &dock->screen_height);
    dock->width = BarWidth(dock);

    if (!dock->enabled) {
        LOG_INFO("the dock bar is switched off");
        return;
    }

    // The window of the bar. It is a separate window, so that it is above
    // the wallpaper and it gets the mouse clicks.
    dock->window = PlatformCreateDockWindow(dock->width, dock->height);
    if (!dock->window) {
        LOG_WARN("the window of the dock bar could not be created");
        dock->enabled = false;
        return;
    }
    dock->renderer = SDL_CreateRenderer(dock->window, -1, SDL_RENDERER_SOFTWARE);
    if (!dock->renderer) {
        LOG_WARN("the renderer of the dock bar could not be created: %s", SDL_GetError());
        PlatformDestroyDockWindow(dock->window);
        dock->window = NULL;
        dock->enabled = false;
        return;
    }
    // The bar starts above the screen, so that it is not visible and it cannot
    // get the clicks of the desktop.
    PlatformSetDockPosition(dock->window, 0, -dock->height);

    DockLoadItems(dock);
    LOG_INFO("the dock bar was created (%dx%d, the screen is %dx%d)",
             dock->width, dock->height, dock->screen_width, dock->screen_height);
}

void DockCleanup(struct Context *context, struct Dock *dock) {
    DockClosePopup(dock);
    for (int i = 0; i < dock->item_count; i++) {
        free(dock->items[i].path);
        free(dock->items[i].label);
    }
    free(dock->items);
    dock->items = NULL;
    dock->item_count = 0;
    if (dock->renderer) {
        SDL_DestroyRenderer(dock->renderer);
        dock->renderer = NULL;
    }
    if (dock->window) {
        PlatformDestroyDockWindow(dock->window);
        dock->window = NULL;
    }
    if (g_dock == dock) g_dock = NULL;
}

void DockSetEnabled(struct Dock *dock, bool enabled) {
    if (dock->enabled == enabled) return;
    dock->enabled = enabled;
    if (enabled) {
        // The window is created again, so that the bar can be switched on
        // without stopping the program.
        PlatformGetScreenSize(&dock->screen_width, &dock->screen_height);
        dock->width = dock->screen_width;
        dock->window = PlatformCreateDockWindow(dock->width, dock->height);
        if (!dock->window) {
            LOG_WARN("the window of the dock bar could not be created");
            dock->enabled = false;
            return;
        }
        dock->renderer = SDL_CreateRenderer(dock->window, -1, SDL_RENDERER_SOFTWARE);
        if (!dock->renderer) {
            LOG_WARN("the renderer of the dock bar could not be created: %s", SDL_GetError());
            PlatformDestroyDockWindow(dock->window);
            dock->window = NULL;
            dock->enabled = false;
            return;
        }
        dock->state = DOCK_STATE_HIDDEN;
            PlatformSetDockPosition(dock->window, 0, -dock->height);
        LOG_INFO("the dock bar was switched on");
    } else {
        DockClosePopup(dock);
        if (dock->renderer) {
            SDL_DestroyRenderer(dock->renderer);
            dock->renderer = NULL;
        }
        if (dock->window) {
            PlatformDestroyDockWindow(dock->window);
            dock->window = NULL;
        }
        dock->state = DOCK_STATE_HIDDEN;
        LOG_INFO("the dock bar was switched off");
    }
}

bool DockIsEnabled(const struct Dock *dock) {
    return dock->enabled;
}

// ---------------------------------------------------------------------------
// The state machine
// ---------------------------------------------------------------------------
// The number of the rows of the popup that are shown.
static int DockVisibleRows(const struct Dock *dock) {
    int rows = dock->row_count;
    if (rows > DOCK_POPUP_MAX_ROWS) rows = DOCK_POPUP_MAX_ROWS;
    return rows;
}

// The height of a row of the popup, in pixels.
static int DockRowHeight(const struct Dock *dock) {
    return dock->popup_mode == DOCK_POPUP_ADD_MENU ? DOCK_ADD_MENU_ROW_HEIGHT
                                                    : DOCK_POPUP_ROW_HEIGHT;
}

// The size of the popup: it is as tall as the rows that are shown and as
// wide as the text of the longest one.
static void DockPopupSize(const struct Dock *dock, int *width, int *height) {
    int row_height = DockRowHeight(dock);
    *height = DockVisibleRows(dock) * row_height;
    // The width of the popup is fixed: the names of the files are cut.
    *width = dock->popup_width;
}

// Creates the window of the popup. It is a window of its own, so that it
// covers only the part of the screen where it is.
static bool DockCreatePopupWindow(struct Dock *dock) {
    if (dock->popup_window) return true;
    int width = dock->popup_width;
    int height = DockVisibleRows(dock) * DockRowHeight(dock);
    dock->popup_window = PlatformCreateDockPopupWindow(width, height);
    if (!dock->popup_window) {
        LOG_WARN("the window of the popup could not be created");
        return false;
    }
    dock->popup_renderer = SDL_CreateRenderer(dock->popup_window, -1, SDL_RENDERER_SOFTWARE);
    if (!dock->popup_renderer) {
        LOG_WARN("the renderer of the popup could not be created: %s", SDL_GetError());
        PlatformDestroyDockWindow(dock->popup_window);
        dock->popup_window = NULL;
        return false;
    }
    // The popup is hidden until it is placed under the bar.
    PlatformSetDockVisible(dock->popup_window, false);
    return true;
}

// Places the popup below the bar and shows it. The bar is at the top of the
// screen, so the popup is right below it.
static void DockShowPopup(struct Dock *dock) {
    if (dock->popup_mode == DOCK_POPUP_NONE) return;
    if (!DockCreatePopupWindow(dock)) return;

    int width = 0, height = 0;
    DockPopupSize(dock, &width, &height);
    if (width != dock->popup_width || height != dock->popup_height) {
        dock->popup_width = width;
        dock->popup_height = height;
        SDL_SetWindowSize(dock->popup_window, width, height);
    }
    // The popup hangs below the bar.
    dock->popup_x = DOCK_ADD_BUTTON_X;
    dock->popup_y = dock->window_y + dock->height;
    PlatformSetDockPosition(dock->popup_window, dock->popup_x, dock->popup_y);
    PlatformSetDockVisible(dock->popup_window, true);
}

// Hides the popup and frees its window.
static void DockHidePopupWindow(struct Dock *dock) {
    if (!dock->popup_window) return;
    PlatformSetDockVisible(dock->popup_window, false);
    if (dock->popup_renderer) {
        SDL_DestroyRenderer(dock->popup_renderer);
        dock->popup_renderer = NULL;
    }
    PlatformDestroyDockWindow(dock->popup_window);
    dock->popup_window = NULL;
}

// Whether the point is inside the bar or inside the popup.
static bool DockPointInside(const struct Dock *dock, int x, int y) {
    if (y < 0 || y >= dock->height) {
        // The popup is below the bar.
        if (dock->popup_mode == DOCK_POPUP_NONE) return false;
        if (y < dock->popup_y || y >= dock->popup_y + dock->popup_height) return false;
        if (x < dock->popup_x || x >= dock->popup_x + dock->popup_width) return false;
    }
    if (x < 0 || x >= dock->width) return false;
    return true;
}

void DockUpdate(struct Context *context, struct Dock *dock) {
    if (!dock->enabled || !dock->window) return;

    uint64_t now = SDL_GetTicks64();

    // The position of the mouse. The bar has no focus, so it does not get the
    // mouse events of the desktop.
    int screen_x = 0, screen_y = 0;
    PlatformGetCursorPos(&screen_x, &screen_y);
    dock->screen_mouse_x = screen_x;
    dock->screen_mouse_y = screen_y;
    // The window is at the left of the screen, so the coordinates of the
    // mouse in the window are the ones on the screen, shifted by the
    // position of the window.
    dock->mouse_x = screen_x;
    dock->mouse_y = screen_y - dock->window_y;

    // The bar shows itself when the mouse reaches the top of the screen.
    bool at_top = screen_y >= 0 && screen_y < dock->trigger;

    switch (dock->state) {
        case DOCK_STATE_HIDDEN:
            if (at_top) {
                dock->state = DOCK_STATE_SHOWING;
                dock->anim_start_ms = now;
                dock->window_y = -dock->height;
                LOG_INFO("the dock bar is coming out (the mouse is at the top of the screen)");
            }
            break;

        case DOCK_STATE_SHOWING: {
            uint64_t elapsed = now - dock->anim_start_ms;
            int progress = (int)((elapsed * 100) / SLIDE_MS);
            if (progress > 100) progress = 100;
            // The bar comes down from above the screen. The window is moved
            // as a whole, so the popup below the bar comes with it.
            int y = -dock->height + (dock->height * progress) / 100;
            dock->window_y = y;
            PlatformSetDockPosition(dock->window, 0, y);
            if (progress >= 100) {
                dock->state = DOCK_STATE_SHOWN;
                dock->last_input_ms = now;
                LOG_DEBUG("the dock bar is visible");
            }
            break;
        }

        case DOCK_STATE_SHOWN: {
            // The bar hides itself when the mouse goes away and nothing
            // happens for a while.
            bool over_bar = dock->mouse_y >= 0 && dock->mouse_y < dock->height
                            && dock->mouse_x >= 0 && dock->mouse_x < dock->width;
            bool over_popup = dock->popup_mode != DOCK_POPUP_NONE
                              && dock->screen_mouse_y >= dock->popup_y
                              && dock->screen_mouse_y < dock->popup_y + dock->popup_height;
            if (over_bar || over_popup) {
                dock->last_input_ms = now;
            }
            // The popup of a folder closes itself after a while.
            if (dock->popup_mode != DOCK_POPUP_NONE &&
                now - dock->last_input_ms > POPUP_IDLE_MS) {
                LOG_DEBUG("the popup of the bar was closed after the timeout");
                DockClosePopup(dock);
            }
            if (now - dock->last_input_ms > (uint64_t)dock->timeout_ms) {
                dock->state = DOCK_STATE_HIDING;
                dock->anim_start_ms = now;
                LOG_DEBUG("the dock bar is going away");
            }
            break;
        }

        case DOCK_STATE_HIDING: {
            uint64_t elapsed = now - dock->anim_start_ms;
            int progress = (int)((elapsed * 100) / SLIDE_MS);
            if (progress > 100) progress = 100;
            int y = -((dock->height * progress) / 100);
            dock->window_y = y;
            PlatformSetDockPosition(dock->window, 0, y);
            if (progress >= 100) {
                dock->state = DOCK_STATE_HIDDEN;
                PlatformSetDockPosition(dock->window, 0, -dock->height);
                LOG_DEBUG("the dock bar is hidden");
            }
            break;
        }
    }

    // The popup is closed when the bar goes away.
    if (dock->popup_mode != DOCK_POPUP_NONE &&
        (dock->state == DOCK_STATE_HIDING || dock->state == DOCK_STATE_HIDDEN)) {
        DockClosePopup(dock);
    }

    // The mouse is over the bar or over the popup.
    dock->mouse_inside = DockPointInside(dock, dock->mouse_x, dock->mouse_y);
    // The row of the popup under the mouse. It is asked for here and not only
    // in the events, because the popup has a window of its own and the mouse
    // events of the desktop do not reach the program.
    dock->row_hover = DockRowAt(dock, screen_x, screen_y);

    // The popup hangs below the bar, so it follows the bar when it moves.
    if (dock->popup_mode != DOCK_POPUP_NONE && dock->popup_window) {
        dock->popup_y = dock->window_y + dock->height;
        PlatformSetDockPosition(dock->popup_window, dock->popup_x, dock->popup_y);
        // Showing a window can change its place in the z-order, so the popup
        // is put above the desktop again.
        PlatformPlaceDockPopupWindow(dock->popup_window);
    }
}

// The drawing of the bar. It is only done when the bar is visible, so that
// the program stays quiet.
void DockDraw(struct Context *context, struct Dock *dock) {
    if (!dock->enabled || !dock->window || !dock->renderer) return;
    if (dock->state == DOCK_STATE_HIDDEN) return;
    DockUiDraw(dock);
}

// ---------------------------------------------------------------------------
// The mouse events
// ---------------------------------------------------------------------------

// The x coordinate of the item with the given number. The items of the bar
// start after the add button.
int DockItemX(int index) {
    return DOCK_ADD_BUTTON_X + DOCK_ADD_BUTTON_SIZE + 8 + index * DOCK_ITEM_WIDTH;
}

// The row of the popup that is under the mouse, or -1. The position is the
// one of the mouse on the screen.
static int DockRowAt(const struct Dock *dock, int screen_x, int screen_y) {
    if (dock->popup_mode == DOCK_POPUP_NONE) return -1;
    if (screen_y < dock->popup_y || screen_y >= dock->popup_y + dock->popup_height) return -1;
    if (screen_x < dock->popup_x || screen_x >= dock->popup_x + dock->popup_width) return -1;
    int index = dock->row_scroll + (screen_y - dock->popup_y) / DockRowHeight(dock);
    if (index < 0 || index >= dock->row_count) return -1;
    return index;
}

// The item of the bar that is under the mouse, or -1.
static int DockItemAt(const struct Dock *dock, int x, int y) {
    if (y < 0 || y >= dock->height) return -1;
    int items_x = DOCK_ADD_BUTTON_X + DOCK_ADD_BUTTON_SIZE + 8;
    if (x < items_x) return -1;
    int index = (x - items_x) / DOCK_ITEM_WIDTH;
    if (index < 0 || index >= dock->item_count) return -1;
    return index;
}

// Whether the add button is under the mouse.
static bool DockAddButtonAt(const struct Dock *dock, int x, int y) {
    if (y < 0 || y >= dock->height) return false;
    return x >= DOCK_ADD_BUTTON_X && x < DOCK_ADD_BUTTON_X + DOCK_ADD_BUTTON_SIZE;
}

// The row of the popup that was clicked.
static void DockClickRow(struct Dock *dock, int index) {
    if (index < 0 || index >= dock->row_count) return;
    struct DockRow *row = &dock->rows[index];

    if (row->is_up) {
        // The parent folder is shown.
        DockOpenFolderPopup(dock, row->path, dock->popup_mode);
        return;
    }
    if (row->is_add_dir) {
        // The folder that is shown is added to the bar.
        DockAddItem(dock, row->path, DOCK_ITEM_FOLDER);
        DockClosePopup(dock);
        return;
    }
    if (row->is_dir) {
        // A folder of the list: it is shown.
        DockOpenFolderPopup(dock, row->path, dock->popup_mode);
        return;
    }
    // A file. In the picker it is added to the bar, in the list of a folder it
    // is launched.
    if (dock->popup_mode == DOCK_POPUP_PICKER) {
        DockAddItem(dock, row->path, DOCK_ITEM_FILE);
        DockClosePopup(dock);
    } else {
        DockClosePopup(dock);
        LauncherOpenFile(row->path);
    }
}

void DockHandleEvent(struct Context *context, struct Dock *dock, const SDL_Event *event) {
    if (!dock->enabled || !dock->window) return;

    if (event->type == SDL_WINDOWEVENT) {
        if (event->window.windowID != SDL_GetWindowID(dock->window)) return;
        if (event->window.event == SDL_WINDOWEVENT_CLOSE) {
            // The bar was closed by the user: it is only hidden.
            dock->state = DOCK_STATE_HIDDEN;
            PlatformSetDockPosition(dock->window, 0, -dock->height);
        }
        return;
    }

    // The mouse events of SDL carry the window that has the mouse focus, and
    // not the window that was clicked. The bar of the dock never has the
    // focus (it is a WS_EX_NOACTIVATE window), so the events cannot be
    // filtered by the window: the position of the mouse decides instead.
    if (event->type != SDL_MOUSEMOTION && event->type != SDL_MOUSEBUTTONDOWN &&
        event->type != SDL_MOUSEWHEEL) {
        return;
    }
    // The position of the mouse on the screen, which is asked for from the
    // system: the bar has no focus, so it does not get the mouse events of
    // the desktop.
    int screen_x = 0, screen_y = 0;
    PlatformGetCursorPos(&screen_x, &screen_y);
    // The window of the bar is at the left of the screen, so the coordinates
    // of the mouse in the window are the ones on the screen, shifted by the
    // position of the window.
    int x = screen_x;
    int y = screen_y - dock->window_y;

    switch (event->type) {
        case SDL_MOUSEMOTION:
            dock->mouse_x = x;
            dock->mouse_y = y;
            // The row of the popup under the mouse.
            dock->row_hover = DockRowAt(dock, screen_x, screen_y);
            break;

        case SDL_MOUSEBUTTONDOWN: {
            if (event->button.button != SDL_BUTTON_LEFT) break;
            dock->last_input_ms = SDL_GetTicks64();
            LOG_DEBUG("a click at (%d,%d) in the bar", x, y);

            // A click outside of the bar and of the popup closes the popup.
            if (!DockPointInside(dock, x, y) && dock->popup_mode != DOCK_POPUP_NONE) {
                DockClosePopup(dock);
                break;
            }

            // The rows of the popup. The menu of the add button is a popup
            // as well, so it is handled here too.
            int row = DockRowAt(dock, screen_x, screen_y);
            if (row >= 0) {
                if (dock->popup_mode == DOCK_POPUP_ADD_MENU) {
                    if (row == 0) {
                        // The file browser of the system.
                        LOG_INFO("the file browser of the system was opened from the bar");
                        DockClosePopup(dock);
                        LauncherOpenFileManager(NULL);
                    } else {
                        // The file or the folder is chosen.
                        LOG_INFO("the file picker of the bar was opened");
                        char *home = LauncherHomeFolder();
                        DockOpenFolderPopup(dock, home ? home : "/", DOCK_POPUP_PICKER);
                        free(home);
                    }
                    break;
                }
                DockClickRow(dock, row);
                break;
            }

            // The add button.
            if (DockAddButtonAt(dock, x, y)) {
                LOG_INFO("the add button of the bar was clicked");
#ifdef _WIN32
                // The native file dialog of Windows is opened directly. It
                // runs on its own thread and the chosen path is sent back
                // with an SDL event, which the main loop handles by adding
                // the chosen file or folder to the bar.
                LauncherPickFile();
#else
                // On other systems the custom popup with the menu of the add
                // button is used.
                if (dock->popup_mode == DOCK_POPUP_ADD_MENU) {
                    DockClosePopup(dock);
                } else {
                    DockOpenAddMenu(dock);
                }
#endif
                break;
            }

            // The items of the bar.
            int item = DockItemAt(dock, x, y);
            if (item >= 0) {
                struct DockItem *entry = &dock->items[item];
                if (entry->kind == DOCK_ITEM_FOLDER) {
                    // The files of the folder are shown in the popup.
                    DockOpenFolderPopup(dock, entry->path, DOCK_POPUP_FOLDER);
                } else {
                    // The file is opened with its program.
                    DockClosePopup(dock);
                    LauncherOpenFile(entry->path);
                }
                break;
            }

            // A click in the empty part of the bar closes the popup.
            if (dock->popup_mode != DOCK_POPUP_NONE) {
                DockClosePopup(dock);
            }
            break;
        }

        case SDL_MOUSEWHEEL: {
            dock->last_input_ms = SDL_GetTicks64();
            if (dock->popup_mode == DOCK_POPUP_NONE) break;
            int rows = DockVisibleRows(dock);
            int max_scroll = dock->row_count - rows;
            if (max_scroll < 0) max_scroll = 0;
            int scroll = dock->row_scroll - event->wheel.y;
            if (scroll < 0) scroll = 0;
            if (scroll > max_scroll) scroll = max_scroll;
            dock->row_scroll = scroll;
            break;
        }

        default:
            break;
    }
}
