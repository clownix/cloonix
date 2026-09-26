/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Mount image onto target, owner ids mapped to the net runner ids. */
int idmap_mount(char *image, char *target, int uid, int gid,
                int subuid_start, int subuid_qty,
                int subgid_start, int subgid_qty);
/* The same, read only: the lower of a non persistent container. */
int idmap_mount_ro(char *image, char *target, int uid, int gid,
                   int subuid_start, int subuid_qty,
                   int subgid_start, int subgid_qty);
