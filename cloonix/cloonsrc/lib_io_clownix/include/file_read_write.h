/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
char *mydirname(char *path);
int is_directory_writable(char *path);
char *read_whole_file(char *file_name, int *len, char *err);
int write_whole_file(char *file_name, char *buf, int len, char *err);
int file_exists(char *path, int mode);
int rm_tree(char *dir, char *err);
int machine_is_kvm_able(void);
int dev_file_rw_ko(char *dev_file, char *why);
int i_am_inside_cloonix(char *name);

