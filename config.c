#include "config.h"
#include "defines.h"
#include "log.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    char val[256];
    char key[128];
} entry_t;

static entry_t entries[CONFIG_MAX_ENTRIES];
static int n_entries = 0;

static void rtrim(char* s)
{
    int len = (int)strlen(s);

    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

const char* config_get(const char* section, const char* key, const char* fallback)
{
    char lookup[128];

    snprintf(lookup, sizeof(lookup), "%s.%s", section, key);

    for (char* c = lookup; *c; ++c)
        *c = (char)tolower((unsigned char)*c);

    for (int i = 0; i < n_entries; ++i)
        if (strcmp(entries[i].key, lookup) == 0)
            return entries[i].val;

    return fallback;
}

void config_load(const char* path)
{
    n_entries = 0;

    FILE* f = fopen(path, "r");

    if (!f)
    {
        WARN("subsystem=config action=load status=fail path=%s", path);

        return;
    }

    char section[64] = "";
    char line[512];

    while (fgets(line, sizeof(line), f))
    {
        char* p = line;

        while (isspace((unsigned char)*p))
            ++p;

        if ((*p == '#') || (*p == ';') || (*p == '\0'))
            continue;

        if (*p == '[')
        {
            char* end = strchr(p, ']');

            if (!end)
                continue;

            *end = '\0';

            snprintf(section, sizeof(section), "%s", p + 1);

            for (char* c = section; *c; ++c)
                *c = (char)tolower((unsigned char)*c);

            continue;
        }

        char* eq = strchr(p, '=');

        if (!eq)
            continue;

        *eq = '\0';

        char* key = p;
        char* val = eq + 1;

        rtrim(key);

        while (isspace((unsigned char)*val))
            ++val;

        rtrim(val);

        int vlen = (int)strlen(val);

        if ((vlen >= 2 && val[0] == '"') && (val[vlen - 1] == '"'))
        {
            val[vlen - 1] = '\0';
            ++val;
        }

        for (char* c = key; *c; ++c)
            *c = (char)tolower((unsigned char)*c);

        if (n_entries < CONFIG_MAX_ENTRIES)
        {
            snprintf(entries[n_entries].key, sizeof(entries[n_entries].key), "%.63s.%.63s", section,
                     key);
            snprintf(entries[n_entries].val, sizeof(entries[n_entries].val), "%s", val);

            ++n_entries;
        }
    }

    fclose(f);
}
