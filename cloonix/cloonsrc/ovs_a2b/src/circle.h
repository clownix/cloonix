/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef struct t_pkt
{
  int len;
  uint8_t *pkt;
} t_pkt;

int  circle_peek(int id, uint64_t *len, uint64_t *usec, t_pkt **pbuf);
void circle_swap(int id);
t_pkt *circle_get(int id);
void circle_put(int id, uint64_t len, uint64_t usec, t_pkt *pbuf);
void circle_init(void);
