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
void dpdk_msg_vlan_exist_no_more(char *lan);
void dpdk_msg_ack_lan(int tid, char *lan_name, 
                      int is_ko, int is_add, char *lab);
void dpdk_msg_ack_lan_endp(int tid, char *lan_name, char *name, int num,
                           int is_ko, int is_add, char *lab);
/*--------------------------------------------------------------------------*/
int dpdk_msg_send_add_kvm(char *name, int num);
void dpdk_msg_send_del_kvm(char *name, int num);

int dpdk_msg_send_add_lan_ethd(char *lan_name, char *name, int num);
int dpdk_msg_send_del_lan_ethd(char *lan_name, char *name, int num);

int dpdk_msg_send_add_lan_ethv(char *lan_name, char *name, int num, char *vhost);
int dpdk_msg_send_del_lan_ethv(char *lan_name, char *name, int num, char *vhost);

int dpdk_msg_send_add_lan_eths(char *lan_name, char *name, int num);
int dpdk_msg_send_del_lan_eths(char *lan_name, char *name, int num);

int dpdk_msg_send_add_lan_nat(char *lan_name, char *name);
int dpdk_msg_send_del_lan_nat(char *lan_name, char *name);
int dpdk_msg_send_add_lan_d2d(char *lan_name, char *name);
int dpdk_msg_send_del_lan_d2d(char *lan_name, char *name);
int dpdk_msg_send_add_lan_a2b(char *lan_name, char *name, int num);
int dpdk_msg_send_del_lan_a2b(char *lan_name, char *name, int num);
int dpdk_msg_send_add_lan_tap(char *lan_name, char *name);
int dpdk_msg_send_del_lan_tap(char *lan_name, char *name);
int dpdk_msg_send_add_lan_phy(char *lan_name, char *name);
int dpdk_msg_send_del_lan_phy(char *lan_name, char *name);
/*--------------------------------------------------------------------------*/
void dpdk_msg_init(void);
/*--------------------------------------------------------------------------*/

