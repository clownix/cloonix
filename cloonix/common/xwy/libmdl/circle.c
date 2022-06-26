/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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

/****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdint.h>

#include "mdl.h"
#include "circle.h"

#define MEMORY_BARRIER()  asm volatile ("": : :"memory")

/*****************************************************************************/
static inline uint32_t circ_free_cnt(uint32_t volatile head,
                                     uint32_t volatile tail)
{
  return ((head - tail) & CIRC_FREE_MASK);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static inline uint32_t circ_free_space(uint32_t volatile head,
                                       uint32_t volatile tail)
{
  return ((tail - (head+1)) & CIRC_FREE_MASK);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void circ_free_slot_put(t_circ_ctx *circ_ctx, void *elem)
{
  uint32_t head, tail, new_head;
  while (__sync_lock_test_and_set(&(circ_ctx->free_lock), 1));
  head = (uint32_t) circ_ctx->free_head;
  tail = (uint32_t) circ_ctx->free_tail;
  if ((circ_free_space(head, tail) == 0) ||
      (circ_ctx->free_queue[head].elem != NULL))
    XOUT(" ");
  circ_ctx->free_queue[head].elem = elem;
  new_head = (head + 1) & CIRC_FREE_MASK;
  MEMORY_BARRIER();
  if (__sync_val_compare_and_swap(&(circ_ctx->free_head),head,new_head)!=head)
    XOUT(" ");
  __sync_lock_release(&(circ_ctx->free_lock));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void *circ_free_slot_get(t_circ_ctx *circ_ctx)
{
  void *elem = NULL;
  uint32_t head, tail, new_tail;
  while (__sync_lock_test_and_set(&(circ_ctx->free_lock), 1));
  head = (uint32_t) circ_ctx->free_head;
  tail = (uint32_t) circ_ctx->free_tail;
  if (circ_free_cnt(head, tail) >= 1)
    {
    elem = circ_ctx->free_queue[tail].elem;
    if (!elem)
      XOUT(" ");
    circ_ctx->free_queue[tail].elem = NULL;
    new_tail = (tail + 1) & CIRC_FREE_MASK;
    MEMORY_BARRIER();
    if (__sync_val_compare_and_swap(&(circ_ctx->free_tail),tail,new_tail)!=tail)
      XOUT(" ");
    }
  __sync_lock_release(&(circ_ctx->free_lock));
  return elem;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static inline uint32_t circ_tx_cnt(uint32_t volatile head, 
                                   uint32_t volatile tail)
{
  return ((head - tail) & CIRC_TX_MASK);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static inline uint32_t circ_tx_space(uint32_t volatile head, 
                                     uint32_t volatile tail)
{
  return ((tail - (head+1)) & CIRC_TX_MASK);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void circ_tx_slot_put(t_circ_ctx *circ_ctx, void *elem, int bytes)
{
  uint32_t head, tail, new_head;
  while (__sync_lock_test_and_set(&(circ_ctx->tx_lock), 1));
  head = (uint32_t) circ_ctx->tx_head;
  tail = (uint32_t) circ_ctx->tx_tail;
  if ((circ_tx_space(head, tail) == 0) ||
      (circ_ctx->tx_queue[head].elem != NULL))
    XOUT(" ");
  circ_ctx->tx_queue[head].elem = elem;
  circ_ctx->tx_queue[head].bytes = bytes;
  new_head = (head + 1) & CIRC_TX_MASK;
  MEMORY_BARRIER();
  __sync_fetch_and_add (&(circ_ctx->tx_stored_bytes), bytes); 
  if (__sync_val_compare_and_swap(&(circ_ctx->tx_head),
                                  head, new_head) != head)
    XOUT(" ");
  __sync_lock_release(&(circ_ctx->tx_lock));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void *circ_tx_slot_get(t_circ_ctx *circ_ctx)
{
  void *elem = NULL;
  uint32_t head, tail, new_tail, bytes;
  while (__sync_lock_test_and_set(&(circ_ctx->tx_lock), 1));
  head = (uint32_t) circ_ctx->tx_head;
  tail = (uint32_t) circ_ctx->tx_tail;
  if (circ_tx_cnt(head, tail) >= 1) 
    {
    elem = circ_ctx->tx_queue[tail].elem;
    bytes = circ_ctx->tx_queue[tail].bytes;
    if (!elem)
      XOUT(" ");
    circ_ctx->tx_queue[tail].elem = NULL;
    new_tail = (tail + 1) & CIRC_TX_MASK;
    MEMORY_BARRIER();
    __sync_fetch_and_sub (&(circ_ctx->tx_stored_bytes), bytes); 
    if (__sync_val_compare_and_swap(&(circ_ctx->tx_tail),tail,new_tail)!=tail)
      XOUT(" ");
    }
  __sync_lock_release(&(circ_ctx->tx_lock));
  return elem;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int circ_tx_empty_slot_nb(t_circ_ctx *circ_ctx)
{
  return(circ_tx_space(circ_ctx->tx_head, circ_ctx->tx_tail));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int circ_tx_used_slot_nb(t_circ_ctx *circ_ctx)
{
  return(circ_tx_cnt(circ_ctx->tx_head, circ_ctx->tx_tail));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int circ_tx_stored_bytes_nb(t_circ_ctx *circ_ctx)
{
  return(circ_ctx->tx_stored_bytes);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void circ_slot_init(t_circ_ctx *circ_ctx)
{
  circ_ctx->free_head = 0;
  circ_ctx->free_tail = 0;
  circ_ctx->free_lock = 0;
  memset(circ_ctx->free_queue, 0, (CIRC_FREE_MASK+1) * sizeof(void *));

  circ_ctx->tx_head = 0;
  circ_ctx->tx_tail = 0;
  circ_ctx->tx_stored_bytes = 0;
  circ_ctx->tx_lock = 0;
  memset(circ_ctx->tx_queue, 0, (CIRC_TX_MASK+1) * sizeof(void *));
}
/*--------------------------------------------------------------------------*/
