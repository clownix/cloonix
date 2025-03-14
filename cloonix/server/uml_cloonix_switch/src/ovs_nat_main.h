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
typedef struct t_ovs_nat_main
{
  char name[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];
  char mac[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  char lan_added[MAX_NAME_LEN];
  int  del_nat_req;
  char socket[MAX_PATH_LEN];
  int count;
  int llid;
  int pid;
  int nat_id;
  int endp_type;
  int closed_count;
  int suid_root_done;
  int watchdog_count;
  int cli_llid;
  int cli_tid;
  int del_snf_ethv_sent;
  struct t_ovs_nat_main *prev;
  struct t_ovs_nat_main *next;
} t_ovs_nat_main;

void ovs_nat_main_del_all(void);
t_ovs_nat_main *ovs_nat_main_get_first(int *nb_nat);
void ovs_nat_main_llid_closed(int llid, int from_clone);
void ovs_nat_main_pid_resp(int llid, char *name, int pid);
int  ovs_nat_main_get_all_pid(t_lst_pid **lst_pid);
int  ovs_nat_main_diag_llid(int llid);
void ovs_nat_main_sigdiag_resp(int llid, int tid, char *line);
void ovs_nat_main_poldiag_resp(int llid, int tid, char *line);
void ovs_nat_main_resp_add_lan(int is_ko, char *name, int num, char *vhost, char *lan);
void ovs_nat_main_resp_del_lan(int is_ko, char *name, int num, char *vhost, char *lan);
t_ovs_nat_main *ovs_nat_main_exists(char *name);
int  ovs_nat_main_dyn_snf(char *name, int val);

void ovs_nat_main_add(int llid, int tid, char *name);
void ovs_nat_main_del(int llid, int tid, char *name);
void ovs_nat_main_add_lan(int llid, int tid, char *name, char *lan);
void ovs_nat_main_del_lan(int llid, int tid, char *name, char *lan);
t_topo_endp *ovs_nat_main_translate_topo_endp(int *nb);
int ovs_nat_main_whatip(int llid, int tid, char *nat_name, char *name);

void ovs_nat_main_vm_event(void);
void ovs_nat_main_cisco_add(char *name);
void ovs_nat_main_cisco_nat_destroy(char *name);
void ovs_nat_main_init(void);
/*--------------------------------------------------------------------------*/
