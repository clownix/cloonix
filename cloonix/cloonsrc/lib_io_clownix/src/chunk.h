/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */

#define PROXY_HEADER_LEN 4
#define MAX_TOT_LEN_PROXY 2000

void chain_delete(t_data_chunk **start, t_data_chunk **last);
int make_a_buf_copy(t_data_chunk *first, int bound_start_offset,
                           char **buf_copy);
int chain_get_prev_len(t_data_chunk *start, int bound_start_offset);
void chain_del(t_data_chunk **start, t_data_chunk *last);
void chain_pop(t_data_chunk **start);
void chain_append(t_data_chunk **start, int len, char *nrx);
t_data_chunk *chain_get_last_chunk(t_data_chunk *start);
void chain_append_tx(t_data_chunk **start, t_data_chunk **last,
                         int len, char *nrx);
void first_elem_delete(t_data_chunk **start, t_data_chunk **last);
int tx_write(char *msg, int len, int fd);
