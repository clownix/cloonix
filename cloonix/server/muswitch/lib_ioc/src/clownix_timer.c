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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <sys/epoll.h>
#include "ioc.h"
#include "ioc_ctx.h"



/*****************************************************************************/
static int plug_elem(t_ioc_ctx *ioc_ctx, t_timeout_elem **head, 
                     t_fct_timeout cb, void *data)
{
  int result = 0;
  t_timeout_elem *cur = *head;
  t_timeout_elem *last = *head;
  ioc_ctx->g_current_ref++;
  if (ioc_ctx->g_current_ref == 0xFFFFFFFF)
    ioc_ctx->g_current_ref = 1;
  result = ioc_ctx->g_current_ref;
  while(cur)
    {
    last = cur;
    cur = cur->next;
    }
  cur = (t_timeout_elem *) malloc(sizeof(t_timeout_elem));
  memset(cur, 0, sizeof(t_timeout_elem));
  cur->prev = last;
  if (last) 
    last->next = cur;
  else
    *head = cur;
  ioc_ctx->g_plugged_elem++;
  cur->cb = cb;
  cur->data = data;
  cur->ref = result;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int unplug_elem(t_ioc_ctx *ioc_ctx, t_timeout_elem **head, 
                       t_timeout_elem *elem)
{
  int result = 0;
  if (elem->prev)
    elem->prev->next = elem->next;
  if (elem->next)
    elem->next->prev = elem->prev;
  if (*head == elem)
    *head = elem->next;
  ioc_ctx->g_plugged_elem--;
  if (ioc_ctx->g_plugged_elem < 0)
    KOUT(" ");
  free(elem);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_all_current_grape_idx(t_all_ctx *all_ctx, 
                                          t_timeout_elem **head)
{
  t_timeout_elem *cur = *head;
  t_timeout_elem *next;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  while(cur)
    {
    next = cur->next;
    if (!cur->inhibited)
      cur->cb(all_ctx, cur->data);
    unplug_elem(ioc_ctx, head, cur);
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

 
/*****************************************************************************/
void clownix_timeout_add(t_all_ctx *all_ctx, int nb_beats, 
                         t_fct_timeout cb, void *data,
                         long long *abs_beat, int *ref)
{
  long long abs;
  int ref1, ref2;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  if ((nb_beats < MIN_TIMEOUT_BEATS) || 
      (nb_beats >= MAX_TIMEOUT_BEATS))
    KOUT("%d", nb_beats);
  abs = ioc_ctx->g_current_beat + nb_beats;
  ref1 = (int) (abs % MAX_TIMEOUT_BEATS);
  ref2 = plug_elem(ioc_ctx, &(ioc_ctx->g_grape[ref1]), cb, data);
  if (ref)
    *ref = ref2;
  if (abs_beat)
    *abs_beat = abs;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int clownix_timeout_exists(t_all_ctx *all_ctx, long long abs_beat, int ref)
{
  int result = 0;
  int ref1, ref2;
  t_timeout_elem *cur;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  ref1 = (int) (abs_beat % MAX_TIMEOUT_BEATS);
  ref2 = ref;
  cur = ioc_ctx->g_grape[ref1];
  while(cur)
    {
    if (cur->ref == ref2)
      break;
    cur = cur->next;
    }
  if (cur)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void *clownix_timeout_del(t_all_ctx *all_ctx, long long abs_beat, int ref, 
                          const char *file, int line)
{
  int result = -1;
  void *data = NULL;
  int ref1, ref2;
  t_timeout_elem *cur;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  ref1 = (int) (abs_beat % MAX_TIMEOUT_BEATS);
  ref2 = ref;
  cur = ioc_ctx->g_grape[ref1];
  while(cur)
    {
    if (cur->ref == ref2)
      {
      data = cur->data;
      break;
      }
    cur = cur->next;
    }
  if (cur)
    {
    cur->inhibited = 1;
    result = 0;
    }
  if (abs_beat && result) 
    KOUT("%s line:%d    %lld %lld %d %d\n", file, line, 
         abs_beat, ioc_ctx->g_current_beat, ref1, ref2);
  return data;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clownix_timeout_del_all(t_all_ctx *all_ctx)
{
  int i;
  t_timeout_elem *cur;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  for (i=0; i<MAX_TIMEOUT_BEATS; i++)
    {
    cur = ioc_ctx->g_grape[i];
    while(cur)
      {
      cur->inhibited = 1;
      cur = cur->next;
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clownix_timer_beat(t_all_ctx *all_ctx)
{
  int idx;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  ioc_ctx->g_current_beat++;
  idx = (int) (ioc_ctx->g_current_beat % MAX_TIMEOUT_BEATS);
  process_all_current_grape_idx(all_ctx, &(ioc_ctx->g_grape[idx]));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clownix_timer_init(t_all_ctx *all_ctx)
{
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  memset(ioc_ctx->g_grape, 0, MAX_TIMEOUT_BEATS * sizeof(t_timeout_elem *));
  ioc_ctx->g_current_ref = 0;
  ioc_ctx->g_current_beat = 0;
  ioc_ctx->g_plugged_elem = 0;
}
/*---------------------------------------------------------------------------*/




