/** @file
 * Declarations of routines for gathering and handling lists of
 * present/absent features (usually actually dependencies)
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __WSUTIL_FEATURE_LIST_H__
#define __WSUTIL_FEATURE_LIST_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef GList **feature_list;
typedef void(*gather_feature_func)(feature_list l);
void with_feature(feature_list l, const char *fmt, ...) G_GNUC_PRINTF(2,3);
void without_feature(feature_list l, const char *fmt, ...) G_GNUC_PRINTF(2,3);
void sort_features(feature_list l);
void free_features(feature_list l);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WSUTIL_FEATURE_LIST_H__ */
