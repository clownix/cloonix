/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
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
  int suid_root_done;
  int peer_conf_done;
  t_topo_c2c topo;



  int ref_tid;
  int udp_connection_probed;
  int pair_llid;
  int peer_llid;
  int peer_listen_llid;
  int peer_watchdog_count;
  int udp_dist_port_chosen;
  int udp_loc_port_chosen;
  int udp_probe_qty_sent;
  int udp_connection_tx_configured;
  int cli_llid;
  int cli_tid;
  int waiting_ack_add_lan;
  int waiting_ack_del_lan;
  int process_waiting_error;
  int master_del_req;
  int received_del_lan_req;
  int nb_dist_mac;
  int state_up;

  int destroy_c2c_done;

  char lan_added[MAX_NAME_LEN];
  char lan_waiting[MAX_NAME_LEN];
  char lan_attached[MAX_NAME_LEN];
  char must_restart_lan[MAX_NAME_LEN];

  int must_call_snf_started;

  int recv_delete_req_from_client;

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
void ovs_c2c_resp_add_lan(int is_ko, char *name, int num, char *vhost, char *lan);
void ovs_c2c_resp_del_lan(int is_ko, char *name, int num, char *vhost, char *lan);
t_ovs_c2c *ovs_c2c_exists(char *name);
int  ovs_c2c_dyn_snf(char *name, int val);

void ovs_c2c_add(int llid, int tid, char *name, uint32_t loc_udp_ip,
                 char *dist, uint32_t dist_ip, uint16_t dist_port,
                 char *dist_passwd, uint32_t dist_udp_ip);

void ovs_c2c_del(int llid, int tid, char *name);
void ovs_c2c_add_lan(int llid, int tid, char *name, char *lan);
void ovs_c2c_del_lan(int llid, int tid, char *name, char *lan);
t_topo_endp *ovs_c2c_translate_topo_endp(int *nb);

t_ovs_c2c *ovs_c2c_find_with_pair_llid(int llid);
t_ovs_c2c *ovs_c2c_find_with_peer_llid(int llid);
t_ovs_c2c *ovs_c2c_find_with_peer_listen_llid(int llid);

void ovs_c2c_peer_conf(int llid, int tid, char *name, int peer_status,
                       char *dist,     char *loc,
                       uint32_t dist_udp_ip,   uint32_t loc_udp_ip,
                       uint16_t dist_udp_port, uint16_t loc_udp_port);

void ovs_c2c_peer_ping(int llid, int tid, char *name, int peer_status);
void ovs_c2c_peer_add(int llid, int tid, char *name, char *dist, char *loc);

t_ovs_c2c *find_c2c(char *name);
void ovs_c2c_peer_add_ack(int llid, int tid, char *name,
                          char *dist, char *loc, int ack);

int ovs_c2c_mac_mangle(char *name, uint8_t *mac);
void ovs_c2c_proxy_dist_udp_ip_port_OK(char *name,
                                       uint32_t ip, uint16_t udp_port);

void ovs_c2c_proxy_dist_tcp_ip_port_OK(char *name,
                                       uint32_t ip, uint16_t tcp_port);

void ovs_c2c_transmit_get_free_udp_port(char *name, uint16_t udp_port);
void ovs_c2c_init(void);
/*--------------------------------------------------------------------------*/
