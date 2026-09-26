/** @file
 *
 * Copyright 1998 Mike Hall <mlh@io.com>
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef __FOLLOW_H__
#define __FOLLOW_H__

#include <epan/epan.h>
#include <epan/packet.h>
#include <wsutil/inet_cidr.h>
#include <epan/tap.h>
#include <epan/wmem_scopes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Show Stream */
typedef enum {
    FROM_CLIENT,
    FROM_SERVER,
    BOTH_HOSTS
} show_stream_t;

typedef union _stream_addr {
  uint32_t ipv4;
  ws_in6_addr ipv6;
} stream_addr;

struct _follow_info;

#define SUBSTREAM_UNUSED	UINT64_C(0xFFFFFFFFFFFFFFFF)

typedef struct {
    bool is_server;
    uint32_t packet_num;
    uint32_t seq; /* TCP only */
    nstime_t abs_ts; /**< Packet absolute time stamp */
    GByteArray *data;
} follow_record_t;

typedef struct _follow_info {
    show_stream_t   show_stream;
    char            *filter_out_filter;
    GList           *payload;   /* "follow_record_t" entries, in reverse order. */
    unsigned        bytes_written[2]; /* Index with FROM_CLIENT or FROM_SERVER for readability. */
    uint32_t        seq[2]; /* TCP only */
    GList           *fragments[2]; /* TCP only */
    unsigned        client_port;
    unsigned        server_port;
    address         client_ip;
    address         server_ip;
    void*           gui_data;
    uint64_t        substream_id;  /**< Sub-stream; used only by HTTP2 and QUIC */
} follow_info_t;

struct register_follow;
typedef struct register_follow register_follow_t;

typedef char* (*follow_conv_filter_func)(epan_dissect_t *edt, packet_info *pinfo, unsigned *stream, unsigned *sub_stream);
typedef char* (*follow_index_filter_func)(unsigned stream, unsigned sub_stream);
typedef char* (*follow_address_filter_func)(address* src_addr, address* dst_addr, int src_port, int dst_port);
typedef char* (*follow_port_to_display_func)(wmem_allocator_t *allocator, unsigned port);
typedef uint32_t (*follow_stream_count_func)(void);
typedef bool (*follow_sub_stream_id_func)(unsigned stream, unsigned sub_stream, bool le, unsigned *sub_stream_out);
void register_follow_stream(const int proto_id, const char* tap_listener,
                            follow_conv_filter_func conv_filter, follow_index_filter_func index_filter, follow_address_filter_func address_filter,
                            follow_port_to_display_func port_to_display, tap_packet_cb tap_handler,
                            follow_stream_count_func stream_count, follow_sub_stream_id_func sub_stream_id);
int get_follow_proto_id(register_follow_t* follower);
const char* get_follow_tap_string(register_follow_t* follower);
register_follow_t* get_follow_by_name(const char* proto_short_name);
register_follow_t* get_follow_by_proto_id(const int proto_id);
follow_conv_filter_func get_follow_conv_func(register_follow_t* follower);
follow_index_filter_func get_follow_index_func(register_follow_t* follower);
follow_address_filter_func get_follow_address_func(register_follow_t* follower);
follow_port_to_display_func get_follow_port_to_display(register_follow_t* follower);
tap_packet_cb get_follow_tap_handler(register_follow_t* follower);
follow_stream_count_func get_follow_stream_count_func(register_follow_t* follower);
follow_sub_stream_id_func get_follow_sub_stream_id_func(register_follow_t* follower);
tap_packet_status follow_tvb_tap_listener(void *tapdata, packet_info *pinfo, epan_dissect_t *edt , const void *data, tap_flags_t flags);
void follow_iterate_followers(wmem_foreach_func func, void *user_data);
char* follow_get_stat_tap_string(register_follow_t* follower);
void follow_reset_stream(follow_info_t* info);
void follow_info_free(follow_info_t* follow_info);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
