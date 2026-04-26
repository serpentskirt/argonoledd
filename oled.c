#include "oled.h"
#include "app.h"
#include "defines.h"
#include "log.h"
#include "render.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int oled_command(uint8_t cmd)
{
    uint8_t buf[2] = {OLED_CONTROL_COMMAND, cmd};
    app_ctx_t* ctx = app_get_ctx();

    if (!ctx)
        return -1;

    int fd = ctx->i2c_fd;

    if (fd < 0)
        return -1;

    if (write(fd, buf, 2) != 2)
    {
        ERROR("subsystem=oled action=cmd status=fail cmd=0x%02x errno=%d", cmd, errno);

        return -1;
    }

    return 0;
}

static int oled_data(const uint8_t* data, size_t len)
{
    app_ctx_t* ctx = app_get_ctx();

    if (!ctx)
        return -1;

    int fd = ctx->i2c_fd;

    if (fd < 0)
        return -1;

    uint8_t buf[1 + FB_SIZE]; // control byte first

    buf[0] = OLED_CONTROL_DATA;

    memcpy(buf + 1, data, len);

    if (write(fd, buf, len + 1) != (ssize_t)(len + 1))
    {
        ERROR("subsystem=oled action=data_write status=fail errno=%d", errno);

        return -1;
    }

    return 0;
}

int oled_flush(void)
{
    // set full column + page range before sending pixel data
    if ((oled_command(OLED_CMD_SET_COLUMN_ADDR) != 0) || (oled_command(0) != 0)
        || (oled_command(OLED_WIDTH - 1) != 0) || (oled_command(OLED_CMD_SET_PAGE_ADDR) != 0)
        || (oled_command(0) != 0) || (oled_command(OLED_PAGE_COUNT - 1) != 0))
    {
        ERROR("subsystem=oled action=flush status=addr_fail errno=%d", errno);

        return -1;
    }

    uint8_t* gfb = oled_getbuffer();

    if (!gfb)
        return -1;

    return oled_data(gfb, FB_SIZE);
}

int oled_init()
{
    app_ctx_t* ctx = app_get_ctx();

    if (!ctx)
        return -1;

    ctx->i2c_fd = open(I2C_DEV, O_RDWR);

    if (ctx->i2c_fd < 0)
    {
        ERROR("open i2c: %s", strerror(errno));

        return -1;
    }

    INFO("subsystem=i2c action=open dev=%s fd=%d", I2C_DEV, ctx->i2c_fd);

    if (ioctl(ctx->i2c_fd, I2C_SLAVE, OLED_ADDR) < 0)
    {
        ERROR("ioctl I2C_SLAVE: %s", strerror(errno));

        if (ctx->i2c_fd >= 0)
        {
            close(ctx->i2c_fd);

            ctx->i2c_fd = -1;
        }

        return -1;
    }

    INFO("subsystem=i2c action=set_addr addr=0x%02x", OLED_ADDR);
    INFO("subsystem=oled action=init status=start");

    // clang-format off
    const uint8_t init_cmds[] = {
        OLED_CMD_DISPLAY_OFF,
        OLED_CMD_SET_MEMORY_MODE, OLED_MEMORY_MODE_HORIZONTAL,
        OLED_CMD_SET_PAGE_START, OLED_CMD_COM_SCAN_DEC,
        OLED_CMD_LOW_COLUMN_START, OLED_CMD_HIGH_COLUMN_START,
        OLED_CMD_SET_START_LINE,
        OLED_CMD_SET_CONTRAST, OLED_CONTRAST,
        OLED_CMD_SEGMENT_REMAP, OLED_CMD_NORMAL_DISPLAY,
        OLED_CMD_SET_MULTIPLEX, OLED_MULTIPLEX,
        OLED_CMD_DISPLAY_RAM, OLED_CMD_SET_DISPLAY_OFFSET, OLED_DISPLAY_OFFSET_NONE,
        OLED_CMD_SET_CLOCK_DIV, OLED_CLOCK_DIV,
        OLED_CMD_SET_PRECHARGE, OLED_PRECHARGE,
        OLED_CMD_SET_COM_PINS, OLED_COM_PINS,
        OLED_CMD_SET_VCOM_DETECT, OLED_VCOM_DETECT,
        OLED_CMD_SET_CHARGE_PUMP, OLED_CHARGE_PUMP_ENABLE,
        OLED_CMD_DISPLAY_ON
                                };
    // clang-format on

    for (size_t i = 0; i < sizeof(init_cmds); ++i)
    {
        if (oled_command(init_cmds[i]) != 0)
        {
            ERROR("subsystem=oled action=init status=fail cmd=0x%02x errno=%d", init_cmds[i],
                  errno);

            if (ctx->i2c_fd >= 0)
            {
                close(ctx->i2c_fd);

                ctx->i2c_fd = -1;
            }

            return -1;
        }
    }

    INFO("subsystem=oled action=init status=done");

    ctx->oled_initialized = 1;

    return 0;
}

uint8_t* oled_getbuffer(void)
{
    app_ctx_t* ctx = app_get_ctx();

    if (!ctx)
        return NULL;

    return ctx->fb;
}

void oled_clearbuffer(int value)
{
    uint8_t* gfb = oled_getbuffer();

    if (!gfb)
        return;

    memset(gfb, value ? OLED_BYTE_ALL_ON : 0, FB_SIZE);
}

void oled_close()
{
    oled_command(OLED_CMD_DISPLAY_OFF);

    app_ctx_t* ctx = app_get_ctx();

    if (ctx && (ctx->i2c_fd >= 0))
    {
        close(ctx->i2c_fd);

        ctx->i2c_fd = -1;
    }

    font_free_cache();
}

void oled_drawfilledrectangle(int x, int y, int wd, int ht, int mode)
{
    render_draw_filled_rect(x, y, wd, ht, mode != 0 ? 1 : 0);
}

void oled_flushimage(int hidescreen)
{
    if (hidescreen)
        oled_power(0);

    oled_flush();

    if (hidescreen)
        oled_power(1);
}

void oled_power(int turnon)
{
    oled_command(turnon ? OLED_CMD_DISPLAY_ON : OLED_CMD_DISPLAY_OFF);
}

void oled_writebitmap(const uint8_t* bmp, size_t len)
{
    render_draw_bitmap(bmp, len);
}

void oled_writetext(const char* textdata, int x, int y, int charwd)
{
    font_t* f = font_get_cached(charwd);

    if (!f)
        return;

    render_draw_text(f, x, y, textdata);
}

void oled_writetextaligned(const char* textdata, int x, int y, int boxwidth, int alignmode,
                           int charwd)
{
    font_t* f = font_get_cached(charwd);

    if (!f)
        return;

    render_draw_text_aligned(f, x, y, boxwidth, alignmode, textdata);
}
