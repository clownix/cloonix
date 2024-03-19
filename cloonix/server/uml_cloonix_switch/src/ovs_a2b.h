/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
typedef struct t_side_a2b
{
  char vhost[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  char lan_added[MAX_NAME_LEN];
  int  endp_type;
  int  del_a2b_req;
  int  del_snf_ethv_sent;
  int must_call_snf_started;
} t_side_a2b;

typedef struct t_ovs_a2b
{
  char name[MAX_NAME_LEN];
  t_side_a2b side[2];
  char socket[MAX_PATH_LEN];
  int count;
  int llid;
  int pid;
  int a2b_id;
  int closed_count;
  int suid_root_done;
  int watchdog_count;
  int cli_llid;
  int cli_tid;
  int del_snf_ethv_sent;
  struct t_ovs_a2b *prev;
  struct t_ovs_a2b *next;
} t_ovs_a2b;

t_ovs_a2b *ovs_a2b_get_first(int *nb_a2b);
void ovs_a2b_llid_closed(int llid);
void ovs_a2b_pid_resp(int llid, char *name, int pid);
int  ovs_a2b_get_all_pid(t_lst_pid **lst_pid);
int  ovs_a2b_diag_llid(int llid);
void ovs_a2b_sigdiag_resp(int llid, int tid, char *line);
void ovs_a2b_poldiag_resp(int llid, int tid, char *line);
void ovs_a2b_resp_add_lan(int is_ko, char *name, int num, char *vhost, char *lan);
void ovs_a2b_resp_del_lan(int is_ko, char *name, int num, char *vhost, char *lan);
t_ovs_a2b *ovs_a2b_exists(char *name);
int  ovs_a2b_dyn_snf(char *name, int num, int val);

void ovs_a2b_add(int llid, int tid, char *name);
void ovs_a2b_del(int llid, int tid, char *name);
void ovs_a2b_add_lan(int llid, int tid, char *name, int num, char *lan);
void ovs_a2b_del_lan(int llid, int tid, char *name, int num, char *lan);
t_topo_endp *ovs_a2b_translate_topo_endp(int *nb);
int ovs_a2b_param_config(int llid, int tid, char *name, char *cmd);
void ovs_a2b_init(void);
/*--------------------------------------------------------------------------*/
