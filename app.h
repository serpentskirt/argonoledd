#ifndef APP_H
#define APP_H

#include "defines.h"
#include <signal.h>
#include <stdint.h>

typedef struct
{
    int oled_initialized;
    int i2c_fd;
    uint8_t fb[FB_SIZE];
} app_ctx_t;

app_ctx_t* app_get_ctx(void);
void app_set_ctx(app_ctx_t* ctx);

void app_cleanup(app_ctx_t* ctx);
void app_init(app_ctx_t* ctx);
void app_setup_signals(void);
void app_startup_display(int sleeptimer);
volatile sig_atomic_t* app_shutdown_flag(void);

#endif
