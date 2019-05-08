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
int dpdk_fmt_tx_add_lan(int tid, char *lan, int spy);
int dpdk_fmt_tx_del_lan(int tid, char *lan, int spy);
int dpdk_fmt_tx_add_eth(int tid, t_vm *kvm, char *name, int num);
int dpdk_fmt_tx_del_eth(int tid, char *name, int num);
int dpdk_fmt_tx_add_lan_eth(int tid, char *lan, char *name, int num);
int dpdk_fmt_tx_del_lan_eth(int tid, char *lan, char *name, int num);
void dpdk_fmt_rx_rpct_recv_diag_msg(int llid, int tid, char *line);
void dpdk_fmt_rx_rpct_recv_evt_msg(int llid, int tid, char *line);
/*--------------------------------------------------------------------------*/

