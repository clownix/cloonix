/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#define MAX_MAC_NB 20
typedef struct t_d2d_cnx
{
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  int ref_tid;
  int ms;
  int pkt_tx;
  int pkt_rx;
  int byte_tx;
  int byte_rx;
  char dist_cloonix[MAX_NAME_LEN];
  char dist_passwd[MSG_DIGEST_LEN];
  uint32_t dist_tcp_ip;
  uint32_t dist_tcp_port;
  uint32_t loc_udp_ip;
  uint32_t dist_udp_ip;
  uint16_t loc_udp_port;
  uint16_t dist_udp_port;
  int local_is_master;
  int tcp_connection_peered;
  int udp_connection_probed;
  int udp_connection_peered;
  int ovs_lan_attach_ready;
  int peer_llid;
  int peer_llid_connect;
  int peer_bad_status_received;
  int watchdog_count;
  int udp_dist_port_chosen;
  int udp_loc_port_chosen;
  int udp_probe_qty_sent;
  int udp_connection_tx_configured;
  int openvswitch_started_and_running;
  int lan_add_cli_llid;
  int lan_add_cli_tid;
  int lan_del_cli_llid;
  int lan_del_cli_tid;
  int waiting_ack_add_lan;
  int waiting_ack_del_lan;
  int lan_ovs_is_attached;
  int process_waiting_error;
  int master_del_req;
  int received_del_lan_req;
  int nb_loc_mac;
  int nb_dist_mac;
  t_d2d_mac loc_tabmac[MAX_MAC_NB];
  t_d2d_mac dist_tabmac[MAX_MAC_NB];
  struct t_d2d_cnx *prev;
  struct t_d2d_cnx *next;
} t_d2d_cnx;

t_d2d_cnx *dpdk_d2d_find_with_peer_llid_connect(int llid);
t_d2d_cnx *dpdk_d2d_find_with_peer_llid(int llid);
t_d2d_cnx *dpdk_d2d_find(char *name);
t_d2d_cnx *dpdk_d2d_get_first(int *nb_d2d_cnx);
int dpdk_d2d_lan_exists(char *lan);
int dpdk_d2d_lan_exists_in_d2d(char *name, char *lan, int activ);


void dpdk_d2d_resp_add_lan(int is_ko, char *lan, char *name);
void dpdk_d2d_resp_del_lan(int is_ko, char *lan, char *name);

void dpdk_d2d_event_from_d2d_dpdk_process(char *name, int on);
int dpdk_d2d_del(char *name, char *err);

int dpdk_d2d_add(char *name, uint32_t local_udp_ip,
                 char *dist, uint32_t ip, uint16_t port,
                 char *passwd, uint32_t udp_ip);

void dpdk_d2d_peer_add(int llid, int tid, char *name, char *dist, char *loc);

void dpdk_d2d_peer_add_ack(int llid, int tid, char *name,
                           char *dist, char *loc, int ack);

void dpdk_d2d_peer_conf(int llid, int tid, char *d2d_name,
                        int status, char *dist, char *loc,
                        uint32_t dist_udp_ip,   uint32_t loc_udp_ip,
                        uint16_t dist_udp_port, uint16_t loc_udp_port);

void dpdk_d2d_peer_ping(int llid, int tid, char *name, int status);

void dpdk_d2d_vhost_started(char *name);
void dpdk_d2d_get_udp_port_done(char *name, uint16_t udp_port, int status);
void dpdk_d2d_dist_udp_ip_port_done(char *name);
void dpdk_d2d_receive_probe_udp(char *name);
int  dpdk_d2d_get_qty(void);
void dpdk_d2d_all_del(void);
void dpdk_d2d_add_lan(int llid, int tid, char *name, char *lan);
void dpdk_d2d_del_lan(int llid, int tid, char *name, char *lan);

char *dpdk_d2d_get_next_matching_lan(char *lan, char *name);
int dpdk_d2d_get_strmac(char *name, t_d2d_mac **tabmac);

t_topo_endp *dpdk_d2d_mngt_translate_topo_endp(int *nb);

void dpdk_d2d_peer_mac(int llid, int tid, char *d2d_name,
                       int nb_mac, t_d2d_mac *tabmac);

void dpdk_d2d_process_possible_change(char *lan);

int dpdk_d2d_eventfull(char *name, int ms, int ptx, int btx, int prx, int brx);
int dpdk_d2d_collect_dpdk(t_eventfull_endp *eventfull);

void dpdk_d2d_init(void);
/*--------------------------------------------------------------------------*/




