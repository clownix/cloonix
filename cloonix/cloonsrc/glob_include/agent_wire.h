/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* What an insider agent and the host say to each other. No include: it is */
/* compiled by gcc and by mingw, and it is the one place the wire is read.  */
#ifndef AGENT_WIRE_H
#define AGENT_WIRE_H

#define MAX_A2D_LEN 50100
#define MAX_RPC_MSG_LEN 50000

#define LABREAK "link_agent_break"
#define LABOOT  "link_agent_reboot %d"
#define LAHALT  "link_agent_halt %d"
#define LAPING  "link_agent_ping %d"

/* The opening of a byte pipe: the host asks with the first, and the only
   answer that means yes is the second. */
#define DBSSH_SERV_DOORS_REQ "DBSSH_SERV_DOORS_REQ cookie=%s"
#define DBSSH_SERV_DOORS_RESP "DBSSH_SERV_DOORS_RESP"

/* The radios of a kvm, header_type_wifi_ctrl: asked with header_val_wifi_on
   and this text, radio i taking the mac with its last byte plus i; answered
   with wifi_on, or wifi_ko and why; wifi_off when frames come to an agent
   that has no radio. A header_type_wifi_frame carries the freq, the rate
   and the index of the radio, 4 bytes each big endian, then the frame. */
#define KVMWIFI_ON_REQ "kvmwifi_on nb=%d mac=%02x:%02x:%02x:%02x:%02x:%02x"
#define KVMWIFI_FRAME_HEAD 12
#define KVMWIFI_MAX_PEERS 64

/* 16 bytes: llid, vwifi base and cid, len, type, val, then CAFEDECA. */
int sock_header_get_size(void);

void sock_header_set_info(char *tx, int llid_dido,
                          int vwifi_base, int vwifi_cid,
                          int type, int val, int len, char **ntx);

/* 0 when the magic is there, -1 otherwise, everything zeroed. */
int sock_header_get_info(char *rx, int *llid_dido,
                         int *vwifi_base, int *vwifi_cid,
                         int *type, int *val, int *len, char **nrx);

#endif
