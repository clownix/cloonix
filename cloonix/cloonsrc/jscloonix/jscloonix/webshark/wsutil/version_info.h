/** @file
 *
 * Declarations of routines to report version information for Wireshark
 * programs
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __WS_VERSION_INFO_H__
#define __WS_VERSION_INFO_H__

#include <glib.h>
#include <wsutil/feature_list.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void ws_init_version_info(const char *appname,
		gather_feature_func gather_compile,
		gather_feature_func gather_runtime);

const char *get_appname_and_version(void);


void
gather_pcre2_compile_info(feature_list l);


void
gather_zlib_compile_info(feature_list l);


void
gather_zlib_ng_compile_info(feature_list l);

GString *get_compiled_version_info(gather_feature_func gather_compile);


void
gather_pcre2_runtime_info(feature_list l);


GString *get_runtime_version_info(gather_feature_func gather_runtime);

const char *get_ws_vcs_version_info(void);

const char *get_lr_vcs_version_info(void);

const char *get_ws_vcs_version_info_short(void);

void get_ws_version_number(int *major, int *minor, int *micro);

void show_version(void);

const char *get_copyright_info(void);


const char *get_license_info(void);


const char *get_license_info_short(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __WS_VERSION_INFO_H__ */
