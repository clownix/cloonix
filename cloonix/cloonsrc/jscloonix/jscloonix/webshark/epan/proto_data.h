/* proto_data.h
 * Definitions for protocol-specific data
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __PROTO_DATA_H__
#define __PROTO_DATA_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void p_add_proto_data(wmem_allocator_t *scope, struct _packet_info* pinfo, int proto, uint32_t key, void *proto_data);
void p_set_proto_data(wmem_allocator_t *scope, struct _packet_info* pinfo, int proto, uint32_t key, void *proto_data);
void *p_get_proto_data(wmem_allocator_t *scope, struct _packet_info* pinfo, int proto, uint32_t key);
void p_remove_proto_data(wmem_allocator_t *scope, struct _packet_info* pinfo, int proto, uint32_t key);
char *p_get_proto_name_and_key(wmem_allocator_t *scope, struct _packet_info* pinfo, unsigned pfd_index);
void p_set_proto_depth(struct _packet_info* pinfo, int proto, unsigned depth);
unsigned p_get_proto_depth(struct _packet_info* pinfo, int proto);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* __PROTO_DATA__ */
