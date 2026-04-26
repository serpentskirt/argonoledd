#include "app.h"
#include "command.h"
#include "config.h"
#include "defines.h"
#include "log.h"
#include "oled.h"
#include "socket.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static screen_t screen_from_name(const char* name)
{
    if (strcmp(name, "clock") == 0)
        return SCREEN_CLOCK;
    if (strcmp(name, "cpu") == 0)
        return SCREEN_CPU;
    if (strcmp(name, "storage") == 0)
        return SCREEN_STORAGE;
    if (strcmp(name, "bandwidth") == 0)
        return SCREEN_BANDWIDTH;
    if (strcmp(name, "raid") == 0)
        return SCREEN_RAID;
    if (strcmp(name, "ram") == 0)
        return SCREEN_RAM;
    if (strcmp(name, "temp") == 0)
        return SCREEN_TEMP;
    if (strcmp(name, "ip") == 0)
        return SCREEN_IP;

    return (screen_t)-1;
}

static int build_screen_list(screen_t* screens, int max)
{
    const char* slist = config_get("oled", "screenlist", DEFAULT_SCREENS);
    char buf[512];
    char* saveptr = NULL;
    int count = 0;

    snprintf(buf, sizeof(buf), "%s", slist);

    char* tok = strtok_r(buf, " \t,", &saveptr);

    while (tok && (count < max))
    {
        screen_t s = screen_from_name(tok);

        if (s != (screen_t)-1)
            screens[count++] = s;
        else
            INFO("subsystem=config action=unknown_screen name=%s", tok);

        tok = strtok_r(NULL, " \t,", &saveptr);
    }

    if (count == 0)
    {
        screen_t defaults[] = {SCREEN_CLOCK, SCREEN_CPU, SCREEN_STORAGE, SCREEN_BANDWIDTH,
                               SCREEN_RAID,  SCREEN_RAM, SCREEN_TEMP,    SCREEN_IP};

        count = (int)(sizeof(defaults) / sizeof(defaults[0]));

        memcpy(screens, defaults, sizeof(defaults));

        INFO("subsystem=config action=fallback_defaults");
    }

    return count;
}

static long now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return ts.tv_sec * MS_PER_SEC + ts.tv_nsec / NS_PER_MS;
}

static ssize_t read_client_command(int client, char* line, size_t line_size)
{
    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(client, &rfds);

    struct timeval tv;

    tv.tv_sec = SOCKET_CLIENT_READ_TIMEOUT_MS / MS_PER_SEC;
    tv.tv_usec = (SOCKET_CLIENT_READ_TIMEOUT_MS % MS_PER_SEC) * MS_PER_SEC;

    int sel = select(client + 1, &rfds, NULL, NULL, &tv);

    if (sel == 0)
    {
        WARN("subsystem=socket action=read status=timeout timeout_ms=%d",
             SOCKET_CLIENT_READ_TIMEOUT_MS);

        return -1;
    }

    if (sel < 0)
    {
        if (errno != EINTR)
            ERROR("subsystem=socket action=read_wait status=fail errno=%d", errno);

        return -1;
    }

    return read(client, line, line_size - 1);
}

int main(int argc, char** argv)
{
    int foreground = 0;
    const char* config_path = DEFAULT_CONFIG;

    for (int i = 1; i < argc; ++i)
    {
        if ((strcmp(argv[i], "--foreground") == 0) || (strcmp(argv[i], "-f") == 0))
            foreground = 1;
        else if (((strcmp(argv[i], "--config") == 0) || (strcmp(argv[i], "-c") == 0))
                 && ((i + 1) < argc))
            config_path = argv[++i];
        else if ((strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0))
        {
            printf("Usage: %s [--foreground|-f] [--config|-c <file>]\n"
                   "\n"
                   "  --foreground  Log to stderr instead of syslog\n"
                   "  --config      Config file path (default: %s)\n"
                   "\n"
                   "Config file keys:\n"
                   "  [oled]\n"
                   "  screenduration = %d      # seconds per screen / message\n"
                   "  screenlist     = %s\n"
                   "  temperature    = C      # C | F\n"
                   "\n"
                   "Socket commands:\n"
                   "  MESSAGE header=<text> [key=val ...]\n"
                   "  STATUS\n"
                   "  PING\n",
                   argv[0], DEFAULT_CONFIG, DEFAULT_DURATION, DEFAULT_SCREENS);

            return 0;
        }
    }

    log_init(foreground);
    config_load(config_path);

    screen_t screens[MAX_SCREEN_LIST];
    int screen_count = build_screen_list(screens, (int)(sizeof(screens) / sizeof(screens[0])));
    app_ctx_t app;

    app_init(&app);
    app_set_ctx(&app);
    app_setup_signals();

    if (oled_init() != 0)
    {
        app_cleanup(&app);

        return 1;
    }

    app_startup_display(DEFAULT_DURATION);

    if (*app_shutdown_flag())
    {
        app_cleanup(&app);

        return 0;
    }

    char sock_path[256];

    snprintf(sock_path, sizeof(sock_path), "%s", config_get("socket", "path", SOCKET_PATH));

    int sock_fd = socket_create(sock_path);

    if (sock_fd < 0)
    {
        app_cleanup(&app);

        return 1;
    }

    disp_ctx_t disp;

    disp_init(&disp, screens, screen_count);

    long t_prev = now_ms();

    while (!*app_shutdown_flag())
    {
        long timeout = state_timeout_ms(&disp);

        fd_set rfds;

        FD_ZERO(&rfds);
        FD_SET(sock_fd, &rfds);

        struct timeval tv;

        tv.tv_sec = timeout / MS_PER_SEC;
        tv.tv_usec = (timeout % MS_PER_SEC) * MS_PER_SEC;

        int sel = select(sock_fd + 1, &rfds, NULL, NULL, &tv);
        long t_now = now_ms();
        long elapsed = t_now - t_prev;

        if (elapsed < 0)
            elapsed = 0;

        t_prev = t_now;

        if ((sel > 0) && FD_ISSET(sock_fd, &rfds))
        {
            int client = accept(sock_fd, NULL, NULL);

            if (client >= 0)
            {
                char line[SOCKET_READ_BUF];
                ssize_t n = read_client_command(client, line, sizeof(line));

                if (n > 0)
                {
                    line[n] = '\0';

                    parsed_cmd_t cmd;

                    parse_command(line, &cmd);

                    INFO("subsystem=socket cmd=%s header=%s", cmd.verb, cmd.header);

                    if (strcmp(cmd.verb, "PING") == 0)
                        write(client, "OK\n", 3);
                    else
                    {
                        dispatch_command(&disp, &cmd, screens, screen_count);

                        t_prev = now_ms();
                        elapsed = 0;
                    }
                }

                close(client);
            }
        }

        tick_state(&disp, elapsed, screens, screen_count);
    }

    socket_destroy(sock_fd, sock_path);
    app_cleanup(&app);

    return 0;
}
