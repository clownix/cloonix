/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "io_clownix.h"
#include "commun_daemon.h"
#include "rpc_clownix.h"
#include "mulan_mngt.h"
#include "doorways_mngt.h"
#include "cfg_store.h"
#include "endp_mngt.h"
#include "blkd_data.h"

/*--------------------------------------------------------------------------*/
typedef struct t_blkd_sub
{
  char name[MAX_NAME_LEN];
  int llid;
  int tid;
  struct t_blkd_sub *prev;
  struct t_blkd_sub *next;
} t_blkd_sub;
/*--------------------------------------------------------------------------*/

static t_blkd_sub  *g_head_sub;
static int g_inhibited;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_blkd_sub *find_blkd_sub(int llid)
{
  t_blkd_sub *cur = g_head_sub;
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
static void alloc_blkd_sub(int llid, int tid)
{
  t_blkd_sub *cur = find_blkd_sub(llid);
  cur = (t_blkd_sub *) clownix_malloc(sizeof(t_blkd_sub), 7);
  memset(cur, 0, sizeof(t_blkd_sub));
  cur->llid = llid;
  cur->tid = tid;
  if (g_head_sub)
    g_head_sub->prev = cur;
  cur->next = g_head_sub;
  g_head_sub = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_blkd_sub(t_blkd_sub *cur)
{
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur == g_head_sub)
    g_head_sub = cur->next;
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_blkd_reports_sub(int llid, int tid, int sub)
{
  t_blkd_sub *cur = find_blkd_sub(llid);
  if (!g_inhibited)
    {
    if (sub)
      {
      if (!cur)
        {
        if (g_head_sub == NULL)
          blkd_data_subscribe_to_all(1);
        alloc_blkd_sub(llid, tid);
        }
      }
    else
      {
      if (cur)
        {
        free_blkd_sub(cur);
        if (g_head_sub == NULL)
          blkd_data_subscribe_to_all(0);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_sub_llid_close(int llid, int from_clone)
{
  t_blkd_sub *cur;
  if (!g_inhibited)
    {
    cur = find_blkd_sub(llid);
    if (cur)
      {
      free_blkd_sub(cur);
      if (g_head_sub == NULL)
        {
        if (!from_clone)
          blkd_data_subscribe_to_all(0);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_sub_heartbeat(void)
{
  static int count_no_sub_present = 0;
  static int count_sub_present = 0;
  t_blkd_reports blkd;
  t_blkd_sub  *cur = g_head_sub;
  if (!g_inhibited)
    { 
    if (g_head_sub == NULL)
      {
      count_sub_present = 0;
      count_no_sub_present += 1;
      if (count_no_sub_present == 10) 
        {
        blkd_data_subscribe_to_all(0);
        count_no_sub_present = 0;
        }
      }
    else
      {
      count_no_sub_present = 0;
      count_sub_present += 1;
      if (count_sub_present == 10) 
        {
        blkd_data_subscribe_to_all(1);
        count_sub_present = 0;
        }
      }
    if (!blkd_data_get_reports(&blkd))
      {
      while (cur)
        {
        send_blkd_reports(cur->llid, cur->tid, &blkd);
        cur = cur->next;
        }
      clownix_free(blkd.blkd_item, __FUNCTION__);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_sub_clean_all(void)
{
  t_blkd_sub *next, *cur = g_head_sub;
  while (cur)
    {
    next = cur->next;
      free_blkd_sub(cur);
    cur = next;
    }
  g_inhibited = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_sub_init(void)
{
  g_inhibited = 0;
  g_head_sub = NULL;
}
/*--------------------------------------------------------------------------*/



