/** @file
 * Definitions for column handling routines
 * Column preference and format settings.
 *
 * For internal Wireshark useonly. Don't include this header in dissectors!
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __COLUMN_H__
#define __COLUMN_H__

#include <epan/column-utils.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _fmt_data {
  char *title;            /* title of the column */
  int fmt;                 /* format of column */
  char *custom_fields;    /* fields names for COL_CUSTOM */
  int custom_occurrence;  /* optional ordinal of occurrence of that field */
  bool visible;            /* if false, hide this column */
  bool resolved;           /* if true, show a more human-readable name */
} fmt_data;

const char          *col_format_to_string(const int);
const char          *col_format_desc(const int);
const char          *col_format_abbrev(const int);
int                  get_column_format(const int);
void                 set_column_format(const int, const int);
void                 get_column_format_matches(bool *, const int);
int                  get_column_format_from_str(const char *);
char                *get_column_title(const int);
void                 set_column_title(const int, const char *);
bool                 get_column_visible(const int);
void                 set_column_visible(const int, bool);
bool                 get_column_resolved(const int);
void                 set_column_resolved(const int, bool);
const char          *get_column_custom_fields(const int);
void                 set_column_custom_fields(const int, const char *);
int                  get_column_custom_occurrence(const int);
void                 set_column_custom_occurrence(const int, const int);
const char          *get_column_longest_string(const int);
const char          *get_column_width_string(const int, const int);
int                  get_column_char_width(const int format);
char                *get_column_tooltip(const int col);
const char          *get_column_text(column_info *cinfo, const int col);
void col_finalize(column_info *cinfo);
void build_column_format_array(column_info *cinfo, const int num_cols, const bool reset_fences);
void                 column_dump_column_formats(void);
bool parse_column_format(fmt_data *cfmt, const char *fmt);
void try_convert_to_custom_column(char **fmt);
const char* try_convert_to_column_field(const char *field);
void column_register_fields(void);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* column.h */
