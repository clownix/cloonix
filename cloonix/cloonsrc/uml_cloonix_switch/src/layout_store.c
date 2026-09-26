/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "llid_trace.h"
#include "cfg_store.h"
#include "layout_store.h"
#include "wifi_air.h"

#define MAX_LAYOUT_ITEM 1024

static t_layout g_layout[MAX_LAYOUT_ITEM];
static int g_nb_layout;

static void layout_broadcast_pos(char *name, int x, int y)
{
  int llid, prev_llid, tid;
  if (llid_get_event2llid(sub_evt_topo, 0, &llid, &tid))
    {
    do
      {
      if (msg_exist_channel(llid))
        send_item_pos(llid, tid, name, x, y);
      prev_llid = llid;
      } while(llid_get_event2llid(sub_evt_topo, prev_llid, &llid, &tid));
    }
}

static void layout_broadcast_eth(char *name, int num, int deg)
{
  int llid, prev_llid, tid;
  if (llid_get_event2llid(sub_evt_topo, 0, &llid, &tid))
    {
    do
      {
      if (msg_exist_channel(llid))
        send_item_poseth(llid, tid, name, num, deg);
      prev_llid = llid;
      } while(llid_get_event2llid(sub_evt_topo, prev_llid, &llid, &tid));
    }
}

static t_layout *layout_find(char *name, int create)
{
  int i;
  for (i=0; i<g_nb_layout; i++)
    {
    if (!strcmp(g_layout[i].name, name))
      return &(g_layout[i]);
    }
  if (!create)
    return NULL;
  if (g_nb_layout >= MAX_LAYOUT_ITEM)
    {
    KERR("ERROR layout table full at %d, %s not kept", MAX_LAYOUT_ITEM, name);
    return NULL;
    }
  memset(&(g_layout[g_nb_layout]), 0, sizeof(t_layout));
  strncpy(g_layout[g_nb_layout].name, name, MAX_NAME_LEN-1);
  g_nb_layout += 1;
  return &(g_layout[g_nb_layout-1]);
}

int layout_set_pos(char *name, int x, int y)
{
  t_layout *cur = layout_find(name, 1);
  if (!cur)
    return 0;
  cur->has_pos = 1;
  cur->x = x;
  cur->y = y;
  layout_broadcast_pos(name, x, y);
  return 1;
}

int layout_set_eth(char *name, int num, int deg)
{
  t_layout *cur;
  if ((num < 0) || (num >= MAX_ETH_VM))
    return 0;
  cur = layout_find(name, 1);
  if (!cur)
    return 0;
  deg = ((deg % 360) + 360) % 360;
  cur->eth_set |= (1U << num);
  cur->eth_deg[num] = (short) deg;
  layout_broadcast_eth(name, num, deg);
  return 1;
}

void layout_replay(int llid, int tid)
{
  int i, num;
  layout_forget_absent();
  for (i=0; i<g_nb_layout; i++)
    {
    if (g_layout[i].has_pos)
      send_item_pos(llid, tid, g_layout[i].name,
                    g_layout[i].x, g_layout[i].y);
    for (num=0; num<MAX_ETH_VM; num++)
      {
      if (g_layout[i].eth_set & (1U << num))
        send_item_poseth(llid, tid, g_layout[i].name,
                         num, (int) g_layout[i].eth_deg[num]);
      }
    }
}

void layout_forget_absent(void)
{
  int i = 0;
  char use[MAX_PATH_LEN];
  while (i < g_nb_layout)
    {
    if (cfg_name_is_in_use(0, g_layout[i].name, use) ||
        wifi_air_in_use(g_layout[i].name))
      i += 1;
    else
      {
      g_nb_layout -= 1;
      if (i < g_nb_layout)
        memcpy(&(g_layout[i]), &(g_layout[g_nb_layout]), sizeof(t_layout));
      memset(&(g_layout[g_nb_layout]), 0, sizeof(t_layout));
      }
    }
}

t_layout *layout_get_first(int *nb)
{
  layout_forget_absent();
  *nb = g_nb_layout;
  return g_layout;
}

void layout_del_all(void)
{
  memset(g_layout, 0, sizeof(g_layout));
  g_nb_layout = 0;
}
