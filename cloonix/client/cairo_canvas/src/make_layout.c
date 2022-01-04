/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "layout_rpc.h"
#include "doorways_sock.h"
#include "client_clownix.h"
#include "bank.h"
#include "interface.h"
#include "layout_topo.h"
#include "make_layout.h"


/****************************************************************************/
t_layout_node *make_layout_node(t_bank_item *cur)
{
  static t_layout_node layout_node;
  double x, y;
  t_list_bank_item *eth;
  int count=0, i;
  memset(&layout_node, 0, sizeof(t_layout_node));
  strncpy(layout_node.name, cur->name, MAX_NAME_LEN-1);
  layout_node.x = cur->pbi.position_x;
  layout_node.y = cur->pbi.position_y;
  layout_node.hidden_on_graph = cur->pbi.hidden_on_graph;
  layout_node.color = cur->pbi.color_choice;
  layout_node.nb_eth_wlan = cur->num;
  eth = cur->head_eth_list;
  if (eth)
    {
    while (eth && eth->next)
      {
      eth = eth->next;
      count++;
      }
    count++;
    }
  if (cur->num != count)
    KOUT("%d  %d", cur->num, count);
  for (i=0; i<count; i++)
    {
    if (!eth)
      KOUT(" ");
    if (eth->bitem->num != i)
      KOUT("%d %d", i, eth->bitem->num);
    x = eth->bitem->pbi.position_x - cur->pbi.position_x;
    y = eth->bitem->pbi.position_y - cur->pbi.position_y;
    if (cur->pbi.endp_type == endp_type_a2b)
      layout_round_a2b_eth_coords(&x, &y);
    else
      {
      if (cur->bank_type == bank_type_cnt)
        layout_round_cnt_eth_coords(&x, &y);
      else
        layout_round_node_eth_coords(&x, &y);
      }
    layout_node.eth_wlan[i].x =  x;
    layout_node.eth_wlan[i].y = y;
    layout_node.eth_wlan[i].hidden_on_graph = eth->bitem->pbi.hidden_on_graph;
    eth = eth->prev;
    }
  return (&layout_node);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_layout_sat *make_layout_sat(t_bank_item *cur)
{
  static t_layout_sat layout_sat;
  double x, y;
  t_list_bank_item *eth;
  int count=0, i;
  memset(&layout_sat, 0, sizeof(t_layout_sat));
  strncpy(layout_sat.name, cur->name, MAX_NAME_LEN-1);
  layout_sat.x = cur->pbi.position_x;
  layout_sat.y = cur->pbi.position_y;
  layout_sat.hidden_on_graph = cur->pbi.hidden_on_graph;
  if (cur->pbi.endp_type == endp_type_a2b)
    {
    eth = cur->head_eth_list;
    while (eth && eth->next)
      {
      eth = eth->next;
      count++;
      }
    count++;
    if (count != 2)
      {
      KOUT("%p %s %d", cur, cur->name, count);
      }
    for (i=0; i<count; i++)
      {
      if (!eth)
        KOUT(" ");
      if (eth->bitem->num != i)
        KOUT("%d %d", i, eth->bitem->num);
      x = eth->bitem->pbi.position_x - cur->pbi.position_x;
      y = eth->bitem->pbi.position_y - cur->pbi.position_y;
      layout_round_a2b_eth_coords(&x, &y);
      if (i == 0)
        {
        layout_sat.xa = x;
        layout_sat.ya = y;
        }
      else
        {
        layout_sat.xb = x;
        layout_sat.yb = y;
        }
      eth = eth->prev;
      }
    }
  return (&layout_sat);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_layout_lan *make_layout_lan(t_bank_item *cur)
{
  static t_layout_lan layout_lan;
  memset(&layout_lan, 0, sizeof(t_layout_lan));
  strncpy(layout_lan.name, cur->name, MAX_NAME_LEN-1);
  layout_lan.x = cur->pbi.position_x;
  layout_lan.y = cur->pbi.position_y;
  layout_lan.hidden_on_graph = cur->pbi.hidden_on_graph;
  return (&layout_lan);
}
/*--------------------------------------------------------------------------*/

