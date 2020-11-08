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

typedef struct t_rxq_dpdk
{
  struct rte_mbuf *mbuf;
  struct t_rxq_dpdk *next;
} t_rxq_dpdk;

static t_rxq_dpdk *g_head_rxq_dpdk;
static t_rxq_dpdk *g_tail_rxq_dpdk;
static uint32_t volatile g_lock;


/****************************************************************************/
void rxq_dpdk_enqueue(struct rte_mbuf *pkt)
{
  t_rxq_dpdk *cur;
  cur = (t_rxq_dpdk *) utils_malloc(sizeof(t_rxq_dpdk));
  if (cur == NULL)
    KERR(" ");
  else
    {
    memset(cur, 0, sizeof(t_rxq_dpdk));
    cur->mbuf = pkt;
    while (__sync_lock_test_and_set(&(g_lock), 1));
    if (g_tail_rxq_dpdk)
      {
      g_tail_rxq_dpdk->next = cur;
      g_tail_rxq_dpdk = cur;
      }
    else
      {
      g_head_rxq_dpdk = cur;
      g_tail_rxq_dpdk = cur;
      }
    __sync_lock_release(&(g_lock));
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
struct rte_mbuf *rxq_dpdk_dequeue(void)
{
  struct rte_mbuf *result = NULL;
  t_rxq_dpdk *cur;
  while (__sync_lock_test_and_set(&(g_lock), 1));
  cur = g_head_rxq_dpdk;
  if (cur)
    {
    result = cur->mbuf;
    if (g_head_rxq_dpdk == g_tail_rxq_dpdk)
      {
      g_head_rxq_dpdk = NULL;
      g_tail_rxq_dpdk = NULL;
      }
    else
      g_head_rxq_dpdk = cur->next;
    utils_free(cur);
    }
  __sync_lock_release(&(g_lock));
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rxq_dpdk_flush(void)
{
  t_rxq_dpdk *next, *cur;
  while (__sync_lock_test_and_set(&(g_lock), 1));
  cur = g_head_rxq_dpdk;
  while (cur)
    {
    next = cur->next;
    rte_pktmbuf_free(cur->mbuf);
    utils_free(cur);
    cur = next;
    }
  g_head_rxq_dpdk = NULL;
  g_tail_rxq_dpdk = NULL;
  __sync_lock_release(&(g_lock));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rxq_dpdk_init(void)
{
  g_lock = 0;
  g_head_rxq_dpdk = NULL;
  g_tail_rxq_dpdk = NULL;
}
/*--------------------------------------------------------------------------*/

