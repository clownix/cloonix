/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
void dpdk_tap_resp_add_lan(int is_ko, char *lan, char *name);
void dpdk_tap_resp_del_lan(int is_ko, char *lan, char *name);
int dpdk_tap_topo_endp(int nb_max, t_topo_endp *endp);
int dpdk_tap_get_qty(void);
int dpdk_tap_add_lan(int llid, int tid, char *lan, char *name);
int dpdk_tap_del_lan(int llid, int tid, char *lan, char *name);
int dpdk_tap_add(int llid, int tid, char *name, int lock);
int dpdk_tap_del(int llid, int tid, char *name);
int dpdk_tap_exist(char *name);
void dpdk_tap_resp_add(int is_ko, char *name);
void dpdk_tap_resp_del(char *name);
void dpdk_tap_end_ovs(void);
int dpdk_tap_collect_dpdk(t_eventfull_endp *eventfull);
int dpdk_tap_eventfull(int tap_id, int num, int ms,
                       int ptx, int btx, int prx, int brx);
void dpdk_tap_init(void);
/*--------------------------------------------------------------------------*/

