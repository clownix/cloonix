/** @file
 * Declarations of routines for reading and writing protocols file that determine
 * enabling and disabling of protocols.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DISABLED_PROTOS_H
#define DISABLED_PROTOS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

bool enabled_protos_unsaved_changes(void);
bool proto_disable_proto_by_name(const char *name);
bool proto_enable_proto_by_name(const char *name);
bool proto_enable_heuristic_by_name(const char *name);
bool proto_disable_heuristic_by_name(const char *name);
void read_enabled_and_disabled_lists(void);
void save_enabled_and_disabled_lists(void);
extern void
cleanup_enabled_and_disabled_lists(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* DISABLED_PROTOS_H */
