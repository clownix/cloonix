/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include "commun_daemon.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "msg.h"
#include "lan_to_name.h"
#include "ovs_snf.h"


/*---------------------------------------------------------------------------*/
typedef struct t_bij_elem
{
  char name[MAX_NAME_LEN];
  int idx;
  int nb_users;
  t_item_el *head_item;
} t_bij_elem;
/*---------------------------------------------------------------------------*/
static int glob_total_elems;
static t_bij_elem *tab_elems[MAX_LAN];
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_hash_of_id(int len, void *ident)
{
  int i, result;
  short *short_ident = (short *) ident;
  char *char_ident;
  short last;
  short hash = 0;
  if (len < 1)
    KOUT(" ");
  else if (len == 1)
    {
    char_ident = (char *) ident;
    result = (*char_ident & 0x3FFF) + 1;
    }
  else
    {
    for (i=0; i<(len/2); i++)
      hash ^= short_ident[i];
    if (len%2)
      {
      char_ident = (char *) ident;
      last = (short) (char_ident[len - 1] & 0xFF);
      hash ^= last;
      }
    result = (hash & 0x3FFF) + 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int find_elem_with_name(char *name)
{
  int result = 0;
  int i, index, idx;
  idx = get_hash_of_id(strlen(name), (void *)name);
  for (i = idx; i < idx + MAX_LAN; i++)
    {
    index = i;
    if (index >= MAX_LAN)
      {
      index -= MAX_LAN;
      if (index == 0)
        index = 1;
      }
    if ((index<=0) || (index>=MAX_LAN))
      KOUT(" ");
    if ((tab_elems[index]) &&
        (!strcmp(tab_elems[index]->name, name)))
      {
      result = index;  
      break;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int find_free_idx_for_name(char *name)
{
  int idx, count = 0;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  idx = get_hash_of_id(strlen(name), (void *)name);
  while(tab_elems[idx])
    {
    count++;
    if (count == MAX_LAN)
      KOUT(" ");
    idx++;
    if (idx == MAX_LAN)
      idx = 1;
    }
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_item_el *find_item_el(t_bij_elem *target, int item_type,
                               char *item_name, int item_num)
{
  t_item_el *cur = target->head_item;
  while(cur)
    {
    if ((cur->item_type == item_type) &&
        (!strcmp(cur->item_name, item_name)) &&
        (cur->item_num == item_num))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_item_mac(char *name, t_item_el *el, char *mac)
{

  t_item_mac *cur = (t_item_mac *) malloc(sizeof(t_item_mac));
  memset(cur, 0, sizeof(t_item_mac));
  strncpy(cur->mac, mac, MAX_NAME_LEN-1);
  cur->next = el->head_mac;
  el->head_mac = cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_item_el(t_bij_elem *target, int item_type,
                        char *item_name, int item_num)
{
  t_item_el *cur = (t_item_el *) malloc(sizeof(t_item_el));
  memset(cur, 0, sizeof(t_item_el));
  cur->item_type = item_type;
  strncpy(cur->item_name, item_name, MAX_NAME_LEN-1); 
  cur->item_num = item_num;
  if (target->head_item)
    target->head_item->prev = cur;
  cur->next = target->head_item;
  target->head_item = cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void del_item_el(t_bij_elem *target, int item_type,
                        char *item_name, int item_num)
{
  t_item_mac *nx, *mc;
  t_item_el *cur = find_item_el(target, item_type, item_name, item_num);
  if (!cur)
    KERR("ERROR %s %d %s %d", target->name, item_type, item_name, item_num); 
  else
    {
    mc = cur->head_mac;
    while(mc)
      {
      nx = mc->next;
      free(mc);
      mc = nx;
      }
    }
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
int lan_add_name(char *name, int item_type, char *item_name, int item_num)
{
  int idx;
  t_bij_elem *target;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  idx = find_elem_with_name(name);
  if (!idx)
    {
    idx = find_free_idx_for_name(name);
    target = (t_bij_elem *) clownix_malloc(sizeof(t_bij_elem), 13);
    memset(target, 0, sizeof(t_bij_elem));
    target->idx = idx;
    strcpy(target->name, name);
    tab_elems[idx] = target;
    glob_total_elems++;
    if (glob_total_elems >= MAX_LAN)
      KOUT(" ");
    layout_add_lan(name, 0);
    msg_lan_add_name(name);
    add_item_el(target, item_type, item_name, item_num);
    }
  else
    {
    target = tab_elems[idx];
    add_item_el(target, item_type, item_name, item_num);
    }
  target->nb_users += 1;
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void lan_add_mac(char *name, int item_type,
                 char *item_name, int item_num, char *mac)
{
  int idx;
  t_bij_elem *target;
  t_item_el *el;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  idx = find_elem_with_name(name);
  if (!idx)
    KERR("ERROR %s %d %s %d %s", name, item_type, item_name, item_num, mac);
  else
    {
    target = tab_elems[idx];
    el = find_item_el(target, item_type, item_name, item_num);
    if (!el)
      KERR("ERROR %s %d %s %d %s", name, item_type, item_name, item_num, mac);
    else
      add_item_mac(name, el, mac);
    ovs_snf_lan_mac_change(name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int lan_del_name(char *name, int item_type, char *item_name, int item_num)
{
  int target_idx;
  t_bij_elem *target;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  target_idx = find_elem_with_name(name);
  if (target_idx)
    {
    if (tab_elems[target_idx]->nb_users <= 0)
      KOUT(" ");
    target = tab_elems[target_idx];
    del_item_el(target, item_type, item_name, item_num);
    tab_elems[target_idx]->nb_users -= 1;
    if (tab_elems[target_idx]->nb_users == 0)
      {
      clownix_free(tab_elems[target_idx], __FUNCTION__);
      tab_elems[target_idx] = NULL;
      glob_total_elems--;
      if (glob_total_elems < 0)
        KOUT(" ");
      layout_del_lan(name);
      msg_lan_del_name(name);
      target_idx = 2*MAX_LAN;
      }
    ovs_snf_lan_mac_change(name);
    }
  return target_idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int lan_get_with_name(char *name)
{
  int target_idx;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT("%p", name);
  target_idx = find_elem_with_name(name);
  return target_idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_item_el *lan_get_head_item(char *name)
{
  int target_idx;
  t_item_el *cur = NULL;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT("%p", name);
  target_idx = find_elem_with_name(name);
  if (target_idx)
    cur = tab_elems[target_idx]->head_item;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void lan_init(void)
{
  glob_total_elems = 0;
  memset(tab_elems, 0, MAX_LAN * sizeof(t_bij_elem *));
}
/*---------------------------------------------------------------------------*/



