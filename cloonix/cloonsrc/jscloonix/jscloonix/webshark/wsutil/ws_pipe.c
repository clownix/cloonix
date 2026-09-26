/* ws_pipe.c
 *
 * Routines for handling pipes.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#define WS_LOG_DOMAIN LOG_DOMAIN_CAPTURE
#include "wsutil/ws_pipe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#if !GLIB_CHECK_VERSION(2, 58, 2)
#ifdef __linux__
#define HAS_G_SPAWN_LINUX_THREAD_SAFETY_BUG
#include <fcntl.h>
#include <sys/syscall.h>        /* for syscall and SYS_getdents64 */
#include <wsutil/file_util.h>   /* for ws_open -> open to pacify checkAPIs.pl */
#endif
#endif


#ifdef HAS_G_SPAWN_LINUX_THREAD_SAFETY_BUG
struct linux_dirent64 {
    uint64_t       d_ino;    /* 64-bit inode number */
    uint64_t       d_off;    /* 64-bit offset to next structure */
    unsigned short d_reclen; /* Size of this dirent */
    unsigned char  d_type;   /* File type */
    char           d_name[]; /* Filename (null-terminated) */
};

/* Async-signal-safe string to integer conversion. */
static int
filename_to_fd(const char *p)
{
    char c;
    int fd = 0;
    const int cutoff = INT_MAX / 10;
    const int cutlim = INT_MAX % 10;

    if (*p == '\0')
        return -1;

    while ((c = *p++) != '\0') {
        if (!g_ascii_isdigit(c))
            return -1;
        c -= '0';

        /* Check for overflow. */
        if (fd > cutoff || (fd == cutoff && c > cutlim))
            return -1;

        fd = fd * 10 + c;
    }

    return fd;
}

static void
close_non_standard_fds_linux(void * user_data )
{
    /*
     * GLib 2.14.2 and newer (up to at least GLib 2.58.1) on Linux with multiple
     * threads can deadlock in the child process due to use of opendir (which
     * is not async-signal-safe). To avoid this, disable the broken code path
     * and manually close file descriptors using async-signal-safe code only.
     * Use CLOEXEC to allow reporting of execve errors to the parent via a pipe.
     * https://gitlab.gnome.org/GNOME/glib/issues/1014
     * https://gitlab.gnome.org/GNOME/glib/merge_requests/490
     */
    int dir_fd = ws_open("/proc/self/fd", O_RDONLY | O_DIRECTORY);
    if (dir_fd >= 0) {
        char buf[4096];
        int nread, fd;
        struct linux_dirent64 *de;

        while ((nread = (int) syscall(SYS_getdents64, dir_fd, buf, sizeof(buf))) > 0) {
            for (int pos = 0; pos < nread; pos += de->d_reclen) {
                de = (struct linux_dirent64 *)(buf + pos);
                fd = filename_to_fd(de->d_name);
                if (fd > STDERR_FILENO && fd != dir_fd) {
                    /* Close all other (valid) file descriptors above stderr. */
                    fcntl(fd, F_SETFD, FD_CLOEXEC);
                }
            }
        }

        close(dir_fd);
    } else {
        /* Slow fallback in case /proc is not mounted */
        for (int fd = STDERR_FILENO + 1; fd < getdtablesize(); fd++) {
            fcntl(fd, F_SETFD, FD_CLOEXEC);
        }
    }
}
#endif

static char **
convert_to_argv(const char *command, int args_count, char *const *args)
{
    char **argv = g_new(char *, args_count + 2);
    argv[0] = g_strdup(command);
    for (int i = 0; i < args_count; i++) {
        if (!*args[i]) {
            ws_warning("Empty argument %d in arguments list", i);
        }
        argv[1 + i] = g_strdup(args[i]);
    }
    argv[args_count + 1] = NULL;
    return argv;
}

static char *
convert_to_command_line(char **argv)
{
    GString *command_line = g_string_sized_new(200);
    for (int i = 0; argv[i]; i++) {
        char *quoted_arg = g_shell_quote(argv[i]);
        if (i != 0) {
            g_string_append_c(command_line, ' ');
        }
        g_string_append(command_line, quoted_arg);
        g_free(quoted_arg);
    }
    return g_string_free(command_line, FALSE);
}

bool ws_pipe_spawn_sync(const char *working_directory, const char *command, int argc, char **args, char **command_output)
{
    bool status = false;
    bool result = false;
    char *local_output = NULL;
    int exit_status = 0;
    char **argv = convert_to_argv(command, argc, args);
    char *command_line = convert_to_command_line(argv);

    ws_debug("command line: %s", command_line);

    uint64_t start_time = g_get_monotonic_time();


    GSpawnFlags flags = (GSpawnFlags)0;
    GSpawnChildSetupFunc child_setup = NULL;
#ifdef HAS_G_SPAWN_LINUX_THREAD_SAFETY_BUG
    flags = (GSpawnFlags)(flags | G_SPAWN_LEAVE_DESCRIPTORS_OPEN);
    child_setup = close_non_standard_fds_linux;
#endif
    status = g_spawn_sync(working_directory, argv, NULL,
                          flags, child_setup, NULL, &local_output, NULL, &exit_status, NULL);

    if (status && exit_status != 0)
        status = false;

    ws_debug("%s finished in %.3fms", argv[0], (g_get_monotonic_time() - start_time) / 1000.0);

    if (status)
    {
        if (local_output != NULL) {
            ws_noisy("spawn output: %s", local_output);
            if (command_output != NULL)
                *command_output = g_strdup(local_output);
        }
        result = true;
    }

    g_free(local_output);
    g_free(command_line);
    g_strfreev(argv);

    return result;
}

void ws_pipe_init(ws_pipe_t *ws_pipe)
{
    if (!ws_pipe) return;
    memset(ws_pipe, 0, sizeof(ws_pipe_t));
    ws_pipe->pid = WS_INVALID_PID;
}

GPid ws_pipe_spawn_async(ws_pipe_t *ws_pipe, GPtrArray *args)
{
    GPid pid = WS_INVALID_PID;
    int stdin_fd, stdout_fd, stderr_fd;
    char **args_array = (char **)args->pdata;
    char **argv = convert_to_argv(args_array[0], args->len - 2, args_array + 1);
    char *command_line = convert_to_command_line(argv);

    ws_debug("command line: %s", command_line);

    GError *error = NULL;
    GSpawnFlags flags = G_SPAWN_DO_NOT_REAP_CHILD;
    GSpawnChildSetupFunc child_setup = NULL;
#ifdef HAS_G_SPAWN_LINUX_THREAD_SAFETY_BUG
    flags = (GSpawnFlags)(flags | G_SPAWN_LEAVE_DESCRIPTORS_OPEN);
    child_setup = close_non_standard_fds_linux;
#endif
    bool spawned = g_spawn_async_with_pipes(NULL, argv, NULL,
                             flags, child_setup, NULL,
                             &pid, &stdin_fd, &stdout_fd, &stderr_fd, &error);
    if (!spawned) {
        ws_debug("Error creating async pipe: %s", error->message);
        g_free(error->message);
    }

    g_free(command_line);
    g_strfreev(argv);

    ws_pipe->pid = pid;

    if (pid != WS_INVALID_PID) {
        ws_pipe->stdin_io = g_io_channel_unix_new(stdin_fd);
        ws_pipe->stdout_io = g_io_channel_unix_new(stdout_fd);
        ws_pipe->stderr_io = g_io_channel_unix_new(stderr_fd);
        g_io_channel_set_encoding(ws_pipe->stdin_io, NULL, NULL);
        g_io_channel_set_encoding(ws_pipe->stdout_io, NULL, NULL);
        g_io_channel_set_encoding(ws_pipe->stderr_io, NULL, NULL);
        g_io_channel_set_buffered(ws_pipe->stdin_io, false);
        g_io_channel_set_buffered(ws_pipe->stdout_io, false);
        g_io_channel_set_buffered(ws_pipe->stderr_io, false);
        g_io_channel_set_close_on_unref(ws_pipe->stdin_io, true);
        g_io_channel_set_close_on_unref(ws_pipe->stdout_io, true);
        g_io_channel_set_close_on_unref(ws_pipe->stderr_io, true);
    }

    return pid;
}


bool
ws_pipe_data_available(int pipe_fd)
{
    fd_set rfds;
    struct timeval timeout;

    FD_ZERO(&rfds);
    FD_SET(pipe_fd, &rfds);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    if (select(pipe_fd + 1, &rfds, NULL, NULL, &timeout) > 0)
    {
        return true;
    }

    return false;
}

