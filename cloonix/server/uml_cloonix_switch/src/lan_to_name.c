/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include "mulan_mngt.h"
#include "layout_rpc.h"
#include "layout_topo.h"





/*---------------------------------------------------------------------------*/
typedef struct t_bij_elem
{
  char name[MAX_NAME_LEN];
  int idx;
  int nb_users;
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
int lan_add_name(char *name, int llid)
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
    layout_add_lan(name, llid);
    }
  else
    target = tab_elems[idx];
  target->nb_users += 1;
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int lan_del_name(char *name)
{
  int target_idx;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  target_idx = find_elem_with_name(name);
  if (target_idx)
    {
    if (tab_elems[target_idx]->nb_users <= 0)
      KOUT(" ");
    tab_elems[target_idx]->nb_users -= 1;

    if (tab_elems[target_idx]->nb_users == 0)
      {
      clownix_free(tab_elems[target_idx], __FUNCTION__);
      tab_elems[target_idx] = NULL;
      glob_total_elems--;
      if (glob_total_elems < 0)
        KOUT(" ");
      layout_del_lan(name);
      }
    }
  return target_idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int lan_get_with_name(char *name)
{
  int target_idx;
  if ((!name) || (!name[0]) || (strlen(name) >= MAX_NAME_LEN))
    KOUT(" ");
  target_idx = find_elem_with_name(name);
  return target_idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *lan_get_with_num(int num)
{
  char *result = NULL;
  if (tab_elems[num])
    result = tab_elems[num]->name;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void lan_init(void)
{
  glob_total_elems = 0;
  memset(tab_elems, 0, MAX_LAN * sizeof(t_bij_elem *));
}
/*---------------------------------------------------------------------------*/



