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

/****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <pthread.h>
#include <stdint.h>

#include "ioc.h"
#include "pcap_fifo.h"
#include "cirspy.h"


#define MEMORY_BARRIER()  asm volatile ("": : :"memory")



#define CIRC_SPY_MASK (0x1FF)

/*****************************************************************************/
typedef struct t_cspy_elem
{
  int idx;
  long long usec;
  int len;
  char buf[CIRC_MAX_LEN];
} t_cspy_elem;
/*---------------------------------------------------------------------------*/
typedef struct t_cspy_set
{
  uint32_t volatile head;
  uint32_t volatile tail;
  uint32_t volatile lock;
  t_cspy_elem *queue[CIRC_SPY_MASK+1];
} t_cspy_set;
/*---------------------------------------------------------------------------*/
typedef struct t_cspy
{
  t_cspy_set full_set;
  t_cspy_set empty_set;
  struct t_cspy *prev;
  struct t_cspy *next;
} t_cspy;
/*---------------------------------------------------------------------------*/
static t_cspy *g_cspy[CIRC_MAX_TAB + 1];
static t_cspy *g_head_cspy;
static pthread_t g_thread;
static uint32_t volatile g_sync_lock;


/****************************************************************************/
static void mutex_lock(void)
{
   while (__sync_lock_test_and_set(&g_sync_lock, 1));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void mutex_unlock(void)
{
  __sync_lock_release(&g_sync_lock);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void mutex_init(void)
{
  g_sync_lock = 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static inline uint32_t ccnt(uint32_t volatile head, uint32_t volatile tail)
{
  return ((head - tail) & CIRC_SPY_MASK);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static inline uint32_t cspace(uint32_t volatile head, uint32_t volatile tail)
{
  return ((tail - (head+1)) & CIRC_SPY_MASK);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void queue_put(t_cspy_set *csy, t_cspy_elem *elem)
{
  uint32_t head, tail, new_head;
  while (__sync_lock_test_and_set(&(csy->lock), 1));
  head = (uint32_t) csy->head;
  tail = (uint32_t) csy->tail;
  if (cspace(head, tail) == 0)
    KOUT(" ");
  if (csy->queue[head] != NULL)
    KOUT(" ");
  csy->queue[head] = elem;
  new_head = (head + 1) & CIRC_SPY_MASK;
  MEMORY_BARRIER();
  if (__sync_val_compare_and_swap(&(csy->head), head, new_head) != head)
    KOUT(" ");
  __sync_lock_release(&(csy->lock));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_cspy_elem *queue_get(t_cspy_set *csy)
{
  void *elem = NULL;
  uint32_t head, tail, new_tail;
  while (__sync_lock_test_and_set(&(csy->lock), 1));
  head = (uint32_t) csy->head;
  tail = (uint32_t) csy->tail;
  if (ccnt(head, tail) >= 1) 
    {
    elem = csy->queue[tail];
    if (!elem)
      KOUT(" ");
    csy->queue[tail] = NULL;
    new_tail = (tail + 1) & CIRC_SPY_MASK;
    MEMORY_BARRIER();
    if (__sync_val_compare_and_swap(&(csy->tail), tail, new_tail) != tail)
      KOUT(" ");
    }
  __sync_lock_release(&(csy->lock));
  return elem;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_elem_set(t_cspy_set *cspy)
{
  int i;
  t_cspy_elem *cur;
  for (i=0; i<CIRC_SPY_MASK; i++)
    {
    cur = (t_cspy_elem *) malloc(sizeof(t_cspy_elem));
    queue_put(cspy, cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_elem_set(t_cspy_set *cspy)
{
  t_cspy_elem *cur = queue_get(cspy);
  while(cur)
    {
    free(cur);
    cur = queue_get(cspy);
    } 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cspy_thread_process(t_cspy *cspy)
{
  int count = 0;
  t_cspy_elem *elem = queue_get(&(cspy->full_set));
  while(elem)
    {
    pcap_fifo_rx_packet(elem->idx, elem->usec, elem->len, elem->buf);
    queue_put(&(cspy->empty_set), elem);
    count += 1;
    if (count == 100)
      break;
    elem = queue_get(&(cspy->full_set));
    } 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void *cspy_thread(void *arg)
{
  t_cspy *cur;
  for (;;)
    {
    mutex_lock();
    cur = g_head_cspy;
    while(cur)
      {
      cspy_thread_process(cur);
      cur = cur->next;
      }
    mutex_unlock();
    usleep(1000);
    }
  KERR("THREAD EXIT");
  pthread_exit(NULL);
  return NULL;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void  cirspy_put(int idx, long long usec, int len, char *buf)
{
  t_cspy *cur;
  t_cspy_elem *elem;
  if ((idx <= 0) || (idx > CIRC_MAX_TAB))
    KOUT("%d", idx);
  if ((len < 0) || (len >= CIRC_MAX_LEN))
    KOUT("%d", len);
  cur = g_cspy[idx];
  if (!cur)
    KERR("%d", idx);
  else
    {
    elem = queue_get(&(cur->empty_set));
    if (elem)
      {
      elem->idx = idx;
      elem->usec = usec;
      elem->len = len;
      memcpy(elem->buf, buf, len);
      queue_put(&(cur->full_set), elem);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void  cirspy_open(int idx)
{
  t_cspy *cur;
  if ((idx <= 0) || (idx > CIRC_MAX_TAB))
    KOUT("%d", idx);
  cur = g_cspy[idx];
  if (cur)
    KERR(" ");
  else
    {
    cur = (t_cspy *) malloc(sizeof(t_cspy));
    memset(cur, 0, sizeof(t_cspy));
    alloc_elem_set(&(cur->empty_set));
    mutex_lock();
    if (g_head_cspy)
      g_head_cspy->prev = cur;
    cur->next = g_head_cspy;
    g_head_cspy = cur;
    g_cspy[idx] = cur;
    mutex_unlock();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void  cirspy_close(int idx)
{
  t_cspy *cur;
  if ((idx <= 0) || (idx > CIRC_MAX_TAB))
    KOUT("%d", idx);
  cur = g_cspy[idx];
  if (!cur)
    KERR(" ");
  else
    {
    mutex_lock();
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur==g_head_cspy)
      g_head_cspy = cur->next;
    g_cspy[idx] = NULL;
    mutex_unlock();
    free_elem_set(&(cur->empty_set));
    free_elem_set(&(cur->full_set));
    free(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void  cirspy_init(void)
{
  mutex_init();
  memset(g_cspy, 0, (CIRC_MAX_TAB + 1) * sizeof(t_cspy *));
  if (pthread_create(&g_thread, NULL, cspy_thread, NULL) != 0)
    KOUT("THREAD CIRSPY BAD START");
}
/*--------------------------------------------------------------------------*/
