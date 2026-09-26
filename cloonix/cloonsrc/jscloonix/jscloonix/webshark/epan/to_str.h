/** @file
 * Definitions for utilities to convert various other types to strings.
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __TO_STR_H__
#define __TO_STR_H__

#include "wsutil/nstime.h"
#include <wsutil/inet_cidr.h>
#include <epan/proto.h>
#include <epan/packet_info.h>
#include <epan/wmem_scopes.h>
#include <wsutil/to_str.h>

#define GUID_STR_LEN     37
#define MAX_ADDR_STR_LEN 256
#define VINES_ADDR_LEN   6
#define EUI64_STR_LEN    24
#define AX25_ADDR_LEN    7
#define FCWWN_ADDR_LEN   8


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


char *address_to_str(wmem_allocator_t *scope, const address *addr);
char *address_with_resolution_to_str(wmem_allocator_t *scope, const address *addr);
const char *address_to_name(const address *addr);
char *address_to_display(wmem_allocator_t *allocator, const address *addr);
void address_to_str_buf(const address *addr, char *buf, int buf_len);
const char *port_type_to_str (port_type type);
char* tvb_address_with_resolution_to_str(wmem_allocator_t *scope, tvbuff_t *tvb, int type, const int offset);

#define tvb_ether_to_str(scope, tvb, offset) tvb_address_to_str(scope, tvb, AT_ETHER, offset)

#define tvb_ip_to_str(scope, tvb, offset) tvb_address_to_str(scope, tvb, AT_IPv4, offset)

#define tvb_ip6_to_str(scope, tvb, offset) tvb_address_to_str(scope, tvb, AT_IPv6, offset)

#define tvb_fcwwn_to_str(scope, tvb, offset) tvb_address_to_str(scope, tvb, AT_FCWWN, offset)

#define tvb_fc_to_str(scope, tvb, offset) tvb_address_to_str(scope, tvb, AT_FC, offset)

#define tvb_eui64_to_str(scope, tvb, offset) tvb_address_to_str(scope, tvb, AT_EUI64, offset)

char* tvb_address_to_str(wmem_allocator_t *scope, tvbuff_t *tvb, int type, const int offset);
char* tvb_address_var_to_str(wmem_allocator_t *scope, tvbuff_t *tvb, address_type type, const int offset, int length);
#define ABS_TIME_TO_STR_SHOW_ZONE       (1U << 0)
#define ABS_TIME_TO_STR_ADD_DQUOTES     (1U << 1)
#define ABS_TIME_TO_STR_SHOW_UTC_ONLY   (1U << 2)
#define ABS_TIME_TO_STR_ISO8601         (1U << 3)
char *abs_time_to_str_ex(wmem_allocator_t *scope,
                                        const nstime_t *, field_display_e fmt,
                                        int flags);

#define abs_time_to_str(scope, nst, fmt, show_zone) \
        abs_time_to_str_ex(scope, nst, fmt, (show_zone) ? ABS_TIME_TO_STR_SHOW_ZONE : 0)

char *
abs_time_to_unix_str(wmem_allocator_t *scope, const nstime_t *rel_time);

char *abs_time_secs_to_str_ex(wmem_allocator_t *scope,
                                        const time_t, field_display_e fmt,
                                        int flags);

#define abs_time_secs_to_str(scope, nst, fmt, show_zone) \
        abs_time_secs_to_str_ex(scope, nst, fmt, (show_zone) ? ABS_TIME_TO_STR_SHOW_ZONE : 0)

char *signed_time_secs_to_str(wmem_allocator_t *scope, const int32_t time_val);
char *unsigned_time_secs_to_str(wmem_allocator_t *scope, const uint32_t);
char *signed_time_msecs_to_str(wmem_allocator_t *scope, int32_t time_val);
char *rel_time_to_str(wmem_allocator_t *scope, const nstime_t *);
char *rel_time_to_secs_str(wmem_allocator_t *scope, const nstime_t *);
char *guid_to_str_buf(const e_guid_t *, char *, int);
char *guid_to_str(wmem_allocator_t *scope, const e_guid_t *);
char *decode_bits_in_field(wmem_allocator_t *scope, const unsigned bit_offset, const int no_of_bits, const uint64_t value, const unsigned encoding);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __TO_STR_H__  */
