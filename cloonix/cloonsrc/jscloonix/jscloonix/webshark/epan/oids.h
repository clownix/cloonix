/* oids.h
 * Object IDentifier Support
 *
 * (c) 2007, Luis E. Garcia Ontanon <luis@ontanon.org>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __OIDS_H__
#define __OIDS_H__

#include <epan/ftypes/ftypes.h>
#include <epan/prefs.h>
#include <epan/wmem_scopes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define BER_TAG_ANY -1

struct _oid_bit_t {
    unsigned offset;
    int hfid;
};

typedef struct _oid_bits_info_t {
    unsigned num;
    int ett;
    struct _oid_bit_t* data;
} oid_bits_info_t;

typedef enum _oid_key_type_t {
    OID_KEY_TYPE_WRONG,
    OID_KEY_TYPE_INTEGER,
    OID_KEY_TYPE_OID,
    OID_KEY_TYPE_STRING,
    OID_KEY_TYPE_BYTES,
    OID_KEY_TYPE_NSAP,
    OID_KEY_TYPE_IPADDR,
    OID_KEY_TYPE_IMPLIED_OID,
    OID_KEY_TYPE_IMPLIED_STRING,
    OID_KEY_TYPE_IMPLIED_BYTES,
    OID_KEY_TYPE_ETHER,
    OID_KEY_TYPE_DATE_AND_TIME
} oid_key_type_t;

typedef struct _oid_value_type_t {
    enum ftenum ft_type;
    int display;
    int8_t ber_class;
    int32_t ber_tag;
    int min_len;
    int max_len;
    oid_key_type_t keytype;
    int keysize;
} oid_value_type_t;

typedef enum _oid_kind_t {
    OID_KIND_UNKNOWN = 0,
    OID_KIND_NODE,
    OID_KIND_SCALAR,
    OID_KIND_TABLE,
    OID_KIND_ROW,
    OID_KIND_COLUMN,
    OID_KIND_NOTIFICATION,
    OID_KIND_GROUP,
    OID_KIND_COMPLIANCE,
    OID_KIND_CAPABILITIES
} oid_kind_t;

typedef struct _oid_key_t {
    char* name;
    uint32_t num_subids;
    oid_key_type_t key_type;
    int hfid;
    enum ftenum ft_type;
    int display;
    struct _oid_key_t* next;
} oid_key_t;

typedef struct _oid_info_t {
    uint32_t subid;
    char* name;
    oid_kind_t kind;
    wmem_tree_t* children;
    const oid_value_type_t* value_type;
    int value_hfid;
    oid_key_t* key;
    oid_bits_info_t* bits;
    struct _oid_info_t* parent;
} oid_info_t;

void oids_init(void);
extern void oid_pref_init(module_t *nameres);
void oids_cleanup(void);
unsigned oid_subid2encoded(wmem_allocator_t *scope, unsigned len, uint32_t* subids, uint8_t** encoded_p);
unsigned oid_string2encoded(wmem_allocator_t *scope, const char *oid_str, uint8_t** encoded_p);
unsigned oid_encoded2subid(wmem_allocator_t *scope, const uint8_t *oid, int len, uint32_t** subids_p);
unsigned oid_encoded2subid_sub(wmem_allocator_t *scope, const uint8_t *oid_bytes, int oid_len, uint32_t** subids_pi,
                bool is_first);
unsigned oid_string2subid(wmem_allocator_t *scope, const char *oid_str, uint32_t** subids_p);
char* oid_encoded2string(wmem_allocator_t *scope, const uint8_t* encoded, unsigned len);
char* rel_oid_encoded2string(wmem_allocator_t *scope, const uint8_t* encoded, unsigned len);
char* oid_subid2string(wmem_allocator_t *scope, uint32_t *subids, unsigned len);
char* rel_oid_subid2string(wmem_allocator_t *scope, uint32_t *subids, unsigned len, bool is_absolute);
char *oid_resolved(wmem_allocator_t *scope, unsigned len, uint32_t *subids);
char *oid_resolved_from_encoded(wmem_allocator_t *scope, const uint8_t *oid, int len);
char *rel_oid_resolved_from_encoded(wmem_allocator_t *scope, const uint8_t *oid, int len);
char *oid_resolved_from_string(wmem_allocator_t *scope, const char *oid_str);
void oid_both(wmem_allocator_t *scope, unsigned oid_len, uint32_t *subids, char** resolved_p, char** numeric_p);
void oid_both_from_encoded(wmem_allocator_t *scope, const uint8_t *oid, int oid_len, char** resolved_p, char** numeric_p);
void oid_both_from_string(wmem_allocator_t *scope, const char *oid_str, char** resolved_p, char** numeric_p);
oid_info_t* oid_get(unsigned oid_len, uint32_t *subids, unsigned* matched_p, unsigned* left_p);
oid_info_t* oid_get_from_encoded(wmem_allocator_t *scope, const uint8_t *oid, int oid_len, uint32_t **subids, unsigned* matched, unsigned* left);
oid_info_t* oid_get_from_string(wmem_allocator_t *scope, const char *oid_str, uint32_t **subids, unsigned* matched, unsigned* left);
void oid_add(const char* name, unsigned oid_len, uint32_t *subids);
void oid_add_from_encoded(const char* name, const uint8_t *oid, int oid_len);
void oid_add_from_string(const char* name, const char *oid_str);
char *oid_get_default_mib_path(void);
#define subid_t uint32_t
#define add_oid_debug_subtree(a,b) ((void)0)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* __OIDS_H__ */

