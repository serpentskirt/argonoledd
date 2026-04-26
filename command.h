#ifndef COMMAND_H
#define COMMAND_H

#include "defines.h"
#include "screen.h"

typedef enum
{
    DISP_IDLE,
    DISP_STATUS,
    DISP_MESSAGE,
} disp_state_t;

typedef struct
{
    char details[CMD_MAX_DETAILS][CMD_DETAIL_WIDTH];
    char header[CMD_HEADER_BUF_SIZE];
    long remaining_ms;
    disp_state_t state;
    int status_idx;
    int status_screens_shown;
    int n_details;
} disp_ctx_t;

typedef struct
{
    char details[CMD_MAX_DETAILS][CMD_DETAIL_WIDTH];
    char header[CMD_HEADER_BUF_SIZE];
    char verb[CMD_VERB_BUF_SIZE];
    int n_details;
} parsed_cmd_t;

long state_timeout_ms(const disp_ctx_t* disp);
void disp_init(disp_ctx_t* disp, const screen_t* screens, int screen_count);
void dispatch_command(disp_ctx_t* disp, const parsed_cmd_t* cmd, const screen_t* screens,
                      int screen_count);
void parse_command(const char* line, parsed_cmd_t* out);
void tick_state(disp_ctx_t* disp, long elapsed, const screen_t* screens, int screen_count);

#endif
