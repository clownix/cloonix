/** @file
 *
 *  User Accessible Tables
 *  Maintain an array of user accessible data structures
 *
 * (c) 2007, Luis E. Garcia Ontanon <luis@ontanon.org>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 2001 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __UAT_H__
#define __UAT_H__

#include <stdlib.h>

#include <wsutil/strtoi.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct epan_uat uat_t;
typedef void (*uat_post_update_cb_t)(void);
typedef void* (*uat_copy_cb_t)(void *dest, const void *source, size_t len);
typedef void (*uat_free_cb_t)(void *record);
typedef void (*uat_reset_cb_t)(void);
typedef bool (*uat_update_cb_t)(void *record, char **error);
typedef bool (*uat_fld_chk_cb_t)(void *record, const char *ptr, unsigned len, const void *chk_data, const void *fld_data, char **error);
typedef void (*uat_fld_set_cb_t)(void *record, const char *ptr, unsigned len, const void *set_data, const void *fld_data);
typedef void (*uat_fld_tostr_cb_t)(void *record, char **out_ptr, unsigned *out_len, const void *tostr_data, const void *fld_data);
typedef enum _uat_text_mode_t {
    PT_TXTMOD_NONE,
    PT_TXTMOD_STRING,
    PT_TXTMOD_HEXBYTES,
    PT_TXTMOD_ENUM,
    PT_TXTMOD_DISSECTOR,
    PT_TXTMOD_COLOR,
    PT_TXTMOD_FILENAME,
    PT_TXTMOD_DIRECTORYNAME,
    PT_TXTMOD_DISPLAY_FILTER,
    PT_TXTMOD_PROTO_FIELD,
    PT_TXTMOD_BOOL
} uat_text_mode_t;

typedef struct _uat_field_t {
    const char* name;
    const char* title;
    uat_text_mode_t mode;

    struct {
        uat_fld_chk_cb_t chk;
        uat_fld_set_cb_t set;
        uat_fld_tostr_cb_t tostr;
    } cb;

    struct {
        const void* chk;
        const void* set;
        const void* tostr;
    } cbdata;

    const void* fld_data;

    const char* desc;
    struct _fld_data_t* priv;
} uat_field_t;

#define FLDFILL NULL
#define UAT_END_FIELDS {NULL,NULL,PT_TXTMOD_NONE,{0,0,0},{0,0,0},0,0,FLDFILL}

#define UAT_AFFECTS_DISSECTION	0x00000001	/* affects packet dissection */
#define UAT_AFFECTS_FIELDS	0x00000002	/* affects what named fields exist */

uat_t* uat_new(const char* name,
               size_t size,
               const char* filename,
               bool from_profile,
               void* data_ptr,
               unsigned* num_items_ptr,
               unsigned flags,
               const char* help,
               uat_copy_cb_t copy_cb,
               uat_update_cb_t update_cb,
               uat_free_cb_t free_cb,
               uat_post_update_cb_t post_update_cb,
               uat_reset_cb_t reset_cb,
               uat_field_t* flds_array);

void uat_destroy(uat_t *uat);

void uat_cleanup(void);

bool uat_load(uat_t* uat_in, const char *filename, char** err);

bool uat_load_str(uat_t* uat_in, const char* entry, char** err);

uat_t *uat_find(char *name);

uat_t* uat_get_table_by_name(const char* name);

void uat_set_default_values(uat_t *uat_in, const char *default_values[]);

bool uat_fld_chk_str(void*, const char*, unsigned, const void*, const void*, char** err);
bool uat_fld_chk_oid(void*, const char*, unsigned, const void*, const void*, char** err);
bool uat_fld_chk_proto(void*, const char*, unsigned, const void*, const void*, char** err);
bool uat_fld_chk_num_dec(void*, const char*, unsigned, const void*, const void*, char** err);
bool uat_fld_chk_num_dec64(void*, const char*, unsigned, const void*, const void*, char** err);
bool uat_fld_chk_num_hex(void*, const char*, unsigned, const void*, const void*, char** err);
bool uat_fld_chk_num_hex64(void*, const char*, unsigned, const void*, const void*, char** err);
bool uat_fld_chk_num_signed_dec(void*, const char*, unsigned, const void*, const void*, char** err);
bool uat_fld_chk_num_signed_dec64(void*, const char*, unsigned, const void*, const void*, char** err);
bool uat_fld_chk_bool(void*, const char*, unsigned, const void*, const void*, char** err);
bool uat_fld_chk_enum(void*, const char*, unsigned, const void*, const void*, char**);
bool uat_fld_chk_range(void*, const char*, unsigned, const void*, const void*, char**);
bool uat_fld_chk_color(void*, const char*, unsigned, const void*, const void*, char**);
typedef void (*uat_cb_t)(void* uat,void* user_data);
void uat_foreach_table(uat_cb_t cb,void* user_data);
void uat_unload_all(void);
char* uat_undquote(const char* si, unsigned in_len, unsigned* len_p);
char* uat_unbinstring(const char* si, unsigned in_len, unsigned* len_p);
char* uat_unesc(const char* si, unsigned in_len, unsigned* len_p);
char* uat_esc(const char* buf, unsigned len);
bool uat_fld_chk_str_isprint(void*, const char*, unsigned, const void*, const void*, char**);
bool uat_fld_chk_str_isalpha(void*, const char*, unsigned, const void*, const void*, char**);
bool uat_fld_chk_str_isalnum(void*, const char*, unsigned, const void*, const void*, char**);
bool uat_fld_chk_str_isdigit(void*, const char*, unsigned, const void*, const void*, char**);
bool uat_fld_chk_str_isxdigit(void*, const char*, unsigned, const void*, const void*, char**);

#ifdef __cplusplus
#define UNUSED_PARAMETER(n)
#else
#define UNUSED_PARAMETER(n) n 
#endif

#define UAT_CSTRING_CB_DEF(basename,field_name,rec_t) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    char* new_buf = g_strndup(buf,len); \
    g_free((((rec_t*)rec)->field_name)); \
    (((rec_t*)rec)->field_name) = new_buf; } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    if (((rec_t*)rec)->field_name ) { \
        *out_ptr = g_strdup((((rec_t*)rec)->field_name)); \
        *out_len = (unsigned)strlen((((rec_t*)rec)->field_name)); \
    } else { \
        *out_ptr = g_strdup(""); *out_len = 0; \
    } }

#define UAT_FLD_CSTRING(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_STRING,{uat_fld_chk_str,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_FLD_CSTRING_ISPRINT(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_STRING,{uat_fld_chk_str_isprint,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_FLD_CSTRING_OTHER(basename,field_name,title,chk,desc) \
    {#field_name, title, PT_TXTMOD_STRING,{ chk ,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_FILENAME_CB_DEF(basename,field_name,rec_t) UAT_CSTRING_CB_DEF(basename,field_name,rec_t)

#define UAT_FLD_FILENAME(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_FILENAME,{uat_fld_chk_str,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_FLD_FILENAME_OTHER(basename,field_name,title,chk,desc) \
    {#field_name, title, PT_TXTMOD_FILENAME,{chk,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_DIRECTORYNAME_CB_DEF(basename,field_name,rec_t) UAT_CSTRING_CB_DEF(basename,field_name,rec_t)

#define UAT_FLD_DIRECTORYNAME(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_DIRECTORYNAME,{uat_fld_chk_str,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_DISPLAY_FILTER_CB_DEF(basename,field_name,rec_t) UAT_CSTRING_CB_DEF(basename,field_name,rec_t)

#define UAT_FLD_DISPLAY_FILTER(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_DISPLAY_FILTER, {uat_fld_chk_str,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_PROTO_FIELD_CB_DEF(basename,field_name,rec_t) UAT_CSTRING_CB_DEF(basename,field_name,rec_t)

#define UAT_FLD_PROTO_FIELD(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_PROTO_FIELD, {uat_fld_chk_str,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_FLD_OID(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_STRING,{uat_fld_chk_oid,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}


#define UAT_LSTRING_CB_DEF(basename,field_name,rec_t,ptr_element,len_element) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    char* new_val = uat_unesc(buf,len,&(((rec_t*)rec)->len_element)); \
    g_free((((rec_t*)rec)->ptr_element)); \
    (((rec_t*)rec)->ptr_element) = new_val; } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    if (((rec_t*)rec)->ptr_element ) { \
        *out_ptr = uat_esc(((rec_t*)rec)->ptr_element, (((rec_t*)rec)->len_element)); \
        *out_len = (unsigned)strlen(*out_ptr); \
    } else { \
        *out_ptr = g_strdup(""); \
        *out_len = 0; \
    } }

#define UAT_FLD_LSTRING(basename,field_name,title, desc) \
{#field_name, title, PT_TXTMOD_STRING,{0,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}


#define UAT_BUFFER_CB_DEF(basename,field_name,rec_t,ptr_element,len_element) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    unsigned char* new_buf = len ? (unsigned char *)g_memdup2(buf,len) : NULL; \
    g_free((((rec_t*)rec)->ptr_element)); \
    (((rec_t*)rec)->ptr_element) = new_buf; \
    (((rec_t*)rec)->len_element) = len; } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    *out_ptr = ((rec_t*)rec)->ptr_element ? (char*)g_memdup2(((rec_t*)rec)->ptr_element,((rec_t*)rec)->len_element) : g_strdup(""); \
    *out_len = ((rec_t*)rec)->len_element; }

#define UAT_FLD_BUFFER(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_HEXBYTES,{0,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}


#define UAT_DEC_CB_DEF(basename,field_name,rec_t) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    char* tmp_str = g_strndup(buf,len); \
    ws_strtou32(tmp_str, NULL, &((rec_t*)rec)->field_name); \
    g_free(tmp_str); } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    *out_ptr = ws_strdup_printf("%u",((rec_t*)rec)->field_name); \
    *out_len = (unsigned)strlen(*out_ptr); }

#define UAT_FLD_DEC(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_STRING,{uat_fld_chk_num_dec,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_DEC64_CB_DEF(basename,field_name,rec_t) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    char* tmp_str = g_strndup(buf,len); \
    ws_strtou64(tmp_str, NULL, &((rec_t*)rec)->field_name); \
    g_free(tmp_str); } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    *out_ptr = ws_strdup_printf("%" PRIu64,((rec_t*)rec)->field_name); \
    *out_len = (unsigned)strlen(*out_ptr); }

#define UAT_FLD_DEC64(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_STRING,{uat_fld_chk_num_dec64,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_SIGNED_DEC_CB_DEF(basename,field_name,rec_t) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    char* tmp_str = g_strndup(buf,len); \
    ws_strtoi32(tmp_str, NULL, &((rec_t*)rec)->field_name); \
    g_free(tmp_str); } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    *out_ptr = ws_strdup_printf("%d",((rec_t*)rec)->field_name); \
    *out_len = (unsigned)strlen(*out_ptr); }

#define UAT_FLD_SIGNED_DEC(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_STRING,{uat_fld_chk_num_signed_dec,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_SIGNED_DEC64_CB_DEF(basename,field_name,rec_t) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    char* tmp_str = g_strndup(buf,len); \
    ws_strtoi64(tmp_str, NULL, &((rec_t*)rec)->field_name); \
    g_free(tmp_str); } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    *out_ptr = ws_strdup_printf("%" PRId64,((rec_t*)rec)->field_name); \
    *out_len = (unsigned)strlen(*out_ptr); }

#define UAT_FLD_SIGNED_DEC64(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_STRING,{uat_fld_chk_num_signed_dec64,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_FLD_NONE(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_NONE,{uat_fld_chk_num_dec,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}


#define UAT_HEX_CB_DEF(basename,field_name,rec_t) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    char* tmp_str = g_strndup(buf,len); \
    ws_hexstrtou32(tmp_str, NULL, &((rec_t*)rec)->field_name); \
    g_free(tmp_str); } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    *out_ptr = ws_strdup_printf("%x",((rec_t*)rec)->field_name); \
    *out_len = (unsigned)strlen(*out_ptr); }

#define UAT_FLD_HEX(basename,field_name,title,desc) \
{#field_name, title, PT_TXTMOD_STRING,{uat_fld_chk_num_hex,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_HEX64_CB_DEF(basename,field_name,rec_t) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    char* tmp_str = g_strndup(buf,len); \
    ws_hexstrtou64(tmp_str, NULL, &((rec_t*)rec)->field_name); \
    g_free(tmp_str); } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    *out_ptr = ws_strdup_printf("%" PRIx64,((rec_t*)rec)->field_name); \
    *out_len = (unsigned)strlen(*out_ptr); }

#define UAT_FLD_HEX64(basename,field_name,title,desc) \
{#field_name, title, PT_TXTMOD_STRING,{uat_fld_chk_num_hex64,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_BOOL_CB_DEF(basename,field_name,rec_t) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    char* tmp_str = g_strndup(buf,len); \
    if (tmp_str && g_ascii_strcasecmp(tmp_str, "true") == 0) \
        ((rec_t*)rec)->field_name = 1; \
    else \
        ((rec_t*)rec)->field_name = 0; \
    g_free(tmp_str); } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    *out_ptr = ws_strdup_printf("%s",((rec_t*)rec)->field_name ? "TRUE" : "FALSE"); \
    *out_len = (unsigned)strlen(*out_ptr); }

#define UAT_FLD_BOOL(basename,field_name,title,desc) \
{#field_name, title, PT_TXTMOD_BOOL,{uat_fld_chk_bool,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_VS_DEF(basename,field_name,rec_t,default_t,default_val,default_str) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* vs, const void* UNUSED_PARAMETER(u2)) {\
    unsigned i; \
    char* str = g_strndup(buf,len); \
    const char* cstr; \
    ((rec_t*)rec)->field_name = default_val; \
    for(i=0; ( cstr = ((const value_string*)vs)[i].strptr ) ;i++) { \
        if (g_str_equal(cstr,str)) { \
            ((rec_t*)rec)->field_name = (default_t)((const value_string*)vs)[i].value; \
            g_free(str); \
            return; \
        } \
    } \
    g_free(str); } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* vs, const void* UNUSED_PARAMETER(u2)) {\
    unsigned i; \
    for(i=0;((const value_string*)vs)[i].strptr;i++) { \
        if ( ((const value_string*)vs)[i].value == ((rec_t*)rec)->field_name ) { \
            *out_ptr = g_strdup(((const value_string*)vs)[i].strptr); \
            *out_len = (unsigned)strlen(*out_ptr); \
            return; \
        } \
    } \
    *out_ptr = g_strdup(default_str); \
    *out_len = (unsigned)strlen(default_str); }

#define UAT_VS_CSTRING_DEF(basename,field_name,rec_t,default_val,default_str) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* vs, const void* UNUSED_PARAMETER(u2)) {\
    unsigned i; \
    char* str = g_strndup(buf,len); \
    const char* cstr; \
    ((rec_t*)rec)->field_name = default_val; \
    for(i=0; ( cstr = ((const value_string*)vs)[i].strptr ) ;i++) { \
        if (g_str_equal(cstr,str)) { \
          ((rec_t*)rec)->field_name = g_strdup(((const value_string*)vs)[i].strptr); \
          g_free(str); \
          return; \
        } \
    } \
    g_free(str);} \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(vs), const void* UNUSED_PARAMETER(u2)) {\
    if (((rec_t*)rec)->field_name ) { \
        *out_ptr = g_strdup((((rec_t*)rec)->field_name)); \
        *out_len = (unsigned)strlen((((rec_t*)rec)->field_name)); \
    } else { \
        *out_ptr = g_strdup(""); *out_len = 0; } }

#define UAT_FLD_VS(basename,field_name,title,enum,desc) \
    {#field_name, title, PT_TXTMOD_ENUM,{uat_fld_chk_enum,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{&(enum),&(enum),&(enum)},&(enum),desc,FLDFILL}


#define UAT_COLOR_CB_DEF(basename,field_name,rec_t) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    if (len < 1) { \
        ((rec_t*)rec)->field_name = 0; \
        return; \
    } \
    char* tmp_str = g_strndup(buf+1,len-1); \
    ((rec_t*)rec)->field_name = (unsigned)strtol(tmp_str,NULL,16); \
    g_free(tmp_str); } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    *out_ptr = ws_strdup_printf("#%06X",((rec_t*)rec)->field_name); \
    *out_len = (unsigned)strlen(*out_ptr); }

#define UAT_FLD_COLOR(basename,field_name,title,desc) \
{#field_name, title, PT_TXTMOD_COLOR,{uat_fld_chk_color,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_DISSECTOR_DEF(basename, field_name, dissector_field, name_field, rec_t) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    if (len) { \
        ((rec_t*)rec)->name_field = g_strndup(buf, len); \
        g_strstrip(((rec_t*)rec)->name_field); \
        ((rec_t*)rec)->dissector_field = find_dissector(((rec_t*)rec)->name_field); \
    } else { \
        ((rec_t*)rec)->dissector_field = find_dissector("data"); \
        ((rec_t*)rec)->name_field = NULL; \
    } } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    if ( ((rec_t*)rec)->name_field ) { \
        *out_ptr = g_strdup((((rec_t*)rec)->name_field)); \
        *out_len = (unsigned)strlen(*out_ptr); \
    } else { \
        *out_ptr = g_strdup(""); *out_len = 0; \
    } }


#define UAT_FLD_DISSECTOR(basename,field_name,title,desc) \
    {#field_name, title, PT_TXTMOD_DISSECTOR,{uat_fld_chk_proto,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},{0,0,0},0,desc,FLDFILL}

#define UAT_RANGE_CB_DEF(basename,field_name,rec_t) \
static void basename ## _ ## field_name ## _set_cb(void* rec, const char* buf, unsigned len, const void* UNUSED_PARAMETER(u1), const void* u2) {\
    char* rng = g_strndup(buf,len);\
        range_convert_str(NULL, &(((rec_t*)rec)->field_name), rng,GPOINTER_TO_UINT(u2)); \
        g_free(rng); \
    } \
static void basename ## _ ## field_name ## _tostr_cb(void* rec, char** out_ptr, unsigned* out_len, const void* UNUSED_PARAMETER(u1), const void* UNUSED_PARAMETER(u2)) {\
    if ( ((rec_t*)rec)->field_name ) { \
        *out_ptr = range_convert_range(NULL, ((rec_t*)rec)->field_name); \
        *out_len = (unsigned)strlen(*out_ptr); \
    } else { \
        *out_ptr = g_strdup(""); *out_len = 0; \
    } }


#define UAT_FLD_RANGE(basename,field_name,title,max,desc) \
    {#field_name, title, PT_TXTMOD_STRING,{uat_fld_chk_range,basename ## _ ## field_name ## _set_cb,basename ## _ ## field_name ## _tostr_cb},\
      {0,0,0},GUINT_TO_POINTER(max),desc,FLDFILL}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __UAT_H__ */

