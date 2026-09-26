/* sync_pipe.h
 * Low-level synchronization pipe routines for use by Wireshark/TShark
 * and dumpcap
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


/** @file
 *
 *  Low-level sync pipe interfaces.
 */

#ifndef __SYNC_PIPE_H__
#define __SYNC_PIPE_H__

#include <ws_posix_compat.h>

#define SP_MAX_MSG_LEN  (512 * 1000)

#define SP_EXEC_FAILED  'X'     /* errno value for the exec failing */
#define SP_FILE         'F'     /* the name of the recently opened file */
#define SP_ERROR_MSG    'E'     /* error message */
#define SP_LOG_MSG      'L'     /* log message */
#define SP_BAD_FILTER   'B'     /* error message for bad capture filter */
#define SP_PACKET_COUNT 'P'     /* count of packets captured since last message */
#define SP_DROPS        'D'     /* count of packets dropped in capture */
#define SP_SUCCESS      'S'     /* success indication, no extra data */
#define SP_TOOLBAR_CTRL 'T'     /* interface toolbar control packet */
#define SP_IFACE_LIST   'I'     /* interface list */
/*
 * Win32 only: Indications sent out on the signal pipe (from parent to child)
 * (UNIX-like sends signals for this)
 */
#define SP_QUIT         'Q'     /* "gracefully" capture quit message (SIGUSR1) */

/* Write a message, with a string body, to the recipient pipe in the
   standard format (1-byte message indicator, 3-byte message length
   (excluding length and indicator field), and the string.
   If msg is NULL, the message has only a length and indicator. */
extern void
sync_pipe_write_string_msg(int pipe_fd, char indicator, const char *msg);

/* Write a message, with an unsigned integer body, to the recipient
   pipe in the standard format (1-byte message indicator, 3-byte
   message length (excluding length and indicator field), and the
   unsigned integer, as a string. */
extern void
sync_pipe_write_uint_msg(int pipe_fd, char indicator, unsigned int num);

/* Write a message, with an integer body, to the recipient pipe in the
   standard format (1-byte message indicator, 3-byte message length
   (excluding length and indicator field), and the integer, as a string. */
extern void
sync_pipe_write_int_msg(int pipe_fd, char indicator, int num);

/** the child encountered an error, notify the parent */
extern void
sync_pipe_write_errmsgs_to_parent(int pipe_fd, const char *error_msg,
                                  const char *secondary_error_msg);

/** Has the parent signalled the child to stop? */
#define SIGNAL_PIPE_CTRL_ID_NONE "none"

#endif /* sync_pipe.h */

