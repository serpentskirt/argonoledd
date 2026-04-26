#ifndef OLED_H
#define OLED_H

#include <stddef.h>
#include <stdint.h>

int oled_flush(void);
int oled_init(void);

uint8_t* oled_getbuffer(void);

void oled_clearbuffer(int value);
void oled_close(void);
void oled_drawfilledrectangle(int x, int y, int wd, int ht, int mode);
void oled_flushimage(int hidescreen);
void oled_power(int turnon);
void oled_writebitmap(const uint8_t* bmp, size_t len);
void oled_writetext(const char* textdata, int x, int y, int charwd);
void oled_writetextaligned(const char* textdata, int x, int y, int boxwidth, int alignmode,
                           int charwd);

#endif
