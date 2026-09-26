/* commandline.c
 * Common command line handling between GUIs
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include <glib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ws_exit_codes.h>
#include <wsutil/ws_getopt.h>
#include <wsutil/version_info.h>
#include <wsutil/clopts_common.h>
#include <wsutil/cmdarg_err.h>
#include <wsutil/ws_assert.h>
#include <epan/ex-opt.h>
#include <epan/packet.h>
#include <epan/proto.h>
#include <epan/prefs.h>
#include <epan/prefs-int.h>
#include <epan/stat_tap_ui.h>
#include "capture_opts.h"
#include "recent.h"
#include "../file.h"
#include "ui/dissect_opts.h"
#include "ui/commandline.h"

commandline_param_info_t global_commandline_info;

capture_options global_capture_opts;


#define LONGOPT_FULL_SCREEN     LONGOPT_BASE_GUI+1
#define LONGOPT_CAPTURE_COMMENT LONGOPT_BASE_GUI+2


void commandline_other_options(int argc, char *argv[], bool opt_reset)
{
    int i, opt;
    int status;
    if (opt_reset) {
        ws_optreset = 1;
        ws_optind = 1;
        ws_opterr = 1;
    }
    global_commandline_info.jump_backwards = SD_FORWARD;
    global_commandline_info.go_to_packet = 0;
    global_commandline_info.jfilter = NULL;
    global_commandline_info.cf_name = NULL;
    global_commandline_info.rfilter = NULL;
    global_commandline_info.dfilter = NULL;
    global_commandline_info.start_capture = true;
    global_commandline_info.list_link_layer_types = false;
    global_commandline_info.list_timestamp_types = false;
    global_commandline_info.quit_after_cap = false;
    global_commandline_info.full_screen = false;

    while ((opt = ws_getopt_long(argc, argv, "i:t:", NULL, NULL)) != -1) {
        switch (opt) {
            case 't':
            case 'i':
                status=capture_opts_add_opt(&global_capture_opts,opt,ws_optarg);
                if(status != 0) {
                    exit_application(status);
                }
                break;
            }
    }
}

static int cl_find_custom(const void *elem_data, const void *search_data) {
    return memcmp(elem_data, search_data, strlen((char *)search_data));
}

void commandline_options_drop(const char *module_name, const char *pref_name) {
    GSList *elem;
    char *opt_prefix;

    if (global_commandline_info.user_opts == NULL) return;

    opt_prefix = ws_strdup_printf("%s.%s:", module_name, pref_name);

    while (NULL != (elem = g_slist_find_custom(global_commandline_info.user_opts,
                        (const void *)opt_prefix, cl_find_custom))) {
        global_commandline_info.user_opts =
                g_slist_remove_link(global_commandline_info.user_opts, elem);
        g_free(elem->data);
        g_slist_free_1(elem);
    }
    g_free(opt_prefix);
}

void commandline_options_reapply(void) {
    char *errmsg = NULL;
    GSList *entry = NULL;

    for (entry = global_commandline_info.user_opts; entry != NULL; entry = g_slist_next(entry)) {
        prefs_set_pref((char *)entry->data, &errmsg);
        if (errmsg != NULL) {
            g_free(errmsg);
            errmsg = NULL;
        }
    }
}

void commandline_options_apply_extcap(void) {
    char *errmsg = NULL;
    GSList *entry = NULL;
    char *pref_arg;

    if (prefs.capture_no_extcap)
        return;

    for (entry = global_commandline_info.user_opts; entry != NULL; entry = g_slist_next(entry)) {
        pref_arg = (char *)entry->data;
        if (g_str_has_prefix(pref_arg, "extcap.")) {
            switch (prefs_set_pref(pref_arg, &errmsg)) {
                case PREFS_SET_OK:
                    break;
                case PREFS_SET_SYNTAX_ERR:
                    cmdarg_err("Invalid -o flag \"%s\"%s%s", pref_arg,
                            errmsg ? ": " : "", errmsg ? errmsg : "");
                    g_free(errmsg);
                    exit_application(1);
                    break;
                case PREFS_SET_NO_SUCH_PREF:
                    exit_application(1);
                    break;
                case PREFS_SET_OBSOLETE:
                    exit_application(1);
                    break;
                default:
                    ws_assert_not_reached();
            }
        }
    }
}

/* Free memory used to hold user-specified command line options */
void commandline_options_free(void) {
    g_slist_free_full(g_steal_pointer(&global_commandline_info.user_opts), g_free);
}
