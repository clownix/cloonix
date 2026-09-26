/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef struct t_lan_cnt
{
  char lan[MAX_NAME_LEN];
  int  lan_attached_ok;
  char lan_added[MAX_NAME_LEN];
} t_lan_cnt;

typedef struct t_cnt
{
  t_topo_cnt cnt;
  t_lan_cnt att_lan[MAX_ETH_VM];
  char mountbear[MAX_PATH_LEN];
  int poweroff_done;
  int process_gone;
  int del_snf_ethv_sent[MAX_ETH_VM];
  int vhost_up[MAX_ETH_VM];
  int lan_add_waiting_ack;
  int lan_del_waiting_ack;
  int count_add;
  int count_del;
  int count_delete;
  int cloonix_rank;
  int count_llid;
  int cli_llid;
  int cli_tid;
  int cnt_pid;
  /* the caps in force, for dump commands, and the ones asked */
  int lim_cpu;
  int lim_ram;
  int lim_cpu_asked;
  int lim_ram_asked;
  struct t_cnt *prev;
  struct t_cnt *next;
} t_cnt;
void cnt_resp_add_lan(int is_ko, char *name, int num, char *vhost, char *lan);
void cnt_resp_del_lan(int is_ko, char *name, int num, char *vhost, char *lan);
t_cnt *cnt_get_first_cnt(int *nb);
void cnt_topo_endp_push(void);
int  cnt_add_lan(int llid, int tid, char *name, int num,
                       char *lan, char *err);
int  cnt_del_lan(int llid, int tid, char *name, int num,
                       char *lan, char *err);
int  cnt_name_exists(char *name, int *nb_eth);
int cnt_dyn_snf(char *name, int num, int val);
int  cnt_create(int llid, int cli_llid, int cli_tid, int vm_id,
                      t_topo_cnt *cnt, char *info);
int  cnt_delete(int llid, int cli_llid, int cli_tid, char *name);
int  cnt_limit(int llid, int cli_llid, int cli_tid, char *name, char *cmd,
                char *err);
int  cnt_delete_all(int llid);
void cnt_timer_beat(int llid);
int  cnt_info(char *name, int *nb_eth, t_eth_table **eth_table);
int  cnt_eth_table(char *name, int *nb_eth, t_eth_table **eth_table);
int  cnt_get_all_pid(t_lst_pid **lst_pid);
void cnt_doorways_ping_ok(char *name);
void cnt_doorways_ping_ko(char *name);
void cnt_doorways_up_and_running(char *name);
t_cnt *find_cnt(char *name);
int send_sig_suid_power(int llid, char *msg);
void cnt_vhost_and_doors_begin(t_cnt *cnt);
int cnt_free_cnt(char *name);
void cnt_init(void);
int cnt_air(int llid, int cli_llid, int cli_tid, char *name, int idx,
            char *air, char *err);
int cnt_vhost_up(char *name, int num, int set);
