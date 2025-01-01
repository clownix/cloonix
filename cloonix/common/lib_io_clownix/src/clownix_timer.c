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
#include "io_clownix.h"

/*****************************************************************************/
#define MAX_TIMEOUT_BEATS 0x3FFFF
#define MIN_TIMEOUT_BEATS 1
/*---------------------------------------------------------------------------*/
typedef struct t_timeout_elem
{
  int inhibited;
  t_fct_timeout cb;
  void *data;
  int ref;
  struct t_timeout_elem *prev;
  struct t_timeout_elem *next;
} t_timeout_elem; 
/*---------------------------------------------------------------------------*/

static t_timeout_elem *grape[MAX_TIMEOUT_BEATS]; 
static int current_ref;
static long long current_beat;
static int plugged_elem;


/*****************************************************************************/
int clownix_get_nb_timeouts(void)
{
  return plugged_elem;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int plug_elem(t_timeout_elem **head, t_fct_timeout cb, void *data)
{
  int result = 0;
  t_timeout_elem *cur = *head;
  t_timeout_elem *last = *head;
  current_ref++;
  if (current_ref == 0xFFFFFFFF)
    current_ref = 1;
  result = current_ref;
  while(cur)
    {
    last = cur;
    cur = cur->next;
    }
  cur = (t_timeout_elem *) clownix_malloc(sizeof(t_timeout_elem), 1);
  memset(cur, 0, sizeof(t_timeout_elem));
  cur->prev = last;
  if (last) 
    last->next = cur;
  else
    *head = cur;
  plugged_elem++;
  cur->cb = cb;
  cur->data = data;
  cur->ref = result;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int unplug_elem(t_timeout_elem **head, t_timeout_elem *elem)
{
  int result = 0;
  if (elem->prev)
    elem->prev->next = elem->next;
  if (elem->next)
    elem->next->prev = elem->prev;
  if (*head == elem)
    *head = elem->next;
  plugged_elem--;
  if (plugged_elem < 0)
    KOUT(" ");
  clownix_free(elem, __FUNCTION__);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_all_current_grape_idx(t_timeout_elem **head)
{
  t_timeout_elem *cur = *head;
  t_timeout_elem *next;
  while(cur)
    {
    next = cur->next;
    if (!cur->inhibited)
      cur->cb(cur->data);
    unplug_elem(head, cur);
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

 
/*****************************************************************************/
void clownix_timeout_add(int nb_beats, t_fct_timeout cb, void *data,
                         long long *abs_beat, int *ref)
{
  long long abs;
  int ref1, ref2;
  if ((nb_beats < MIN_TIMEOUT_BEATS) || 
      (nb_beats >= MAX_TIMEOUT_BEATS))
    KOUT("%d", nb_beats);
  abs = current_beat + nb_beats;
  ref1 = (int) (abs % MAX_TIMEOUT_BEATS);
  ref2 = plug_elem(&(grape[ref1]), cb, data);
  if (ref)
    *ref = ref2;
  if (abs_beat)
    *abs_beat = abs;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int clownix_timeout_exists(long long abs_beat, int ref)
{
  int result = 0;
  int ref1, ref2;
  t_timeout_elem *cur;
  ref1 = (int) (abs_beat % MAX_TIMEOUT_BEATS);
  ref2 = ref;
  cur = grape[ref1];
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
void *clownix_timeout_del(long long abs_beat, int ref, 
                          const char *file, int line)
{
  int result = -1;
  void *data = NULL;
  int ref1, ref2;
  t_timeout_elem *cur;
  ref1 = (int) (abs_beat % MAX_TIMEOUT_BEATS);
  ref2 = ref;
  cur = grape[ref1];
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
         abs_beat, current_beat, ref1, ref2);
  return data;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clownix_timeout_del_all(void)
{
  int i;
  t_timeout_elem *cur;
  for (i=0; i<MAX_TIMEOUT_BEATS; i++)
    {
    cur = grape[i];
    while(cur)
      {
      cur->inhibited = 1;
      cur = cur->next;
      }
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void clownix_timer_beat(void)
{
  int idx;
  current_beat++;
  idx = (int) (current_beat % MAX_TIMEOUT_BEATS);
  process_all_current_grape_idx(&(grape[idx]));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clownix_timer_init(void)
{
  memset(grape, 0, MAX_TIMEOUT_BEATS * sizeof(t_timeout_elem *));
  current_ref = 0;
  current_beat = 0;
  plugged_elem = 0;
}
/*---------------------------------------------------------------------------*/




