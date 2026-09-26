/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef void (*t_from_child_to_main)(void *data, char *msg);
typedef int (*t_fct_to_launch)(void *data);
typedef void (*t_launched_death)(void *data, int status, char *nm);
int pid_clone_launch(t_fct_to_launch fct, t_launched_death death, 
                     t_from_child_to_main fct_msg, 
                     void *data_start, void *data_end, void *data_msg, 
                     char *vm_name, int fd_not_to_close, int no_kerr);
void pid_clone_init(void);
void send_to_daddy (char *str);
char *pid_get_clone_internal_com(void);

void pid_clone_kill_all(void);
int pid_clone_kill_single(int pid);

void kerr_running_pids(void);

