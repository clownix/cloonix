/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void suid_power_self_kill(void);
void suid_power_llid_closed(int llid, int from_clone);
int suid_power_pid(void);
typedef void (*t_qemu_vm_end)(char *name, int ok);
void suid_power_launch_vm(t_vm *vm, t_qemu_vm_end end);
void suid_power_spawn(char **argv);
int suid_power_ready(void);
void suid_power_kill_vm(int vm_id);
void suid_power_kill_pid(int pid);
int suid_power_ovs_cmd(int cli_llid, int cli_tid, char *tool, char *args);
int suid_power_ns_cmd(int cli_llid, int cli_tid, int nb, t_pid_lst *lst);
void suid_power_sigdiag_resp(int llid, int tid, char *line);
void suid_power_poldiag_resp(int llid, int tid, char *line);
int  suid_power_diag_llid(int llid);
void suid_power_pid_resp(int llid, int tid, char *name, int pid);
void suid_power_first_start(void);
int suid_power_get_pid(int vm_id);
int suid_power_info_phy_exists(char *name);

int suid_power_get_topo_info_phy(t_topo_info_phy **phy);

int suid_power_get_topo_bridges(t_topo_bridges **bridges);
int suid_power_create_cnt(int cli_llid, int cli_tid, int vm_id,
                                t_topo_cnt *cnt, char *err);
void suid_power_delete_cnt(int cli_llid, int cli_tid, char *name);
int  suid_power_limit_cnt(int cli_llid, int cli_tid, char *name,
                          char *cmd, char *err);
int suid_power_delete_cnt_all(void);

void suid_power_init(void);
int suid_power_air_req(char *req);
int suid_power_air_cnt(int cli_llid, int cli_tid, char *name, int idx,
                   char *air, char *err);
