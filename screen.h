#ifndef SCREEN_H
#define SCREEN_H

#include "defines.h"

typedef enum
{
    SCREEN_LOGO,
    SCREEN_CPU,
    SCREEN_RAM,
    SCREEN_TEMP,
    SCREEN_CLOCK,
    SCREEN_RAID,
    SCREEN_IP,
    SCREEN_STORAGE,
    SCREEN_BANDWIDTH
} screen_t;

void render_message(const char* header, const char (*details)[CMD_DETAIL_WIDTH], int n_details);
void render_screen(screen_t screen);

#endif
