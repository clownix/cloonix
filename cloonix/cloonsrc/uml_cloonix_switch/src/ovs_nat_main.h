/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef struct t_ovs_nat_main
{
  char name[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];
  char mac[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  char lan_added[MAX_NAME_LEN];
  int  del_nat_req;
  char socket[MAX_PATH_LEN];
  char ports[MAX_PATH_LEN];
  char prev_ports[MAX_PATH_LEN];
  int count;
  int llid;
  int pid;
  int nat_id;
  int endp_type;
  int closed_count;
  int kil_sent;
  int suid_root_done;
  int watchdog_count;
  int cli_llid;
  int stuck_count;
  /* beats its lan has been gone while still named */
  int lan_gone_count;
  int pub_llid;
  int pub_tid;
  int pub_old_pid;
  int pub_old_gen;
  int last_gen;
  /* The pid of the passt process this nat launched, once seen. It is not
     our child and it answers no heartbeat, so it has to be watched. */
  int passt_pid;
  /* passt liveness: each beat asks proxymous "is passt connected", consecutive
     no answers reap the nat (passt never writes its pid file). */
  int passt_probe_sent;
  long long pub_since_ms;
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

void ovs_nat_main_add(int llid, int tid, char *name, char *ports);
void ovs_nat_main_del(int llid, int tid, char *name);
void ovs_nat_main_add_lan(int llid, int tid, char *name, char *lan);
void ovs_nat_main_del_lan(int llid, int tid, char *name, char *lan);
void ovs_nat_main_topo_endp_push(void);
int ovs_nat_main_whatip(int llid, int tid, char *nat_name, char *name);
int ovs_nat_main_publish(int llid, int tid, char *name, char *ports);
void ovs_nat_main_publish_ko(char *name);

void ovs_nat_main_vm_event(void);
void ovs_nat_main_init(void);
