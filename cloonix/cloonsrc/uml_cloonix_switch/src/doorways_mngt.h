/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
int doorways_get_distant_pid(void);
int get_doorways_llid(void);
void doorways_first_start(void);
void doorways_init(char *net, char *root_work, char *password);
void doorways_err_cb (int llid);
void doors_pid_resp(int llid, char *name, int pid);
void kill_doors(void);

