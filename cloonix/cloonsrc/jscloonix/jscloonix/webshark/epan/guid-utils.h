/** @file
 * Definitions for GUID handling
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 *
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __GUID_UTILS_H__
#define __GUID_UTILS_H__

#include <stdint.h>
#include <epan/wmem_scopes.h>

#define GUID_LEN	16

/* Note: this might be larger than GUID_LEN, so don't overlay data in packets
   with this. */
typedef struct _e_guid_t {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} e_guid_t;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void guids_init(void);

void guids_add_guid(const e_guid_t *guid, const char *name);

void guids_delete_guid(const e_guid_t *guid);

const char *guids_get_guid_name(const e_guid_t *guid, wmem_allocator_t *scope);

const char* guids_resolve_guid_to_str(const e_guid_t *guid, wmem_allocator_t *scope);

#define guids_add_uuid(uuid, name) guids_add_guid((const e_guid_t *) (uuid), (name))

#define guids_get_uuid_name(uuid, scope) guids_get_guid_name((e_guid_t *) (uuid), scope)

#define guids_resolve_uuid_to_str(uuid) guids_resolve_guid_to_str((e_guid_t *) (uuid))

int guid_cmp(const e_guid_t *g1, const e_guid_t *g2);

unsigned guid_hash(const e_guid_t *guid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GUID_UTILS_H__ */
