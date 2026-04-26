#define _DEFAULT_SOURCE

#include "log.h"
#include "defines.h"
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

static int g_foreground = 0;

void log_close(void)
{
    if (!g_foreground)
        closelog();
}

void log_error(const char* fmt, ...)
{
    va_list ap;
    char buf[1024];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (g_foreground)
        fprintf(stderr, "%s\n", buf);
    else
        syslog(LOG_ERR, "%s", buf);
}

void log_info(const char* fmt, ...)
{
    va_list ap;
    char buf[1024];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (g_foreground)
        fprintf(stderr, "%s\n", buf);
    else
        syslog(LOG_INFO, "%s", buf);
}

void log_init(int foreground)
{
    g_foreground = foreground ? 1 : 0;

    if (!g_foreground)
        openlog(LOG_IDENT, LOG_OPTIONS, LOG_FACILITY);
}
