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
void d2d_dpdk_send_probe_udp(char *name);
void d2d_dpdk_set_dist_udp_ip_port(char *name, uint32_t ip, uint16_t port);
void d2d_dpdk_get_udp_port(char *name);
void d2d_dpdk_eal_init(char *name);
void d2d_dpdk_start_vhost(char *name);
void d2d_dpdk_stop_vhost(char *name);
int  d2d_dpdk_diag_llid(int llid);
int  d2d_dpdk_get_all_pid(t_lst_pid **lst_pid);
void d2d_dpdk_pid_resp(int llid, int tid, char *name, int pid);
void d2d_dpdk_diag_resp(int llid, int tid, char *line);
void d2d_dpdk_start_stop_process(char *name, int on);
void d2d_dpdk_llid_closed(int llid);
void d2d_dpdk_init(void);
/*--------------------------------------------------------------------------*/

