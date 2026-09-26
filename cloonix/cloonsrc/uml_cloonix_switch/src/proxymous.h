/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void proxymous_kill(void);
int proxymous_get_pid(t_lst_pid **lst_pid);
void proxymous_pid_resp(int llid, char *name, int pid);
void proxymous_llid_closed(int llid, int from_clone);
void proxymous_init(char *passwd);

