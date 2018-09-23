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

/*--------------------------------------------------------------------------*/
typedef struct t_blkd_data
{
  t_blkd_item item;
  int max_life;
  struct t_blkd_data *prev;
  struct t_blkd_data *next;
} t_blkd_data;
/*--------------------------------------------------------------------------*/
static int g_nb_blkd_data;
static t_blkd_data *g_head_data;
static int g_inhibited;


/****************************************************************************/
static int same_blkd_ident(t_blkd_item *cur, t_blkd_item *target)
{
  int result = 0;
  if ((cur->pid  ==  target->pid)   &&
      (cur->llid ==  target->llid)  &&
      (cur->fd   ==  target->fd))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_blkd_data *find_blkd_data(t_blkd_item *target)
{
  t_blkd_data *cur = g_head_data;
  while (cur)
    {
    if (same_blkd_ident(&(cur->item), target))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_blkd_data *alloc_blkd_data(t_blkd_item *item)
{
  t_blkd_data *cur = (t_blkd_data *) clownix_malloc(sizeof(t_blkd_data), 7);
  memset(cur, 0, sizeof(t_blkd_data));
  if (g_head_data)
    g_head_data->prev = cur;
  cur->next = g_head_data;
  g_head_data = cur;
  memcpy(&(cur->item), item, sizeof(t_blkd_item));
  g_nb_blkd_data += 1;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_blkd_data(t_blkd_data *cur)
{
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur == g_head_data)
    g_head_data = cur->next;
  clownix_free(cur, __FUNCTION__);
  g_nb_blkd_data -= 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_data_get_reports(t_blkd_reports *blkd)
{
  t_blkd_data *cur = g_head_data;
  int i, len, result = -1;
  t_blkd_item *it;
  if (!g_inhibited)
    {
    if ((g_nb_blkd_data > 0) && (cur))
      {
      result = 0;
      len = g_nb_blkd_data * sizeof(t_blkd_item);
      blkd->nb_blkd_reports = g_nb_blkd_data;
      blkd->blkd_item = (t_blkd_item *) clownix_malloc(len, 7);
      memset(blkd->blkd_item, 0, len);
      it = blkd->blkd_item;
      for (i=0; i<g_nb_blkd_data; i++)
        {
        if (!cur)
          KERR(" ");
        else
          {
          memcpy(&(it[i]), &(cur->item), sizeof(t_blkd_item));
          cur = cur->next;
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_report(void *ptr, int llid, t_blkd_item *item)
{
  t_blkd_data *cur;
  if (!g_inhibited)
    {
    cur = find_blkd_data(item);
    if (!cur)
      {
      cur = alloc_blkd_data(item);
      }
    else
      {
      memcpy(&(cur->item), item, sizeof(t_blkd_item));
      }
    cur->max_life = 5;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_data_clean_all(void)
{
  t_blkd_data *next, *cur = g_head_data;
  while (cur)
    {
    next = cur->next;
      free_blkd_data(cur);
    cur = next;
    }
  g_inhibited = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_data_heartbeat(void)
{
  t_blkd_data *next, *cur = g_head_data;
  if (!g_inhibited)
    {
    while (cur)
      {
      next = cur->next;
      cur->max_life -= 1;
      if (cur->max_life == 0)
        free_blkd_data(cur);
      cur = next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_data_subscribe_to_all(int on)
{
  int i, nb;
  int32_t *llid_tab;
  if (!g_inhibited)
    {
    rpct_send_report_sub(NULL, 0, on);
    rpct_send_report_sub(NULL, get_doorways_llid(), on);
    nb = mulan_get_all_llid(&llid_tab);
    for (i=0; i<nb; i++)
      rpct_send_report_sub(NULL, llid_tab[i], on);
    clownix_free(llid_tab, __FUNCTION__);
    nb = endp_mngt_get_all_llid(&llid_tab);
    for (i=0; i<nb; i++)
      rpct_send_report_sub(NULL, llid_tab[i], on);
    clownix_free(llid_tab, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_data_init(void)
{
  g_inhibited = 0;
  g_nb_blkd_data = 0;
  g_head_data = NULL;
}
/*--------------------------------------------------------------------------*/



