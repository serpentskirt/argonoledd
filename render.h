#ifndef RENDER_H
#define RENDER_H

#include "font.h"
#include <stddef.h>

void render_draw_bitmap(const uint8_t* bmp, size_t len);
void render_draw_filled_rect(int x, int y, int w, int h, int color);
void render_draw_text(const font_t* font, int x, int y, const char* str);
void render_draw_text_aligned(const font_t* font, int x, int y, int boxwidth, int alignmode,
                              const char* str);
void set_pixel_fb(uint8_t* fb, int x, int y, int color);

#endif
