#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <SDL.h>

#include "dirlist.h"

struct Context;

// The bar of the dock.
//
// The bar is a small window at the top of the screen. It is hidden above the
// screen and it slides down when the mouse reaches the top of the screen.
//
// The bar is a separate top level window, so that it is not painted below
// the layer of the desktop icons.

// What an item of the bar is.
enum DockItemKind {
    DOCK_ITEM_FILE = 0,   // a file: clicking it launches the file
    DOCK_ITEM_FOLDER,     // a folder: clicking it shows the list of the files
};

// One entry of the bar.
struct DockItem {
    enum DockItemKind kind;
    char *path;           // the full path of the file or of the folder
    char *label;          // the name that is drawn
};

// The state of the bar.
enum DockState {
    DOCK_STATE_HIDDEN = 0,    // above the screen, not visible
    DOCK_STATE_SHOWING,       // sliding down
    DOCK_STATE_SHOWN,         // fully down, the mouse is over it
    DOCK_STATE_HIDING,        // sliding up
};

// What the popup below the bar shows.
enum DockPopupMode {
    DOCK_POPUP_NONE = 0,      // no popup
    DOCK_POPUP_ADD_MENU,      // the two options of the add button
    DOCK_POPUP_FOLDER,        // the files of an added folder
    DOCK_POPUP_PICKER,        // the file picker (to add a file or a folder)
};

// One row of the popup below the bar.
struct DockRow {
    char *label;      // the text that is shown
    char *path;       // the path of the file or of the folder
    bool is_dir;      // it is a folder
    bool is_up;       // the row goes to the parent folder
    bool is_add_dir;  // the row adds the folder that is shown
    bool is_browser;  // the row opens the file browser of the system
    bool is_picker;   // the row opens the file picker
};

// The layout of the bar. The values are shared by the model (the hit
// testing) and by the drawing, so that they cannot get out of sync.

// The size of the add button, in pixels.
#define DOCK_ADD_BUTTON_SIZE 28

// The distance of the add button from the left of the bar, in pixels.
#define DOCK_ADD_BUTTON_X 8

// The width of an item of the bar, in pixels.
#define DOCK_ITEM_WIDTH 120

// The height of a row of the popup below the bar, in pixels.
#define DOCK_POPUP_ROW_HEIGHT 20

// The number of the rows of the popup that are shown at the same time.
#define DOCK_POPUP_MAX_ROWS 12

// The width of the menu of the add button, in pixels. It has to be wide
// enough for the longest text of the menu ("Add file or folder").
#define DOCK_ADD_MENU_WIDTH 240

// The height of a row of the menu of the add button, in pixels.
#define DOCK_ADD_MENU_ROW_HEIGHT 20

// The x coordinate of the item with the given number.
int DockItemX(int index);

// The width of the popup below the bar.
#define DOCK_POPUP_WIDTH 240

// The whole state of the bar.
struct Dock {
    struct Context *context;

    SDL_Window *window;           // the window of the bar
    SDL_Renderer *renderer;       // its renderer
    int width, height;            // the size of the bar
    int screen_width, screen_height;

    enum DockState state;
    uint64_t anim_start_ms;       // the beginning of the slide
    uint64_t last_input_ms;       // the last click (the bar hides itself)
    bool enabled;                 // false: the bar is not created at all

    struct DockItem *items;       // the added items
    int item_count;
    int item_capacity;

    // The menu of the add button is a popup as well, so it does not need a
    // state of its own.

    // The popup below the bar. It is a window of its own, so that it does not
    // cover the rest of the screen with the background of the bar.
    enum DockPopupMode popup_mode;
    SDL_Window *popup_window;     // the window of the popup
    SDL_Renderer *popup_renderer; // its renderer
    char *popup_folder;           // the folder that is shown
    struct DockRow *rows;         // the rows of the popup
    int row_count;
    int row_hover;                // the row under the mouse
    int row_scroll;               // the first row that is shown
    int popup_x, popup_y;         // the position of the popup on the screen
    int popup_width, popup_height; // the size of the popup

    int screen_mouse_x, screen_mouse_y;  // the position of the mouse on the screen
    int mouse_x, mouse_y;         // the position of the mouse in the window
    bool mouse_inside;            // the mouse is over the bar or the popup
    int window_y;                 // the position of the window on the screen

    int trigger;                  // how many pixels at the top show the bar
    int timeout_ms;               // after how many ms the bar hides itself
};

// The options of the bar, that come from the command line.
struct DockOptions {
    bool enabled;             // the bar is created at all
    int height;               // the height of the bar in pixels
    int trigger;              // how many pixels at the top show the bar
    int timeout;              // after how many seconds the bar hides itself
};

// Creates the bar. The bar is enabled by default.
void DockInit(struct Context *context, struct Dock *dock, const struct DockOptions *options);

// The state machine of the bar: it is called once per frame.
void DockUpdate(struct Context *context, struct Dock *dock);

// The drawing of the bar. It is called once per frame, after DockUpdate().
void DockDraw(struct Context *context, struct Dock *dock);
// The mouse events of the bar.
void DockHandleEvent(struct Context *context, struct Dock *dock, const SDL_Event *event);

// Frees the bar.
void DockCleanup(struct Context *context, struct Dock *dock);

// Adds an item to the bar: a file that is launched when it is clicked, or a
// folder whose files are shown. The items are saved in the config file.
void DockAddItem(struct Dock *dock, const char *path, enum DockItemKind kind);

// Shows the content of a folder in the popup below the bar. The rows are: the
// parent folder, the content of the folder and the row that adds the folder
// to the bar.
void DockOpenFolderPopup(struct Dock *dock, const char *path, enum DockPopupMode mode);

// Opens the menu of the add button: the file browser of the system and the
// file picker.
void DockOpenAddMenu(struct Dock *dock);

// Reads the items of the bar from the config file and writes them back.
void DockLoadItems(struct Dock *dock);
void DockSaveItems(const struct Dock *dock);

// Enables or disables the bar. It is used by the item of the tray menu, so
// that the bar can be switched off and on without stopping the program.
void DockSetEnabled(struct Dock *dock, bool enabled);
bool DockIsEnabled(const struct Dock *dock);

// The bar of the program. It is a global, so that the tray menu of the
// platform layer can switch it on and off.
extern struct Dock *g_dock;
