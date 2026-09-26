/** @file
 *
 * Routines for handling preferences
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __PREFERENCE_UTILS_H__
#define __PREFERENCE_UTILS_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** @file
 * Preference utility routines.
 *  @ingroup prefs_group
 */

/** If autoscroll in live captures is active or not
 */
extern bool auto_scroll_live;

/** Fill in capture options with values from the preferences
 */
extern void prefs_to_capture_opts(void);

/** Save all preferences
 */
extern void prefs_main_write(void);

extern unsigned int prefs_store_ext(const char * module, const char * key, const char * value);

/** Convenient function for the writing of multiple preferences, without
 * explicitly having prefs_t variables.
 *
 * Note: The preferences must exist, it is not possible to create entries
 * using this function
 *
 * @param module the module for the preference
 * @param pref_values a hash table
 *
 * @return true if the value has been stored successfully
 */
extern bool prefs_store_ext_multiple(const char * module, GHashTable * pref_values);

/** Add a custom column.
 *
 * @param fmt column format
 * @param title column title
 * @param custom_field column custom field
 * @param position the intended position of the insert
 *
 * @return The index of the inserted column
 */
int column_prefs_add_custom(int fmt, const char *title,
                             const char *custom_field,
                             int position);

/** Check if a custom column exists.
 *
 * @param custom_field column custom field
 *
 * @return The index of the column if existing, -1 if not existing
 */
int column_prefs_has_custom(const char *custom_field);

/** Check if a custom column's data can be displayed differently
 * resolved or unresolved, e.g. it has a field with a value string.
 *
 * This is for when adding or editing custom columns. Compare with
 * resolve_column() in packet_list_utils.h, which is for columns
 * that have already been added.
 *
 * @param custom_field column custom field
 *
 * @return true if a custom column with the field description
 * would support being displayed differently resolved or unresolved,
 * false otherwise.
 */
bool column_prefs_custom_resolve(const char *custom_field);

/** Remove a column.
 *
 * @param col_link Column list entry
 */
void column_prefs_remove_link(GList* col_link);

/** Remove a column.
 *
 * @param col Column number
 */
void column_prefs_remove_nth(int col);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __PREFERENCE_UTILS_H__ */
