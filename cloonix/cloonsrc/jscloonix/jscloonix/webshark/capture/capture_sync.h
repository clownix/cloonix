/* capture_sync.h
 * Synchronisation between Wireshark capture parent and child instances
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


/** @file
 *
 *  Sync mode capture (internal interface).
 *
 *  Will start a new Wireshark child instance which will do the actual capture
 *  work.
 */

#ifndef __CAPTURE_SYNC_H__
#define __CAPTURE_SYNC_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

struct _info_data;

extern bool
sync_pipe_start(capture_options *capture_opts,
                capture_session *cap_session, struct _info_data* cap_data,
                void(*update_cb)(void));

/** User wants to stop capturing, gracefully close the capture child */
extern void
sync_pipe_stop(capture_session *cap_session);

/** User wants to stop the program, just kill the child as soon as possible */
extern void
sync_pipe_kill(ws_process_id fork_child);

extern int
sync_pipe_gets_nonblock(int pipe_fd, char *bytes, int max);

/* set a callback to be called after fork with the pid of the forked child */
extern void capture_sync_set_fetch_dumpcap_pid_cb(void(*cb)(ws_process_id pid));

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAPTURE_SYNC_H__ */
