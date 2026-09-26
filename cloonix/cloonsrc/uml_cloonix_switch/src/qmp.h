/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void qmp_begin_qemu_unix(char *name, int first);
void qmp_msg_recv(char *name, char *msg);
int  qmp_greeting_seen(char *name);
void qmp_init(void);
void qmp_clean_all_data(void);
void qmp_request_qemu_reboot(char *name);
void qmp_request_save_rootfs(char *name, char *path, int llid, int tid);

int qmp_braces_complete(char *whole_rx, char **next_rx);
