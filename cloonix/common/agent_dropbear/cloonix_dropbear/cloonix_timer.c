/****************************************************************************/
/* Copyright (C) 2006-2017 Cloonix <clownix@clownix.net>  License GPL-3.0+  */
/****************************************************************************/
/*                                                                          */
/*   This program is free software: you can redistribute it and/or modify   */
/*   it under the terms of the GNU General Public License as published by   */
/*   the Free Software Foundation, either version 3 of the License, or      */
/*   (at your option) any later version.                                    */
/*                                                                          */
/*   This program is distributed in the hope that it will be useful,        */
/*   but WITHOUT ANY WARRANTY; without even the implied warranty of         */
/*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          */
/*   GNU General Public License for more details.                           */
/*                                                                          */
/*   You should have received a copy of the GNU General Public License      */
/*   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */
/*                                                                          */
/****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "io_clownix.h"
#include "cloonix_timer.h"

/*****************************************************************************/
#define MAX_TIMEOUT_BEATS 0xFFF
#define MIN_TIMEOUT_BEATS 1
/*---------------------------------------------------------------------------*/
typedef struct t_timeout_elem
{
  t_cloonix_timer_callback cb;
  void *data;
  struct t_timeout_elem *prev;
  struct t_timeout_elem *next;
} t_timeout_elem; 
/*---------------------------------------------------------------------------*/

static t_timeout_elem *grape[MAX_TIMEOUT_BEATS]; 
static int current_ref;
static long long current_beat;
static int plugged_elem;

/*****************************************************************************/
static void plug_elem(t_timeout_elem **head, t_cloonix_timer_callback cb, void *data)
{
  t_timeout_elem *cur = *head;
  t_timeout_elem *last = *head;
  current_ref++;
  if (current_ref == 0xFFFFFFFF)
    current_ref = 1;
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
  plugged_elem++;
  cur->cb = cb;
  cur->data = data;
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
  free(elem);
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
    cur->cb(cur->data);
    unplug_elem(head, cur);
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/
 
/*****************************************************************************/
void cloonix_timer_add(int beats, t_cloonix_timer_callback cb, void *data)
{
  long long abs;
  int ref;
  if ((beats < MIN_TIMEOUT_BEATS) || 
      (beats >= MAX_TIMEOUT_BEATS))
    KOUT("%d", beats);
  abs = current_beat + beats;
  ref = (int) (abs % MAX_TIMEOUT_BEATS);
  plug_elem(&(grape[ref]), cb, data);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cloonix_timer_beat(void)
{
  int idx;
  current_beat++;
  idx = (int) (current_beat % MAX_TIMEOUT_BEATS);
  process_all_current_grape_idx(&(grape[idx]));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cloonix_timer_init(void)
{
  memset(grape, 0, MAX_TIMEOUT_BEATS * sizeof(t_timeout_elem *));
  current_ref = 0;
  current_beat = 0;
  plugged_elem = 0;
}
/*---------------------------------------------------------------------------*/




