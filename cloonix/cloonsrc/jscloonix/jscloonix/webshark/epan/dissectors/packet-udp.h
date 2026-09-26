/* packet-udp.h
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __PACKET_UDP_H__
#define __PACKET_UDP_H__

#include <epan/conversation.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _e_udphdr {
    uint16_t uh_sport;
    uint16_t uh_dport;
    uint32_t uh_ulen;
    uint32_t uh_sum_cov;
    uint16_t uh_sum;
    uint32_t uh_stream;
    address ip_src;
    address ip_dst;
} e_udphdr;

typedef struct _udp_flow_t {
    uint32_t process_uid;    /* UID of local process */
    uint32_t process_pid;    /* PID of local process */
    char *username;        /* Username of the local process */
    char *command;         /* Local process name + path + args */
} udp_flow_t;

struct udp_analysis {
    udp_flow_t  flow1;
    udp_flow_t  flow2;
    udp_flow_t  *fwd;
    udp_flow_t  *rev;
    uint32_t    stream;
    uint32_t    pnum;
    nstime_t    ts_first;
    nstime_t    ts_prev;
};
extern void
add_udp_process_info(uint32_t frame_num, address *local_addr, address *remote_addr,
                     uint16_t local_port, uint16_t remote_port,
                     uint32_t uid, uint32_t pid,
                     char *username, char *command);

uint32_t get_udp_stream_count(void);
void decode_udp_ports(tvbuff_t *, int, packet_info *, proto_tree *, int, int, int);
struct udp_analysis * get_udp_conversation_data(conversation_t *, packet_info *);
int udp_dissect_pdus(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree,
                 unsigned fixed_len,
                 bool (*heuristic_check)(packet_info *, tvbuff_t *, int, void*),
                 unsigned (*get_pdu_len)(packet_info *, tvbuff_t *, int, void*),
                 dissector_t dissect_pdu, void* dissector_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
