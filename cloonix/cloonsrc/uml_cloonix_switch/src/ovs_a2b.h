/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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

/* the settings of an a2b, per direction, for dump commands */
#define A2B_NB_CNF 6
#define A2B_NB_ASKED 8
typedef struct t_a2b_asked
{
  int llid;
  int tid;
  char cmd[MAX_PATH_LEN];
} t_a2b_asked;

typedef struct t_ovs_a2b
{
  char name[MAX_NAME_LEN];
  t_side_a2b side[2];
  int cnf_set[2][A2B_NB_CNF];
  int cnf_val[2][A2B_NB_CNF];
  char cnf_repsrc[2][MAX_NAME_LEN];
  char cnf_repdst[2][MAX_NAME_LEN];
  t_a2b_asked asked[A2B_NB_ASKED];
  char socket[MAX_PATH_LEN];
  int count;
  int llid;
  int pid;
  int a2b_id;
  int closed_count;
  int kil_sent;
  int suid_root_done;
  int watchdog_count;
  int cli_llid;
  int cli_tid;
  int del_snf_ethv_sent;
  struct t_ovs_a2b *prev;
  struct t_ovs_a2b *next;
} t_ovs_a2b;

t_ovs_a2b *ovs_a2b_get_first(int *nb_a2b);
void ovs_a2b_llid_closed(int llid, int from_clone);
void ovs_a2b_pid_resp(int llid, char *name, int pid);
int  ovs_a2b_get_all_pid(t_lst_pid **lst_pid);
int  ovs_a2b_diag_llid(int llid);
void ovs_a2b_sigdiag_resp(int llid, int tid, char *line);
void ovs_a2b_resp_add_lan(int is_ko, char *name, int num, char *vhost, char *lan);
void ovs_a2b_resp_del_lan(int is_ko, char *name, int num, char *vhost, char *lan);
t_ovs_a2b *ovs_a2b_exists(char *name);
int  ovs_a2b_dyn_snf(char *name, int num, int val);
char *ovs_a2b_cnf_word(int i);

void ovs_a2b_add(int llid, int tid, char *name);
void ovs_a2b_del(int llid, int tid, char *name);
void ovs_a2b_add_lan(int llid, int tid, char *name, int num, char *lan);
void ovs_a2b_del_lan(int llid, int tid, char *name, int num, char *lan);
void ovs_a2b_topo_endp_push(void);
int ovs_a2b_param_config(int llid, int tid, char *name, char *cmd);
void ovs_a2b_init(void);
