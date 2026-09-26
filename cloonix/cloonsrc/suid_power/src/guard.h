/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Group whose members may use cloonix-suid-power; absent, anyone may. */
#define CLOONIX_GROUP "cloonix"
void guard_init(uid_t uid, gid_t gid, char *net, char *root_path);
int  guard_group_allows(uid_t uid);
int  guard_path_in_root(char *path);
int  guard_caller_can_access(char *path, int mode);
int  guard_launch_line(char *line);
int  guard_pid_is_callers(int pid);
