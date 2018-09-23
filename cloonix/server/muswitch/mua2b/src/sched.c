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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "ioc.h"
#include "config.h"
#include "sock_fd.h"
#include "circ_slot.h"

/****************************************************************************/
typedef struct t_queue
{
  t_blkd *blkd;
  long long arrival_date_us;
  struct t_queue *next;
} t_queue;
/*--------------------------------------------------------------------------*/
static t_all_ctx *g_all_ctx0 = NULL;
static t_all_ctx *g_all_ctx1 = NULL;
static long long g_prev_date_us_a;
static long long g_prev_date_us_b;

#define TOTAL_LOSS_VALUE   10000 


/*****************************************************************************/
static long long get_date_us(void)
{
  struct timespec ts;
  long long sec;
  long long nsec;
  long long result;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  sec = (long long) ts.tv_sec;
  sec *= 1000000;
  nsec = (long long) ts.tv_nsec;
  nsec /= 1000;
  result = sec + nsec;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void inc_queue_size(t_connect_side *side, int len)
{
  side->enqueue += len;
  side->stored += len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dec_queue_size(t_connect_side *side, int len)
{
  side->dequeue += len;
  side->stored -= len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int tocken_authorization(t_connect_side *side, int len)
{
  int result = 0;
  if (side->tockens > (len * 1000))
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void update_remove_tockens(t_connect_side *side, int len)
{
  side->tockens -= (len * 1000);
  if (side->tockens < 0)
    KOUT("%lld %d", side->tockens, len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_add_tockens(t_connect_side *side, int ms_delta)
{
  long long conf_bsize = (1000 * side->conf_bsize);
  side->tockens += side->conf_brate * ms_delta;
  if (side->tockens > conf_bsize)
    side->tockens = conf_bsize;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_blkd *do_dequeue(int num)
{
  t_blkd *result = NULL;
  t_queue *q;
  int count = circ_used_slot_nb(num);
  if (count)
    {
    q = (t_queue *) circ_slot_get(num);
    if (!q)
      KOUT("%d %d", num, count);
    result = q->blkd;
    free(q);    
    }
  else
    KERR(" ");
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_blkd *action_dequeue_if_ok(int num, t_connect_side *side, int len)
{
  t_blkd *blkd = NULL;
  if (tocken_authorization(side, len))
    {
    blkd = do_dequeue(num);
    dec_queue_size(side, len);
    update_remove_tockens(side, len);
    }
  return blkd;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void activate_tx(int num, long long now, 
                        t_connect_side *side, int ms_delta)
{
  t_blkd *blkd;
  int delta;
  t_queue *q = circ_slot_lookat(num);
  update_add_tockens(side, ms_delta);
  while(q)
    {
    delta = (int) (now - q->arrival_date_us);
    delta /= 1000;
    if (delta < side->conf_delay)
      {
      KERR("Delay not reached, must wait");
      break;
      }
    else
      {  
      blkd = action_dequeue_if_ok(num, side, q->blkd->payload_len);
      if (blkd)
        {
        if (num == 0)
          sock_fd_tx(g_all_ctx0, blkd);
        else
          sock_fd_tx(g_all_ctx1, blkd);
        }
      else
        {
        KERR("NO TOCKEN BUCKET");
        break;
        }
      }
    q = circ_slot_lookat(num);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_sched(long delta_ns, void *data)
{
  long long date_us;
  int delta, ms_delta;
  int num = (int)((unsigned long) data);
  t_connect_side *side;
  if (clownix_real_timer_add(num, 1000, timer_sched, data, &date_us))
    KOUT(" ");
  if (num == 0)
    {
    side = get_sideA();
    delta = (int) (date_us - g_prev_date_us_a);
    g_prev_date_us_a = date_us;
    ms_delta = delta/1000;
    }
  else if (num == 1)
    {
    side = get_sideB();
    delta = (int) (date_us - g_prev_date_us_b);
    g_prev_date_us_b = date_us;
    ms_delta = delta/1000;
    }
  else
    KOUT(" ");
  activate_tx(num, date_us, side, ms_delta);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int is_lost(int loss)
{
  int cmp_loss, lost_pkt = 0;
  if (loss)
    {
    cmp_loss = rand()%TOTAL_LOSS_VALUE;
    if (cmp_loss <= loss)
      lost_pkt = 1;
    }
  return lost_pkt;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int can_enqueue(int num, int len, t_connect_side *side)
{
  int result = 0;
  if (is_lost(side->conf_loss))
    {
    side->lost += len;
    KERR("loss: %d", len);
    }
  else
    {
    if (side->stored + len < side->conf_qsize)
      {
      inc_queue_size(side, len);
      result = 1;
      }
    else
      {
      side->dropped += len;
      KERR("Q big: %d + %lld and %d", len, side->stored, side->conf_qsize);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void do_enqueue(int num, long long now,  
                       t_blkd *blkd, t_connect_side *side)
{
  t_queue *q;
  int count = circ_empty_slot_nb(num);
  if (count > 0)
    {
    q = (t_queue *) malloc(sizeof(t_queue));
    memset(q, 0, sizeof(t_queue));
    q->arrival_date_us = now;
    q->blkd = blkd;
    circ_slot_put(num, (void *) q);
    }
  else
    {
    side->dropped += blkd->payload_len;
    KERR("%d %d", num, count);
    blkd_free(NULL, blkd);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void sched_tx0_activate(t_all_ctx *all_ctx)
{
  int delta, ms_delta;
  long long now = get_date_us();
  t_connect_side *side = get_sideA();
  delta = (int) (now - g_prev_date_us_a);
  g_prev_date_us_a = now;
  ms_delta = delta/1000;
  activate_tx(0, now, side, ms_delta);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void sched_tx1_activate(t_all_ctx *all_ctx)
{
  int delta, ms_delta;
  long long now = get_date_us();
  t_connect_side *side = get_sideB();
  delta = (int) (now - g_prev_date_us_b);
  g_prev_date_us_b = now;
  ms_delta = delta/1000;
  activate_tx(1, now, side, ms_delta);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void sched_tx_pkt(int num, t_blkd *blkd) 
{
  long long now = get_date_us();
  t_connect_side *side;
  if (num == 0)
    side = get_sideA();
  else if (num == 1)
    side = get_sideB();
  else
    KOUT(" ");
  if (can_enqueue(num, blkd->payload_len, side))
    do_enqueue(num, now, blkd, side);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void sched_init(int num, t_all_ctx *all_ctx)
{
  long long date_us;
  unsigned long data_num = (unsigned long) num;
  if (num == 0)
    g_all_ctx0 = all_ctx;
  else if (num == 1)
    g_all_ctx1 = all_ctx;
  else
    KOUT(" ");
  circ_slot_init(num);
  if (clownix_real_timer_add(num,1000,timer_sched,(void *)data_num,&date_us)) 
    KOUT(" ");
  g_prev_date_us_a = date_us;
  g_prev_date_us_b = date_us;
}
/*---------------------------------------------------------------------------*/

