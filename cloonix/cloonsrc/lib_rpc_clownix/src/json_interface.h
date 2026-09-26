/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef JSON_INTERFACE_H
#define JSON_INTERFACE_H

#define MAX_CLOWNIX_BOUND_LEN  64
#define MIN_CLOWNIX_BOUND_LEN  2

void doors_io_basic_json_init(t_llid_tx llid_tx);
int  doors_io_basic_decoder(int llid, int len, char *buf);
void doors_io_basic_tx_set(t_llid_tx llid_tx);

#endif
