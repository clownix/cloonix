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
t_topo_endp *translate_topo_endp_ethdv(int *nb);
int dpdk_kvm_exists(char *name, int num);
void dpdk_kvm_add_whole_vm(char *name, int nb_tot_eth, t_eth_table *eth_tab);
void dpdk_kvm_resp_add_lan(int is_ko, char *lan, char *name, int num);
void dpdk_kvm_resp_del_lan(int is_ko, char *lan, char *name, int num);
int dpdk_kvm_add_lan(int llid, int tid, char *name, int num,
                     char *lan, int endp_type);
int dpdk_kvm_del_lan(int llid, int tid, char *name, int num,
                     char *lan, int endp_type);
int dpdk_kvm_del(int llid, int tid, char *name, int num, int endp_type);
void dpdk_kvm_resp_add(int is_ko, char *name, int num);
void dpdk_kvm_resp_add_eths2(int is_ko, char *name, int num);
void dpdk_kvm_resp_del(int is_ko, char *name, int num);
void dpdk_kvm_init(void);
/*--------------------------------------------------------------------------*/

