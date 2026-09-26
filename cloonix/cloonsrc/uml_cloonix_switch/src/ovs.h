/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void ovs_llid_closed(int llid, int from_clone);
char *ovs_format_ethv(t_vm *vm, int eth, char *ifname);
void ovs_destroy(void);
int get_daemon_done(void);
int  ovs_try_send_sigdiag_msg(int tid, char *cmd);
void ovs_rpct_recv_sigdiag_msg(int llid, int tid, char *line);
void ovs_pid_resp(int llid, char *name, int toppid, int pid);
int  ovs_find_with_llid(int llid);
int  ovs_get_all_pid(t_lst_pid **lst_pid);
int  ovs_get_a_pid_in_the_netns(void);
int ovs_still_present(void);
void ovs_init(void);

