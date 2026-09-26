/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Canvas positions set from the cli, keyed by item name, shared by all
   browsers and kept across reloads. */

typedef struct t_layout
{
  char name[MAX_NAME_LEN];
  int  has_pos;
  int  x;
  int  y;
  unsigned int eth_set;
  short eth_deg[MAX_ETH_VM];
} t_layout;

int  layout_set_pos(char *name, int x, int y);
int  layout_set_eth(char *name, int num, int deg);
void layout_replay(int llid, int tid);
void layout_del_all(void);
void layout_forget_absent(void);
t_layout *layout_get_first(int *nb);
