/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef void (*t_fct_cb)(char *name, int num, char *vhost);

int ovs_snf_send_add_snf_lan(char *name, int num, char *vhost, char *lan);
int ovs_snf_send_del_snf_lan(char *name, int num, char *vhost, char *lan);

void ovs_dyn_snf_start_process(char *name, int num, int item_type,
                               char *vhost, t_fct_cb cb);
void ovs_dyn_snf_stop_process(char *tap);

int ovs_snf_collect_snf(t_eventfull_endp *eventfull);
int ovs_snf_get_qty(void);
/* 2 running (pid and channel), 1 known but not running, 0 none */
int ovs_snf_state(char *name, int num);
void ovs_snf_state_explain(char *name, int num, char *err);
void ovs_snf_state_stalled(char *name, int num);
void ovs_snf_llid_closed(int llid, int from_clone);
void ovs_snf_pid_resp(int llid, char *name, int pid);
int  ovs_snf_get_all_pid(t_lst_pid **lst_pid);
int  ovs_snf_diag_llid(int llid);
void ovs_snf_sigdiag_resp(int llid, int tid, char *line);
void ovs_snf_poldiag_resp(int llid, int tid, char *line);
void ovs_snf_resp_msg_add_lan(int is_ko, char *vhost, char *name, int num, char *lan);
void ovs_snf_resp_msg_del_lan(int is_ko, char *vhost, char *name, int num, char *lan);
void ovs_snf_resp_msg_vhost_up(int is_ko, char *vhost, char *name, int num);
void ovs_snf_stop_all(void);
int  ovs_snf_is_running(char *name, int num);
void ovs_snf_init(void);
void ovs_snf_sync_wireshark_req(int llid, int tid, char *name, int num, int cmd);
