/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Wire values shared with other builds (clients, c2c peers). Since 76 never
 * break: never remove/reorder an enum (retire as <name>_reserved_N), add only
 * before _max, new fields at the end. debug/check_wire.sh checks against 76.
 * Version is logged, never enforced. The guest agent is not a party. */
#ifndef GLOB_WIRE_H
#define GLOB_WIRE_H

/* Sent in the leg request. Bump VERSION on additions; move MIN only on a deliberate break. */
/* 78: nat_add carries a ports field; empty from an older client. */
/* 79: cnt_cnf, the cap of a running container; images_req/resp, bulk. */
#define CLOONIX_WIRE_VERSION     79
#define CLOONIX_WIRE_VERSION_MIN 76
#define CLOONIX_WIRE_VERSION_FMT " wire=%d"

enum{
  doors_type_min = 100,

  doors_type_listen_server,
  doors_type_server,

  doors_type_high_prio_begin,
  doors_type_switch,
  doors_type_high_prio_end,

  doors_type_low_prio_begin,
  doors_type_dbssh,
  doors_type_openssh,  /* retired: kept for the numbering, answered link_ko */
  doors_type_vnc,      /* retired: the canvas reaches vnc through the gui server */
  doors_type_low_prio_end,

  doors_type_max,
};

enum{
  doors_val_min = 200,
  doors_val_c2c,
  doors_val_none,
  doors_val_sig,
  doors_val_init_link,
  doors_val_link_ok,
  doors_val_link_ko,
  doors_val_max,
};

enum{
  header_type_traffic = 999,
  header_type_vwifi_cli, /* retired */
  header_type_vwifi_spy, /* retired */
  header_type_vwifi_ctr, /* retired */
  header_type_ctrl,
  header_type_ctrl_agent,
  header_type_stats,     /* retired: sysinfo is no longer asked of an agent */
  /* the radios of a kvm: freq, rate, index, 802.11 frame; on, ko, off */
  header_type_wifi_frame,
  header_type_wifi_ctrl,
  /* agent to host: one KERR line of the guest, for the syslog of the net */
  header_type_kerr,

};

enum{
  header_val_none = 777,
  header_val_vwifi_syn_ok, /* retired */
  header_val_vwifi_syn_ko, /* retired */
  header_val_vwifi_trf_ok, /* retired */
  header_val_vwifi_trf_ko, /* retired */
  header_val_vwifi_bad_ko, /* retired */
  header_val_add_dido_llid,
  header_val_del_dido_llid,
  header_val_ping,
  header_val_ack,
  header_val_reboot,
  header_val_halt,
  header_val_sysinfo,    /* retired */
  header_val_sysinfo_df, /* retired */
  /* Appended, never inserted. clip_get carries a marker: a zero payload
   * length is fatal to rx_pktbuf_fill. */
  header_val_clip_set,
  header_val_clip_get,
  header_val_clip_txt,
  header_val_wifi_on,
  header_val_wifi_ko,
  header_val_wifi_off,
  /* host to agent: the addresses on the air of a radio, its index then
   6 bytes each, for the acks the agent gives */
  header_val_wifi_peers,
  /* header_type_ctrl_agent on a byte pipe, either way: the side that sends
     it has too much of that pipe queued, the other stops sending it bytes
     until xon */
  header_val_xoff,
  header_val_xon,

};

#endif
