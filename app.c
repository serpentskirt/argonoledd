#include "app.h"
#include "font.h"
#include "log.h"
#include "oled.h"
#include "screen.h"
#include "stats.h"
#include "util.h"
#include <string.h>

static app_ctx_t* g_app = NULL;
static volatile sig_atomic_t g_shutdown_requested = 0;

static void handle_sig(int sig)
{
    (void)sig;

    g_shutdown_requested = 1;
}

app_ctx_t* app_get_ctx(void)
{
    return g_app;
}

void app_set_ctx(app_ctx_t* ctx)
{
    g_app = ctx;
}

void app_cleanup(app_ctx_t* ctx)
{
    if (!ctx)
        return;

    if (ctx->oled_initialized)
    {
        INFO("subsystem=app action=cleanup target=oled");

        oled_close();

        ctx->oled_initialized = 0;
    }
    else
        font_free_cache();

    log_close();
}

void app_init(app_ctx_t* ctx)
{
    INFO("subsystem=app action=startup");

    if (!ctx)
        return;

    ctx->oled_initialized = 0;
    ctx->i2c_fd = -1;

    memset(ctx->fb, 0, sizeof(ctx->fb));

    cpu_stats_t tmp_cpu;
    bandwidth_stats_t tmp_bandwidth;

    stats_read_cpu(&tmp_cpu);
    stats_read_bandwidth(&tmp_bandwidth);
}

void app_setup_signals(void)
{
    struct sigaction sa = {0};

    sa.sa_handler = handle_sig;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
}

void app_startup_display(int sleeptimer)
{
    render_screen(SCREEN_LOGO);

    if (sleeptimer > 0)
        wait_ms((long)sleeptimer * MS_PER_SEC);

    if (g_shutdown_requested)
        return;
}

volatile sig_atomic_t* app_shutdown_flag(void)
{
    return &g_shutdown_requested;
}
