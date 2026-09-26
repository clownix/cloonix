/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "io_clownix.h"

#define MAX_TIMEOUT_BEATS 0x3FFFF
#define MIN_TIMEOUT_BEATS 1
typedef struct t_timeout_elem
{
  int inhibited;
  t_fct_timeout cb;
  void *data;
  int ref;
  struct t_timeout_elem *prev;
  struct t_timeout_elem *next;
} t_timeout_elem; 

static t_timeout_elem *grape[MAX_TIMEOUT_BEATS]; 
static int current_ref;
static long long current_beat;
static int plugged_elem;

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

/* Top callbacks by pending count; walked at most every ten minutes.       */
void clownix_timer_report_top(char *txt, int txt_len)
{
  #define NB_TALLY 12
  void *cb[NB_TALLY];
  int nb[NB_TALLY];
  int i, j, k, used = 0, len = 0, total = 0;
  t_timeout_elem *cur;

  memset(cb, 0, sizeof(cb));
  memset(nb, 0, sizeof(nb));
  if ((!txt) || (txt_len < 32))
    return;
  txt[0] = 0;
  for (i = 0; i < MAX_TIMEOUT_BEATS; i++)
    {
    for (cur = grape[i]; cur; cur = cur->next)
      {
      total += 1;
      for (j = 0; j < used; j++)
        {
        if (cb[j] == (void *) cur->cb)
          { nb[j] += 1; break; }
        }
      if (j == used)
        {
        if (used < NB_TALLY)
          { cb[used] = (void *) cur->cb; nb[used] = 1; used += 1; }
        }
      }
    }
  /* Biggest first: only the head of this list is worth reading. */
  for (i = 0; i < used; i++)
    {
    for (j = i+1; j < used; j++)
      {
      if (nb[j] > nb[i])
        {
        k = nb[i]; nb[i] = nb[j]; nb[j] = k;
        { void *t = cb[i]; cb[i] = cb[j]; cb[j] = t; }
        }
      }
    }
  len += snprintf(txt+len, txt_len-len, "%d pending timers, by callback: ",
                  total);
  for (i = 0; (i < used) && (i < 5) && (len < txt_len - 32); i++)
    len += snprintf(txt+len, txt_len-len, "%p:%d ", cb[i], nb[i]);
  if (used >= NB_TALLY)
    snprintf(txt+len, txt_len-len, "(more than %d distinct callbacks)",
             NB_TALLY);
}

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

void clownix_timer_beat(void)
{
  int idx;
  current_beat++;
  idx = (int) (current_beat % MAX_TIMEOUT_BEATS);
  process_all_current_grape_idx(&(grape[idx]));
}

void clownix_timer_init(void)
{
  memset(grape, 0, MAX_TIMEOUT_BEATS * sizeof(t_timeout_elem *));
  current_ref = 0;
  current_beat = 0;
  plugged_elem = 0;
}

