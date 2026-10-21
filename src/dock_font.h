#pragma once

#include <SDL.h>

// The built-in bitmap font of the dock bar.
//
// The font is 5x7 pixels per character, from the ASCII 32 (the space) to the
// ASCII 126 (the tilde). The characters are drawn with the pixels doubled
// (10x14), so that they are readable on a high resolution screen.
//
// The font is drawn with rectangles, so it works with the software renderer
// as well as with the accelerated one.

// The width of a character in font pixels (5 pixels + 1 pixel of space).
#define FONT_ADVANCE 6

// The height of a character in font pixels.
#define FONT_HEIGHT 7

// Draws the text at the given position of the renderer. The scale is the
// number of screen pixels per font pixel (1 or 2). The colour is given as
// three bytes.
void FontDrawText(SDL_Renderer *renderer, const char *text, int x, int y,
                  int scale, uint8_t r, uint8_t g, uint8_t b);

// The width of the text in screen pixels, for the given scale.
int FontTextWidth(const char *text, int scale);

// Draws a single character. Returns the width that was used.
int FontDrawChar(SDL_Renderer *renderer, char c, int x, int y, int scale,
                 uint8_t r, uint8_t g, uint8_t b);
