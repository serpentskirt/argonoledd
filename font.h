#ifndef FONT_H
#define FONT_H

#include <stdint.h>

typedef struct
{
    uint8_t* data;
    int width;
    int height;
    int bytes_per_char;
} font_t;

font_t* font_get_cached(int charwd);
void font_free_cache(void);

#endif
