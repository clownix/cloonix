/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef void (*t_cb_end_automate)(void *data, int status, char *nm);
void automates_init(void);
void auto_self_destruction(int llid, int tid, char *reason);
void automates_safety_heartbeat(void);
void inc_lock_self_destruction_dir(void);
void dec_lock_self_destruction_dir(void);

int get_glob_req_self_destruction(void);

