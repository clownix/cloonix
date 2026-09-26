/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
int tar_img_check(char *bulk, char *image, int *uid_image,
                  int is_persistent, char *brandtype,
                  char *sbin_init, char *sh_startup,
                  int *subuid_start, int *subuid_qty,
                  int *subgid_start, int *subgid_qty, char *why);
int tar_img_rootfs_get(char *name, char *bulk, char *image,
                       char *brandtype, char *mnt_tar_img);
int tar_img_add(char *name, char *bulk, char *image, int uid_image,
                 char *cnt_dir, int is_persistent, char *brandtype);
int tar_img_del(char *name, char *bulk, char *image,
                 char *cnt_dir, int is_persistent, char *brandtype);
void tar_img_init(void);
