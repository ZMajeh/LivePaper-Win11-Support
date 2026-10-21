#include "dock_ui.h"

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
    int x = DOCK_ADD_BUTTON_X;
    int y = (dock->height - DOCK_ADD_BUTTON_SIZE) / 2;
    bool hovered = dock->mouse_x >= x && dock->mouse_x < x + DOCK_ADD_BUTTON_SIZE
                   && dock->mouse_y >= y && dock->mouse_y < y + DOCK_ADD_BUTTON_SIZE;
    uint8_t r = hovered ? COLOR_ACCENT_R : COLOR_TEXT_R;
    uint8_t g = hovered ? COLOR_ACCENT_G : COLOR_TEXT_G;
    uint8_t b = hovered ? COLOR_ACCENT_B : COLOR_TEXT_B;

    // The frame of the button.
    FillRect(renderer, x, y, DOCK_ADD_BUTTON_SIZE, 1, r, g, b, 255);
    FillRect(renderer, x, y + DOCK_ADD_BUTTON_SIZE - 1, DOCK_ADD_BUTTON_SIZE, 1, r, g, b, 255);
    FillRect(renderer, x, y, 1, DOCK_ADD_BUTTON_SIZE, r, g, b, 255);
    FillRect(renderer, x + DOCK_ADD_BUTTON_SIZE - 1, y, 1, DOCK_ADD_BUTTON_SIZE, r, g, b, 255);
    // The cross in the middle.
    FillRect(renderer, x + 7, y + 13, 14, 2, r, g, b, 255);
    FillRect(renderer, x + 13, y + 7, 2, 14, r, g, b, 255);
}

// Draws the menu of the add button: the two options.
static void DrawAddMenu(SDL_Renderer *renderer, const struct Dock *dock) {
    int x = DOCK_ADD_BUTTON_X;
    int y = dock->height;
    FillRect(renderer, x, y, DOCK_ADD_MENU_WIDTH, DOCK_ADD_MENU_ROW_HEIGHT * 2,
             COLOR_POPUP_R, COLOR_POPUP_G, COLOR_POPUP_B, COLOR_POPUP_A);

    const char *options[2] = { "File browser", "Add file or folder" };
    for (int i = 0; i < 2; i++) {
        int row_y = y + i * DOCK_ADD_MENU_ROW_HEIGHT;
        if (dock->add_menu_hover == i) {
            FillRect(renderer, x, row_y, DOCK_ADD_MENU_WIDTH, DOCK_ADD_MENU_ROW_HEIGHT,
                     COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 90);
        }
        int text_y = row_y + (DOCK_ADD_MENU_ROW_HEIGHT - FONT_HEIGHT * FONT_SCALE) / 2;
        DrawRowLabel(renderer, options[i], x + 8, text_y,
                     DOCK_ADD_MENU_WIDTH - 16, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
    }
}

// Draws the popup with the files of a folder.
static void DrawPopup(SDL_Renderer *renderer, const struct Dock *dock) {
    if (dock->popup_mode == DOCK_POPUP_NONE) return;
    int x = dock->popup_x;
    int y = dock->height;
    int rows = DockUiVisibleRows(dock);
    int height = rows * DOCK_POPUP_ROW_HEIGHT;

    FillRect(renderer, x, y, dock->popup_width, height,
             COLOR_POPUP_R, COLOR_POPUP_G, COLOR_POPUP_B, COLOR_POPUP_A);

    for (int i = 0; i < rows; i++) {
        int index = dock->row_scroll + i;
        if (index >= dock->row_count) break;
        struct DockRow *row = &dock->rows[index];
        int row_y = y + i * DOCK_POPUP_ROW_HEIGHT;

        if (index == dock->row_hover) {
            FillRect(renderer, x, row_y, dock->popup_width, DOCK_POPUP_ROW_HEIGHT,
                     COLOR_ACCENT_R, COLOR_ACCENT_G, COLOR_ACCENT_B, 90);
        }

        int text_y = row_y + (DOCK_POPUP_ROW_HEIGHT - FONT_HEIGHT * FONT_SCALE) / 2;
        if (!row->is_up && !row->is_add_dir) {
            DrawIcon(renderer, x + 6, row_y + 4, row->is_dir,
                     COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
        }
        DrawRowLabel(renderer, row->label, x + 22, text_y,
                     dock->popup_width - 28, COLOR_TEXT_R, COLOR_TEXT_G, COLOR_TEXT_B);
    }
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

    // The popup below the bar.
    if (dock->popup_mode == DOCK_POPUP_ADD_MENU) {
        DrawAddMenu(renderer, dock);
    } else {
        DrawPopup(renderer, dock);
    }

    SDL_RenderPresent(renderer);
}
