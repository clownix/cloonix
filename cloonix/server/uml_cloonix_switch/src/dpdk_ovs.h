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
int dpdk_ovs_get_dpdk_usable(void);
int dpdk_ovs_muovs_ready(void);

int dpdk_ovs_get_nb(void);
int  dpdk_ovs_try_send_diag_msg(int tid, char *cmd);
void dpdk_ovs_erase_endp_path(char *name, int nb_dpdk);
void dpdk_ovs_rpct_recv_diag_msg(int llid, int tid, char *line);
void dpdk_ovs_pid_resp(int llid, char *name, int toppid, int pid);
int  dpdk_ovs_find_with_llid(int llid);
void dpdk_ovs_add_vm(char *name, int nb_tot_eth, t_eth_table *eth_tab);
void dpdk_ovs_del_vm(char *name);
void dpdk_ovs_ack_add_eth_vm(char *name, int is_ko);
int  dpdk_ovs_eth_exists(char *name, int num);
void dpdk_ovs_init(void);
int  dpdk_ovs_get_all_pid(t_lst_pid **lst_pid);
char *dpdk_ovs_format_net(t_vm *vm, int eth, int tot_eth);
int dpdk_ovs_still_present(void);
t_topo_endp *dpdk_ovs_translate_topo_endp(int *nb);
void dpdk_ovs_urgent_client_destruct(void);
/*--------------------------------------------------------------------------*/



