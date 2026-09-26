/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void mk_cnt_dir(void);
void mk_endp_dir(void);
void mk_dtach_screen_dir(void);
int rm_work_dir(char *dir, char *err);

int mk_machine_dirs(char *name, int vm_id);
int rm_machine_dirs(int vm_id, char *err);

int mk_cnt_dirs(char *dir_path, char *name);

int  my_mkdir(char *dst_dir, int wr_all);

void mk_ovs_db_dir(void);

