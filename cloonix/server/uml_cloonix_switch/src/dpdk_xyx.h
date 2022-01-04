/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
typedef struct t_xyx_lan
{
  char lan[MAX_NAME_LEN];
  int ms;
  int pkt_tx;
  int pkt_rx;
  int byte_tx;
  int byte_rx;
  int delay;
  int loss;
  int qsize;
  int bsize;
  int brate;
  int waiting_ack_add_lan;
  int waiting_ack_del_lan;
  int attached_lan_ok;
  int timer_count;
  int llid;
  int tid;

} t_xyx_lan;

typedef struct t_xyx_cnx
{
  char name[MAX_NAME_LEN];
  char nm[MAX_NAME_LEN];
  int num;
  int endp_type;
  t_xyx_lan lan;
  int arp_mangle;
  uint8_t arp_mac[6];
  int to_be_destroyed;
  int to_be_destroyed_count;
  int ovs_started_and_running;
  int vhost_started_and_running;
  int process_started_and_running;
  int state_up;
  int state_down;
  int add_llid;
  int add_tid;
  int timer_count;
  int eths_done_ok;
  struct t_xyx_cnx *prev;
  struct t_xyx_cnx *next;
} t_xyx_cnx;

t_xyx_cnx *dpdk_xyx_get_first(int *nb_tap, int *nb_phy, int *nb_kvm);
void dpdk_xyx_vhost_started(char *name);
int dpdk_xyx_collect_dpdk(t_eventfull_endp *eventfull);
int dpdk_xyx_eventfull(char *name, int ms, int ptx, int btx, int prx, int brx);
void dpdk_xyx_event_from_xyx_dpdk_process(char *name, int on);
void dpdk_xyx_resp_add_lan(int is_ko, char *lan, char *name, int num);
void dpdk_xyx_resp_del_lan(int is_ko, char *lan, char *name, int num);
int dpdk_xyx_get_qty(void);
int dpdk_xyx_lan_exists(char *lan);
int dpdk_xyx_lan_empty(char *name, int num);
int dpdk_xyx_lan_exists_in_xyx(char *name, int num, char *lan);
void dpdk_xyx_end_ovs(void);
int dpdk_xyx_add_lan(int llid, int tid, char *name, int num, char *lan);
int dpdk_xyx_del_lan(int llid, int tid, char *name, int num, char *lan);
int dpdk_xyx_add(int llid, int tid, char *name, int num, int endp_type);
int dpdk_xyx_del(int llid, int tid, char *name, int num);
int dpdk_xyx_exists(char *name, int num);
int dpdk_xyx_exists_fullname(char *name);
int dpdk_xyx_ready(char *name, int num);
int dpdk_xyx_name_exists(char *name);
t_topo_endp *translate_topo_endp_xyx(int *nb);
void dpdk_xyx_resp_add(int is_ko, char *name, int num);
void dpdk_xyx_resp_del(int is_ko, char *name, int num);
void dpdk_xyx_cnf(char *name, int type, uint8_t *mac);
void dpdk_xyx_eths2_resp_ok(int is_ok, char *name, int num);
void dpdk_xyx_init(void);
/*--------------------------------------------------------------------------*/

