/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
typedef struct t_a2b_side
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
} t_a2b_side;

typedef struct t_a2b_cnx
{
  char name[MAX_NAME_LEN];
  t_a2b_side side[2];
  int to_be_destroyed;
  int openvswitch_started_and_running;
  int vhost_started_and_running;
  int process_started_and_running;
  int state_up;
  int state_down;
  int add_llid;
  int add_tid;
  int timer_count;
  struct t_a2b_cnx *prev;
  struct t_a2b_cnx *next;
} t_a2b_cnx;

void dpdk_a2b_vhost_started(char *name);
t_a2b_cnx *dpdk_a2b_get_first(int *nb_a2b);
int dpdk_a2b_collect_dpdk(t_eventfull_endp *eventfull);
int dpdk_a2b_eventfull(char *name, int num, int ms,
                       int ptx, int btx, int prx, int brx);
void dpdk_a2b_event_from_a2b_dpdk_process(char *name, int on);
void dpdk_a2b_resp_add_lan(int is_ko, char *lan, char *name, int num);
void dpdk_a2b_resp_del_lan(int is_ko, char *lan, char *name, int num);
int dpdk_a2b_get_qty(void);
int dpdk_a2b_lan_exists(char *lan);
int dpdk_a2b_lan_exists_in_a2b(char *name, int num, char *lan);
void dpdk_a2b_end_ovs(void);
int dpdk_a2b_add_lan(int llid, int tid, char *name, int num, char *lan);
int dpdk_a2b_del_lan(int llid, int tid, char *name, int num, char *lan);
int dpdk_a2b_add(int llid, int tid, char *name);
int dpdk_a2b_del(char *name);
int dpdk_a2b_exists(char *name);
t_topo_endp *dpdk_a2b_mngt_translate_topo_endp(int *nb);
void dpdk_a2b_cnf(char *name, int dir, int type, int val);
void dpdk_a2b_process_possible_change(char *lan);
void dpdk_a2b_init(void);
/*--------------------------------------------------------------------------*/

