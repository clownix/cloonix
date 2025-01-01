/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "bank.h"
#include "bank_item.h"
#include "move.h"
#include "main_timer_loop.h"

void topo_bitem_hide(t_bank_item *bitem);
void topo_bitem_show(t_bank_item *bitem);


typedef struct t_saved_edges
{
  char name_eth[MAX_NAME_LEN];
  char name_lan[MAX_NAME_LEN];
  t_bank_item *bi_eth;
  t_bank_item *bi_lan;
  struct t_saved_edges *prev;
  struct t_saved_edges *next;
} t_saved_edges;

static t_saved_edges *head = NULL;
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static t_saved_edges *get_edg(t_bank_item *bi)
{
  t_saved_edges *edg = head;
  while(edg)
    {
    if (edg->bi_eth == bi)
      break;
    if (edg->bi_lan == bi)
      break;
    edg = edg->next;
    }
  return edg;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_saved_edges *get_edg_pair(t_bank_item *bi_eth, t_bank_item *bi_lan)
{
  t_saved_edges *edg = head;
  while(edg)
    {
    if ((edg->bi_eth == bi_eth) && (edg->bi_lan == bi_lan))
      break;
    edg = edg->next;
    }
  return edg;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_edg(t_saved_edges *edg)
{
  if (edg->next)
    edg->next->prev = edg->prev;
  if (edg->prev)
    edg->prev->next = edg->next;
  if (edg == head)
    head = edg->next;
  clownix_free(edg, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_edg(t_bank_item *bi_eth, t_bank_item *bi_lan)
{
  t_saved_edges *edg;
  edg = (t_saved_edges *) clownix_malloc(sizeof(t_saved_edges), 7);
  memset(edg, 0, sizeof(t_saved_edges));
  strncpy(edg->name_eth, bi_eth->name, MAX_NAME_LEN-1);
  strncpy(edg->name_lan, bi_lan->name, MAX_NAME_LEN-1);
  edg->bi_eth = bi_eth;
  edg->bi_lan = bi_lan;
  if (head)
    head->prev = edg; 
  edg->next = head;
  head = edg;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void del_bitem(t_bank_item *bi)
{
  t_saved_edges *edg = get_edg(bi);
  while(edg)
    {
    free_edg(edg);
    edg = get_edg(bi);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void hidden_visible_del_bitem(t_bank_item *bi)
{
  t_list_bank_item *cur;
  if ((bi->bank_type == bank_type_node) ||
      (bi->bank_type == bank_type_cnt))
    {
    cur = bi->head_eth_list;
    while (cur)
      {
      del_bitem(cur->bitem);
      cur = cur->next;
      }
    }
  else
    del_bitem(bi);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void hidden_visible_save_edge(t_bank_item *bi_eth, t_bank_item *bi_lan)
{
  t_saved_edges *edg = get_edg_pair(bi_eth, bi_lan);
  if (!edg)
    alloc_edg(bi_eth, bi_lan);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void kout_if_not_exist(t_bank_item *bi, char *name)
{
  t_bank_item *cur = get_first_glob_bitem();
  while (cur)
    {
    if (cur == bi)
      break;
    cur = cur->glob_next;
    }
  if (cur == NULL)
    KOUT("%s", name);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void restore_all_edges(t_bank_item *bi)
{ 
  t_saved_edges *next, *edg = head;
  while(edg)
    {
    next = edg->next;
    if (edg->bi_eth == bi)
      {
      if ((!edg->bi_eth->pbi.hidden_on_graph) &&
          (!edg->bi_lan->pbi.hidden_on_graph))
        {
        kout_if_not_exist(edg->bi_eth, edg->name_eth);
        kout_if_not_exist(edg->bi_lan, edg->name_lan);
        add_new_edge(edg->bi_eth, edg->bi_lan, eorig_update);
        free_edg(edg);
        }
      }
    else if (edg->bi_lan == bi)
      {
      if ((!edg->bi_eth->pbi.hidden_on_graph) &&
          (!edg->bi_lan->pbi.hidden_on_graph))
        {
        kout_if_not_exist(edg->bi_eth, edg->name_eth);
        kout_if_not_exist(edg->bi_lan, edg->name_lan);
        add_new_edge(edg->bi_eth, edg->bi_lan, eorig_update);
        free_edg(edg);
        }
      }
    edg = next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void hidden_visible_event_bitem(t_bank_item *bi)
{
  t_list_bank_item *intf;
  if ((bi->bank_type == bank_type_node) ||
      (bi->bank_type == bank_type_cnt))
    {
    intf = bi->head_eth_list;
    while (intf)
      {
      if (!(intf->bitem->pbi.hidden_on_graph))
        {
        topo_bitem_show(intf->bitem);
        restore_all_edges(intf->bitem);
        }
      else
        topo_bitem_hide(intf->bitem);
      intf = intf->next;
      }
    }
  if (!(bi->pbi.hidden_on_graph))
    {
    topo_bitem_show(bi);
    restore_all_edges(bi);
    }
  else
    topo_bitem_hide(bi);
}
/*---------------------------------------------------------------------------*/

/*HIDDEN_VISIBLE*/
/****************************************************************************/
void hidden_visible_modification(t_bank_item *bi, int is_hidden)
{
  bi->pbi.hidden_on_graph = is_hidden;
  hidden_visible_event_bitem(bi);
  refresh_all_connected_groups();
  attached_edge_update_all(bi);
  move_manager_update_item(bi);
}
/*--------------------------------------------------------------------------*/




