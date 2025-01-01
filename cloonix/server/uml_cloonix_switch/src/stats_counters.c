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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <dirent.h>



#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "commun_daemon.h"
#include "header_sock.h"



typedef struct t_stats_sub
{
  char name[MAX_NAME_LEN];
  int  num;
  int llid;
  int tid;
  t_stats_counts stats_counts;
  struct t_stats_sub *prev;
  struct t_stats_sub *next;
} t_stats_sub;

static t_stats_sub *g_head_stats_sub;


/****************************************************************************/
static t_stats_count_item *get_next_count(t_stats_sub *stats_sub)
{
  t_stats_count_item *result = NULL;
  int idx;
  if (stats_sub->stats_counts.nb_items + 1 >= MAX_STATS_ITEMS)
    KERR("%s %d", stats_sub->name, stats_sub->stats_counts.nb_items);
  else
    {
    idx = stats_sub->stats_counts.nb_items;
    result = &(stats_sub->stats_counts.item[idx]);
    stats_sub->stats_counts.nb_items += 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_stats_sub *find_stats_sub_with_llid(int llid)
{
  t_stats_sub *cur = g_head_stats_sub;
  while (cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_stats_sub *find_stats_sub_with_name(char *name, int num)
{
  t_stats_sub *cur = g_head_stats_sub;
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
static t_stats_sub *find_stats_sub(char *name, int num, int llid)
{
  t_stats_sub *cur = g_head_stats_sub;
  while (cur)
    {
    if ((!strcmp(cur->name, name)) && (cur->num == num) && (cur->llid == llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int alloc_stats_sub(char *name, int num, int llid, int tid)
{
  int result = -1;
  t_stats_sub *cur = find_stats_sub(name, num, llid);
  if (!cur)
    {
    result = 0;
    cur = (t_stats_sub *) clownix_malloc(sizeof(t_stats_sub), 7);
    memset(cur, 0, sizeof(t_stats_sub));
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    cur->num = num;
    cur->llid = llid;
    cur->tid = tid;
    if (g_head_stats_sub)
      g_head_stats_sub->prev = cur;
    cur->next = g_head_stats_sub;
    g_head_stats_sub = cur;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_stats_sub(t_stats_sub *stats_sub)
{
  if (stats_sub->next)
    stats_sub->next->prev = stats_sub->prev;
  if (stats_sub->prev)
    stats_sub->prev->next = stats_sub->next;
  if (stats_sub == g_head_stats_sub)
    g_head_stats_sub = stats_sub->next;
  clownix_free(stats_sub, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int local_stats_sub(int sub, char *name, int num, int llid, int tid)
{
  int result = -1;
  t_stats_sub *cur;
  if (sub)
    {
    if (!(alloc_stats_sub(name, num, llid, tid)))
      result = 0;
    else
      KERR("ERROR: %s %d", name, num);
    }
  else
    {
    cur = find_stats_sub(name, num, llid);
    if (cur)
      {
      free_stats_sub(cur);
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_stats_sub *find_next_stats_sub(t_stats_sub *head, char *name, int num)
{
  t_stats_sub *cur;
  if (head)
    cur = head->next;
  else
    cur = g_head_stats_sub;
  while (cur)
    {
    if ((!strcmp(name, cur->name)) && (cur->num == num))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_counters_update_endp_tx_rx(char *name, int num, unsigned int ms, 
                                      int ptx, int btx, int prx, int brx)
{
  t_stats_count_item *sci;
  t_stats_sub *sub = find_next_stats_sub(NULL, name, num);
  while(sub)
    {
    sci = get_next_count(sub);
    if (sci)
      {
      sci->time_ms = ms;
      sci->ptx = ptx;
      sci->btx = btx;
      sci->prx = prx;
      sci->brx = brx;
      }
    sub = find_next_stats_sub(sub, name, num);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_evt_stats_endp_sub(int llid, int tid, char *name, int num, int sub)
{
  char *network = cfg_get_cloonix_name();
  t_stats_counts sc;
  memset(&sc, 0, sizeof(t_stats_counts));
  if (1)
    send_evt_stats_endp(llid, tid, network, name, num, &sc, 1);
  else
    {
    if (local_stats_sub(sub, name, num, llid, tid))
      send_evt_stats_endp(llid, tid, network, name, num, &sc, 1);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_counters_heartbeat(void)
{
  char *network = cfg_get_cloonix_name();
  t_stats_sub *cur = g_head_stats_sub;
  while (cur)
    {
    if (msg_exist_channel(cur->llid))
      {
      if (cur->stats_counts.nb_items)
        {
        send_evt_stats_endp(cur->llid, cur->tid, 
                            network, cur->name, cur->num,
                            &cur->stats_counts, 0);
        }
      memset(&cur->stats_counts, 0, sizeof(t_stats_counts));
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_counters_llid_close(int llid)
{
  t_stats_sub *cur;
  cur = find_stats_sub_with_llid(llid);
  while (cur)
    {
    free_stats_sub(cur);
    cur = find_stats_sub_with_llid(llid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_counters_death(char *name, int num)
{
  char *network = cfg_get_cloonix_name();
  t_stats_sub *cur;
  t_stats_counts sc;
  memset(&sc, 0, sizeof(t_stats_counts));
  cur = find_stats_sub_with_name(name, num);
  while (cur)
    {
    send_evt_stats_endp(cur->llid, cur->tid, network, 
                        cur->name, cur->num, &sc, 1);
    free_stats_sub(cur);
    cur = find_stats_sub_with_name(name, num);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void stats_counters_init(void)
{
  g_head_stats_sub = NULL;
}
/*--------------------------------------------------------------------------*/

