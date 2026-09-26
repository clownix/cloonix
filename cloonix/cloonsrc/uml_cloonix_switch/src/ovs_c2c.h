/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef OVS_C2C_H
#define OVS_C2C_H
#include "commun_daemon.h"
typedef struct t_ovs_c2c
{
  int get_udp_port_done;
  int must_restart;
  char socket[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];
  char mac[MAX_NAME_LEN];
  char dist_passwd[MSG_DIGEST_LEN];
  int  destroy_c2c_req;
  int  del_snf_ethv_sent;
  int  free_c2c_req;
  int count;
  int ovs_llid;
  int pid;
  int c2c_id;
  int closed_count;
  int closed_llid_received;
  int suid_root_done;
  int peer_conf_done;
  t_topo_c2c topo;

  int ref_tid;
  int udp_connection_probed;
  int pair_llid;
  int peer_llid;
  int peer_listen_llid;
  long long watch_ms;
  int peer_silence_ms;
  int probe_silence_ms;
  int udp_dist_port_chosen;
  int udp_loc_port_chosen;
  int udp_connection_tx_configured;
  int waiting_ack_add_lan;
  int waiting_ack_del_lan;
  int process_waiting_error;
  int master_del_req;
  int received_del_lan_req;
  int nb_dist_mac;
  int state_up;
  int c2c_udp_port_low;

  int destroy_c2c_done;

  int recv_delete_req_from_client;

  int count_state_master_c2c_start_done;
  int count_state_master_up_initialised;
  int count_state_master_try_connect_to_peer;
  /* Master reborn from a restart: bounded wait for its peer, a fresh one waits for ever. */
  int born_from_restart;
  /* Set when this side has given up for good: carefull_destroy_c2c must then
     let it die without re-arming the restart a master normally gets. */
  int give_up_no_restart;
  int count_state_master_connection_peered;
  int count_state_master_udp_peer_conf_sent;
  int count_state_master_udp_peer_conf_received;
  int count_state_udp_connection_tx_configured;
  int count_state_slave_up_initialised;
  int count_state_slave_connection_peered;
  int count_state_slave_udp_peer_conf_sent;
  int count_state_slave_udp_peer_conf_received;

  struct t_ovs_c2c *prev;
  struct t_ovs_c2c *next;
} t_ovs_c2c;

t_ovs_c2c *ovs_c2c_get_first(int *nb_c2c);
void ovs_c2c_llid_closed(int llid, int from_clone);
void ovs_c2c_pid_resp(int llid, char *name, int pid);
int  ovs_c2c_get_all_pid(t_lst_pid **lst_pid);
int  ovs_c2c_diag_llid(int llid);
void ovs_c2c_sigdiag_resp(int llid, int tid, char *line);
void ovs_c2c_poldiag_resp(int llid, int tid, char *line);
t_ovs_c2c *ovs_c2c_exists(char *name);
void ovs_c2c_attlan_clear(char *name);
int  ovs_c2c_dyn_snf(char *name, int val);

void ovs_c2c_add(int llid, int tid, char *name, uint32_t loc_udp_ip,
                 char *dist, uint32_t dist_ip, uint16_t dist_port,
                 char *dist_passwd, uint32_t dist_udp_ip,
                 uint16_t c2c_udp_port_low);

void ovs_c2c_del(int llid, int tid, char *name);
void ovs_c2c_topo_endp_push(void);

t_ovs_c2c *ovs_c2c_find_with_pair_llid(int llid);
/* 1 the c2c process is up, 0 not yet, -1 gone, err filled */
int ovs_c2c_up_test(void *ctx, char *err);

void ovs_c2c_peer_conf(int llid, int tid, char *name, int peer_status,
                       char *dist,     char *loc,
                       uint32_t dist_udp_ip,   uint32_t loc_udp_ip,
                       uint16_t dist_udp_port, uint16_t loc_udp_port);

void ovs_c2c_peer_ping(int llid, int tid, char *name, int peer_status);
void ovs_c2c_peer_add(int llid, int tid, char *name, char *dist,
                      char *loc, uint16_t c2c_udp_port_low);

t_ovs_c2c *find_c2c(char *name);
void ovs_c2c_peer_add_ack(int llid, int tid, char *name,
                          char *dist, char *loc, int ack);

int ovs_c2c_mac_mangle(char *name, uint8_t *mac);
void ovs_c2c_proxy_dist_udp_ip_port_OK(char *name,
                                       uint32_t ip, uint16_t udp_port);

void ovs_c2c_proxy_dist_tcp_ip_port_OK(char *name,
                                       uint32_t ip, uint16_t tcp_port);

void ovs_c2c_transmit_get_free_udp_port(char *name, uint16_t udp_port);
void carefull_destroy_c2c(t_ovs_c2c *cur);
void ovs_c2c_init(void);

#endif
