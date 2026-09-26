/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
int crun_utils_unlink_sub_dir_files(char *dir);
int my_mkdir(char *dst_dir);
char *get_var_crun_log(void);
int force_waitpid(int pid, int *status);
void log_write_req(char *line);
FILE *cmd_lio_popen(char *cmd, int *child_pid, int uid);
int execute_cmd(char *cmd, int do_log, int uid);
/* The same, keeping stdout and stderr. */
int execute_cmd_out(char *cmd, int do_log, int uid, char *out, int out_len);
void crun_utils_startup_env(char *mountbear, char *startup_env,
                            int nb_vwif, char *mac, int nb_eth);
int crun_utils_create_config_json(char *image, char *path, char *rootfs,
                                 char *nspacecrun,
                                 char *mountbear, char *mounttmp,
                                 int is_persistent, int is_privileged,
                                 char *brandtype,
                                 char *startup_env, char *vmount,
                                 char *name, int ovspid, int uid_image,
                                 char *sbin_init, char *sh_starter,
                                 int subuid_start, int subuid_qty,
                                 int subgid_start, int subgid_qty);
void utils_init(char *log);
uid_t get_caller_uid(void);
gid_t get_caller_gid(void);

