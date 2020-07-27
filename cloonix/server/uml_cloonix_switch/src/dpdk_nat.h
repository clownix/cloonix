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
int dpdk_nat_collect_dpdk(t_eventfull_endp *eventfull);
int dpdk_nat_eventfull(char *name, int ms, int ptx, int btx, int prx, int brx);
char *dpdk_nat_get_mac(char *name, int num);
char *dpdk_nat_get_next(char *name);
void dpdk_nat_event_from_nat_dpdk_process(char *name, char *lan, int on);
void dpdk_nat_resp_add_lan(int is_ko, char *lan, char *name);
void dpdk_nat_resp_del_lan(int is_ko, char *lan, char *name);
int dpdk_nat_get_qty(void);
int dpdk_nat_exist(char *name);
int dpdk_nat_lan_exists(char *lan);
int dpdk_nat_lan_exists_in_nat(char *name, char *lan);
void dpdk_nat_end_ovs(void);
int dpdk_nat_add(int llid, int tid, char *name, char *lan);
int dpdk_nat_del(int llid, int tid, char *name);
void dpdk_nat_init(void);
/*--------------------------------------------------------------------------*/

