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
int nat_dpdk_llid_exists_with_name(char *name);
int nat_dpdk_name_exists_with_llid(int llid, char *name);
void nat_dpdk_vm_event(void);
void nat_dpdk_llid_closed(int llid);
void nat_dpdk_pid_resp(int llid, int tid, char *name, int pid);
int  nat_dpdk_get_all_pid(t_lst_pid **lst_pid);
int  nat_dpdk_diag_llid(int llid);
void nat_dpdk_diag_resp(int llid, int tid, char *line);
void nat_dpdk_start_stop_process(char *name, char *lan, int on);
void nat_dpdk_init(void);
/*--------------------------------------------------------------------------*/
