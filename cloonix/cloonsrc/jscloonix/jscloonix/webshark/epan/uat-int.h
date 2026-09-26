/** @file
 *
 *  User Accessible Tables
 *  Maintain an array of user accessible data structures
 *  Internal interface
 *
 * (c) 2007, Luis E. Garcia Ontanon <luis@ontanon.org>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 2001 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */
#ifndef __UAT_INT_H__
#define __UAT_INT_H__

#include <glib.h>

#include "uat.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _uat_fld_rep_t uat_fld_rep_t;
typedef struct _uat_rep_t uat_rep_t;

typedef void (*uat_rep_fld_free_cb_t)(uat_fld_rep_t*);
typedef void (*uat_rep_free_cb_t)(uat_rep_t*);

typedef struct _fld_data_t {
    unsigned colnum;
    uat_fld_rep_t* rep;
    uat_rep_fld_free_cb_t free_rep;
} fld_data_t;

struct epan_uat {
    char* name;
    size_t record_size;
    char* filename;
    bool from_profile;
    char* help;
    unsigned flags;
    void** user_ptr;    /**< Pointer to a dissector variable where an array of valid records are stored. */
    unsigned* nrows_p;     /**< Pointer to a dissector variable where the number of valid records in user_ptr are written. */
    uat_copy_cb_t copy_cb;
    uat_update_cb_t update_cb;
    uat_free_cb_t free_cb;
    uat_post_update_cb_t post_update_cb;
    uat_reset_cb_t reset_cb;

    uat_field_t* fields;
    const char** default_values;
    unsigned ncols;
    GArray* user_data;  /**< An array of valid records that will be exposed to the dissector. */
    GArray* raw_data;   /**< An array of records containing possibly invalid data. For internal use only. */
    GArray* valid_data; /**< An array of booleans describing whether the records in 'raw_data' are valid or not. */
    bool changed;
    uat_rep_t* rep;
    uat_rep_free_cb_t free_rep;
    bool loaded;
};

char* uat_get_actual_filename(uat_t* uat, bool for_writing);
void* uat_add_record(uat_t *uat, const void *orig_rec_ptr, bool valid_rec);
void uat_update_record(uat_t *uat, const void *record, bool valid_rec);
void uat_swap(uat_t *uat, unsigned idx_a, unsigned idx_b);
void uat_insert_record_idx(uat_t *uat, unsigned rec_idx, const void *src_record);
void uat_remove_record_idx(uat_t *uat, unsigned rec_idx);
void uat_remove_record_range(uat_t *uat, unsigned rec_idx, unsigned count);
void uat_move_index(uat_t *uat, unsigned old_idx, unsigned new_idx);
void uat_clear(uat_t *uat);
void uat_load_all(void);
char *uat_fld_tostr(void *rec, uat_field_t *f);
#define UAT_UPDATE(uat) do { *((uat)->user_ptr) = (void*)((uat)->user_data->data); *((uat)->nrows_p) = (uat)->user_data->len; } while(0)
#define UAT_INDEX_PTR(uat,idx) (uat->raw_data->data + (uat->record_size * (idx)))
#define UAT_USER_INDEX_PTR(uat,idx) (uat->user_data->data + (uat->record_size * (idx)))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __UAT_INT_H__ */

