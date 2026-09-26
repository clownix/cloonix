/** @file
 * Definitions for value_string structures and routines
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __VALUE_STRING_H__
#define __VALUE_STRING_H__

#include <stdint.h>

#include <epan/wmem_scopes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _value_string {
    uint32_t     value;
    const char *strptr;
} value_string;

#define VALUE_STRING_ENUM(              array_name) _VS_ENUM_XXX( array_name, _VS_ENUM_ENTRY)
#define VALUE_STRING_ARRAY(             array_name) _VS_ARRAY_SC_XXX(array_name, _VS_ARRAY_ENTRY, static)
#define VALUE_STRING_ARRAY_GLOBAL_DEF(  array_name) _VS_ARRAY_XXX(array_name, _VS_ARRAY_ENTRY)
#define VALUE_STRING_ARRAY_GLOBAL_DCL(  array_name) _VS_ARRAY_SC_TYPE_NAME(array_name, extern)

#define VALUE_STRING_ENUM2(             array_name) _VS_ENUM_XXX( array_name, _VS_ENUM_ENTRY2)
#define VALUE_STRING_ARRAY2(            array_name) _VS_ARRAY_SC_XXX(array_name, _VS_ARRAY_ENTRY2, static)
#define VALUE_STRING_ARRAY2_GLOBAL_DEF( array_name) _VS_ARRAY_XXX(array_name, _VS_ARRAY_ENTRY2)
#define VALUE_STRING_ARRAY2_GLOBAL_DCL( array_name) _VS_ARRAY_SC_TYPE_NAME(array_name, extern)

#define _VS_ENUM_XXX(array_name, macro) \
enum { \
    array_name##_VALUE_STRING_LIST(macro) \
    _##array_name##_ENUM_DUMMY = 0 \
}

#define _VS_ARRAY_SC_XXX(array_name, macro, sc)  \
    _VS_ARRAY_SC_TYPE_NAME(array_name, sc) = { \
    array_name##_VALUE_STRING_LIST(macro) \
    { 0, NULL } \
}

#define _VS_ARRAY_XXX(array_name, macro)  \
    _VS_ARRAY_TYPE_NAME(array_name) = { \
    array_name##_VALUE_STRING_LIST(macro) \
    { 0, NULL } \
}

#define _VS_ARRAY_SC_TYPE_NAME(array_name, sc) sc const value_string array_name[]
#define _VS_ARRAY_TYPE_NAME(array_name) const value_string array_name[]

#define _VS_ENUM_ENTRY( name, value, string) name = value,
#define _VS_ARRAY_ENTRY(name, value, string) { value, string },

#define _VS_ENUM_ENTRY2( name, value) name = value,
#define _VS_ARRAY_ENTRY2(name, value) { value, #name },

const char * val_to_str(const uint32_t val, const value_string *vs, const char *fmt)
G_GNUC_PRINTF(3, 0);

char * val_to_str_wmem(wmem_allocator_t *scope, const uint32_t val, const value_string *vs, const char *fmt)
G_GNUC_PRINTF(4, 0);

const char * val_to_str_const(const uint32_t val, const value_string *vs, const char *unknown_str);

const char * try_val_to_str(const uint32_t val, const value_string *vs);

const char * try_val_to_str_idx(const uint32_t val, const value_string *vs, int *idx);

const char * char_val_to_str(char val, const value_string *vs, const char *msg);

typedef struct _val64_string {
    uint64_t     value;
    const char *strptr;
} val64_string;

const char * val64_to_str(const uint64_t val, const val64_string *vs, const char *fmt)
G_GNUC_PRINTF(3, 0);

const char * val64_to_str_const(const uint64_t val, const val64_string *vs, const char *unknown_str);

const char * try_val64_to_str(const uint64_t val, const val64_string *vs);

const char * try_val64_to_str_idx(const uint64_t val, const val64_string *vs, int *idx);

uint32_t str_to_val(const char *val, const value_string *vs, const uint32_t err_val);

int str_to_val_idx(const char *val, const value_string *vs);

typedef struct _value_string_ext value_string_ext;
typedef const value_string *(*_value_string_match2_t)(const uint32_t, value_string_ext*);

struct _value_string_ext {
    _value_string_match2_t _vs_match2;
    uint32_t               _vs_first_value; /* first value of the value_string array       */
    unsigned               _vs_num_entries; /* number of entries in the value_string array */
                                            /*  (excluding final {0, NULL})                */
    const value_string    *_vs_p;           /* the value string array address              */
    const char            *_vs_name;        /* vse "Name" (for error messages)             */
};

#define VALUE_STRING_EXT_VS_P(x)           (x)->_vs_p
#define VALUE_STRING_EXT_VS_NUM_ENTRIES(x) (x)->_vs_num_entries
#define VALUE_STRING_EXT_VS_NAME(x)        (x)->_vs_name

const value_string * _try_val_to_str_ext_init(const uint32_t val, value_string_ext *vse);
#define VALUE_STRING_EXT_INIT(x) { _try_val_to_str_ext_init, 0, G_N_ELEMENTS(x)-1, x, #x }

value_string_ext * value_string_ext_new(const value_string *vs, unsigned vs_tot_num_entries, const char *vs_name);

void value_string_ext_free(value_string_ext *vse);

const char * val_to_str_ext(const uint32_t val, value_string_ext *vse, const char *fmt)
G_GNUC_PRINTF(3, 0);

char * val_to_str_ext_wmem(wmem_allocator_t *scope, const uint32_t val, value_string_ext *vse, const char *fmt)
G_GNUC_PRINTF(4, 0);

const char * val_to_str_ext_const(const uint32_t val, value_string_ext *vs, const char *unknown_str);

const char * try_val_to_str_ext(const uint32_t val, value_string_ext *vse);

const char * try_val_to_str_idx_ext(const uint32_t val, value_string_ext *vse, int *idx);

typedef struct _val64_string_ext val64_string_ext;
typedef const val64_string *(*_val64_string_match2_t)(const uint64_t, val64_string_ext*);

struct _val64_string_ext {
    _val64_string_match2_t _vs_match2;
    uint64_t               _vs_first_value; /* first value of the val64_string array       */
    unsigned               _vs_num_entries; /* number of entries in the val64_string array */
                                            /*  (excluding final {0, NULL})                */
    const val64_string    *_vs_p;           /* the value string array address              */
    const char            *_vs_name;        /* vse "Name" (for error messages)             */
};

#define VAL64_STRING_EXT_VS_P(x)           (x)->_vs_p
#define VAL64_STRING_EXT_VS_NUM_ENTRIES(x) (x)->_vs_num_entries
#define VAL64_STRING_EXT_VS_NAME(x)        (x)->_vs_name

const val64_string * _try_val64_to_str_ext_init(const uint64_t val, val64_string_ext *vse);
#define VAL64_STRING_EXT_INIT(x) { _try_val64_to_str_ext_init, 0, G_N_ELEMENTS(x)-1, x, #x }

val64_string_ext * val64_string_ext_new(const val64_string *vs, unsigned vs_tot_num_entries, const char *vs_name);

void val64_string_ext_free(val64_string_ext *vse);

const char * val64_to_str_ext(const uint64_t val, val64_string_ext *vse, const char *fmt)
G_GNUC_PRINTF(3, 0);

char * val64_to_str_ext_wmem(wmem_allocator_t *scope, const uint64_t val, val64_string_ext *vse, const char *fmt)
G_GNUC_PRINTF(4, 0);

const char * val64_to_str_ext_const(const uint64_t val, val64_string_ext *vs, const char *unknown_str);

const char * try_val64_to_str_ext(const uint64_t val, val64_string_ext *vse);

const char * try_val64_to_str_idx_ext(const uint64_t val, val64_string_ext *vse, int *idx);

typedef struct _string_string {
    const char *value;
    const char *strptr;
} string_string;

const char * str_to_str(const char *val, const string_string *vs, const char *fmt)
G_GNUC_PRINTF(3, 0);

const char * try_str_to_str(const char *val, const string_string *vs);

const char * try_str_to_str_idx(const char *val, const string_string *vs, int *idx);

typedef struct _range_string {
    uint64_t     value_min;
    uint64_t     value_max;
    const char *strptr;
} range_string;

const char * rval_to_str(const uint32_t val, const range_string *rs, const char *fmt)
G_GNUC_PRINTF(3, 0);

const char * rval_to_str_const(const uint32_t val, const range_string *rs, const char *unknown_str);
const char * try_rval_to_str(const uint32_t val, const range_string *rs);
const char * try_rval_to_str_idx(const uint32_t val, const range_string *rs, int *idx);
const char * try_rval64_to_str(const uint64_t val, const range_string *rs);
const char * try_rval64_to_str_idx(const uint64_t val, const range_string *rs, int *idx);

typedef struct _bytes_string {
  const uint8_t *value;
  const size_t  value_length;
  const char   *strptr;
} bytes_string;

const char * bytesval_to_str(const uint8_t *val, const size_t val_len, const bytes_string *bs, const char *fmt)
G_GNUC_PRINTF(4, 0);
const char * try_bytesval_to_str(const uint8_t *val, const size_t val_len, const bytes_string *bs);
const char * bytesprefix_to_str(const uint8_t *haystack, const size_t haystack_len, const bytes_string *bs, const char *fmt)
G_GNUC_PRINTF(4, 0);
const char * try_bytesprefix_to_str(const uint8_t *haystack, const size_t haystack_len, const bytes_string *bs);
bool value_string_ext_validate(const value_string_ext *vse);
const char * value_string_ext_match_type_str(const value_string_ext *vse);
bool val64_string_ext_validate(const val64_string_ext *vse);
const char * val64_string_ext_match_type_str(const val64_string_ext *vse);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __VALUE_STRING_H__ */
