#include "render.h"
#include "defines.h"
#include "oled.h"
#include <string.h>

static void render_draw_char(uint8_t* fb, const font_t* font, int x, int y, char c)
{
    int bytes_per_col = (font->height + OLED_PAGE_HEIGHT - 1) / OLED_PAGE_HEIGHT;
    int index = (unsigned char)c * font->bytes_per_char;
    int page_aligned = (y % OLED_PAGE_HEIGHT) == 0;
    const uint8_t* glyph = font->data + index;

    for (int col = 0; col < font->width; ++col)
    {
        int px = x + col;

        if ((px < 0) || (px >= OLED_WIDTH))
            continue;

        for (int slice = 0; slice < bytes_per_col; ++slice)
        {
            int src_y = slice * OLED_PAGE_HEIGHT;
            int rows = font->height - src_y;

            if (rows <= 0)
                break;

            if (rows > OLED_PAGE_HEIGHT)
                rows = OLED_PAGE_HEIGHT;

            uint8_t byte = glyph[col + slice * font->width];
            int dst_y = y + src_y;

            if (page_aligned && (rows == OLED_PAGE_HEIGHT) && (dst_y >= 0)
                && (dst_y + OLED_PAGE_HEIGHT <= OLED_HEIGHT))
            {
                fb[(dst_y / OLED_PAGE_HEIGHT) * OLED_WIDTH + px] = byte;

                continue;
            }

            int visible_start = dst_y < 0 ? 0 : dst_y;
            int visible_end = dst_y + rows;

            if (visible_end > OLED_HEIGHT)
                visible_end = OLED_HEIGHT;

            if (visible_start >= visible_end)
                continue;

            int first_page = visible_start / OLED_PAGE_HEIGHT;
            int last_page = (visible_end - 1) / OLED_PAGE_HEIGHT;

            for (int page = first_page; page <= last_page; ++page)
            {
                int page_y = page * OLED_PAGE_HEIGHT;
                int start = visible_start > page_y ? visible_start : page_y;
                int end = visible_end < page_y + OLED_PAGE_HEIGHT ? visible_end
                                                                  : page_y + OLED_PAGE_HEIGHT;
                int count = end - start;
                int dst_bit_start = start - page_y;
                int src_bit_start = start - dst_y;
                uint8_t mask = (uint8_t)(((1u << count) - 1u) << dst_bit_start);
                uint8_t bits =
                    (uint8_t)(((byte >> src_bit_start) & ((1u << count) - 1u)) << dst_bit_start);
                uint8_t* dst = fb + page * OLED_WIDTH + px;

                *dst = (uint8_t)((*dst & (uint8_t)~mask) | bits);
            }
        }
    }
}

void render_draw_bitmap(const uint8_t* bmp, size_t len)
{
    uint8_t* fb = oled_getbuffer();

    if (!fb || !bmp || (len == 0))
        return;

    if (len > BITMAP_COPY_MAX)
        len = BITMAP_COPY_MAX;

    memcpy(fb, bmp, len);
}

void render_draw_filled_rect(int x, int y, int w, int h, int color)
{
    uint8_t* fb = oled_getbuffer();

    if (!fb || (w <= 0) || (h <= 0))
        return;

    if (x < 0)
    {
        w += x;
        x = 0;
    }

    if (y < 0)
    {
        h += y;
        y = 0;
    }

    int x_end = x + w;
    int y_end = y + h;

    if ((x >= OLED_WIDTH) || (y >= OLED_HEIGHT) || (x_end <= 0) || (y_end <= 0))
        return;

    if (x_end > OLED_WIDTH)
        x_end = OLED_WIDTH;

    if (y_end > OLED_HEIGHT)
        y_end = OLED_HEIGHT;

    int width = x_end - x;
    int first_page = y / OLED_PAGE_HEIGHT;
    int last_page = (y_end - 1) / OLED_PAGE_HEIGHT;

    for (int page = first_page; page <= last_page; ++page)
    {
        int page_y = page * OLED_PAGE_HEIGHT;
        int bit_start = y > page_y ? y - page_y : 0;
        int bit_end = y_end < page_y + OLED_PAGE_HEIGHT ? y_end - page_y : OLED_PAGE_HEIGHT;
        uint8_t mask = (uint8_t)(((1u << (bit_end - bit_start)) - 1u) << bit_start);
        uint8_t* dst = fb + page * OLED_WIDTH + x;

        if (mask == OLED_BYTE_ALL_ON)
            memset(dst, color ? OLED_BYTE_ALL_ON : 0, (size_t)width);
        else if (color)
            for (int i = 0; i < width; ++i)
                dst[i] |= mask;
        else
            for (int i = 0; i < width; ++i)
                dst[i] &= (uint8_t)~mask;
    }
}

void render_draw_text(const font_t* font, int x, int y, const char* str)
{
    uint8_t* fb = oled_getbuffer();

    if (!fb)
        return;

    int cursor = x;

    while (*str)
    {
        render_draw_char(fb, font, cursor, y, *str);

        cursor += font->width + 1;
        ++str;
    }
}

void render_draw_text_aligned(const font_t* font, int x, int y, int boxwidth, int alignmode,
                              const char* str)
{
    int len = strlen(str);
    int text_w = len * (font->width + 1) - 1;
    int leftoffset = 0;

    if (alignmode == 1)
        leftoffset = (boxwidth - text_w) / 2;
    else if (alignmode == 2)
        leftoffset = (boxwidth - text_w);

    render_draw_text(font, x + leftoffset, y, str);
}

void set_pixel_fb(uint8_t* fb, int x, int y, int color)
{
    if ((x < 0) || (x >= OLED_WIDTH) || (y < 0) || (y >= OLED_HEIGHT))
        return;

    int index = x + (y / OLED_PAGE_HEIGHT) * OLED_WIDTH;

    if (color)
        fb[index] |= (uint8_t)(1 << (y % OLED_PAGE_HEIGHT));
    else
        fb[index] &= (uint8_t)~(1 << (y % OLED_PAGE_HEIGHT));
}
