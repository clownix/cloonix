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
typedef struct t_nat_cnx
{
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  int waiting_ack_add_lan;
  int timer_count;
  int add_llid;
  int add_tid;
  int waiting_ack_del_lan;
  int attached_lan_ok;
  int del_llid;
  int del_tid;
  int process_cmd_on;
  int process_running;
  int to_be_destroyed;
  int to_be_destroyed_timer_count;
  struct t_nat_cnx *prev;
  struct t_nat_cnx *next;
} t_nat_cnx;
/*--------------------------------------------------------------------------*/
char *dpdk_nat_get_mac(char *name, int num);
char *dpdk_nat_get_next(char *name);
void dpdk_nat_event_from_nat_dpdk_process(char *name, int on);
void dpdk_nat_resp_add_lan(int is_ko, char *lan, char *name);
void dpdk_nat_resp_del_lan(int is_ko, char *lan, char *name);
int dpdk_nat_get_qty(void);
int dpdk_nat_exists(char *name);
int dpdk_nat_lan_exists(char *lan);
int dpdk_nat_lan_exists_in_nat(char *name, char *lan);
void dpdk_nat_end_ovs(void);
int dpdk_nat_add_lan(int llid, int tid, char *name, char *lan);
int dpdk_nat_del_lan(int llid, int tid, char *name, char *lan);
int dpdk_nat_add(int llid, int tid, char *name);
int dpdk_nat_del(int llid, int tid, char *name);

t_nat_cnx *dpdk_nat_get_first(int *nb_nat);
t_topo_endp *translate_topo_endp_nat(int *nb);

void dpdk_nat_init(void);
/*--------------------------------------------------------------------------*/

