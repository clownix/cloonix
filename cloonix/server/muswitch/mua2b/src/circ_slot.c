/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <stdint.h>


#include "ioc.h"
#include <pthread.h>

#define MEMORY_BARRIER()  asm volatile ("": : :"memory")

/****************************************************************************/
#define CIRC_MASK (0xFFF)
typedef struct t_circ_ctx
{
  uint32_t volatile head;
  uint32_t volatile tail;
  uint32_t volatile lock;
  void *queue[CIRC_MASK+1];
} t_circ_ctx;
/*---------------------------------------------------------------------------*/

static t_circ_ctx g_circ_ctx0;
static t_circ_ctx g_circ_ctx1;

/*****************************************************************************/
static inline uint32_t circ_cnt(uint32_t volatile head, 
                                uint32_t volatile tail)
{
  return ((head - tail) & CIRC_MASK);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static inline uint32_t circ_space(uint32_t volatile head, 
                                  uint32_t volatile tail)
{
  return ((tail - (head+1)) & CIRC_MASK);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void circ_slot_put(int num, void *elem)
{
  uint32_t head, tail, new_head;
  t_circ_ctx *circ_ctx; 
  if (num == 0)
    circ_ctx = &g_circ_ctx0;
  else if (num == 1)
    circ_ctx = &g_circ_ctx1;
  else
    KOUT("%d", num);

  while (__sync_lock_test_and_set(&(circ_ctx->lock), 1));
  head = (uint32_t) circ_ctx->head;
  tail = (uint32_t) circ_ctx->tail;
  if ((circ_space(head, tail) == 0) || (circ_ctx->queue[head] != NULL))
    KOUT(" ");
  circ_ctx->queue[head] = elem;
  new_head = (head + 1) & CIRC_MASK;
  MEMORY_BARRIER();
  if (__sync_val_compare_and_swap(&(circ_ctx->head), head, new_head) != head)
    KOUT(" ");
  __sync_lock_release(&(circ_ctx->lock));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void *circ_slot_lookat(int num)
{
  void *elem = NULL;
  uint32_t head, tail;
  t_circ_ctx *circ_ctx;
  if (num == 0)
    circ_ctx = &g_circ_ctx0;
  else if (num == 1)
    circ_ctx = &g_circ_ctx1;
  else
    KOUT("%d", num);
  head = (uint32_t) circ_ctx->head;
  tail = (uint32_t) circ_ctx->tail;
  if (circ_cnt(head, tail) >= 1)
    elem = circ_ctx->queue[tail];
  return elem;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void *circ_slot_get(int num)
{
  void *elem = NULL;
  uint32_t head, tail, new_tail;
  t_circ_ctx *circ_ctx; 
  if (num == 0)
    circ_ctx = &g_circ_ctx0;
  else if (num == 1)
    circ_ctx = &g_circ_ctx1;
  else
    KOUT("%d", num);

  while (__sync_lock_test_and_set(&(circ_ctx->lock), 1));
  head = (uint32_t) circ_ctx->head;
  tail = (uint32_t) circ_ctx->tail;
  if (circ_cnt(head, tail) >= 1) 
    {
    elem = circ_ctx->queue[tail];
    if (!elem)
      KOUT(" ");
    circ_ctx->queue[tail] = NULL;
    new_tail = (tail + 1) & CIRC_MASK;
    MEMORY_BARRIER();
    if (__sync_val_compare_and_swap(&(circ_ctx->tail),tail,new_tail)!=tail)
      KOUT(" ");
    }
  __sync_lock_release(&(circ_ctx->lock));
  return elem;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int circ_empty_slot_nb(int num)
{
  int result;
  t_circ_ctx *circ_ctx; 
  if (num == 0)
    circ_ctx = &g_circ_ctx0;
  else if (num == 1)
    circ_ctx = &g_circ_ctx1;
  else
    KOUT("%d", num);

  while (__sync_lock_test_and_set(&(circ_ctx->lock), 1));
  result = circ_space(circ_ctx->head, circ_ctx->tail);
  __sync_lock_release(&(circ_ctx->lock));

  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int circ_used_slot_nb(int num)
{
  int result;
  t_circ_ctx *circ_ctx; 
  if (num == 0)
    circ_ctx = &g_circ_ctx0;
  else if (num == 1)
    circ_ctx = &g_circ_ctx1;
  else
    KOUT("%d", num);

  while (__sync_lock_test_and_set(&(circ_ctx->lock), 1));
  result = circ_cnt(circ_ctx->head, circ_ctx->tail);
  __sync_lock_release(&(circ_ctx->lock));

  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void circ_slot_init(int num)
{
  t_circ_ctx *circ_ctx;
  if (num == 0)
    circ_ctx = &g_circ_ctx0;
  else if (num == 1)
    circ_ctx = &g_circ_ctx1;
  else
    KOUT("%d", num);

  circ_ctx->head = 0;
  circ_ctx->tail = 0;
  circ_ctx->lock = 0;
  memset(circ_ctx->queue, 0, (CIRC_MASK+1) * sizeof(void *));
}
/*--------------------------------------------------------------------------*/
