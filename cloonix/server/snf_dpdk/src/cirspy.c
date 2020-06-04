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

/****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cirspy.h"
#include "pcap_record.h"
#include "eventfull.h"


#define MEMORY_BARRIER()  asm volatile ("": : :"memory")



#define CIRC_SPY_MASK (0x1FF)

/*****************************************************************************/
typedef struct t_cspy_elem
{
  long long usec;
  int len;
  uint8_t buf[CIRC_MAX_LEN];
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
} t_cspy;
/*---------------------------------------------------------------------------*/

static t_cspy_elem g_queue[CIRC_SPY_MASK+1];
static t_cspy g_cspy;


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
    KOUT("%d %d %d", new_head, head, csy->head);
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
      KOUT("%d %d %d", new_tail, tail, csy->tail);
    }
  __sync_lock_release(&(csy->lock));
  return elem;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cirspy_run(void)
{
  t_cspy_elem *elem;
  elem = queue_get(&(g_cspy.full_set));
  while(elem)
    {
    pcap_record_rx_packet(elem->usec, elem->len, elem->buf);
    eventfull_hook_spy(elem->len, elem->buf);
    queue_put(&(g_cspy.empty_set), elem);
    elem = queue_get(&(g_cspy.full_set));
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int cirspy_put(long long usec, int len, char *buf)
{
  int result = -1;
  t_cspy_elem *elem;
  elem = queue_get(&(g_cspy.empty_set));
  if (elem)
    {
    elem->usec = usec;
    elem->len = len;
    memcpy(elem->buf, buf, len);
    queue_put(&(g_cspy.full_set), elem);
    result = 0;
    }
  return result; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void  cirspy_init(void)
{
  int i;
  memset(&g_cspy, 0, sizeof(t_cspy));
  for (i=0; i<CIRC_SPY_MASK; i++)
    queue_put(&(g_cspy.empty_set), &(g_queue[i]));
}
/*--------------------------------------------------------------------------*/
