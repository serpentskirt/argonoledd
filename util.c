#include "util.h"
#include "app.h"
#include "log.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int load_file(const char* path, uint8_t** data, size_t* size)
{
    FILE* f = fopen(path, "rb");

    if (!f)
    {
        ERROR("load_file fopen: %s", strerror(errno));

        return -1;
    }

    fseek(f, 0, SEEK_END);

    long sz = ftell(f);

    rewind(f);

    if (sz <= 0)
    {
        fclose(f);

        return -1;
    }

    uint8_t* buf = malloc(sz);

    if (!buf)
    {
        fclose(f);

        return -1;
    }

    if (fread(buf, 1, sz, f) != (size_t)sz)
    {
        ERROR("load_file fread failed: %s", strerror(errno));

        free(buf);
        fclose(f);

        return -1;
    }

    fclose(f);

    *data = buf;
    *size = sz;

    return 0;
}

int wait_ms(long ms)
{
    struct timespec req, rem;
    volatile sig_atomic_t* stop = app_shutdown_flag();

    while ((ms > 0) && !*stop)
    {
        req.tv_sec = ms / MS_PER_SEC;
        req.tv_nsec = (ms % MS_PER_SEC) * NS_PER_MS;

        if (nanosleep(&req, &rem) == 0)
            return 0;

        if (*stop)
            return 1;

        ms = rem.tv_sec * MS_PER_SEC + rem.tv_nsec / NS_PER_MS;
    }

    return *stop ? 1 : 0;
}
