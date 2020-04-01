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
void dpdk_dyn_add_eth(t_vm *kvm, char *name, int num);
int  dpdk_dyn_del_all_lan(char *name);
void dpdk_dyn_end_vm_qmp_shutdown(char *name, int num);

int dpdk_dyn_add_lan_to_eth(int llid, int tid, char *lan_name,
                            char *name, int num, char *info);
int dpdk_dyn_del_lan_from_eth(int llid, int tid, char *lan_name,
                              char *name, int num, char *info);

void dpdk_dyn_ack_add_lan_eth_OK(char *lan, char *name, int num, char *lab);
void dpdk_dyn_ack_add_lan_eth_KO(char *lan, char *name, int num, char *lab);
void dpdk_dyn_ack_del_lan_eth_OK(char *lan, char *name, int num, char *lab);
void dpdk_dyn_ack_del_lan_eth_KO(char *lan, char *name, int num, char *lab);

void dpdk_dyn_ack_add_eth_OK(char *name, int num, char *lab);
void dpdk_dyn_ack_add_eth_KO(char *name, int num, char *lab);

int dpdk_dyn_topo_endp(char *name, int num, t_topo_endp *endp);

int dpdk_dyn_eth_exists(char *name, int num);
int dpdk_dyn_lan_exists(char *name);
int dpdk_dyn_lan_exists_in_vm(char *name, int num, char *lan);
int dpdk_dyn_is_all_empty(void);
void dpdk_dyn_init(void);
/*--------------------------------------------------------------------------*/



