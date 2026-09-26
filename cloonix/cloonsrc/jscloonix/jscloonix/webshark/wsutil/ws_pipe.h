/** @file
 *
 * Routines for handling pipes.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __WS_PIPE_H__
#define __WS_PIPE_H__

#include <stdbool.h>

#include "wsutil/processes.h"

#include <glib.h>

#define ws_pipe_handle			int
#define ws_get_pipe_handle(pipe_fd)	(pipe_fd)

typedef struct _ws_pipe_t {
    GPid pid;
    GIOChannel *stdin_io;
    GIOChannel *stdout_io;
    GIOChannel *stderr_io;
} ws_pipe_t;

 bool ws_pipe_spawn_sync(const char * working_directory, const char * command, int argc, char ** args, char ** command_output);

 void ws_pipe_init(ws_pipe_t *ws_pipe);

static inline bool ws_pipe_valid(ws_pipe_t *ws_pipe)
{
    return ws_pipe && ws_pipe->pid && ws_pipe->pid != WS_INVALID_PID;
}

 GPid ws_pipe_spawn_async (ws_pipe_t * ws_pipe, GPtrArray * args );

 bool ws_pipe_data_available(int pipe_fd);

#endif /* __WS_PIPE_H__ */

