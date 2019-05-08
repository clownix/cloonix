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
#include <gtk/gtk.h>
#include <libcrcanvas.h>
#include <string.h>
#include <stdlib.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_consts.h"
#include "bank.h"
#include "eventfull_eth.h"
#include "main_timer_loop.h"
#include "bdplot.h"

typedef struct t_eth_blinks
{
  t_bank_item *bitem_eth;  
  int blink_rx;
  int blink_tx;
} t_eth_blinks;

typedef struct t_obj_blinks
{
  char name[MAX_NAME_LEN];
  int to_be_deleted;
  int nb_eth;
  t_eth_blinks eth_blinks[MAX_ETH_VM+MAX_WLAN_VM];
  t_bank_item *bitem;  
  struct t_obj_blinks *hash_prev;
  struct t_obj_blinks *hash_next;
  struct t_obj_blinks *glob_prev;
  struct t_obj_blinks *glob_next;
} t_obj_blinks;

static int glob_current_obj_nb;
static t_obj_blinks *head_glob_obj_blinks;
static t_obj_blinks *head_hash_obj_blinks[0xFF+1];

/*****************************************************************************/
static int get_hash_of_name(char *name)
{
  int len = strlen(name);
  int i, result;
  char hash = 0;
  for (i=0; i<len; i++)
    hash ^= name[i];
  result = (hash & 0xFF);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_obj_blinks *get_obj_blinks_with_name(char *name)
{
  int idx = get_hash_of_name(name);
  t_obj_blinks *cur = head_hash_obj_blinks[idx];
  while (cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->hash_next;
    }
  if (cur && cur->to_be_deleted)
    cur = NULL;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void transfert_blink_on_eth_bitem(t_eth_blinks *cur)
{ 
  if (cur->bitem_eth)
    {
    if (cur->blink_rx)
      {
      cur->bitem_eth->pbi.blink_rx = 1;
      cur->blink_rx = 0;
      }
    else
      cur->bitem_eth->pbi.blink_rx = 0;

    if (cur->blink_tx)
      {
      cur->bitem_eth->pbi.blink_tx = 1;
      cur->blink_tx = 0;
      }
    else
      cur->bitem_eth->pbi.blink_tx = 0;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void transfert_blink_on_sat(t_eth_blinks *cur, t_bank_item *bitem)
{
  if (cur->blink_rx)
    {
    bitem->pbi.blink_rx = 1;
    cur->blink_rx = 0;
    }
  else
    bitem->pbi.blink_rx = 0;
  if (cur->blink_tx)
    {
    bitem->pbi.blink_tx = 1;
    cur->blink_tx = 0;
    }
  else
    bitem->pbi.blink_tx = 0;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void blink_on_obj(void)
{
  int i;
  t_obj_blinks *cur = head_glob_obj_blinks;
  while (cur)
    {
    if (!(cur->to_be_deleted))
      {
      if (cur->nb_eth == 0)
        {
        transfert_blink_on_sat(&(cur->eth_blinks[0]), cur->bitem);
        }
      else
        {
        for (i=0; i<cur->nb_eth; i++)
          transfert_blink_on_eth_bitem(&(cur->eth_blinks[i]));
        }
      }
    cur = cur->glob_next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void eventfull_arrival(int nb_endp, t_eventfull_endp *endp)
{
  t_obj_blinks *cur;
  int i, num;
  char *name;
  for (i=0; i<nb_endp; i++)
    {
    name = endp[i].name;
    cur = get_obj_blinks_with_name(name);
    if (cur)
      {
      num = endp[i].num;
      if ((num < 0) || (num > MAX_ETH_VM+MAX_WLAN_VM))
        KOUT("%d", num);
      if ((cur->bitem->pbi.pbi_node) && (num == 0))
        {
        cur->bitem->pbi.pbi_node->node_cpu = endp[i].cpu;
        cur->bitem->pbi.pbi_node->node_ram = endp[i].ram;
        }
      if ((num == 0) && (!(cur->eth_blinks[num].bitem_eth)))
        {
        if (endp[i].prx)
          cur->eth_blinks[num].blink_rx = 1;
        if (endp[i].ptx)
          cur->eth_blinks[num].blink_tx = 1;
        bdplot_newdata(name, num, endp[i].ms, endp[i].btx, endp[i].brx);
        }
      else if (cur->eth_blinks[num].bitem_eth)
        {
        if (cur->eth_blinks[num].bitem_eth->num != num)
          KOUT(" ");
        if (endp[i].prx)
          cur->eth_blinks[num].blink_rx = 1;
        if (endp[i].ptx)
          cur->eth_blinks[num].blink_tx = 1;
        bdplot_newdata(name, num, endp[i].ms, endp[i].btx, endp[i].brx);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void eventfull_packets_data(t_eventfull *eventfull)
{
  if (!eventfull)
    KOUT(" ");
  eventfull_arrival(eventfull->nb_endp, eventfull->endp);
  blink_on_obj();
  eventfull_periodic_work();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void update_eth_blinks(t_obj_blinks *obj_blinks)
{
  int i;
  t_list_bank_item *cur,*head_eth_lst=obj_blinks->bitem->head_eth_list;
  cur = head_eth_lst;
  while(cur)
    {
    if (cur->bitem->bank_type == bank_type_eth) 
      obj_blinks->nb_eth += 1;
    cur = cur->next;
    }
  for (i=0; i<obj_blinks->nb_eth; i++) 
    {
    cur = head_eth_lst;
    while (cur)
      {
      if (cur->bitem->num == i)
        {
        obj_blinks->eth_blinks[i].bitem_eth = cur->bitem;
        break; 
        }
      cur = cur->next;
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_obj_create(void *data)
{
  t_obj_blinks *cur, *nblk = (t_obj_blinks *) data;
  int idx;
  t_bank_item *bitem;
  if (!nblk)
    KOUT(" ");
  cur = get_obj_blinks_with_name(nblk->name);
  if (cur)
    KOUT(" ");
  idx = get_hash_of_name(nblk->name);
  bitem = look_for_node_with_id(nblk->name);
  if (bitem == NULL)
    bitem = look_for_sat_with_id(nblk->name);
  if (bitem == NULL)
    clownix_free(nblk, __FUNCTION__);
  else
    {
    cur = nblk;
    cur->bitem = bitem;
    update_eth_blinks(cur);
    if (head_hash_obj_blinks[idx])
      head_hash_obj_blinks[idx]->hash_prev = cur;
    cur->hash_next = head_hash_obj_blinks[idx];
    head_hash_obj_blinks[idx] = cur;
    if (head_glob_obj_blinks)
      head_glob_obj_blinks->glob_prev = cur;
    cur->glob_next = head_glob_obj_blinks;
    head_glob_obj_blinks = cur;
    glob_current_obj_nb += 1;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void eventfull_obj_create(char *name)
{
  t_obj_blinks *cur = get_obj_blinks_with_name(name);
  if (cur)
    KOUT("%s", name);
  cur = (t_obj_blinks *) clownix_malloc(sizeof(t_obj_blinks), 13);
  memset(cur, 0, sizeof(t_obj_blinks));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  clownix_timeout_add(1, timeout_obj_create, cur, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_obj_delete(void *data)
{
  int idx;
  t_obj_blinks *cur = (t_obj_blinks *) data;
  if (!cur)
    KOUT(" ");
  if (cur->to_be_deleted != 1) 
    KOUT("%d", cur->to_be_deleted);
  idx = get_hash_of_name(cur->name);
  if (cur->hash_prev)
    cur->hash_prev->hash_next = cur->hash_next;
  if (cur->hash_next)
    cur->hash_next->hash_prev = cur->hash_prev;
  if (cur == head_hash_obj_blinks[idx])
    head_hash_obj_blinks[idx] = cur->hash_next;
  if (cur->glob_prev)
    cur->glob_prev->glob_next = cur->glob_next;
  if (cur->glob_next)
    cur->glob_next->glob_prev = cur->glob_prev;
  if (cur == head_glob_obj_blinks)
    head_glob_obj_blinks = cur->glob_next;
  clownix_free(cur, __FUNCTION__);
  glob_current_obj_nb -= 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void eventfull_obj_delete(char *name)
{
  t_obj_blinks *cur = get_obj_blinks_with_name(name);
  if (!cur)
    KERR("%s", name);
  else
    {
    cur->to_be_deleted = 1;
    clownix_timeout_add(1, timeout_obj_delete, cur, NULL, NULL);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void eventfull_init(void)
{
  glob_current_obj_nb = 0;
}
/*---------------------------------------------------------------------------*/

