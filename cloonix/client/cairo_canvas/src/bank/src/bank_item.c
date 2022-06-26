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
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "bank.h" 
#include "bank_item.h" 
#include "external_bank.h"
#include "subsets.h"
#include "bijection.h"
#include "interface.h"


static t_list_bank_item *head_connected_groups;

static t_head_bank head_bank[bank_type_max];
/*--------------------------------------------------------------------------*/
static int max_edge_nb_per_item;
/*--------------------------------------------------------------------------*/
static int g_hysteresis_refresh_topo_info;


/****************************************************************************/
static void free_all_connected_groups(void)
{
  t_list_bank_item *next_group, *cur_group = head_connected_groups;
  t_bank_item *next, *cur;
  head_connected_groups = NULL;
  while(cur_group)
    {
    cur = cur_group->bitem; 
    while(cur)
      {
      next = cur->group_next;
      cur->group_next = NULL;
      cur->group_head = NULL;
      cur = next;
      }
    next_group = cur_group->next;
    clownix_free(cur_group, __FUNCTION__);
    cur_group = next_group;
    }
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
static int exists_in_group_chain(t_bank_item *head, t_bank_item *bitem)
{
  t_bank_item *cur = head;
  while(cur)
    {
    if (cur == bitem)
      return 1;  
    cur = cur->group_next;
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_all_connected_groups(t_subsets *subsets)
{
  t_list_bank_item *group;
  t_subsets *cur = subsets;
  t_subelem *elem;
  char *name;
  int bank_type, num;
  t_bank_item *prev_bitem, *bitem, *head_group;
  while(cur)
    {
    group = (t_list_bank_item *)clownix_malloc(sizeof(t_list_bank_item), 7);
    memset(group, 0, sizeof(t_list_bank_item));
    group->next = head_connected_groups;
    head_connected_groups = group;
    bij_get(cur->subelem->name, &bank_type, &name, &num);
    head_group = bank_get_item(bank_type, name, num, NULL); 
    if (!head_group)
      KOUT("%d :%s: %d\n", bank_type, name, num);
    if (head_group->group_head || head_group->group_next)
      KOUT(" ");
    group->bitem = head_group;
    group->bitem->group_head = head_group;
    prev_bitem = head_group;
    elem = cur->subelem->next;
    while(elem)
      {
      bij_get(elem->name, &bank_type, &name, &num);
      bitem = bank_get_item(bank_type, name, num, NULL); 
      if ((!bitem) || (exists_in_group_chain(head_group, bitem)))
        KOUT("%p %p", head_group, bitem);
      if (bitem->group_head || bitem->group_next)
        KOUT(" ");
      prev_bitem->group_next = bitem;
      prev_bitem = bitem;
      bitem->group_head = head_group;
      elem = elem->next;
      }
    cur = cur->next;
    }
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int is_not_an_eth(t_bank_item *bitem)
{
  int result = 1;
  if (bitem->bank_type == bank_type_eth) 
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int is_not_an_edge(t_bank_item *bitem)
{
  int result = 1;
  if (bitem->bank_type == bank_type_edge)
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void refresh_max_edge_nb_per_item(void)
{
  t_bank_item *cur = head_bank[bank_type_all_non_edges_items].head;
  max_edge_nb_per_item = 0;
  while(cur)
    {
    if (cur->nb_edges > max_edge_nb_per_item)
      max_edge_nb_per_item = cur->nb_edges;
    cur = cur->glob_next;
    } 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void add_to_nb_edge(t_bank_item *bitem)
{
  if (!(is_not_an_edge(bitem)))
    {
    if ((!bitem->att_eth) || (!bitem->att_lan))
      KOUT(" ");
    bitem->att_eth->nb_edges += 1;
    bitem->att_lan->nb_edges  += 1;
    if (!(is_not_an_eth(bitem->att_eth)))
      {
      if (!bitem->att_eth->att_node)
        KOUT(" ");
      bitem->att_eth->att_node->nb_edges += 1;
      }
    refresh_max_edge_nb_per_item();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void del_from_nb_edge(t_bank_item *bitem)
{
  if (!(is_not_an_edge(bitem)))
    {
    if ((!bitem->att_eth) || (!bitem->att_lan))
      KOUT(" ");
    if (bitem->att_eth->nb_edges <= 0) 
      KOUT("%s", bitem->att_eth->name);
    if (bitem->att_lan->nb_edges <= 0) 
      KOUT("%s", bitem->att_lan->name);
    bitem->att_eth->nb_edges -= 1;
    bitem->att_lan->nb_edges  -= 1;
    if (!(is_not_an_eth(bitem->att_eth)))
      {
      if (!bitem->att_eth->att_node)
        KOUT(" ");
      if (bitem->att_eth->att_node->nb_edges <= 0)
         KOUT("%s", bitem->att_eth->att_node->name);
      bitem->att_eth->att_node->nb_edges -= 1;
      }
    refresh_max_edge_nb_per_item();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void add_to_glob_chain(t_bank_item *bitem)
{
  t_head_bank *head;
  if (is_not_an_edge(bitem))
    {
    head = &(head_bank[bank_type_all_non_edges_items]);
    bitem->glob_next = head->head;
    if (head->head)
      head->head->glob_prev = bitem;
    head->head = bitem;
    head->count_nb_items++;
    }
  else 
    {
    head = &(head_bank[bank_type_all_edges_items]);
    bitem->glob_next = head->head;
    if (head->head)
      head->head->glob_prev = bitem;
    head->head = bitem;
    head->count_nb_items++;
    }

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void del_from_glob_chain(t_bank_item *bitem)
{
  t_head_bank *head;
  if (is_not_an_edge(bitem))
    {
    head = &(head_bank[bank_type_all_non_edges_items]);
    if (bitem->glob_next)
      bitem->glob_next->glob_prev = bitem->glob_prev;
    if (bitem->glob_prev)
      bitem->glob_prev->glob_next = bitem->glob_next;
    if (bitem == head->head)
      head->head = bitem->glob_next;
    if (head->count_nb_items <= 0)
      KOUT("%d", head->count_nb_items);
    head->count_nb_items--;
    }
  else 
    {
    head = &(head_bank[bank_type_all_edges_items]);
    if (bitem->glob_next)
      bitem->glob_next->glob_prev = bitem->glob_prev;
    if (bitem->glob_prev)
      bitem->glob_prev->glob_next = bitem->glob_next;
    if (bitem == head->head)
      head->head = bitem->glob_next;
    if (head->count_nb_items <= 0)
      KOUT("%d", head->count_nb_items);
    head->count_nb_items--;
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void unchain_from_head(t_bank_item *bitem)
{
  t_head_bank *head;
  if ((bitem->bank_type <= bank_type_all_non_edges_items) || 
      (bitem->bank_type >= bank_type_max))
    KOUT(" ");
  head = &(head_bank[bitem->bank_type]);
  if (bitem->next)
    bitem->next->prev = bitem->prev;
  if (bitem->prev)
    bitem->prev->next = bitem->next;
  if (bitem == head->head)
    head->head = bitem->next;
  if (head->count_nb_items <= 0)
    KOUT("%d", head->count_nb_items);
  head->count_nb_items--;
  del_from_glob_chain(bitem);
  del_from_nb_edge(bitem);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void chain_to_head(t_bank_item *bitem)
{
  t_head_bank *head;
  if ((bitem->bank_type <= bank_type_all_non_edges_items) || 
      (bitem->bank_type >= bank_type_max))
    KOUT("%d %d %d", bitem->bank_type, bank_type_max, 
                     bank_type_all_non_edges_items);
  head = &(head_bank[bitem->bank_type]);
  bitem->next = head->head;
  if (head->head)
    head->head->prev = bitem;
  head->head = bitem;
  head->count_nb_items++;
  add_to_glob_chain(bitem);
  add_to_nb_edge(bitem);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void timer_refresh_all_connected_groups(void *data)
{
  t_subsets *subsets;
  int nb_nodes = get_nb_total_items();
  int nb_edges = get_max_edge_nb_per_item();
  t_bank_item *cur = head_bank[bank_type_all_edges_items].head;
  g_hysteresis_refresh_topo_info = 0;
  free_all_connected_groups();
  if (nb_nodes >= 2)
    {
    sub_init(nb_nodes, nb_edges);
    while(cur)
      {
      if ((!cur->att_eth) || (!cur->att_lan))
        KOUT(" ");
      if (cur->att_eth->bank_type == bank_type_eth) 
        {
        if (!cur->att_eth->att_node) 
          KOUT(" ");
        sub_mak_link(cur->att_eth->att_node->tag, cur->att_eth->tag);
        }
      if ((!cur->att_eth->pbi.hidden_on_graph) &&
          (!cur->att_lan->pbi.hidden_on_graph))
        sub_mak_link(cur->att_eth->tag, cur->att_lan->tag);
      cur = cur->glob_next;
      }
    sub_breakdown();
    subsets = get_subsets();
    alloc_all_connected_groups(subsets);
    free_subsets();
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void refresh_all_connected_groups(void)
{
  free_all_connected_groups();
  if (g_hysteresis_refresh_topo_info == 0)
    {
    g_hysteresis_refresh_topo_info = 1;
    clownix_timeout_add(10, timer_refresh_all_connected_groups, NULL, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_add_item(int bank_type, char *name, char *lan, int num,
                   t_bank_item *intf, t_bank_item *blan, int eorig) 
{
  char *tag;
  t_bank_item *elem = (t_bank_item *)clownix_malloc(sizeof(t_bank_item), 7);
  memset(elem, 0, sizeof(t_bank_item));
  elem->att_eth = intf;
  elem->att_lan = blan;
  elem->bank_type = bank_type;
  strncpy(elem->name, name, MAX_NAME_LEN-1);
  if (lan)
    strncpy(elem->lan, lan, MAX_NAME_LEN-1);
  elem->num = num;
  if (is_not_an_edge(elem))
    {
    tag = bij_alloc_tag(bank_type, name, num);
    strcpy(elem->tag, tag);
    }
  chain_to_head(elem);
  if (eorig == eorig_modif)
    refresh_all_connected_groups();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_del_item(t_bank_item *bitem, int eorig)
{
  if (!bitem)
    KOUT(" ");
  if (is_not_an_edge(bitem))
    bij_free_tag(bitem->tag); 
//TODO
  unchain_from_head(bitem);
  clownix_free(bitem->pbi.pbi_node, __FUNCTION__);
  clownix_free(bitem->pbi.pbi_sat, __FUNCTION__);
  clownix_free(bitem, __FUNCTION__);
  if (eorig == eorig_modif)
    refresh_all_connected_groups();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_bank_item *bank_look_for_item(int bank_type, char *name, int num,
                                       char *lan)
{
  t_bank_item *cur;
  if ((bank_type <= bank_type_min) || (bank_type >= bank_type_max))
    KOUT(" %d", bank_type);
  cur = head_bank[bank_type].head;
  if (cur && (cur->name[0] == 0))
    KOUT(" %d", bank_type);
  switch(bank_type)
    {
    case bank_type_cnt:
    case bank_type_node:
    case bank_type_lan:
    case bank_type_sat:
      while (cur)
        {
        if (!strcmp(cur->name, name))
          break;
        cur = cur->next;
        }
      break;
    case bank_type_eth:
      while (cur)
        {
        if ((!strcmp(cur->name, name)) &&
            (cur->num == num))
          break;
        cur = cur->next;
        }
      break;
    case bank_type_edge:
      while (cur)
        {
        if (cur->lan[0] == 0)
          KOUT(" ");
        if ((!strcmp(cur->name, name)) &&
            (!strcmp(cur->lan, lan)) &&
            (cur->num == num))
          break;
        cur = cur->next;
        }
      break;
    default:
      KOUT(" ");
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_bank_item *bank_get_item(int bank_type, char *name, int num, char *lan)
{
  return bank_look_for_item( bank_type, name, num, lan);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_bank_item *look_for_node_with_id(char *name)
{
  t_bank_item *result;
  result = bank_look_for_item(bank_type_node, name, 0, NULL);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_bank_item *look_for_cnt_with_id(char *name)
{
  t_bank_item *result;
  result = bank_look_for_item(bank_type_cnt, name, 0, NULL);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void look_for_lan_and_del_all(void)
{
  t_bank_item *cur, *next;
  cur = head_bank[bank_type_lan].head;
  while (cur)
    {
    next = cur->next;
    leave_item_action_surface(cur);
    from_cloonix_switch_delete_lan(cur->name);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_bank_item *match_name_in_list(t_bank_item *head, char *name)
{
  t_bank_item *cur = head;
  while (cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_bank_item *match_name_and_num_in_list(t_bank_item *head, 
                                               char *name, int num)
{
  t_bank_item *cur = head;
  while (cur)
    {
    if ((!strcmp(cur->name, name)) && (cur->num == num))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
t_bank_item *look_for_lan_with_id(char *name)
{
  t_bank_item *result, *head = head_bank[bank_type_lan].head;
  result = match_name_in_list(head, name);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_bank_item *look_for_sat_with_id(char *name)
{
  t_bank_item *result, *head = head_bank[bank_type_sat].head;
  result = match_name_in_list(head, name);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int sat_is_a_a2b(char *name)
{
  int result = -1;
  t_bank_item *bitem = look_for_sat_with_id(name);
  if (bitem)
    {
    result = 0;
    if (bitem->pbi.endp_type == endp_type_a2b)
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
t_bank_item *look_for_eth_with_id(char *name, int num)
{
  t_bank_item *result, *head = head_bank[bank_type_eth].head;
  result = match_name_and_num_in_list(head, name, num); 
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_nb_total_items(void)
{
  return (head_bank[bank_type_all_non_edges_items].count_nb_items);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_bank_item *get_first_glob_bitem(void)
{
  return (head_bank[bank_type_all_non_edges_items].head);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_bank_item *get_head_lan(void)
{
  return (head_bank[bank_type_lan].head);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_max_edge_nb_per_item(void)
{
  return max_edge_nb_per_item;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_list_bank_item *get_head_connected_groups(void)
{
  return head_connected_groups;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int bank_get_dtach_pid(char *name)
{
  t_bank_item *bitem = look_for_node_with_id(name);
  int result = 0;
  if (bitem)
    result = bitem->dtach_pid;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_set_dtach_pid(char *name, int val)
{
  t_bank_item *bitem = look_for_node_with_id(name);
  if (bitem)
    bitem->dtach_pid = val;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int bank_get_spicy_gtk_pid(char *name)
{
  t_bank_item *bitem = look_for_node_with_id(name);
  int result = 0;
  if (bitem)
    result = bitem->spicy_gtk_pid;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int bank_get_wireshark_pid(char *name)
{
  t_bank_item *bitem = look_for_sat_with_id(name);
  int result = 0;
  if (bitem)
    result = bitem->wireshark_pid;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_set_spicy_gtk_pid(char *name, int val)
{
  t_bank_item *bitem = look_for_node_with_id(name);
  if (bitem)
    bitem->spicy_gtk_pid = val;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void bank_set_wireshark_pid(char *name, int val)
{
  t_bank_item *bitem = look_for_sat_with_id(name);
  if (bitem)
    bitem->wireshark_pid = val;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void init_bank_item(void)
{
  g_hysteresis_refresh_topo_info = 0;
  memset(head_bank, 0, bank_type_max*sizeof(t_head_bank));
  selectioned_item_init();
  max_edge_nb_per_item = 0;
  bij_init();
}
/*--------------------------------------------------------------------------*/


