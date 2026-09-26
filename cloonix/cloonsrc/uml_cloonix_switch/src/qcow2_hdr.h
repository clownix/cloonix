/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef QCOW2_HDR_H
#define QCOW2_HDR_H
int qcow2_create_derived(char *backing, char *used);
int qcow2_get_backing(char *path, char *backing, int len);
#endif
