/** @file
 *
 * State of a capture session
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __CAPCHILD_CAPTURE_SESSION_H__
#define __CAPCHILD_CAPTURE_SESSION_H__

#include <sys/types.h>
#include <stdint.h>
#include "capture_opts.h"
#include <epan/fifo_string_cache.h>
#include <wsutil/processes.h>
#include "cfile.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    CAPTURE_STOPPED,
    CAPTURE_PREPARING,
    CAPTURE_RUNNING
} capture_state;

struct _info_data;

typedef struct _capture_session capture_session;

typedef bool (*new_file_fn)(capture_session *cap_session, char *new_file);

typedef void (*new_packets_fn)(capture_session *cap_session, int to_read);

typedef void (*drops_fn)(capture_session *cap_session, uint32_t dropped,
                         const char *interface_name);

typedef void (*error_fn)(capture_session *cap_session, char *error_msg,
                         char *secondary_error_msg);

typedef void (*cfilter_error_fn)(capture_session *cap_session, unsigned i,
                                 const char *error_message);

typedef void (*closed_fn)(capture_session *cap_session, char *msg);

struct _capture_session {
    ws_process_id fork_child;             /**< If not WS_INVALID_PID, in parent, process ID of child */
    int       fork_child_status;          /**< Child exit status */
    int       pipe_input_id;              /**< GLib input pipe source ID */
    capture_state state;                  /**< current state of the capture engine */
    uid_t     owner;                      /**< owner of the cfile */
    gid_t     group;                      /**< group of the cfile */
    bool  session_will_restart;       /**< Set when session will restart */
    uint32_t   count;                      /**< Total number of frames captured */
    uint32_t   count_pending;              /**< Number of frames captured but not yet read */
    capture_options *capture_opts;        /**< options for this capture */
    capture_file *cf;                     /**< handle to cfile */
    wtap_rec rec;                         /**< record we're reading packet metadata into */
    Buffer buf;                           /**< Buffer we're reading packet data into */
    struct wtap *wtap;                    /**< current wtap file */
    struct _info_data *cap_data_info;     /**< stats for this capture */
    fifo_string_cache_t frame_dup_cache;
    GChecksum           *frame_cksum;
    new_file_fn new_file;
    new_packets_fn new_packets;
    drops_fn drops;
    error_fn error;
    cfilter_error_fn cfilter_error;
    closed_fn closed;
};

extern void
capture_session_init(capture_session *cap_session, capture_file *cf,
                     new_file_fn new_file, new_packets_fn new_packets,
                     drops_fn drops, error_fn error,
                     cfilter_error_fn cfilter_error, closed_fn closed);

void capture_process_finished(capture_session *cap_session);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CAPCHILD_CAPTURE_SESSION_H__ */
