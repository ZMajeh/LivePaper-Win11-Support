#include "dock_ui.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "dock.h"
#include "dock_font.h"
#include "dock_metrics.h"
#include "log.h"

// The colours of the bar.
#define COLOR_BG_R 24
#define COLOR_BG_G 24
#define COLOR_BG_B 28
#define COLOR_BG_A 235

#define COLOR_TEXT_R 220
#define COLOR_TEXT_G 220
#define COLOR_TEXT_B 225

#define COLOR_ACCENT_R 90
#define COLOR_ACCENT_G 170
#define COLOR_ACCENT_B 240

#define COLOR_POPUP_R 32
#define COLOR_POPUP_G 32
#define COLOR_POPUP_B 38
#define COLOR_POPUP_A 245

// The scale of the font: the pixels of the font are doubled, so that the
// text is readable.
#define FONT_SCALE 2

// The number of the rows of the popup that are shown.
static int DockUiVisibleRows(const struct Dock *dock) {
    int rows = dock->row_count;
    if (rows > DOCK_POPUP_MAX_ROWS) rows = DOCK_POPUP_MAX_ROWS;
    return rows;
}

// Draws a filled rectangle.
static void FillRect(SDL_Renderer *renderer, int x, int y, int w, int h,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(renderer, &rect);
}

// Draws the text of an item of the bar. The text that is too long is cut.
static void DrawItemLabel(SDL_Renderer *renderer, const char *text, int x, int y,
                          uint8_t r, uint8_t g, uint8_t b) {
    if (!text) return;
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%s", text);
    // The text is cut with three dots, if it does not fit.
    int max_chars = (DOCK_ITEM_WIDTH - 16) / (FONT_ADVANCE * FONT_SCALE);
    if (max_chars > 0 && (int)strlen(buffer) > max_chars) {
        if (max_chars > 3) {
            memcpy(buffer + max_chars - 3, "...", 3);
            buffer[max_chars] = '\0';
        } else {
            buffer[0] = '\0';
        }
    }
    FontDrawText(renderer, buffer, x, y, FONT_SCALE, r, g, b);
}

// Draws the text of a row of the popup. The text that is too long is cut.
static void DrawRowLabel(SDL_Renderer *renderer, const char *text, int x, int y,
                         int max_width, uint8_t r, uint8_t g, uint8_t b) {
    if (!text) return;
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s", text);
    int max_chars = max_width / (FONT_ADVANCE * FONT_SCALE);
    if (max_chars > 0 && (int)strlen(buffer) > max_chars) {
        if (max_chars > 3) {
            memcpy(buffer + max_chars - 3, "...", 3);
            buffer[max_chars] = '\0';
        } else {
            buffer[0] = '\0';
        }
    }
    FontDrawText(renderer, buffer, x, y, FONT_SCALE, r, g, b);
}

// Draws the icon of a file or of a folder, so that the two can be told
// apart: a folder has a tab on the top, a file is a plain rectangle.
static void DrawIcon(SDL_Renderer *renderer, int x, int y, bool is_dir,
                     uint8_t r, uint8_t g, uint8_t b) {
    if (is_dir) {
        // The tab of the folder.
        FillRect(renderer, x, y, 6, 3, r, g, b, 255);
        // The body of the folder.
        FillRect(renderer, x, y + 2, 12, 9, r, g, b, 255);
    } else {
        // The file is a rectangle with a folded corner.
        FillRect(renderer, x + 2, y, 8, 12, r, g, b, 255);
        FillRect(renderer, x, y + 2, 3, 10, r, g, b, 255);
    }
}

// Draws the system details: the load of the processor, the used memory, the
// speed of the network, the load of the disks and the time.
static void DrawSystemDetails(SDL_Renderer *renderer, const struct Dock *dock) {
    char cpu[32] = "CPU --";
    if (g_metrics_cpu_percent >= 0)
        snprintf(cpu, sizeof(cpu), "CPU %d%%", g_metrics_cpu_percent);

    char ram[48] = "RAM --";
    if (g_metrics_ram_total > 0) {
        double used = (double)g_metrics_ram_used / (1024.0 * 1024.0 * 1024.0);
        double total = (double)g_metrics_ram_total / (1024.0 * 1024.0 * 1024.0);
        snprintf(ram, sizeof(ram), "RAM %.1f/%.0fG", used, total);
    }

    // The speed of the network is shown in kilobytes per second.
    char net[48] = "NET --";
    if (g_metrics_net_up > 0 || g_metrics_net_down > 0) {
        snprintf(net, sizeof(net), "^%lluK v%lluK",
                 (unsigned long long)(g_metrics_net_up / 1024),
                 (unsigned long long)(g_metrics_net_down / 1024));
    }

    char disk[32] = "DISK --";
    if (g_metrics_disk_percent >= 0)
        snprintf(disk, sizeof(disk), "DISK %d%%", g_metrics_disk_percent);

    char clock[16];
    snprintf(clock, sizeof(clock), "%s", g_metrics_clock);

    // The groups are shown from the right of the bar.
    const char *groups[5] = { cpu, ram, net, disk, clock };
    int x = dock->width - 12;
    int y = (dock->height - FONT_HEIGHT * FONT_SCALE) / 2;

    for (int i = 4; i >= 0; i--) {
        int width = FontTextWidth(groups[i], FONT_SCALE);
        if (x - width < 0) break;
        FontDrawText(renderer, groups[i], x - width, y, FONT_SCALE,
                     COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
        x -= width + 12;
    }
}

// Draws the add button: a square with a cross in it.
static void DrawAddButton(SDL_Renderer *renderer, const struct Dock *dock) {
    int y = (dock->height - DOCK_ADD_BUTTON_SIZE) / 2;
    
    // Button 1: "Add file"
    int x1 = DOCK_ADD_BUTTON1_X;
    bool hovered1 = dock->mouse_x >= x1 && dock->mouse_x < x1 + DOCK_ADD_BUTTON_SIZE
                    && dock->mouse_y >= y && dock->mouse_y < y + DOCK_ADD_BUTTON_SIZE;
    uint8_t r1 = hovered1 ? COLOR_ACCENT_R : COLOR_TEXT_R;
    uint8_t g1 = hovered1 ? COLOR_ACCENT_G : COLOR_TEXT_G;
    uint8_t b1 = hovered1 ? COLOR_ACCENT_B : COLOR_TEXT_B;
    
    // The frame of the button.
    FillRect(renderer, x1, y, DOCK_ADD_BUTTON_SIZE, 1, r1, g1, b1, 255);
    FillRect(renderer, x1, y + DOCK_ADD_BUTTON_SIZE - 1, DOCK_ADD_BUTTON_SIZE, 1, r1, g1, b1, 255);
    FillRect(renderer, x1, y, 1, DOCK_ADD_BUTTON_SIZE, r1, g1, b1, 255);
    FillRect(renderer, x1 + DOCK_ADD_BUTTON_SIZE - 1, y, 1, DOCK_ADD_BUTTON_SIZE, r1, g1, b1, 255);
    // The "F" for file.
    DrawItemLabel(renderer, "F", x1 + 8, y + 4, r1, g1, b1);
    
    // Button 2: "Add folder"
    int x2 = DOCK_ADD_BUTTON2_X;
    bool hovered2 = dock->mouse_x >= x2 && dock->mouse_x < x2 + DOCK_ADD_BUTTON_SIZE
                    && dock->mouse_y >= y && dock->mouse_y < y + DOCK_ADD_BUTTON_SIZE;
    uint8_t r2 = hovered2 ? COLOR_ACCENT_R : COLOR_TEXT_R;
    uint8_t g2 = hovered2 ? COLOR_ACCENT_G : COLOR_TEXT_G;
    uint8_t b2 = hovered2 ? COLOR_ACCENT_B : COLOR_TEXT_B;
    
    // The frame of the button.
    FillRect(renderer, x2, y, DOCK_ADD_BUTTON_SIZE, 1, r2, g2, b2, 255);
    FillRect(renderer, x2, y + DOCK_ADD_BUTTON_SIZE - 1, DOCK_ADD_BUTTON_SIZE, 1, r2, g2, b2, 255);
    FillRect(renderer, x2, y, 1, DOCK_ADD_BUTTON_SIZE, r2, g2, b2, 255);
    FillRect(renderer, x2 + DOCK_ADD_BUTTON_SIZE - 1, y, 1, DOCK_ADD_BUTTON_SIZE, r2, g2, b2, 255);
    // The folder icon.
    DrawIcon(renderer, x2 + 8, y + 4, true, r2, g2, b2);
}

// Draws the popup below the bar: the menu of the add button or the list of
// the files of a folder. The popup has a window of its own.
void DockUiDrawPopup(struct Dock *dock) {
    if (dock->popup_mode == DOCK_POPUP_NONE || !dock->popup_renderer) return;
    SDL_Renderer *renderer = dock->popup_renderer;
    int row_height = dock->popup_mode == DOCK_POPUP_ADD_MENU ? DOCK_ADD_MENU_ROW_HEIGHT
                                                            : DOCK_POPUP_ROW_HEIGHT;

    // For folder popups, we have a search field at the top
    bool has_search = dock->popup_mode == DOCK_POPUP_FOLDER || dock->popup_mode == DOCK_POPUP_PICKER;
    int search_height = has_search ? DOCK_POPUP_ROW_HEIGHT : 0;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    FillRect(renderer, 0, 0, dock->popup_width, dock->popup_height,
             COLOR_POPUP_R, COLOR_POPUP_G, COLOR_POPUP_B, 255);

    // Draw the search field at the top
    if (has_search) {
        // Search field background
        FillRect(renderer, 2, 2, dock->popup_width - 4, search_height - 4,
                 COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B, 60);
        // Search field border (highlight if active)
        uint8_t br = dock->popup_search_active ? COLOR_ACCENT_R : COLOR_TEXT_R;
        uint8_t bg = dock->popup_search_active ? COLOR_ACCENT_G : COLOR_TEXT_G;
        uint8_t bb = dock->popup_search_active ? COLOR_ACCENT_B : COLOR_TEXT_B;
        FillRect(renderer, 2, 2, dock->popup_width - 4, 1, br, bg, bb, 255);
        FillRect(renderer, 2, search_height - 3, dock->popup_width - 4, 1, br, bg, bb, 255);
        FillRect(renderer, 2, 2, 1, search_height - 4, br, bg, bb, 255);
        FillRect(renderer, dock->popup_width - 3, 2, 1, search_height - 4, br, bg, bb, 255);
        // Search text
        int text_y = 2 + (search_height - FONT_HEIGHT * FONT_SCALE) / 2;
        char search_label[256];
        snprintf(search_label, sizeof(search_label), "Search: %s", dock->popup_search ? dock->popup_search : "");
        DrawRowLabel(renderer, search_label, 10, text_y,
                     dock->popup_width - 16, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
    }

    // Build a list of visible row indices (filtered by search)
    int visible_indices[DOCK_POPUP_MAX_ROWS];
    int visible_count = 0;
    bool is_menu = dock->popup_mode == DOCK_POPUP_ADD_MENU;
    for (int i = 0; i < dock->row_count && visible_count < DOCK_POPUP_MAX_ROWS; i++) {
        struct DockRow *row = &dock->rows[i];
        if (RowMatchesSearch(dock, row)) {
            visible_indices[visible_count++] = i;
        }
    }

    for (int i = 0; i < visible_count; i++) {
        int index = visible_indices[i];
        struct DockRow *row = &dock->rows[index];
        int row_y = search_height + i * row_height;

        if (index == dock->row_hover) {
            FillRect(renderer, 0, row_y, dock->popup_width, row_height,
                     COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 150);
        }

        int text_y = row_y + (row_height - FONT_HEIGHT * FONT_SCALE) / 2;
        // The rows of the menu of the add button have no icon.
        int text_x = 8;
        if (!is_menu) {
            if (!row->is_up && !row->is_add_dir) {
                DrawIcon(renderer, 6, row_y + 4, row->is_dir,
                         COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
            }
            text_x = 22;
        }
        DrawRowLabel(renderer, row->label, text_x, text_y,
                     dock->popup_width - text_x - 6, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
    }

    SDL_RenderPresent(renderer);
}

// The width of the system details at the right of the bar. The items of the
// bar have to stay clear of them.
#define DETAILS_WIDTH 420

// Draws the items of the bar.
static void DrawItems(SDL_Renderer *renderer, const struct Dock *dock) {
    int text_y = (dock->height - FONT_HEIGHT * FONT_SCALE) / 2;
    for (int i = 0; i < dock->item_count; i++) {
        int x = DockItemX(i);
        // The items stop before the system details.
        if (x + DOCK_ITEM_WIDTH > dock->width - DETAILS_WIDTH) break;
        struct DockItem *item = &dock->items[i];
        DrawIcon(renderer, x, (dock->height - 12) / 2, item->kind == DOCK_ITEM_FOLDER,
                 COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
        DrawItemLabel(renderer, item->label, x + 16, text_y,
                      COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
    }
}

void DockUiDraw(struct Dock *dock) {
    if (!dock->renderer) return;
    SDL_Renderer *renderer = dock->renderer;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    // The background of the bar.
    FillRect(renderer, 0, 0, dock->width, dock->height,
             COLOR_BG_R, COLOR_BG_G, COLOR_BG_B, COLOR_BG_A);
    // A thin line at the bottom of the bar.
    FillRect(renderer, 0, dock->height - 1, dock->width, 1,
             COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 160);

    DrawAddButton(renderer, dock);
    DrawItems(renderer, dock);
    DrawSystemDetails(renderer, dock);

    SDL_RenderPresent(renderer);

    // The popup below the bar has a window of its own.
    DockUiDrawPopup(dock);
}
