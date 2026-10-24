/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <errno.h>
#include "io_clownix.h"
#include "channel.h"
#include "msg_layer.h"
#include "chunk.h"

t_data_chunk *chain_get_last_chunk(t_data_chunk *start)
{
  t_data_chunk *cur = start;
  if (cur)
    while (cur->next)
      cur = cur->next;
  return cur;
}

void chain_append(t_data_chunk **start, int len, char *nrx)
{
  t_data_chunk *cur;
  t_data_chunk *last;
  cur = (t_data_chunk *)clownix_malloc(sizeof(t_data_chunk), IOCMLC);
  memset(cur, 0, sizeof(t_data_chunk));
  last = chain_get_last_chunk(*start);
  cur->chunk = nrx;
  cur->len   = len;
  if (last)
    last->next = cur;
  else
    *start = cur;
}

void chain_pop(t_data_chunk **start)
{
  t_data_chunk *cur = *start;
  *start = cur->next;
  clownix_free(cur->chunk, __FUNCTION__);
  clownix_free(cur, __FUNCTION__);
}

void chain_del(t_data_chunk **start, t_data_chunk *last)
{
  while ((*start) && ((*start) != last))
    chain_pop(start);
}

int chain_get_prev_len(t_data_chunk *start, int bound_start_offset)
{
  int len = 0;
  t_data_chunk *cur = start;
  if (cur->len < bound_start_offset)
    KOUT("ERROR %d %d", cur->len, bound_start_offset);
  while (cur)
    {
    len += cur->len;
    cur = cur->next;
    }
  return (len - bound_start_offset);
}

static void chain_copy(t_data_chunk *first, int offset, char *tot_rx, int max)
{
  int len = 0;
  t_data_chunk *cur = first;
  while (cur)
    {
    if (cur == first)
      {
      if (len)
        KOUT(" %d ", len);
      if ((cur->len - offset) > max)
        KOUT("%d %d %d", len, cur->len, max);
      memcpy(tot_rx + len, cur->chunk+offset, cur->len-offset);
      len += cur->len-offset;
      }
    else
      {
      if ((len + cur->len) > max)
        KOUT("%d %d %d", len, cur->len, max);
      memcpy(tot_rx + len, cur->chunk, cur->len);
      len += cur->len;
      }
    cur = cur->next;
    }
}

int make_a_buf_copy(t_data_chunk *first, int bound_start_offset, 
                           char **buf_copy)
{
  char *tot_rx = NULL;
  int tot_len = 0;
  if (first)
    {
    tot_len = chain_get_prev_len(first, bound_start_offset);
    if (tot_len)
      {
      tot_rx = (char *)clownix_malloc(tot_len+1, IOCMLC);
      tot_rx[tot_len] = 0;
      chain_copy(first, bound_start_offset, tot_rx, tot_len);
      }
    *buf_copy = tot_rx;
    }
  else
    *buf_copy = NULL;
  return (tot_len);
}

void first_elem_delete(t_data_chunk **start, t_data_chunk **last)
{
  t_data_chunk *cur = *start;
  if (last && (*start == *last))
    *last = NULL;
  *start = cur->next;
  clownix_free(cur->chunk, __FUNCTION__);
  clownix_free(cur, __FUNCTION__);
}

void chain_delete(t_data_chunk **start, t_data_chunk **last)
{
  while (*start)
    first_elem_delete(start, last);
}

void chain_append_tx(t_data_chunk **start, t_data_chunk **last,
                     int len, char *nrx)
{
  t_data_chunk *cur;
  cur = (t_data_chunk *)clownix_malloc(sizeof(t_data_chunk), IOCMLC);
  memset(cur, 0, sizeof(t_data_chunk));
  cur->chunk = nrx;
  cur->len   = len;
  if (*last)
    (*last)->next = cur;
  else
    *start = cur;
  *last = cur;
}

