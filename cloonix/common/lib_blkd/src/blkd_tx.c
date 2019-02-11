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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>

#include "ioc_blkd.h"
#include "blkd.h"
#include "sock_unix.h"

#define MEMORY_BARRIER()  asm volatile ("": : :"memory")

/*****************************************************************************/
static void pool_tx_init(t_blkd_fifo_tx *pool)
{
  int i;
  for(i = 0; i < MASK_TX_BLKD_POOL + 1; i++)
    {
    pool->rec[i].len_to_do = 0;
    pool->rec[i].len_done = 0;
    pool->rec[i].blkd = NULL;
    }
  pool->put = 0;
  pool->get = MASK_TX_BLKD_POOL;
  pool->qty = 0;
  pool->max_qty = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_blkd_record *pool_tx_alloc(t_blkd_fifo_tx *pool, t_blkd *blkd)
{
  uint32_t put, new_put;

  while (__sync_lock_test_and_set(&(pool->circ_lock), 1));

  if(pool->put == pool->get)
    KOUT(" ");
  if (pool->rec[pool->put].blkd)
    KOUT(" ");
  pool->rec[pool->put].blkd = blkd;
  pool->slot_bandwidth[pool->current_slot] += blkd->payload_len;
  pool->tx_queued_bytes += blkd->header_blkd_len + blkd->payload_len;
  pool->rec[pool->put].len_to_do = 0;
  pool->rec[pool->put].len_done = 0;

  put = pool->put;
  new_put = (pool->put + 1) & MASK_TX_BLKD_POOL;
  MEMORY_BARRIER();
  if (__sync_val_compare_and_swap(&(pool->put), put, new_put) != put)
    KOUT(" ");

  __sync_fetch_and_add(&(pool->qty), 1);
  if (pool->qty > pool->slot_qty[pool->current_slot])
    pool->slot_qty[pool->current_slot] = pool->qty;
  if (pool->tx_queued_bytes > pool->slot_queue[pool->current_slot])
    pool->slot_queue[pool->current_slot] = pool->tx_queued_bytes;
  if (pool->qty > pool->max_qty)
    pool->max_qty = pool->qty;

  __sync_lock_release(&(pool->circ_lock));

  return (&(pool->rec[put]));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_blkd_record *pool_tx_get(t_blkd_fifo_tx *pool)
{
  t_blkd_record *rec = NULL;
  int get;

  while (__sync_lock_test_and_set(&(pool->circ_lock), 1));

  if (pool->qty > 0)
    {
    get = (pool->get + 1) & MASK_TX_BLKD_POOL;
    if (!pool->rec[get].blkd)
      KOUT(" ");
    rec = &(pool->rec[get]);
    }

  __sync_lock_release(&(pool->circ_lock));

  return rec;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_blkd *pool_tx_free(t_blkd_fifo_tx *pool)
{
  t_blkd *blkd = NULL;
  uint32_t get, new_get;

  while (__sync_lock_test_and_set(&(pool->circ_lock), 1));

  if (pool->qty > 0)
    {

    get = pool->get; 
    new_get = (pool->get + 1) & MASK_TX_BLKD_POOL;
    MEMORY_BARRIER();
    if (__sync_val_compare_and_swap(&(pool->get), get, new_get) != get)
      KOUT(" ");

    blkd = pool->rec[pool->get].blkd;
    if (!blkd)
      KOUT(" ");
    pool->tx_queued_bytes -= blkd->header_blkd_len + blkd->payload_len;
    pool->rec[pool->get].blkd = NULL;
    pool->rec[pool->get].len_to_do = 0;
    pool->rec[pool->get].len_done = 0;
    __sync_fetch_and_sub(&(pool->qty), 1);
    __sync_fetch_and_sub(&(blkd->count_reference), 1);
    }

  __sync_lock_release(&(pool->circ_lock));

  return blkd;
}
/*--------------------------------------------------------------------------*/



/*****************************************************************************/
static int try_sending(int fd, t_blkd_record *rec, int *get_out)
{
  int len, len_2_tx, result = -1;
  char *ptr_2_tx;
  int qemu_group_rank = rec->blkd->qemu_group_rank;
  if (qemu_group_rank)
    {
    if (rec->len_done < rec->blkd->header_blkd_len)
      {
      len_2_tx = rec->blkd->header_blkd_len - rec->len_done;
      ptr_2_tx = rec->blkd->header_blkd + rec->len_done;
      len = sock_unix_write(ptr_2_tx, len_2_tx, fd, get_out);
      }
    else
      {
      len_2_tx = rec->len_to_do - rec->len_done;
      ptr_2_tx=rec->blkd->payload_blkd+rec->len_done-rec->blkd->header_blkd_len;
      len = sock_unix_write(ptr_2_tx, len_2_tx, fd, get_out);
      }
    }
  else
    {
    len_2_tx = rec->len_to_do - rec->len_done;
    ptr_2_tx = rec->blkd->header_blkd + rec->len_done;
    len = sock_unix_write(ptr_2_tx, len_2_tx, fd, get_out);
    }
  if (len >= 0)
    {
    rec->len_done += len;
    if (len == len_2_tx)
      result = 0;
    else
      result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_fd_event_tx(void *ptr, int fd, t_blkd_fifo_tx *pool)
{
  int result = 0, get_out = 0;
  t_blkd_record *rec = pool_tx_get(pool);
  t_blkd *blkd;
  if (!rec)
    {
    KERR(" ");
    if ((pool->qty) || (pool->tx_queued_bytes))
      KOUT("%d %d", (int) pool->qty, (int) pool->tx_queued_bytes);
    }
  while(rec)
    {
    while(rec->blkd != NULL)
      {
      result = try_sending(fd, rec, &get_out);
      if ((get_out) || (result < 0))
        break;
      if (rec->len_done == rec->len_to_do)
        {
        blkd = pool_tx_free(pool);
        if (blkd->count_reference == 0)
          {
          blkd_free(ptr, blkd);
          }
        }
      } 
    rec = NULL;
    if ((get_out == 0) && (result == 0))
      rec = pool_tx_get(pool);      
    } 
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_fd_event_purge_tx(void *ptr, t_blkd_fifo_tx *pool)
{
  t_blkd_record *rec = pool_tx_get(pool);
  t_blkd *blkd;
  if (!rec)
    {
    if ((pool->qty) || (pool->tx_queued_bytes))
      KOUT("%d %d", (int) pool->qty, (int) pool->tx_queued_bytes);
    }
  while(rec)
    {
    while(rec->blkd != NULL)
      {
      blkd = pool_tx_free(pool);
      if (blkd->count_reference == 0)
        blkd_free(ptr, blkd);
      } 
    rec = pool_tx_get(pool);
    } 
  if ((pool->qty) || (pool->tx_queued_bytes))
    KOUT("%d %d", (int) pool->qty, (int) pool->tx_queued_bytes);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void trig_dist_flow_control(void *ptr)
{
  int num, tidx, llid, rank, our_mutype = blkd_get_our_mutype(ptr);
  char name[MAX_NAME_LEN];
  t_blkd_fifo_rx *pool;
  if (our_mutype == mulan_type)
    {
    llid = blkd_get_max_rx_flow_llid(ptr, &pool);
    if (llid && pool && (pool->dist_flow_control_on == 0))
      { 
      rank = blkd_get_rank(ptr, llid, name, &num, &tidx);
      if ((rank == 0) || (num < 0))
        KERR("%d %d", rank, num);
      else
        {
        pool->dist_flow_control_on = 1;
        pool->dist_flow_control_count = 5;
        pool->slot_dist_flow_ctrl[pool->current_slot] += 1;
        blkd_rx_dist_flow_control(ptr, name, num, tidx, rank, 1);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_tx_fifo_alloc(void *ptr, t_blkd_fifo_tx *pool, t_blkd *blkd)
{
  int result = -1;
  t_blkd_record *rec;
  if (pool->tx_queued_bytes > MAX_GLOB_BLKD_QUEUED_BYTES/10)
    { 
    trig_dist_flow_control(ptr);
    }
  if ((pool->tx_queued_bytes > (3*MAX_GLOB_BLKD_QUEUED_BYTES)/4) ||
      (pool->put == pool->get))
    {
    KERR("%lu %d %d %d", pool->tx_queued_bytes, pool->put, pool->get,
                          (int) (3*MAX_GLOB_BLKD_QUEUED_BYTES)/4);
    }
  else
    {
    __sync_fetch_and_add(&(blkd->count_reference), 1);
    rec = pool_tx_alloc(pool, blkd);
    blkd_header_tx_setup(rec);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_tx_init(t_blkd_fifo_tx *pool)
{
  pool_tx_init(pool);
}
/*--------------------------------------------------------------------------*/


