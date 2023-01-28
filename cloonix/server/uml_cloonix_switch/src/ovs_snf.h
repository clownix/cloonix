/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
typedef void (*t_fct_cb)(char *name, int num, char *vhost);

int ovs_snf_send_add_snf_lan(char *name, int num, char *vhost, char *lan);
int ovs_snf_send_del_snf_lan(char *name, int num, char *vhost, char *lan);

void ovs_dyn_snf_start_process(char *name, int num, int item_type,
                               char *vhost, t_fct_cb cb);
void ovs_dyn_snf_stop_process(char *tap);

int ovs_snf_collect_snf(t_eventfull_endp *eventfull);
int ovs_snf_get_qty(void);
void ovs_snf_llid_closed(int llid);
void ovs_snf_pid_resp(int llid, char *name, int pid);
int  ovs_snf_get_all_pid(t_lst_pid **lst_pid);
int  ovs_snf_diag_llid(int llid);
void ovs_snf_sigdiag_resp(int llid, int tid, char *line);
void ovs_snf_poldiag_resp(int llid, int tid, char *line);
void ovs_snf_resp_msg_add_lan(int is_ko, char *vhost, char *name, int num, char *lan);
void ovs_snf_resp_msg_del_lan(int is_ko, char *vhost, char *name, int num, char *lan);
void ovs_snf_resp_msg_vhost_up(int is_ko, char *vhost, char *name, int num);
void ovs_snf_lan_mac_change(char *lan);
void ovs_snf_c2c_update_mac(char *name);
void ovs_snf_a2b_update_mac(char *name);
void ovs_snf_init(void);
/*--------------------------------------------------------------------------*/
