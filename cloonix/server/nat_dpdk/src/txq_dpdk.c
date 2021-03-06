/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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

#include <rte_compat.h>
#include <rte_bus_pci.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_mbuf.h>
#include <rte_meter.h>
#include <rte_pci.h>
#include <rte_version.h>
#include <rte_vhost.h>
#include <rte_arp.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "rxtx.h"
#include "dhcp.h"
#include "utils.h"

typedef struct t_txq_dpdk
{
  struct rte_mbuf *mbuf;
  struct t_txq_dpdk *next;
} t_txq_dpdk;

static t_txq_dpdk *g_head_txq_dpdk;
static t_txq_dpdk *g_tail_txq_dpdk;
static uint32_t volatile g_lock;


/****************************************************************************/
void txq_dpdk_enqueue(struct rte_mbuf *pkt, int top_prio)
{
  t_txq_dpdk *cur;
  cur = (t_txq_dpdk *) utils_malloc(sizeof(t_txq_dpdk));
  if (cur == NULL)
    KERR(" ");
  else
    {
    memset(cur, 0, sizeof(t_txq_dpdk));
    cur->mbuf = pkt;
    while (__sync_lock_test_and_set(&(g_lock), 1));
    if (top_prio == 0)
      { 
      if (g_tail_txq_dpdk)
        {
        g_tail_txq_dpdk->next = cur;
        g_tail_txq_dpdk = cur;
        }
      else
        {
        g_head_txq_dpdk = cur;
        g_tail_txq_dpdk = cur;
        }
      }
    else
      {
      if (g_head_txq_dpdk)
        {
        cur->next = g_head_txq_dpdk;
        g_head_txq_dpdk = cur;
        }
      else
        {
        g_head_txq_dpdk = cur;
        g_tail_txq_dpdk = cur;
        }
      }
    __sync_lock_release(&(g_lock));
    }

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void txq_dpdk_dequeue_end(int one_dequeue)
{
  t_txq_dpdk *cur;
  if (one_dequeue)
    { 
    if (!g_head_txq_dpdk)
      KOUT(" ");
    cur = g_head_txq_dpdk;
    if (g_head_txq_dpdk == g_tail_txq_dpdk)
      {
      g_head_txq_dpdk = NULL;
      g_tail_txq_dpdk = NULL;
      }
    else
      g_head_txq_dpdk = g_head_txq_dpdk->next;
    utils_free(cur);
    }
  __sync_lock_release(&(g_lock));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
struct rte_mbuf *txq_dpdk_dequeue_begin(void)
{
  struct rte_mbuf *result = NULL;
  t_txq_dpdk *cur;
  while (__sync_lock_test_and_set(&(g_lock), 1));
  cur = g_head_txq_dpdk;
  if (cur)
    result = cur->mbuf;
  else
  __sync_lock_release(&(g_lock));
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void txq_dpdk_flush(void)
{
  t_txq_dpdk *next, *cur;
  while (__sync_lock_test_and_set(&(g_lock), 1));
  cur = g_head_txq_dpdk;
  while (cur)
    {
    next = cur->next;
    rte_pktmbuf_free(cur->mbuf);
    utils_free(cur);
    cur = next;
    }
  g_head_txq_dpdk = NULL;
  g_tail_txq_dpdk = NULL;
  __sync_lock_release(&(g_lock));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void txq_dpdk_init(void)
{
  g_lock = 0;
  g_head_txq_dpdk = NULL;
  g_tail_txq_dpdk = NULL;
}
/*--------------------------------------------------------------------------*/

