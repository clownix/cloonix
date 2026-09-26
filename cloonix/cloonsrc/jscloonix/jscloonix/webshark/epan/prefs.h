/** @file prefs.h
 * Definitions for preference handling routines
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __PREFS_H__
#define __PREFS_H__

#include <glib.h>

#include <epan/params.h>
#include <epan/range.h>

#include <wsutil/color.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define DEF_WIDTH 750
#define DEF_HEIGHT 550

#define MAX_VAL_LEN  1024

#define TAP_UPDATE_DEFAULT_INTERVAL 3000
#define ST_DEF_BURSTRES 5
#define ST_DEF_BURSTLEN 100
#define ST_MAX_BURSTRES 600000 /* somewhat arbitrary limit of 10 minutes */
#define ST_MAX_BURSTBUCKETS 100 /* somewhat arbitrary limit - more buckets degrade performance */
#define DEF_GUI_DECIMAL_PLACES1 2
#define DEF_GUI_DECIMAL_PLACES2 4
#define DEF_GUI_DECIMAL_PLACES3 6

#define CONV_DEINT_KEY_CAPFILE    0x01 /* unused yet */
#define CONV_DEINT_KEY_INTERFACE  0x02
#define CONV_DEINT_KEY_MAC        0x04
#define CONV_DEINT_KEY_VLAN       0x08

struct epan_uat;
#define FO_STYLE_LAST_OPENED    0 /* start in last directory we looked at */
#define FO_STYLE_SPECIFIED      1 /* start in specified directory */
#define FO_STYLE_CWD            2 /* start in current working directory at startup */
#define TB_STYLE_ICONS          0
#define TB_STYLE_TEXT           1
#define TB_STYLE_BOTH           2
#define COLOR_STYLE_DEFAULT     0
#define COLOR_STYLE_FLAT        1
#define COLOR_STYLE_GRADIENT    2
#define COLOR_STYLE_ALPHA       0.25

typedef enum {
    layout_unused,  /* entry currently unused */
    layout_type_5,
    layout_type_2,
    layout_type_1,
    layout_type_4,
    layout_type_3,
    layout_type_6,
    layout_type_max
} layout_type_e;

typedef enum {
    layout_pane_content_none,
    layout_pane_content_plist,
    layout_pane_content_pdetails,
    layout_pane_content_pbytes,
    layout_pane_content_pdiagram,
} layout_pane_content_e;

typedef enum {
    version_welcome_only,
    version_title_only,
    version_both,
    version_neither
} version_info_e;

typedef enum {
    layout_vertical,
    layout_horizontal
} splitter_layout_e;

typedef enum {
    pref_default,
    pref_stashed,
    pref_current
} pref_source_t;

typedef enum {
    ELIDE_LEFT,
    ELIDE_RIGHT,
    ELIDE_MIDDLE,
    ELIDE_NONE
} elide_mode_e;

typedef enum {
    COPY_FORMAT_TEXT,
    COPY_FORMAT_CSV,
    COPY_FORMAT_YAML,
    COPY_FORMAT_HTML
} copy_format_e;

typedef enum {
    ABS_TIME_ASCII_NEVER,
    ABS_TIME_ASCII_TREE,
    ABS_TIME_ASCII_COLUMN,
    ABS_TIME_ASCII_ALWAYS,
} abs_time_format_e;

typedef enum {
    UPDATE_CHANNEL_DEVELOPMENT,
    UPDATE_CHANNEL_STABLE
} software_update_channel_e;

typedef struct _e_prefs {
  GList       *col_list;
  int          num_cols;
  color_t      st_client_fg, st_client_bg, st_server_fg, st_server_bg;
  color_t      gui_text_valid, gui_text_invalid, gui_text_deprecated;
  bool         restore_filter_after_following_stream;
  int          gui_toolbar_main_style;
  char        *gui_font_name;
  color_t      gui_active_fg;
  color_t      gui_active_bg;
  int          gui_active_style;
  color_t      gui_inactive_fg;
  color_t      gui_inactive_bg;
  int          gui_inactive_style;
  color_t      gui_marked_fg;
  color_t      gui_marked_bg;
  color_t      gui_ignored_fg;
  color_t      gui_ignored_bg;
  char        *gui_colorized_fg;
  char        *gui_colorized_bg;
  bool         gui_geometry_save_position;
  bool         gui_geometry_save_size;
  bool         gui_geometry_save_maximized;
  unsigned     gui_recent_df_entries_max;
  unsigned     gui_recent_files_count_max;
  unsigned     gui_fileopen_style;
  char        *gui_fileopen_dir;
  unsigned     gui_fileopen_preview;
  char        *gui_tlskeylog_command;
  bool         gui_ask_unsaved;
  bool         gui_autocomplete_filter;
  bool         gui_find_wrap;
  char        *gui_window_title;
  char        *gui_prepend_window_title;
  char        *gui_start_title;
  version_info_e gui_version_placement;
  unsigned     gui_max_tree_items;
  unsigned     gui_max_tree_depth;
  layout_type_e gui_layout_type;
  layout_pane_content_e gui_layout_content_1;
  layout_pane_content_e gui_layout_content_2;
  layout_pane_content_e gui_layout_content_3;
  splitter_layout_e gui_packet_dialog_layout;
  char        *gui_interfaces_hide_types;
  bool         gui_interfaces_show_hidden;
  bool         gui_interfaces_remote_display;
  bool         gui_io_graph_automatic_update;
  bool         gui_io_graph_enable_legend;
  bool         gui_packet_details_show_byteview;
  char        *capture_device;
  char        *capture_devices_linktypes;
  char        *capture_devices_descr;
  char        *capture_devices_hide;
  char        *capture_devices_monitor_mode;
  char        *capture_devices_buffersize;
  char        *capture_devices_snaplen;
  char        *capture_devices_pmode;
  char        *capture_devices_filter; /* XXX - Mostly unused. Deprecate? */
  bool         capture_prom_mode;
  bool         capture_monitor_mode;
  bool         capture_pcap_ng;
  bool         capture_real_time;
  unsigned     capture_update_interval;
  bool         capture_no_extcap;
  bool         capture_show_info;
  GList       *capture_columns;
  unsigned     tap_update_interval;
  bool         display_hidden_proto_items;
  bool         display_byte_fields_with_spaces;
  abs_time_format_e display_abs_time_ascii;
  bool         enable_incomplete_dissectors_check;
  bool         incomplete_dissectors_check_debug;
  bool         strict_conversation_tracking_heuristics;
  int          conversation_deinterlacing_key;
  bool         ignore_dup_frames;
  unsigned     ignore_dup_frames_cache_entries;
  bool         filter_expressions_old;  /* true if old filter expressions preferences were loaded. */
  bool         cols_hide_new; /* true if the new (index-based) gui.column.hide preference was loaded. */
  bool         gui_update_enabled;
  software_update_channel_e gui_update_channel;
  int          gui_update_interval;
  int          gui_debounce_timer;
  char        *saved_at_version;
  bool         unknown_prefs; /* unknown or obsolete pref(s) */
  bool         gui_packet_list_separator;
  bool         gui_packet_header_column_definition;
  bool         gui_packet_list_hover_style; /* Enable/Disable mouse-over colorization */
  bool         gui_show_selected_packet;
  bool         gui_show_file_load_time;
  elide_mode_e gui_packet_list_elide_mode;
  copy_format_e gui_packet_list_copy_format_options_for_keyboard_shortcut;
  bool         gui_packet_list_copy_text_with_aligned_columns;
  bool         gui_packet_list_show_related;
  bool         gui_packet_list_show_minimap;
  bool         gui_packet_list_sortable;
  unsigned     gui_packet_list_cached_rows_max;
  int          gui_decimal_places1; /* Used for type 1 calculations */
  int          gui_decimal_places2; /* Used for type 2 calculations */
  int          gui_decimal_places3; /* Used for type 3 calculations */
  bool         gui_rtp_player_use_disk1;
  bool         gui_rtp_player_use_disk2;
  unsigned     flow_graph_max_export_items;
  bool         st_enable_burstinfo;
  bool         st_burst_showcount;
  int          st_burst_resolution;
  int          st_burst_windowlen;
  bool         st_sort_casesensitve;
  bool         st_sort_rng_fixorder;
  bool         st_sort_rng_nameonly;
  int          st_sort_defcolflag;
  bool         st_sort_defdescending;
  bool         st_sort_showfullname;
  bool         extcap_save_on_start;
} e_prefs;

extern e_prefs prefs;

struct pref_module;
struct pref_custom_cbs;
typedef struct pref_module module_t;
void prefs_init(void);
void prefs_reset(void);
void prefs_cleanup(void);
void prefs_set_gui_theme_is_dark(bool is_dark);
module_t *prefs_register_protocol(int id, void (*apply_cb)(void));
void prefs_register_module_alias(const char *name, module_t *module);
void prefs_deregister_protocol(int id);
module_t *prefs_register_stat(const char *name, const char *title,
    const char *description, void (*apply_cb)(void));
module_t *prefs_register_codec(const char *name, const char *title,
    const char *description, void (*apply_cb)(void));
module_t *prefs_register_protocol_subtree(const char *subtree, int id,
    void (*apply_cb)(void));
module_t *prefs_register_protocol_obsolete(int id);
typedef unsigned (*module_cb)(module_t *module, void *user_data);
bool prefs_module_has_submodules(module_t *module);
unsigned prefs_modules_foreach(module_cb callback, void *user_data);
unsigned prefs_modules_foreach_submodules(module_t *module, module_cb callback, void *user_data);
void prefs_apply_all(void);
void prefs_apply(module_t *module);

struct preference;

typedef struct preference pref_t;

bool prefs_is_registered_protocol(const char *name);
const char *prefs_get_title_by_name(const char *name);
module_t *prefs_find_module(const char *name);
pref_t *prefs_find_preference(module_t * module, const char *pref);
void prefs_register_uint_preference(module_t *module, const char *name,
    const char *title, const char *description, unsigned base, unsigned *var);
void prefs_register_bool_preference(module_t *module, const char *name,
    const char *title, const char *description, bool *var);
void prefs_register_enum_preference(module_t *module, const char *name,
    const char *title, const char *description, int *var,
    const enum_val_t *enumvals, bool radio_buttons);
void prefs_register_string_preference(module_t *module, const char *name,
    const char *title, const char *description, const char **var);
void prefs_register_filename_preference(module_t *module, const char *name,
    const char *title, const char *description, const char **var, bool for_writing);
void prefs_register_directory_preference(module_t *module, const char *name,
    const char *title, const char *description, const char **var);
void prefs_register_range_preference(module_t *module, const char *name,
    const char *title, const char *description, range_t **var,
    uint32_t max_value);
void prefs_register_static_text_preference(module_t *module, const char *name,
    const char *title, const char *description);
void prefs_register_uat_preference(module_t *module,
    const char *name, const char* title, const char *description,  struct epan_uat* uat);
void prefs_register_uat_preference_qt(module_t *module,
    const char *name, const char* title, const char *description,  struct epan_uat* uat);
void prefs_register_color_preference(module_t *module, const char *name,
    const char *title, const char *description, color_t *color);
void prefs_register_custom_preference(module_t *module, const char *name,
    const char *title, const char *description, struct pref_custom_cbs* custom_cbs,
    void** custom_data);
void prefs_register_decode_as_range_preference(module_t *module, const char *name,
    const char *title, const char *description, range_t **var,
    uint32_t max_value, const char *dissector_table, const char *dissector_description);
void prefs_register_password_preference(module_t *module, const char *name,
    const char *title, const char *description, const char **var);
void prefs_register_dissector_preference(module_t *module, const char *name,
    const char *title, const char *description, const char **var);
void prefs_register_obsolete_preference(module_t *module,
    const char *name);
void prefs_register_custom_preference_TCP_Analysis(module_t *module, const char *name,
    const char *title, const char *description, int *var,
    const enum_val_t *enumvals, bool radio_buttons);
void prefs_set_preference_effect_fields(module_t *module,
    const char *name);

typedef unsigned (*pref_cb)(pref_t *pref, void *user_data);

unsigned prefs_pref_foreach(module_t *module, pref_cb callback,
    void *user_data);
GList *prefs_get_string_list(const char *str);
void prefs_clear_string_list(GList *sl);
const char *prefs_pref_type_name(pref_t *pref);
char *prefs_pref_type_description(pref_t *pref);
char *prefs_pref_to_str(pref_t *pref, pref_source_t source);
extern e_prefs *read_prefs(void);
 int write_prefs(char **pf_path_return);
typedef enum {
    PREFS_SET_OK,               /* succeeded */
    PREFS_SET_SYNTAX_ERR,       /* syntax error in string */
    PREFS_SET_NO_SUCH_PREF,     /* no such preference */
    PREFS_SET_OBSOLETE          /* preference used to exist but no longer does */
} prefs_set_pref_e;

prefs_set_pref_e prefs_set_pref(char *prefarg, char **errmsg);
bool prefs_get_preference_obsolete(pref_t *pref);
prefs_set_pref_e prefs_set_preference_obsolete(pref_t *pref);
unsigned prefs_get_uint_value(const char *module_name, const char* pref_name);
range_t* prefs_get_range_value(const char *module_name, const char* pref_name);
bool prefs_is_capture_device_hidden(const char *name);
bool prefs_capture_device_monitor_mode(const char *name);
bool prefs_capture_options_dialog_column_is_visible(const char *column);
bool prefs_has_layout_pane_content (layout_pane_content_e layout_pane_content);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* prefs.h */
