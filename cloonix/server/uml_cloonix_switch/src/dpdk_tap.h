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
int dpdk_tap_eventfull(char *name, int ms, int ptx, int btx, int prx, int brx);
int dpdk_tap_collect_dpdk(t_eventfull_endp *eventfull);
char *dpdk_tap_get_next(char *name);
char *dpdk_tap_get_mac(char *name);
void dpdk_tap_resp_add_lan(int is_ko, char *lan, char *name);
void dpdk_tap_resp_del_lan(int is_ko, char *lan, char *name);
int dpdk_tap_get_qty(void);
int dpdk_tap_add(int llid, int tid, char *name, char *lan);
int dpdk_tap_del(int llid, int tid, char *name);
int dpdk_tap_exist(char *name);
int dpdk_tap_lan_exists(char *lan);
int dpdk_tap_lan_exists_in_tap(char *name, char *lan);
void dpdk_tap_resp_add(int is_ko, char *name, char *strmac);
void dpdk_tap_resp_del(int is_ko, char *name);
void dpdk_tap_end_ovs(void);
void dpdk_tap_init(void);
/*--------------------------------------------------------------------------*/

