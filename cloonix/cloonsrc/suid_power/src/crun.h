/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdint.h>
#define TMPFS "tmpfs"
#define UPPER "upper"
#define WORKDIR "workdir"
#define ROOTFS "rootfs"
#define LOWER "lower"

void crun_kill_all(void);
void crun_recv_poldiag_msg(int llid, int tid, char *line);
void crun_recv_sigdiag_msg(int llid, int tid, char *line);
void crun_beat(int llid);
int  crun_get_is_privileged(char *name, int *is_privileged);
void crun_init(char *var_root);
