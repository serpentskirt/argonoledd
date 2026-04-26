#include "command.h"
#include "config.h"
#include "log.h"
#include "oled.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int screen_duration(void)
{
    char fallback[16];

    snprintf(fallback, sizeof(fallback), "%d", DEFAULT_DURATION);

    int d = atoi(config_get("oled", "screenduration", fallback));

    return d > 0 ? d : 1;
}

static void enter_idle(disp_ctx_t* disp)
{
    INFO("subsystem=state action=enter_idle");

    oled_power(0);
    oled_clearbuffer(0);

    disp->state = DISP_IDLE;
    disp->remaining_ms = 0;
}

static void advance_status(disp_ctx_t* disp, const screen_t* screens, int screen_count)
{
    if (!screens || (screen_count <= 0))
    {
        enter_idle(disp);

        return;
    }

    disp->status_idx = (disp->status_idx + 1) % screen_count;

    if (disp->status_screens_shown >= screen_count)
    {
        enter_idle(disp);

        return;
    }

    disp->state = DISP_STATUS;
    disp->remaining_ms = (long)screen_duration() * MS_PER_SEC;

    render_screen(screens[disp->status_idx]);

    ++disp->status_screens_shown;
}

static void enter_message(disp_ctx_t* disp, const parsed_cmd_t* cmd)
{
    disp->state = DISP_MESSAGE;
    disp->remaining_ms = (long)screen_duration() * MS_PER_SEC;
    disp->n_details = cmd->n_details;

    snprintf(disp->header, CMD_HEADER_BUF_SIZE, "%s", cmd->header);

    for (int i = 0; i < cmd->n_details; ++i)
        snprintf(disp->details[i], CMD_DETAIL_WIDTH, "%s", cmd->details[i]);

    oled_power(1);

    render_message(disp->header, (const char(*)[CMD_DETAIL_WIDTH])disp->details, disp->n_details);
}

static void enter_status(disp_ctx_t* disp, const screen_t* screens, int screen_count, int reset_idx)
{
    if (!screens || (screen_count <= 0))
    {
        enter_idle(disp);

        return;
    }

    if (reset_idx)
    {
        disp->status_idx = 0;
        disp->status_screens_shown = 0;
    }

    disp->state = DISP_STATUS;
    disp->remaining_ms = (long)screen_duration() * MS_PER_SEC;

    oled_power(1);
    render_screen(screens[disp->status_idx]);

    ++disp->status_screens_shown;
}

long state_timeout_ms(const disp_ctx_t* disp)
{
    switch (disp->state)
    {
        case DISP_IDLE:
            return DISP_IDLE_TIMEOUT_MS;

        case DISP_STATUS:
        case DISP_MESSAGE:
        {
            long t = disp->remaining_ms;

            if (t > DISP_TICK_MAX_MS)
                t = DISP_TICK_MAX_MS;

            if (t < DISP_TICK_MIN_MS)
                t = DISP_TICK_MIN_MS;

            return t;
        }
    }

    return DISP_FALLBACK_TIMEOUT_MS;
}

void disp_init(disp_ctx_t* disp, const screen_t* screens, int screen_count)
{
    memset(disp, 0, sizeof(*disp));
    enter_status(disp, screens, screen_count, 1);
}

void dispatch_command(disp_ctx_t* disp, const parsed_cmd_t* cmd, const screen_t* screens,
                      int screen_count)
{
    if (strcmp(cmd->verb, "MESSAGE") == 0)
        enter_message(disp, cmd);
    else if (strcmp(cmd->verb, "STATUS") == 0)
        enter_status(disp, screens, screen_count, 1);
    else
        INFO("subsystem=socket cmd=%s status=unknown", cmd->verb);
}

void parse_command(const char* line, parsed_cmd_t* out)
{
    memset(out, 0, sizeof(*out));

    char buf[CMD_LINE_MAX];

    snprintf(buf, sizeof(buf), "%s", line);

    int len = (int)strlen(buf);

    while ((len > 0) && ((buf[len - 1] == '\n') || (buf[len - 1] == '\r') || (buf[len - 1] == ' ')))
        buf[--len] = '\0';

    char* saveptr = NULL;
    char* tok = strtok_r(buf, " \t", &saveptr);

    if (!tok)
        return;

    snprintf(out->verb, CMD_VERB_BUF_SIZE, "%s", tok);

    while ((tok = strtok_r(NULL, " \t", &saveptr)) != NULL)
    {
        char* eq = strchr(tok, '=');

        if (!eq)
            continue;

        *eq = '\0';

        const char* key = tok;
        const char* val = eq + 1;

        if (strcmp(key, "header") == 0)
            snprintf(out->header, CMD_HEADER_BUF_SIZE, "%s", val);
        else if (out->n_details < CMD_MAX_DETAILS)
        {
            int klen = (int)strlen(key);
            int vmax = (CMD_DETAIL_WIDTH - 1) - klen - 2; // usable chars minus key minus ": "

            if (vmax > 0)
                snprintf(out->details[out->n_details], CMD_DETAIL_WIDTH, "%s: %.*s", key, vmax,
                         val);
            else
                snprintf(out->details[out->n_details], CMD_DETAIL_WIDTH, "%s", key);

            ++out->n_details;
        }
    }
}

void tick_state(disp_ctx_t* disp, long elapsed, const screen_t* screens, int screen_count)
{
    switch (disp->state)
    {
        case DISP_IDLE:
            break;

        case DISP_STATUS:
        {
            disp->remaining_ms -= elapsed;

            if (disp->remaining_ms > 0)
                break;

            advance_status(disp, screens, screen_count);

            break;
        }

        case DISP_MESSAGE:
        {
            disp->remaining_ms -= elapsed;

            if (disp->remaining_ms > 0)
                break;

            enter_idle(disp);

            break;
        }
    }
}
