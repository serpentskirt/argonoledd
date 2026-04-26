#include "screen.h"
#include "config.h"
#include "defines.h"
#include "log.h"
#include "oled.h"
#include "stats.h"
#include "util.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static int to_display_temp(int celsius, int use_fahrenheit)
{
    if (!use_fahrenheit)
        return celsius;

    return 32 + (9 * celsius / 5);
}

static void draw_message_layout(const char* header, const char (*details)[CMD_DETAIL_WIDTH],
                                int n_details)
{
    oled_writetext(header, 0, MSG_HEADER_Y, LAYOUT_LARGE_CHARWD);

    for (int i = 0; i < n_details && i < CMD_MAX_DETAILS; ++i)
        oled_writetext(details[i], 0, MSG_DETAIL_Y_START + (i * MSG_DETAIL_ROW_HEIGHT),
                       LAYOUT_SMALL_CHARWD);
}

static void kbstr(long kb, char* buf, int buflen)
{
    const char* suffixes[] = {"KB", "MB", "GB", "TB"};
    int sidx = 0;
    long remainder = 0;

    while ((kb > 1023) && (sidx < (int)ARRAY_SIZE(suffixes)))
    {
        remainder = kb & 1023;
        kb >>= 10;
        ++sidx;
    }

    if (sidx >= (int)ARRAY_SIZE(suffixes))
        sidx = (int)ARRAY_SIZE(suffixes) - 1;

    if (remainder >= 500)
        ++kb;

    snprintf(buf, buflen, "%ld%s", kb, suffixes[sidx]);
}

static void show_background(const uint8_t* bg, size_t size)
{
    if (bg && (size > 0))
    {
        if (size > BITMAP_COPY_MAX)
            size = BITMAP_COPY_MAX;

        oled_writebitmap(bg, size);
    }
}

static void load_bg(const char* name)
{
    char path[RESOURCE_PATH_MAX];
    uint8_t* bg = NULL;
    size_t bg_size = 0;

    int n = snprintf(path, sizeof(path), "res/%s.bin", name);

    if ((n < 0) || (n >= (int)sizeof(path)))
    {
        ERROR("subsystem=screen action=load_bg status=fail reason=path_too_long name=%s", name);

        return;
    }

    load_file(path, &bg, &bg_size);
    show_background(bg, bg_size);
    free(bg);
}

static void show_bandwidth(void)
{
    bandwidth_stats_t bs;

    memset(&bs, 0, sizeof(bs));
    stats_read_bandwidth(&bs);
    oled_clearbuffer(0);
    oled_writetext("BANDWIDTH", 0, BW_HEADER_Y, LAYOUT_SMALL_CHARWD);
    oled_writetextaligned("Write", BW_SIZE_X, BW_COLHEADER_Y, OLED_WIDTH - BW_SIZE_X, 2,
                          LAYOUT_SMALL_CHARWD);
    oled_writetextaligned("Read", BW_PCT_X, BW_COLHEADER_Y,
                          OLED_WIDTH - BW_SIZE_X - BW_PCT_X_OFFSET, 2, LAYOUT_SMALL_CHARWD);
    oled_writetext("Device", 0, BW_COLHEADER_Y, LAYOUT_SMALL_CHARWD);

    int yoffset = BW_ROW_Y_START;

    for (int i = 0; (i < bs.n_bandwidth) && (i < BW_MAX_DISPLAY); ++i)
    {
        char readbuf[SCREEN_VALUE_BUF_SIZE];
        char writebuf[SCREEN_VALUE_BUF_SIZE];
        char namebuf[BW_NAME_MAX_CHARS + 1];

        kbstr(bs.entries[i].read_kbps, readbuf, sizeof(readbuf));
        kbstr(bs.entries[i].write_kbps, writebuf, sizeof(writebuf));
        oled_writetextaligned(writebuf, BW_SIZE_X, yoffset, OLED_WIDTH - BW_SIZE_X, 2,
                              LAYOUT_SMALL_CHARWD);
        oled_writetextaligned(readbuf, BW_PCT_X, yoffset, OLED_WIDTH - BW_SIZE_X - BW_PCT_X_OFFSET,
                              2, LAYOUT_SMALL_CHARWD);
        strncpy(namebuf, bs.entries[i].name, BW_NAME_MAX_CHARS);

        namebuf[BW_NAME_MAX_CHARS] = '\0';

        oled_writetext(namebuf, 0, yoffset, LAYOUT_SMALL_CHARWD);

        yoffset += BW_ROW_HEIGHT;
    }
}

static void show_clock(void)
{
    time_t t = time(NULL);
    struct tm lt;

    localtime_r(&t, &lt);
    load_bg("bgtime");

    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                            "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    const char* weekday[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    char out[SCREEN_VALUE_BUF_SIZE];
    int widx = (lt.tm_wday + 6) % 7;

    snprintf(out, sizeof(out), "%s %d", months[lt.tm_mon], lt.tm_mday);
    oled_writetextaligned(out, LAYOUT_DATA_X, CLOCK_DATE_Y, LAYOUT_DATA_W, 1, LAYOUT_LARGE_CHARWD);
    oled_writetextaligned(weekday[widx], LAYOUT_DATA_X, CLOCK_WEEKDAY_Y, LAYOUT_DATA_W, 1,
                          LAYOUT_LARGE_CHARWD);
    snprintf(out, sizeof(out), "%02d:%02d", lt.tm_hour, lt.tm_min);
    oled_writetextaligned(out, LAYOUT_DATA_X, CLOCK_TIME_Y, LAYOUT_DATA_W, 1, LAYOUT_LARGE_CHARWD);
}

static void show_cpu(void)
{
    cpu_stats_t cs;
    char buf[SCREEN_VALUE_BUF_SIZE];
    int yoffset = 0;

    memset(&cs, 0, sizeof(cs));
    stats_read_cpu(&cs);
    load_bg("bgcpu");

    for (int i = 0; i < cs.n_cores && i < CPU_MAX_DISPLAY_CORES; ++i)
    {
        int bar_x = LAYOUT_DATA_X;
        int bar_y = yoffset + CPU_BAR_Y_OFFSET;
        int fill = (cs.per_core[i] * CPU_BAR_WIDTH) / 100;

        snprintf(buf, sizeof(buf), "C%d: %d%%", i, cs.per_core[i]);
        oled_writetext(buf, LAYOUT_DATA_X, yoffset, LAYOUT_SMALL_CHARWD);
        oled_drawfilledrectangle(bar_x, bar_y, CPU_BAR_WIDTH, CPU_BAR_HEIGHT, 0);
        oled_drawfilledrectangle(bar_x, bar_y, fill, CPU_BAR_HEIGHT, 1);

        yoffset += CPU_ROW_HEIGHT;
    }
}

static void show_ip(void)
{
    ip_stats_t ips;

    memset(&ips, 0, sizeof(ips));
    stats_read_ip(&ips);
    load_bg("bgip");

    if (ips.n_ip == 0)
    {
        oled_writetextaligned("No IP", 0, IP_NO_IP_Y, OLED_WIDTH, 1, LAYOUT_LARGE_CHARWD);

        return;
    }

    oled_writetextaligned(ips.entries[0].ifname, 0, IP_IFNAME_Y, OLED_WIDTH, 1,
                          LAYOUT_LARGE_CHARWD);
    oled_writetextaligned(ips.entries[0].ip, 0, IP_ADDR_Y, OLED_WIDTH, 1, LAYOUT_LARGE_CHARWD);
}

static void show_logo(void)
{
    uint8_t* logo = NULL;
    size_t size = 0;

    if ((load_file("res/logo.bin", &logo, &size) != 0) || !logo)
    {
        ERROR("subsystem=screen action=show_logo status=fail file=res/logo.bin");

        return;
    }

    if (size > FB_SIZE)
        size = FB_SIZE;

    oled_writebitmap(logo, size);
    free(logo);
}

static void show_raid(void)
{
    raid_stats_t rs;

    memset(&rs, 0, sizeof(rs));
    stats_read_raid(&rs);
    load_bg("bgraid");

    if (rs.n_raid == 0)
    {
        oled_writetextaligned("No RAID", LAYOUT_DATA_X, RAID_NO_RAID_Y, LAYOUT_DATA_W, 1,
                              LAYOUT_LARGE_CHARWD);

        return;
    }

    raid_entry_t* e = &rs.entries[0];
    char sizebuf[SCREEN_SHORT_BUF_SIZE];
    char statebuf[SCREEN_LINE_BUF_SIZE];

    oled_writetextaligned(e->title, 0, RAID_TITLE_Y, LAYOUT_DATA_X, 1, LAYOUT_SMALL_CHARWD);
    oled_writetextaligned(e->value, 0, RAID_VALUE_Y, LAYOUT_DATA_X, 1, LAYOUT_SMALL_CHARWD);
    kbstr(e->size_kb, sizebuf, sizeof(sizebuf));
    oled_writetextaligned(sizebuf, 0, RAID_SIZE_Y, LAYOUT_DATA_X, 1, LAYOUT_SMALL_CHARWD);
    snprintf(statebuf, sizeof(statebuf), "%s", e->state);

    char* last_seg = statebuf;
    char* comma = statebuf;

    while ((comma = strchr(comma, ',')) != NULL)
    {
        ++comma;

        while (*comma == ' ')
            ++comma;

        last_seg = comma;
    }

    if ((last_seg[0] >= 'a') && (last_seg[0] <= 'z'))
        last_seg[0] = (char)(last_seg[0] - 'a' + 'A');

    oled_writetext(last_seg, LAYOUT_DATA_X, RAID_STATE_Y, LAYOUT_SMALL_CHARWD);

    if (e->resync[0])
    {
        char pctbuf[SCREEN_LINE_BUF_SIZE];

        snprintf(pctbuf, sizeof(pctbuf), "%s", e->resync);

        char* sp = strchr(pctbuf, ' ');

        if (sp)
            *sp = '\0';

        int is_checking = (strcasecmp(last_seg, "checking") == 0);
        char line[SCREEN_RAID_LINE_BUF_SIZE];

        snprintf(line, sizeof(line), "%s%s", is_checking ? "Progress: " : "Rebuild: ", pctbuf);
        oled_writetext(line, LAYOUT_DATA_X, RAID_RESYNC_Y, LAYOUT_SMALL_CHARWD);
    }

    int total = e->total_devices;
    char cntbuf[SCREEN_VALUE_BUF_SIZE];

    snprintf(cntbuf, sizeof(cntbuf), "Active:%d/%d", e->active_devices, total);
    oled_writetext(cntbuf, LAYOUT_DATA_X, RAID_ACTIVE_Y, LAYOUT_SMALL_CHARWD);
    snprintf(cntbuf, sizeof(cntbuf), "Working:%d/%d", e->working_devices, total);
    oled_writetext(cntbuf, LAYOUT_DATA_X, RAID_WORKING_Y, LAYOUT_SMALL_CHARWD);
    snprintf(cntbuf, sizeof(cntbuf), "Failed:%d/%d", e->failed_devices, total);
    oled_writetext(cntbuf, LAYOUT_DATA_X, RAID_FAILED_Y, LAYOUT_SMALL_CHARWD);
}

static void show_ram(void)
{
    ram_stats_t rs;

    memset(&rs, 0, sizeof(rs));
    stats_read_ram(&rs);
    load_bg("bgram");

    char buf[SCREEN_VALUE_BUF_SIZE];

    snprintf(buf, sizeof(buf), "%d%%", rs.available_percent);
    oled_writetextaligned(buf, LAYOUT_DATA_X, RAM_PCT_Y, LAYOUT_DATA_W, 1, LAYOUT_LARGE_CHARWD);
    oled_writetextaligned("of", LAYOUT_DATA_X, RAM_OF_Y, LAYOUT_DATA_W, 1, LAYOUT_LARGE_CHARWD);

    if (rs.total_ram_kb > 0)
    {
        int gb = (int)((rs.total_ram_kb + 512L * 1024L) >> 20);

        snprintf(buf, sizeof(buf), "%dGB", gb);
        oled_writetextaligned(buf, LAYOUT_DATA_X, RAM_TOTAL_Y, LAYOUT_DATA_W, 1,
                              LAYOUT_LARGE_CHARWD);
    }
    else
        oled_writetextaligned("N/A", LAYOUT_DATA_X, RAM_TOTAL_Y, LAYOUT_DATA_W, 1,
                              LAYOUT_LARGE_CHARWD);
}

static void show_storage(void)
{
    storage_stats_t ss;

    memset(&ss, 0, sizeof(ss));
    stats_read_storage(&ss);
    load_bg("bgstorage");

    int yoffset = STORAGE_ROW_Y_START;

    for (int i = 0; (i < ss.n_storage) && (i < STORAGE_MAX_DISPLAY); ++i)
    {
        char valbuf[SCREEN_VALUE_BUF_SIZE];
        char pctbuf[SCREEN_SHORT_BUF_SIZE];

        kbstr(ss.entries[i].total_kb, valbuf, sizeof(valbuf));
        snprintf(pctbuf, sizeof(pctbuf), "%d%%", ss.entries[i].percent);
        oled_writetextaligned(valbuf, STORAGE_SIZE_X, yoffset, OLED_WIDTH - STORAGE_SIZE_X, 2,
                              LAYOUT_SMALL_CHARWD);
        oled_writetextaligned(pctbuf, STORAGE_PCT_X, yoffset,
                              OLED_WIDTH - STORAGE_SIZE_X - STORAGE_PCT_X_OFFSET, 2,
                              LAYOUT_SMALL_CHARWD);

        char namebuf[STORAGE_NAME_MAX_CHARS + 1];

        snprintf(namebuf, sizeof(namebuf), "%s", ss.entries[i].name);
        oled_writetext(namebuf, 0, yoffset, LAYOUT_SMALL_CHARWD);

        yoffset += STORAGE_ROW_HEIGHT;
    }
}

static void str_toupper(char* s)
{
    for (; *s; ++s)
        if ((*s >= 'a') && (*s <= 'z'))
            *s -= ('a' - 'A');
}

static void show_temp(void)
{
    int cpuc = 0;
    hdd_temp_stats_t hs;

    stats_read_cpu_temp(&cpuc);
    memset(&hs, 0, sizeof(hs));
    stats_read_hdd_temp(&hs);

    const char* tempunit = config_get("oled", "temperature", "C");
    int use_fahrenheit = (tempunit[0] == 'F' || tempunit[0] == 'f');

    load_bg("bgtemp");

    int maxcval = cpuc;

    if ((hs.n_hdd > 0) && (hs.max_c > maxcval))
        maxcval = hs.max_c;

    char buf[SCREEN_VALUE_BUF_SIZE];

    if (hs.n_hdd > 0)
    {
        typedef struct
        {
            const char* label;
            int val;
        } temprow_t;

        temprow_t rows[3] = {
            {"cpu", cpuc},
            {"hdd min", hs.min_c},
            {"hdd max", hs.max_c},
        };

        int displayrow = TEMP_ROW_Y_START;

        for (int i = 0; i < (int)ARRAY_SIZE(rows); ++i)
        {
            snprintf(buf, sizeof(buf), "%d%c%s", to_display_temp(rows[i].val, use_fahrenheit),
                     OLED_CHAR_DEGREE, use_fahrenheit ? "F" : "C");

            if ((int)strlen(rows[i].label) <= TEMP_LABEL_SHORT_MAX)
            {
                char line[TEMP_LINE_BUF_SIZE];
                char upper[TEMP_LABEL_BUF_SIZE];

                snprintf(upper, sizeof(upper), "%s", rows[i].label);
                str_toupper(upper);
                snprintf(line, sizeof(line), "%s: %s", upper, buf);
                oled_writetext(line, LAYOUT_DATA_X, displayrow, LAYOUT_SMALL_CHARWD);
            }
            else
            {
                char upper[SCREEN_SHORT_BUF_SIZE];

                snprintf(upper, sizeof(upper), "%s:", rows[i].label);
                str_toupper(upper);
                oled_writetext(upper, LAYOUT_DATA_X, displayrow, LAYOUT_SMALL_CHARWD);
                oled_writetext(buf, LAYOUT_DATA_X + TEMP_SECOND_LINE_XOFFSET,
                               displayrow + TEMP_ROW_HEIGHT, LAYOUT_SMALL_CHARWD);
            }

            displayrow += TEMP_ROW_HEIGHT * 2;
        }
    }
    else
    {
        snprintf(buf, sizeof(buf), "%d%c%s", to_display_temp(cpuc, use_fahrenheit),
                 OLED_CHAR_DEGREE, use_fahrenheit ? "F" : "C");
        oled_writetextaligned(buf, LAYOUT_DATA_X, TEMP_SINGLE_VALUE_Y, LAYOUT_DATA_W, 1,
                              LAYOUT_LARGE_CHARWD);
    }

    int barht = (int)(TEMP_BAR_MAX_HEIGHT * maxcval / 100.0f);

    if (barht > TEMP_BAR_MAX_HEIGHT)
        barht = TEMP_BAR_MAX_HEIGHT;

    if (barht < 1)
        barht = 1;

    oled_drawfilledrectangle(TEMP_BAR_X, TEMP_BAR_Y + (TEMP_BAR_MAX_HEIGHT - barht), TEMP_BAR_WIDTH,
                             barht, 1);
}

void render_message(const char* header, const char (*details)[CMD_DETAIL_WIDTH], int n_details)
{
    oled_clearbuffer(0);
    draw_message_layout(header, details, n_details);
    oled_flushimage(1);

    INFO("subsystem=message action=render header=%s", header);
}

void render_screen(screen_t screen)
{
    static screen_t prev_screen = (screen_t)-1;
    int screen_changed = (screen != prev_screen);

    prev_screen = screen;

    oled_clearbuffer(0);

    switch (screen)
    {
        case SCREEN_LOGO:
        {
            show_logo();
            break;
        }
        case SCREEN_CPU:
        {
            show_cpu();
            break;
        }
        case SCREEN_STORAGE:
        {
            show_storage();
            break;
        }
        case SCREEN_RAID:
        {
            show_raid();
            break;
        }
        case SCREEN_IP:
        {
            show_ip();
            break;
        }
        case SCREEN_RAM:
        {
            show_ram();
            break;
        }
        case SCREEN_TEMP:
        {
            show_temp();
            break;
        }
        case SCREEN_CLOCK:
        {
            show_clock();
            break;
        }
        case SCREEN_BANDWIDTH:
        {
            show_bandwidth();
            break;
        }
    }

    oled_flushimage(screen_changed);
}
