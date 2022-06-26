/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
int ovs_snf_collect_snf(t_eventfull_endp *eventfull);
int ovs_snf_get_qty(void);
void ovs_snf_llid_closed(int llid);
void ovs_snf_pid_resp(int llid, char *name, int pid);
int  ovs_snf_get_all_pid(t_lst_pid **lst_pid);
int  ovs_snf_diag_llid(int llid);
void ovs_snf_sigdiag_resp(int llid, int tid, char *line);
void ovs_snf_poldiag_resp(int llid, int tid, char *line);
void ovs_snf_start_stop_process(char *name, int num, char *vhost,
                                char *tap, int on, char *mac);
void ovs_snf_resp_msg_add_lan(int is_ko, char *vhost, char *name, int num, char *lan);
void ovs_snf_resp_msg_del_lan(int is_ko, char *vhost, char *name, int num, char *lan);
void ovs_snf_resp_msg_vhost_up(int is_ko, char *vhost, char *name, int num);
void ovs_snf_init(void);
/*--------------------------------------------------------------------------*/
