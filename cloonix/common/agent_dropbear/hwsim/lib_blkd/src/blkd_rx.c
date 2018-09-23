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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>

#include "ioc_blkd.h"
#include "blkd.h"
#include "sock_unix.h"

#define MEMORY_BARRIER()  asm volatile ("": : :"memory")

enum {
  blkd_no_state = 0,
  blkd_read_head,
  blkd_read_body,
  blkd_all_done,
};
enum {
  read_state_error = 1,
  read_state_drop,
  read_state_end,
  read_state_ok,
};



/****************************************************************************/
t_blkd_group *malloc_create_new_group(int len_left, char *data_left)
{
  t_blkd_group *blkd_group = malloc(sizeof(t_blkd_group));
  memset(blkd_group, 0, sizeof(t_blkd_group));
  blkd_group->head_data = (char *) malloc(GROUP_BLKD_MAX_SIZE);
  if ((len_left < 0) || (len_left >= 2*MAX_TOTAL_BLKD_SIZE))
    KOUT("%d", len_left);
  if (len_left)
    memcpy(blkd_group->head_data, data_left, len_left);
  blkd_group->len_data_read = len_left;
  blkd_group->len_data_done = 0;
  blkd_group->len_data_max = GROUP_BLKD_MAX_SIZE;
  blkd_group->count_blkd_tied = 0;
  return blkd_group;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_blkd *malloc_create_new_blkd(t_blkd_group *group) 
{
  t_blkd *cur;
  if (!group)
    KOUT(" ");
  if (group->len_data_max < group->len_data_done + MAX_TOTAL_BLKD_SIZE)
    KOUT("%d %d", group->len_data_read, group->len_data_done);
  cur = malloc(sizeof(t_blkd));
  memset(cur, 0, sizeof(t_blkd));
  cur->header_blkd_len = HEADER_BLKD_SIZE;
  cur->header_blkd = group->head_data + group->len_data_done;
  cur->payload_blkd = cur->header_blkd + cur->header_blkd_len;
  cur->group = group;
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void pool_rx_init(t_blkd_fifo_rx *pool)
{
  int i;
  for(i = 0; i < MASK_RX_BLKD_POOL + 1; i++)
    {
    pool->rec[i].len_to_do = 0;
    pool->rec[i].len_done = 0;
    pool->rec[i].blkd = NULL;
    }
  pool->put = 0;
  pool->get = MASK_RX_BLKD_POOL;
  pool->qty = 0;
  pool->max_qty = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void pool_rx_alloc(t_blkd_fifo_rx *pool, t_blkd *blkd)
{
  uint32_t put, new_put;

  while (__sync_lock_test_and_set(&(pool->circ_lock), 1));

  if(pool->put == pool->get)
    KOUT(" ");
  if (pool->rec[pool->put].blkd)
    KOUT(" ");
  pool->rec[pool->put].len_to_do = 0;
  pool->rec[pool->put].len_done = 0;
  pool->rec[pool->put].blkd = blkd;

  put = pool->put;
  new_put = (pool->put + 1) & MASK_RX_BLKD_POOL;
  MEMORY_BARRIER();
  if (__sync_val_compare_and_swap(&(pool->put), put, new_put) != put)
    KOUT(" ");

  __sync_fetch_and_add(&(pool->qty), 1);
  if (pool->qty > pool->max_qty)
    pool->max_qty = pool->qty;

  __sync_lock_release(&(pool->circ_lock));

}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_blkd_record *pool_rx_get_newest_rec(t_blkd_fifo_rx *pool)
{
  t_blkd_record *rec = NULL;
  int idx;

  while (__sync_lock_test_and_set(&(pool->circ_lock), 1));

  if (pool->qty > 0)
    {
    idx = (pool->put - 1) & MASK_RX_BLKD_POOL;
    if (!pool->rec[idx].blkd)
      KOUT(" ");
    rec = &(pool->rec[idx]);
    }

  __sync_lock_release(&(pool->circ_lock));

  return rec;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_blkd *pool_rx_get(t_blkd_fifo_rx *pool)
{
  t_blkd *blkd = NULL;
  uint32_t get, new_get;

  while (__sync_lock_test_and_set(&(pool->circ_lock), 1));

  if (pool->qty > 0)
    {

    get = pool->get;
    new_get = (pool->get + 1) & MASK_RX_BLKD_POOL;
    MEMORY_BARRIER();
    if (__sync_val_compare_and_swap(&(pool->get), get, new_get) != get)
      KOUT(" ");
      
    blkd = pool->rec[pool->get].blkd;
    if (!blkd)
      KOUT(" ");
    pool->rx_queued_bytes -= pool->rec[pool->get].len_to_do;
    pool->rec[pool->get].len_to_do = 0;
    pool->rec[pool->get].len_done = 0;
    pool->rec[pool->get].blkd = NULL;
    __sync_fetch_and_sub(&(pool->qty), 1);
    }

  __sync_lock_release(&(pool->circ_lock));

  return blkd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_rec_state(t_blkd_record *rec)
{
  int result;
  if (((rec->len_done) <  rec->blkd->header_blkd_len) ||
      (((rec->len_done) == rec->blkd->header_blkd_len) &&
       (!rec->len_to_do)))
    result = blkd_read_head;
  else if ((rec->len_to_do) &&
           ((rec->len_to_do) ==
            (rec->len_done)))
    result = blkd_all_done;
  else if ((rec->len_done >= rec->blkd->header_blkd_len) &&
           (rec->len_to_do))
    result = blkd_read_body;
  else
    KOUT("%d %d", rec->len_done, rec->len_to_do);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int pool_rx_oldest_record_state(t_blkd_fifo_rx *pool)
{
  int idx, result = blkd_no_state;
  if (pool->qty > 0)
    {
    idx = (pool->get + 1) & MASK_RX_BLKD_POOL;
    result = get_rec_state(&(pool->rec[idx]));
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int group_read_up(t_blkd_group *group, int max)
{
  int left, result = -1;
  if (group->len_data_read > group->len_data_done)
    {
    left = group->len_data_read - group->len_data_done;
    if (max > left)
      {
      result = left;
      group->len_data_done = group->len_data_read;
      }
    else
      {
      result = max;
      group->len_data_done += max;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int auto_extract_header(t_blkd_fifo_rx *pool, 
                               t_blkd_group *group, 
                               t_blkd_record *rec) 
{
  int len, result = -1;
  if (rec->len_done == rec->blkd->header_blkd_len)
    {
    blkd_header_rx_extract(rec);
    result = 0;
    }
  else
    {
    len = group_read_up(group, rec->blkd->header_blkd_len - rec->len_done);
    if (len >= 0)
      {
      rec->len_done += len;
      if (rec->len_done == rec->blkd->header_blkd_len)
        {
        blkd_header_rx_extract(rec);
        result = 0;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void prepare_new_group_and_blkd(t_blkd_fifo_rx *pool,
                                       t_blkd_group *last_group)
{
  t_blkd_group *group;
  t_blkd *blkd;
  int left_len;
  char *left_data;
  left_len = last_group->len_data_read - last_group->len_data_done;
  left_data = last_group->head_data + last_group->len_data_done;
  group = malloc_create_new_group(left_len, left_data);
  __sync_fetch_and_add(&(group->count_blkd_tied), 1);
  blkd = malloc_create_new_blkd(group);
  pool_rx_alloc(pool, blkd);
  last_group->len_data_read = last_group->len_data_done;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void prepare_new_blkd(t_blkd_fifo_rx *pool, t_blkd_group *group)
{
  t_blkd *blkd;
  __sync_fetch_and_add(&(group->count_blkd_tied), 1);
  blkd = malloc_create_new_blkd(group);
  pool_rx_alloc(pool, blkd);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void maybe_new_group(t_blkd_fifo_rx *pool, t_blkd_group *group)
{
  int todo_space_left, virgin_space_left;
  todo_space_left = group->len_data_read - group->len_data_done;
  virgin_space_left = group->len_data_max - group->len_data_read;
  if ((todo_space_left < 0)||(todo_space_left > GROUP_BLKD_MAX_SIZE))
    KOUT("%d %d", group->len_data_read, group->len_data_done);
  if ((virgin_space_left < 0)||(virgin_space_left > GROUP_BLKD_MAX_SIZE))
    KOUT("%d %d", group->len_data_read, group->len_data_max);
  if (todo_space_left + virgin_space_left <= MAX_TOTAL_BLKD_SIZE)
    prepare_new_group_and_blkd(pool, group);
  else
    prepare_new_blkd(pool, group);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int auto_fill_body(t_blkd_fifo_rx *pool, 
                          t_blkd_group *group,
                          t_blkd_record *rec, 
                          int llid, void *ptr)
{
  int data_len, len, result = -1;
  len = group_read_up(group, rec->len_to_do - rec->len_done);
  if (len >= 0)
    {
    rec->len_done += len;
    if (rec->len_to_do == rec->len_done)
      {
      maybe_new_group(pool, group);
      pool->rx_queued_bytes += rec->len_to_do;
      data_len = rec->len_to_do - rec->blkd->header_blkd_len;
      pool->slot_bandwidth[pool->current_slot] += data_len;
      if (pool->qty > pool->slot_qty[pool->current_slot])
        pool->slot_qty[pool->current_slot] = pool->qty;
      if (pool->rx_queued_bytes > pool->slot_queue[pool->current_slot])
        pool->slot_queue[pool->current_slot] = pool->rx_queued_bytes;
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int pool_rx_newest_record_state(t_blkd_fifo_rx *pool,
                                       t_blkd_group   **group,
                                       t_blkd_record  **rec)
{
  int result;
  *rec = pool_rx_get_newest_rec(pool);
  if (*rec)
    {
    result = get_rec_state(*rec);
    *group = (*rec)->blkd->group;
    }
  else
    KOUT(" ");
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void trig_dist_flow_control(void *ptr, t_blkd_fifo_rx *pool, int llid)
{
  int num, tidx, rank, our_mutype = blkd_get_our_mutype(ptr);
  char name[MAX_NAME_LEN];
  if ((our_mutype == mulan_type)     ||
      (our_mutype == endp_type_kvm_eth)  ||
      (our_mutype == endp_type_kvm_wlan) ||
      (our_mutype == endp_type_tap)  ||
      (our_mutype == endp_type_snf)  ||
      (our_mutype == endp_type_c2c)  ||
      (our_mutype == endp_type_nat)  ||
      (our_mutype == endp_type_a2b))
    {
    rank = blkd_get_rank(ptr, llid, name, &num, &tidx);
    if ((rank == 0) || (num == -1))
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
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int group_read_sock_unix(void *ptr, t_blkd_fifo_rx *pool, 
                                t_blkd_group *group, int llid, int fd)
{
  int read_state, len, len_max;
  char *buf_rx;
  read_state = read_state_error;
  if (pool->rx_queued_bytes > MAX_GLOB_BLKD_QUEUED_BYTES/10)
    {
    if (!pool->dist_flow_control_on)
      trig_dist_flow_control(ptr, pool, llid);
    }
  if (pool->rx_queued_bytes > MAX_GLOB_BLKD_QUEUED_BYTES/2)
    {
    KERR(" ");
    read_state = read_state_drop;
    }
  len_max = group->len_data_max - group->len_data_read;
  buf_rx = group->head_data + group->len_data_read;
  if (len_max <= 0)
    KOUT("%d", len_max);
  len = sock_unix_read(buf_rx, len_max, fd);
  if (len == 0)
    {
    read_state = read_state_end;
    }
  else if (len > 0)
    {
    group->len_data_read += len;
    if (read_state != read_state_drop)
      read_state = read_state_ok;
    }
  else
    {
    read_state = read_state_error;
    }
  return read_state;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_purge_all(void *ptr, int llid)
{
  t_blkd *blkd = blkd_get_rx(ptr, llid);
  while(blkd)
    {
    blkd_drop_rx_counter_increment(ptr, llid, blkd->payload_len);
    blkd_free(ptr, blkd);
    blkd = blkd_get_rx(ptr, llid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_fd_event_rx(void *ptr, 
                     int llid, int fd,
                     t_blkd_fifo_rx *pool,
                     t_blkd_rx_cb rx_ready_cb)
{
  int read_state, state, result = 0;
  t_blkd_group *group;
  t_blkd_record *rec;
  state = pool_rx_newest_record_state(pool, &group, &rec);
  read_state = group_read_sock_unix(ptr, pool, group, llid, fd);
  if ((read_state == read_state_ok) ||
      (read_state == read_state_drop)) 
    {
    while(!result)
      {
      switch (state)
        {
        case blkd_read_head:
          result = auto_extract_header(pool, group, rec);
          break;
  
        case blkd_read_body:
          result = auto_fill_body(pool, group, rec, llid, ptr);
          break;
  
        default:
          KOUT("%d", state);
        }
      state = pool_rx_newest_record_state(pool, &group, &rec);
      }
    result = 0;
    if (pool->rx_queued_bytes)
      {
      if (read_state == read_state_ok)
        rx_ready_cb(ptr, llid);
      else
        {
        rx_purge_all(ptr, llid);
        KERR(" ");
        }
      }
    }
  if (read_state == read_state_error)
    result = -1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_blkd *blkd_rx_fifo_get(t_blkd_fifo_rx *pool)
{
  t_blkd *blkd = NULL;
  int result = pool_rx_oldest_record_state(pool);
  if (result == blkd_all_done)
    blkd = pool_rx_get(pool);
  return blkd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_rx_init(t_blkd_fifo_rx *pool)
{
 t_blkd_group *dummy = malloc_create_new_group(0, NULL);
  pool_rx_init(pool);
  prepare_new_group_and_blkd(pool, dummy);
  free(dummy->head_data);
  free(dummy);
}
/*--------------------------------------------------------------------------*/


