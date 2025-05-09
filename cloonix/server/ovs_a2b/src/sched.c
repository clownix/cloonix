/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"
#include "sched.h"
#include "circle.h"

#define MAX_ETH_LEN 1518
#define TOTAL_LOSS_VALUE 10000

static uint64_t g_cnf_loss[2]; 
static uint64_t g_cnf_delay[2]; 
static uint64_t g_cnf_qsize[2]; 
static uint64_t g_cnf_bsize[2]; 
static uint64_t g_cnf_brate[2]; 
static uint64_t g_tocken[2];
static uint64_t g_pkt_stop[2];
static uint64_t g_pkt_drop[2];
static uint64_t g_pkt_loss[2];
static uint64_t g_pkt_enqueue[2];
static uint64_t g_pkt_dequeue[2];
static uint64_t g_pkt_stored[2];
static uint64_t g_byt_stop[2];
static uint64_t g_byt_drop[2];
static uint64_t g_byt_loss[2];
static uint64_t g_byt_enqueue[2];
static uint64_t g_byt_dequeue[2];
static uint64_t g_byt_stored[2];



#define ETHERNET_HEADER_LEN     14
#define IPV4_HEADER_LEN         20


/*****************************************************************************/
//  static int packet_rx_is_dscp_44(uint64_t len, uint8_t *buf)
//  {
//    int result = 0;
//  
//    if ((len > ETHERNET_HEADER_LEN + IPV4_HEADER_LEN) &&
//        ((buf[12] == 0x08) && (buf[13] == 0x00)) &&
//        (buf[15] == 0x44))
//      {
//      result = 1;
//      }
//    return result;
//  }
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int random_loss(int id, uint64_t len, uint8_t *buf)
{
  int cmp_loss, result = 0;
  if (g_cnf_loss[id])
    {
//    if (packet_rx_is_dscp_44(len, buf))
//      {
      cmp_loss = (int) (rand()%TOTAL_LOSS_VALUE);
      if (cmp_loss <= g_cnf_loss[id])
        result = 1;
//      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int tocken_authorization(int id, uint64_t len)
{
  int result = 0;
  if (g_tocken[id] > (len*1000))
    {
    result = 1;
    g_tocken[id] -= (len*1000);
    }
  else
    {
    g_pkt_stop[id] += 1;
    g_pkt_stop[id] += len;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int shaped_dequeue(int id, uint64_t len)
{
  int result = -1;
  if (g_cnf_brate[id] == 0)
    {
    result = 0;
    }
  else if (tocken_authorization(id, len))
    {
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void sched_inc_queue_stored(int id, uint64_t len)
{
  g_pkt_enqueue[id] += 1;
  g_pkt_stored[id]  += 1;
  g_byt_enqueue[id] += len;
  g_byt_stored[id]  += len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void sched_dec_queue_stored(int id, uint64_t len)
{
  g_pkt_dequeue[id] += 1;
  g_pkt_stored[id]  -= 1;
  g_byt_dequeue[id] += len;
  g_byt_stored[id]  -= len;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void sched_cnf(int dir, char *type, int val)
{   
  if ((dir != 0) && (dir != 1))
    KERR("%d %s %d", dir, type, val);
  if (!strcmp(type, "loss"))
    g_cnf_loss[dir] = (uint64_t ) val;
  else if (!strcmp(type, "delay"))
    g_cnf_delay[dir] = (uint64_t ) val * 1000;
  else if (!strcmp(type, "qsize"))
    g_cnf_qsize[dir] = (uint64_t ) val * 1000;
  else if (!strcmp(type, "bsize"))
    g_cnf_bsize[dir] = (uint64_t ) val * 1000000;
  else if (!strcmp(type, "rate"))
    {
    g_cnf_brate[dir] = (uint64_t ) val;
    if (val > 1000)
      g_cnf_qsize[dir] = (uint64_t ) ((g_cnf_brate[dir] * 1000) / 4);
    else if (val > 100)
      g_cnf_qsize[dir] = (uint64_t ) ((g_cnf_brate[dir] * 1000) / 2);
    else
      g_cnf_qsize[dir] = (uint64_t ) ((g_cnf_brate[dir] * 1000));
    g_cnf_bsize[dir] = (uint64_t ) ((g_cnf_brate[dir] * 1000000) / 8)+2000000;
    }
  else
    KERR("ERROR %d %s %d", dir, type, val);

  KERR("CNF SCHED %d: rate %ld  qsize %ld  bsize %ld  loss %ld delay %ld",
       dir, g_cnf_brate[dir], g_cnf_qsize[dir],
       g_cnf_bsize[dir]/1000, g_cnf_loss[dir], g_cnf_delay[dir]);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void sched_mngt(int id, uint64_t current_usec, uint64_t delta)
{
  uint64_t len;
  uint64_t usec;
  t_pkt *pbuf;
  int result = -1;
  g_tocken[id] += (g_cnf_brate[id] * delta);
  if (g_tocken[id] > (g_cnf_bsize[id]))
    {
    g_tocken[id] = (g_cnf_bsize[id]);
    }
  if (!circle_peek(id, &len, &usec, &pbuf))
    {
    if (g_cnf_delay[id] == 0)
      result = shaped_dequeue(id, len);
    else if ((current_usec - usec) >= g_cnf_delay[id])
      result = shaped_dequeue(id, len);
    }
  while(result == 0)
    {
    circle_swap(id);
    sched_dec_queue_stored(id, len);
    result = -1;
    if (!circle_peek(id, &len, &usec, &pbuf))
      {
      if (g_cnf_delay[id] == 0)
        result = shaped_dequeue(id, len);
      else if ((current_usec - usec) >= g_cnf_delay[id])
        result = shaped_dequeue(id, len);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int sched_can_enqueue(int id, uint64_t len, uint8_t *buf)
{
  int result = 0;
  if (random_loss(id, len, buf))
    {
    g_pkt_loss[id] += 1;
    g_byt_loss[id] += len;
    }
  else if (g_cnf_brate[id] == 0)
    {
    result = 1;
    }
  else
    {
    if (g_byt_stored[id] + MAX_ETH_LEN  < g_cnf_qsize[id])
      {
      result = 1;
      }
    else
      {
      if (g_pkt_drop[id] == 0)
        {
        KERR("DROP %lu %d %lu %lu %d", len, id, g_cnf_qsize[id],
                                       g_byt_stored[id], MAX_ETH_LEN);
        }
      g_pkt_drop[id] += 1;
      g_byt_drop[id] += len;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void sched_init(void)
{
  int i;
  for (i=0; i<2; i++)
    {
    g_cnf_loss[i] = 0;
    g_cnf_delay[i] = 0;
    g_cnf_qsize[i] = 0;
    g_cnf_bsize[i] = 0;
    g_cnf_brate[i] = 0;
    g_tocken[i] = 0;
    g_pkt_stop[i] = 0;
    g_pkt_drop[i] = 0;
    g_pkt_loss[i] = 0;
    g_pkt_enqueue[i] = 0;
    g_pkt_dequeue[i] = 0;
    g_pkt_stored[i] = 0;
    g_byt_stop[i] = 0;
    g_byt_drop[i] = 0;
    g_byt_loss[i] = 0;
    g_byt_enqueue[i] = 0;
    g_byt_dequeue[i] = 0;
    g_byt_stored[i] = 0;
    }
}
/*--------------------------------------------------------------------------*/
