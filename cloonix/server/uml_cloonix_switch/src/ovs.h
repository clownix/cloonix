/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
char *ovs_format_ethv(t_vm *vm, int eth, char *ifname);
void ovs_destroy(void);
int get_daemon_done(void);
int  ovs_try_send_sigdiag_msg(int tid, char *cmd);
void ovs_rpct_recv_sigdiag_msg(int llid, int tid, char *line);
void ovs_pid_resp(int llid, char *name, int toppid, int pid);
int  ovs_find_with_llid(int llid);
int  ovs_get_all_pid(t_lst_pid **lst_pid);
int ovs_still_present(void);
void ovs_init(void);
/*--------------------------------------------------------------------------*/



