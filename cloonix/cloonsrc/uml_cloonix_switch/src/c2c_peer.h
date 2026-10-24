/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
enum {
  state_idle = 0,
  state_master_c2c_start_done,
  state_master_up_initialised,
  state_master_try_connect_to_peer,
  state_master_connection_peered,
  state_master_udp_peer_conf_sent,
  state_master_udp_peer_conf_received,
  state_master_up_process_running,

  state_udp_connection_tx_configured,

  state_slave_up_initialised,
  state_slave_connection_peered,
  state_slave_udp_peer_conf_sent,
  state_slave_udp_peer_conf_received,
  state_slave_up_process_running,
};

enum {
status_peer_ping_zero = 0,
status_peer_ping_one,
status_peer_ping_two,
status_peer_ping_error = -1,
/* the master deleted by its client: the slave lets its lan go */
status_peer_ping_deleted = -2,
};

enum {
cmd_mac_mangle,
};

void peer_doorways_client_tx_status(char *name, int status);
void wrap_proxy_callback_connect(char *name, int pair_llid);
void wrap_try_connect_to_peer(t_ovs_c2c *c2c);
void wrap_send_c2c_peer_create(t_ovs_c2c *c2c, int is_ack);
void wrap_send_c2c_peer_ping(t_ovs_c2c *c2c,  int status_peer_ping);
int wrap_send_c2c_peer_conf(t_ovs_c2c *cur, int is_ack);

