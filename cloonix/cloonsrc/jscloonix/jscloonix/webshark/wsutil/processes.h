/** @file
 *
 * Process utility definitions
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _WSUTIL_PROCESSES_H_
#define _WSUTIL_PROCESSES_H_


#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef pid_t ws_process_id;

#define WS_INVALID_PID    -1

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _WSUTIL_PROCESSES_H_ */
