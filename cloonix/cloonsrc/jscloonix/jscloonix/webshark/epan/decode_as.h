/* decode_as.h
 * Routines for dissector Decode As handlers
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __DECODE_AS_H__
#define __DECODE_AS_H__

#include "ftypes/ftypes.h"
#include "packet_info.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define MAX_DECODE_AS_PROMPT_LEN    200
#define DECODE_AS_ENTRY "decode_as_entry"
#define DECODE_AS_NONE "(none)"

#define DECODE_AS_ENTRIES_FILE_NAME "decode_as_entries"

typedef void (*build_label_func)(packet_info *pinfo, char* result);

typedef void * (*build_valid_func)(packet_info *pinfo);

typedef void (*decode_as_add_to_list_func)(const char *table_name, const char *proto_name, void *value, void *user_data);
typedef void (*decode_as_populate_list_func)(const char *table_name, decode_as_add_to_list_func add_to_list, void *ui_element);
typedef void (*decode_as_free_func)(void *value);

typedef bool (*decode_as_reset_func)(const char *name, const void *pattern);
typedef bool (*decode_as_change_func)(const char *name, const void *pattern, const void *handle, const char *list_name);

typedef struct decode_as_value_s {
    build_label_func label_func;            /**< function pointer to the function used to create the label*/
    unsigned num_values;                       /**< Number of values */
    build_valid_func* build_values;         /**< Function used to build the value to go into the table. Retrieve from current frame */
} decode_as_value_t;

typedef struct decode_as_s {
    const char *name;                               /**< Protocol name */
    const char *table_name;                        /**< Disector table name */
    unsigned num_items;                                /**< Number of index in the decode_as_value_t struct */
    unsigned default_index_value;                      /**< Which display function to use first, set to zero if only one function*/
    decode_as_value_t* values;                      /**< The array of function pointers, see decode_as_value_t */
    const char* pre_value_str;                      /**< String to prepend the value, NULL if none */
    const char* post_value_str;                     /**< String to append the value, NULL if none */
    decode_as_populate_list_func populate_list;     /**< function pointer to the function used to populate the list, NULL if none */
    decode_as_reset_func reset_value;               /**< function pointer to the function used resetting the value, NULL if none */
    decode_as_change_func change_value;             /**< function pointer to the function used resetting the value, NULL if none */
    decode_as_free_func free_func;                  /**< function pointer to the function used freeing the entry, NULL if none */

} decode_as_t;

void register_decode_as(decode_as_t* reg);

struct dissector_table;

struct dissector_table* register_decode_as_next_proto(int proto, const char *table_name, const char *ui_name, build_label_func label_func);

void decode_as_default_populate_list(const char *table_name, decode_as_add_to_list_func add_to_list, void *ui_element);
bool decode_as_default_reset(const char *name, const void *pattern);
bool decode_as_default_change(const char *name, const void *pattern, const void *handle, const char *list_name);
extern GList *decode_as_list;
extern void load_decode_as_entries(void);
int save_decode_as_entries(char** err);
void decode_clear_all(void);
void decode_cleanup(void);
void decode_build_reset_list (const char *table_name, ftenum_t selector_type,
                         void *key, void *value,
                         void *user_data);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* decode_as.h */
