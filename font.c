#include "font.h"
#include "defines.h"
#include "log.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compute_charht_for_width(int charwd)
{
    if (charwd < FONT_MIN_CHARWD)
        charwd = FONT_MIN_CHARWD;

    int charht = (charwd * FONT_ASPECT_NUM) / FONT_ASPECT_DEN;

    if (charht % OLED_PAGE_HEIGHT != 0)
        charht = (charht & ~(OLED_PAGE_HEIGHT - 1)) + OLED_PAGE_HEIGHT;

    return charht;
}

static int font_load(font_t* font, const char* path, int width, int height)
{
    FILE* f = fopen(path, "rb");

    if (!f)
    {
        WARN("font_load fopen %s: %s", path, strerror(errno));

        return -1;
    }

    fseek(f, 0, SEEK_END);

    long size = ftell(f);

    rewind(f);

    if (size <= 0)
    {
        fclose(f);

        return -1;
    }

    int bytes_per_row = width;
    int num_rows = (height + OLED_PAGE_HEIGHT - 1) / OLED_PAGE_HEIGHT;
    int bytes_per_char = bytes_per_row * num_rows;

    uint8_t* filebuf = malloc((size_t)size);

    if (!filebuf)
    {
        fclose(f);

        return -1;
    }

    size_t read = fread(filebuf, 1, (size_t)size, f);

    fclose(f);

    font->data = malloc((size_t)NUM_FONT_CHARS * bytes_per_char);

    if (!font->data)
    {
        free(filebuf);

        return -1;
    }

    long expected_interleaved = (long)NUM_FONT_CHARS * bytes_per_row * num_rows;

    if ((long)read >= expected_interleaved)
        for (int c = 0; c < NUM_FONT_CHARS; ++c)
            for (int row = 0; row < num_rows; ++row)
                for (int col = 0; col < bytes_per_row; ++col)
                {
                    size_t src = (size_t)row * (NUM_FONT_CHARS * bytes_per_row)
                                 + (size_t)c * bytes_per_row + (size_t)col;
                    size_t dst =
                        (size_t)c * bytes_per_char + (size_t)row * bytes_per_row + (size_t)col;
                    ((uint8_t*)font->data)[dst] = filebuf[src];
                }
    else
    {
        size_t tocopy = read < ((size_t)NUM_FONT_CHARS * bytes_per_char)
                            ? read
                            : (size_t)NUM_FONT_CHARS * bytes_per_char;

        memcpy(font->data, filebuf, tocopy);

        if (tocopy < ((size_t)NUM_FONT_CHARS * bytes_per_char))
            memset((uint8_t*)font->data + tocopy, 0,
                   (size_t)NUM_FONT_CHARS * bytes_per_char - tocopy);
    }

    free(filebuf);

    font->width = width;
    font->height = height;
    font->bytes_per_char = bytes_per_char;

    return 0;
}

static void font_free(font_t* font)
{
    if (font->data)
        free(font->data);
}

struct cached_font
{
    font_t font;
    struct cached_font* next;
    int charwd;
};

static struct cached_font* cache_head = NULL;

font_t* font_get_cached(int charwd)
{
    struct cached_font* it = cache_head;

    while (it)
    {
        if (it->charwd == charwd)
            return &it->font;

        it = it->next;
    }

    int charht = compute_charht_for_width(charwd);
    char path[128];

    snprintf(path, sizeof(path), FONT_PATH_FMT, charht, charwd);

    struct cached_font* entry = malloc(sizeof(*entry));

    if (!entry)
        return NULL;

    entry->charwd = charwd;
    entry->next = NULL;

    if (font_load(&entry->font, path, charwd, charht) != 0)
        if (font_load(&entry->font, FONT_FALLBACK_PATH, FONT_FALLBACK_WIDTH, FONT_FALLBACK_HEIGHT)
            != 0)
        {
            free(entry);

            return NULL;
        }

    entry->next = cache_head;
    cache_head = entry;

    return &entry->font;
}

void font_free_cache(void)
{
    struct cached_font* it = cache_head;

    while (it)
    {
        struct cached_font* next = it->next;

        font_free(&it->font);
        free(it);

        it = next;
    }

    cache_head = NULL;
}
