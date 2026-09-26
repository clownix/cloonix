/* recent.c
 * Recent "preference" handling routines
 * Copyright 2004, Ulf Lamping <ulf.lamping@web.de>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <wireshark.h>

#include <stdlib.h>
#include <errno.h>

#include <epan/prefs.h>
#include <epan/prefs-int.h>
#include <epan/column.h>
#include <epan/value_string.h>

#include "ui/util.h"
#include "ui/recent.h"
#include "ui/recent_utils.h"
#include "ui/packet_list_utils.h"
#include "ui/simple_dialog.h"

#include <wsutil/file_util.h>
#include <wsutil/strtoi.h>

#define RECENT_KEY_MAIN_TOOLBAR_SHOW            "gui.toolbar_main_show"
#define RECENT_KEY_FILTER_TOOLBAR_SHOW          "gui.filter_toolbar_show"
#define RECENT_KEY_PACKET_LIST_SHOW             "gui.packet_list_show"
#define RECENT_KEY_TREE_VIEW_SHOW               "gui.tree_view_show"
#define RECENT_KEY_BYTE_VIEW_SHOW               "gui.byte_view_show"
#define RECENT_KEY_STATUSBAR_SHOW               "gui.statusbar_show"
#define RECENT_KEY_PACKET_LIST_COLORIZE         "gui.packet_list_colorize"
#define RECENT_KEY_CAPTURE_AUTO_SCROLL          "capture.auto_scroll"
#define RECENT_GUI_TIME_FORMAT                  "gui.time_format"
#define RECENT_GUI_TIME_PRECISION               "gui.time_precision"
#define RECENT_GUI_SECONDS_FORMAT               "gui.seconds_format"
#define RECENT_GUI_ZOOM_LEVEL                   "gui.zoom_level"
#define RECENT_GUI_BYTES_VIEW                   "gui.bytes_view"
#define RECENT_GUI_BYTES_ENCODING               "gui.bytes_encoding"
#define RECENT_GUI_ALLOW_HOVER_SELECTION        "gui.allow_hover_selection"
#define RECENT_GUI_GEOMETRY_MAIN_X              "gui.geometry_main_x"
#define RECENT_GUI_GEOMETRY_MAIN_Y              "gui.geometry_main_y"
#define RECENT_GUI_GEOMETRY_MAIN_WIDTH          "gui.geometry_main_width"
#define RECENT_GUI_GEOMETRY_MAIN_HEIGHT         "gui.geometry_main_height"
#define RECENT_GUI_GEOMETRY_MAIN_MAXIMIZED      "gui.geometry_main_maximized"
#define RECENT_GUI_GEOMETRY_MAIN                "gui.geometry_main"
#define RECENT_GUI_GEOMETRY_LEFTALIGN_ACTIONS   "gui.geometry_leftalign_actions"
#define RECENT_GUI_GEOMETRY_MAIN_UPPER_PANE     "gui.geometry_main_upper_pane"
#define RECENT_GUI_GEOMETRY_MAIN_LOWER_PANE     "gui.geometry_main_lower_pane"
#define RECENT_GUI_GEOMETRY_MAIN_MASTER_SPLIT   "gui.geometry_main_master_split"
#define RECENT_GUI_GEOMETRY_MAIN_EXTRA_SPLIT    "gui.geometry_main_extra_split"
#define RECENT_LAST_USED_PROFILE                "gui.last_used_profile"
#define RECENT_PROFILE_SWITCH_CHECK_COUNT       "gui.profile_switch_check_count"
#define RECENT_GUI_FILEOPEN_REMEMBERED_DIR      "gui.fileopen_remembered_dir"
#define RECENT_GUI_CONVERSATION_TABS            "gui.conversation_tabs"
#define RECENT_GUI_CONVERSATION_TABS_COLUMNS    "gui.conversation_tabs_columns"
#define RECENT_GUI_ENDPOINT_TABS                "gui.endpoint_tabs"
#define RECENT_GUI_ENDPOINT_TABS_COLUMNS        "gui.endpoint_tabs_columns"
#define RECENT_GUI_RLC_PDUS_FROM_MAC_FRAMES     "gui.rlc_pdus_from_mac_frames"
#define RECENT_GUI_CUSTOM_COLORS                "gui.custom_colors"
#define RECENT_GUI_INTERFACE_TOOLBAR_SHOW       "gui.interface_toolbar_show"
#define RECENT_GUI_SEARCH_IN                    "gui.search_in"
#define RECENT_GUI_SEARCH_CHAR_SET              "gui.search_char_set"
#define RECENT_GUI_SEARCH_CASE_SENSITIVE        "gui.search_case_sensitive"
#define RECENT_GUI_SEARCH_REVERSE_DIR           "gui.search_reverse_dir"
#define RECENT_GUI_SEARCH_MULTIPLE_OCCURS       "gui.search_multiple_occurs"
#define RECENT_GUI_SEARCH_TYPE                  "gui.search_type"
#define RECENT_GUI_FOLLOW_SHOW                  "gui.follow_show"
#define RECENT_GUI_FOLLOW_DELTA                 "gui.follow_delta"
#define RECENT_GUI_SHOW_BYTES_DECODE            "gui.show_bytes_decode"
#define RECENT_GUI_SHOW_BYTES_SHOW              "gui.show_bytes_show"

#define RECENT_GUI_GEOMETRY                   "gui.geom."

#define RECENT_KEY_PRIVS_WARN_IF_ELEVATED     "privs.warn_if_elevated"
#define RECENT_KEY_SYS_WARN_IF_NO_CAPTURE     "sys.warn_if_no_capture"

#define RECENT_FILE_NAME "recent"
#define RECENT_COMMON_FILE_NAME "recent_common"

recent_settings_t recent;

static const value_string ts_type_values[] = {
    { TS_RELATIVE,             "RELATIVE"           },
    { TS_ABSOLUTE,             "ABSOLUTE"           },
    { TS_ABSOLUTE_WITH_YMD,    "ABSOLUTE_WITH_YMD"  },
    { TS_ABSOLUTE_WITH_YDOY,   "ABSOLUTE_WITH_YDOY" },
    { TS_ABSOLUTE_WITH_YMD,    "ABSOLUTE_WITH_DATE" },
    { TS_DELTA,                "DELTA"              },
    { TS_DELTA_DIS,            "DELTA_DIS"          },
    { TS_EPOCH,                "EPOCH"              },
    { TS_UTC,                  "UTC"                },
    { TS_UTC_WITH_YMD,         "UTC_WITH_YMD"       },
    { TS_UTC_WITH_YDOY,        "UTC_WITH_YDOY"      },
    { TS_UTC_WITH_YMD,         "UTC_WITH_DATE"      },
    { 0, NULL }
};

static const value_string ts_precision_values[] = {
    { TS_PREC_AUTO,            "AUTO" },
    { TS_PREC_FIXED_SEC,       "SEC"  },
    { TS_PREC_FIXED_100_MSEC,  "DSEC" },
    { TS_PREC_FIXED_10_MSEC,   "CSEC" },
    { TS_PREC_FIXED_MSEC,      "MSEC" },
    { TS_PREC_FIXED_USEC,      "USEC" },
    { TS_PREC_FIXED_NSEC,      "NSEC" },
    { 0, NULL }
};

static const value_string ts_seconds_values[] = {
    { TS_SECONDS_DEFAULT,      "SECONDS"      },
    { TS_SECONDS_HOUR_MIN_SEC, "HOUR_MIN_SEC" },
    { 0, NULL }
};

static const value_string bytes_view_type_values[] = {
    { BYTES_HEX,    "HEX"  },
    { BYTES_BITS,   "BITS" },
    { BYTES_DEC,    "DEC" },
    { BYTES_OCT,    "OCT" },
    { 0, NULL }
};

static const value_string bytes_encoding_type_values[] = {
    { BYTES_ENC_FROM_PACKET,    "FROM_PACKET"  },
    { BYTES_ENC_ASCII,          "ASCII"  },
    { BYTES_ENC_EBCDIC,         "EBCDIC"  },
    { 0, NULL }
};

static const value_string search_in_values[] = {
    { SEARCH_IN_PACKET_LIST,    "PACKET_LIST" },
    { SEARCH_IN_PACKET_DETAILS, "PACKET_DETAILS" },
    { SEARCH_IN_PACKET_BYTES,   "PACKET_BYTES" },
    { 0, NULL }
};

static const value_string search_char_set_values[] = {
    { SEARCH_CHAR_SET_NARROW_AND_WIDE, "NARROW_AND_WIDE" },
    { SEARCH_CHAR_SET_NARROW,          "NARROW" },
    { SEARCH_CHAR_SET_WIDE,            "WIDE" },
    { 0, NULL }
};

static const value_string search_type_values[] = {
    { SEARCH_TYPE_DISPLAY_FILTER, "DISPLAY_FILTER" },
    { SEARCH_TYPE_HEX_VALUE,      "HEX_VALUE" },
    { SEARCH_TYPE_STRING,         "STRING" },
    { SEARCH_TYPE_REGEX,          "REGEX" },
    { 0, NULL }
};

static const value_string bytes_show_values[] = {
    { SHOW_ASCII,         "ASCII" },
    { SHOW_ASCII_CONTROL, "ASCII_CONTROL" },
    { SHOW_CARRAY,        "C_ARRAYS" },
    { SHOW_EBCDIC,        "EBCDIC" },
    { SHOW_HEXDUMP,       "HEX_DUMP" },
    { SHOW_HTML,          "HTML" },
    { SHOW_IMAGE,         "IMAGE" },
    { SHOW_JSON,          "JSON" },
    { SHOW_RAW,           "RAW" },
    { SHOW_RUSTARRAY,     "RUST_ARRAY" },
    { SHOW_CODEC,         "UTF-8" },
    // Other codecs are generated at runtime
    { SHOW_YAML,          "YAML"},
    { 0, NULL }
};

static const value_string follow_delta_values[] = {
    { FOLLOW_DELTA_NONE,    "NONE" },
    { FOLLOW_DELTA_TURN,    "TURN" },
    { FOLLOW_DELTA_ALL,     "ALL" },
    { 0, NULL }
};

static const value_string show_bytes_decode_values[] = {
    { DecodeAsNone,            "NONE" },
    { DecodeAsBASE64,          "BASE64" },
    { DecodeAsCompressed,      "COMPRESSED" },
    { DecodeAsHexDigits,       "HEX_DIGITS" },
    { DecodeAsPercentEncoding, "PERCENT_ENCODING" },
    { DecodeAsQuotedPrintable, "QUOTED_PRINTABLE" },
    { DecodeAsROT13,           "ROT13"},
    { 0, NULL }
};

static void
free_col_width_data(void *data)
{
    col_width_data *cfmt = (col_width_data *)data;
    g_free(cfmt);
}

void
recent_free_column_width_info(recent_settings_t *rs)
{
    g_list_free_full(rs->col_width_list, free_col_width_data);
    rs->col_width_list = NULL;
}


static GHashTable *window_geom_hash;

static GHashTable *window_splitter_hash;

void
window_geom_free(void *data)
{
    window_geometry_t *geom = (window_geometry_t*)data;
    g_free(geom->key);
    g_free(geom->qt_geom);
    g_free(geom);
}

void
window_geom_save(const char *name, window_geometry_t *geom)
{
    char *key;
    window_geometry_t *work;

    if (!window_geom_hash) {
        window_geom_hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, window_geom_free);
    }

    work = g_new(window_geometry_t, 1);
    *work = *geom;
    key = g_strdup(name);
    work->key = key;
    g_hash_table_replace(window_geom_hash, key, work);
}

bool
window_geom_load(const char        *name,
                 window_geometry_t *geom)
{
    window_geometry_t *p;

    if (!window_geom_hash) {
        window_geom_hash = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, window_geom_free);
    }

    p = (window_geometry_t *)g_hash_table_lookup(window_geom_hash, name);
    if (p) {
        *geom = *p;
        return true;
    } else {
        return false;
    }
}

const char*
window_splitter_load(const char *name)
{
    if (!window_splitter_hash) {
        return NULL;
    }

    return g_hash_table_lookup(window_splitter_hash, name);
}


static void
parse_recent_boolean(const char *val_str, bool *valuep)
{
    if (g_ascii_strcasecmp(val_str, "true") == 0) {
        *valuep = true;
    }
    else {
        *valuep = false;
    }
}


static GList *recent_cfilter_list;

static GHashTable *per_interface_cfilter_lists_hash;

static unsigned cfilter_combo_max_recent = 20;

GList *
recent_get_cfilter_list(const char *ifname)
{
    if (ifname == NULL)
        return recent_cfilter_list;
    if (per_interface_cfilter_lists_hash == NULL) {
        /* No such lists exist. */
        return NULL;
    }
    return (GList *)g_hash_table_lookup(per_interface_cfilter_lists_hash, ifname);
}


bool
write_recent(void)
{
        return false;
}


/* Attempt to Write out profile "recent" to the user's profile recent file.
   If we got an error report it with a dialog box and return false,
   otherwise return true. */
bool
write_profile_recent(void)
{
    return false;
}




void
recent_read_static(char **rf_path_return, int *rf_errno_return)
{
}



void recent_read_profile_static(char **rf_path_return, int *rf_errno_return)
{
   recent.main_toolbar_show         = false;
   recent.filter_toolbar_show       = true;
   recent.packet_list_show          = true;
   recent.tree_view_show            = true;
   recent.byte_view_show            = true;
   recent.statusbar_show            = true;
   recent.packet_list_colorize      = true;
   recent.capture_auto_scroll       = true;
   recent.gui_time_format           = TS_RELATIVE;
   recent.gui_time_precision        = TS_PREC_AUTO;
   recent.gui_seconds_format        = TS_SECONDS_DEFAULT;
   recent.gui_zoom_level            = 0;
   recent.gui_bytes_view            = BYTES_HEX;
   recent.gui_bytes_encoding        = BYTES_ENC_FROM_PACKET;
   recent.gui_allow_hover_selection = true;
   recent.gui_follow_show           = SHOW_ASCII;
   recent.gui_follow_delta          = FOLLOW_DELTA_NONE;
   recent.gui_show_bytes_decode     = DecodeAsNone;
   recent.gui_show_bytes_show       = SHOW_ASCII;
}

void recent_read_dynamic(char **rf_path_return, int *rf_errno_return)
{
}

void
recent_insert_column(int col)
{
    col_width_data *col_w;

    col_w = g_new(col_width_data, 1);
    col_w->width = -1;
    col_w->xalign = COLUMN_XALIGN_DEFAULT;
    recent.col_width_list = g_list_insert(recent.col_width_list, col_w, col);
}

void
recent_remove_column(int col)
{
    GList *col_l = g_list_nth(recent.col_width_list, col);
    col_width_data *col_w;

    if (!col_l) return;

    col_w = (col_width_data*)col_l->data;

    if (col_w) {
        free_col_width_data(col_w);
    }

    recent.col_width_list = g_list_delete_link(recent.col_width_list, col_l);
}

int
recent_get_column_width(int col)
{
    col_width_data *col_w;

    col_w = g_list_nth_data(recent.col_width_list, col);
    if (col_w) {
        return col_w->width;
    } else {
        /* Make sure the recent column list isn't out of sync with the
         * number of columns (e.g., for a brand new profile.)
         */
        for (unsigned colnr = g_list_length(recent.col_width_list); colnr < g_list_length(prefs.col_list); colnr++) {
            recent_insert_column(colnr);
        }
    }

    return -1;
}

void
recent_set_column_width(int col, int width)
{
    col_width_data *col_w;

    col_w = g_list_nth_data(recent.col_width_list, col);
    if (col_w) {
        col_w->width = width;
    } else {
        /* Make sure the recent column list isn't out of sync with the
         * number of columns (e.g., for a brand new profile.)
         */
        for (unsigned colnr = g_list_length(recent.col_width_list); colnr < g_list_length(prefs.col_list); colnr++) {
            recent_insert_column(colnr);
        }
        col_w = g_list_nth_data(recent.col_width_list, col);
        if (col_w) {
            col_w->width = width;
        }
    }
}

char
recent_get_column_xalign(int col)
{
    col_width_data *col_w;

    col_w = g_list_nth_data(recent.col_width_list, col);
    if (col_w) {
        return col_w->xalign;
    } else {
        /* Make sure the recent column list isn't out of sync with the
         * number of columns (e.g., for a brand new profile.)
         */
        for (unsigned colnr = g_list_length(recent.col_width_list); colnr < g_list_length(prefs.col_list); colnr++) {
            recent_insert_column(colnr);
        }
    }

    return COLUMN_XALIGN_DEFAULT;
}

void
recent_set_column_xalign(int col, char xalign)
{
    col_width_data *col_w;

    col_w = g_list_nth_data(recent.col_width_list, col);
    if (col_w) {
        col_w->xalign = xalign;
    } else {
        /* Make sure the recent column list isn't out of sync with the
         * number of columns (e.g., for a brand new profile.)
         */
        for (unsigned colnr = g_list_length(recent.col_width_list); colnr < g_list_length(prefs.col_list); colnr++) {
            recent_insert_column(colnr);
        }
        col_w = g_list_nth_data(recent.col_width_list, col);
        if (col_w) {
            col_w->xalign = xalign;
        }
    }
}

void
recent_init(void)
{
    memset(&recent, 0, sizeof(recent_settings_t));
}

void
recent_cleanup(void)
{
    recent_free_column_width_info(&recent);
    g_free(recent.gui_geometry_main);
    g_free(recent.gui_geometry_main_master_split);
    g_free(recent.gui_geometry_main_extra_split);
    g_free(recent.gui_fileopen_remembered_dir);
    g_list_free_full(recent.interface_toolbars, g_free);
    prefs_clear_string_list(recent.conversation_tabs);
    prefs_clear_string_list(recent.conversation_tabs_columns);
    prefs_clear_string_list(recent.endpoint_tabs);
    prefs_clear_string_list(recent.endpoint_tabs_columns);
    prefs_clear_string_list(recent.custom_colors);
}
