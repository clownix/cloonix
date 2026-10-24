/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
char *pthexec_agent_iso_amd64(void);
char *pthexec_agent_dir_cnt(void);
char *pthexec_agent_iso_win(void);
char *pthexec_agent_iso_bsd(void);
FILE *pthexec_popen(char *cmd, int *child_pid);
int   pthexec_pclose(FILE *iop, int child_pid);
char *pthexec_iw_bin(void);
char *pthexec_qemu_bin(void);
char *pthexec_ovmf_code(void);
char *pthexec_ovmf_vars(void);
void  pthexec_set_rank(int rank);
char *pthexec_ovs_port(int vm_id, int eth);
char *pthexec_ovs_peer(int vm_id, int eth);

char *pthexec_crun_bin(void);
char *pthexec_ovsdb_tool_bin(void);
char *pthexec_cp_bin(void);
char *pthexec_umount_bin(void);
char *pthexec_mount_bin(void);
char *pthexec_bash_bin(void);
char *pthexec_nsenter_bin(void);
char *pthexec_ip_bin(void);
char *pthexec_awk_bin(void);
char *pthexec_grep_bin(void);
char *pthexec_ps_bin(void);
char *pthexec_mknod_bin(void);
char *pthexec_chmod_bin(void);
char *pthexec_tar_bin(void);
char *pthexec_gzip_bin(void);
char *pthexec_cloonix_cfg(void);
char *pthexec_cxp_cli(void);
char *pthexec_dtach_bin(void);
char *pthexec_usr_libexec_cloonix(void);
char *pthexec_cloonfs_lib64(void);
char *pthexec_cloonfs_ovs_vswitchd(void);
char *pthexec_cloonfs_ovs_drv(void);
char *pthexec_cloonfs_ovs_ovsdb(void);
char *pthexec_cloonfs_ovs_vsctl(void);
char *pthexec_cloonfs_suid_power(void);
char *pthexec_cloonfs_doorways(void);
char *pthexec_cloonfs_gui_server(void);
char *pthexec_self_bin_dir(void);

int pthexec_running_in_crun(char *name);

void pthexec_init(void);

