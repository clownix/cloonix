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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"
#include "vhost_client.h"
#include "rxtx.h"
#include "udp.h"
#include "tcp.h"
#include "tcp_llid.h"
#include "icmp.h"
#include "dhcp.h"
#include "machine.h"
#include "txq_dpdk.h"
#include "rxq_dpdk.h"
#include "tcp_flagseq.h"



static struct rte_mempool *g_mempool;
static int g_enable;
static char g_memid[MAX_NAME_LEN];
static char g_nat_socket[MAX_PATH_LEN];

static int g_virtio_device_on;
static int g_rxtx_worker;
static struct vhost_device_ops g_virtio_net_device_ops;

static uint32_t volatile g_lock;
static int g_running_lcore;



void end_clean_unlink(void);

/****************************************************************************/
struct rte_mempool *get_rte_mempool(void)
{
  return g_mempool;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vhost_lock_acquire(void)
{
  while (__sync_lock_test_and_set(&(g_lock), 1));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vhost_lock_release(void)
{
  __sync_lock_release(&(g_lock));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int virtio_new_device(int vid)
{
  g_virtio_device_on = 1;
  rte_vhost_enable_guest_notification(vid, 0, 0);
  rte_vhost_enable_guest_notification(vid, 1, 0);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void virtio_destroy_device(int vid)
{
  g_virtio_device_on = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int virtio_vring_state_changed(int vid, uint16_t queue_id, int enable)
{
  g_virtio_device_on = 1;
  if (queue_id==0)
    {
    if (enable)
      g_enable = 1;
    else
      g_enable = 0;
    }
  else if (queue_id==1)
    {
    }
  else
    KOUT("%d", queue_id);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rxtx_worker(void *arg __rte_unused)
{
  struct rte_mbuf *pkts_rx[MAX_PKT_BURST];
  struct rte_mbuf *mbuf;
  int i, nb;
  while(g_rxtx_worker)
    {
    usleep(100);
    vhost_lock_acquire();
    if ((g_rxtx_worker) && (g_enable))
      {
      mbuf = txq_dpdk_dequeue_begin();
      while (mbuf)
        {
        nb = rte_vhost_enqueue_burst(0, 0, &mbuf, 1);
        if (nb != 1)
          {
          KERR("SATURATION TX");
          txq_dpdk_dequeue_end(0);
          break;
          }
        txq_dpdk_dequeue_end(1);
        rte_pktmbuf_free(mbuf);
        mbuf = txq_dpdk_dequeue_begin();
        }

      nb = rte_vhost_dequeue_burst(0, 1, g_mempool, pkts_rx, MAX_PKT_BURST);
      for (i=0; i<nb; i++)
        rxq_dpdk_enqueue(pkts_rx[i]);
      rxtx_job_trigger();
      }
    vhost_lock_release();
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_end_and_exit(void)
{
  vhost_lock_acquire();
  g_enable = 0;
  g_rxtx_worker = 0;
  vhost_lock_release();
  if (g_running_lcore == -1)
    KERR("ERROR STOP NAT");
  else
    rte_eal_wait_lcore(g_running_lcore);
  g_running_lcore = -1;
  if (rte_vhost_driver_unregister(g_nat_socket))
    KERR("ERROR UNREGISTER");
  tcp_flush_all();
  txq_dpdk_flush();
  rxq_dpdk_flush();
  rte_mempool_free(g_mempool);
  end_clean_unlink();
  rte_exit(EXIT_SUCCESS, "Exit nat");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_start(char *memid, char *path)
{
  uint64_t flags = 0;
  uint64_t unsup_flags = (1ULL << VIRTIO_NET_F_STATUS);
  int i, j, err, sid;
  uint32_t mcache = MBUF_MCACHE;
  uint32_t mbufs = MBUF_MAX;
  uint32_t msize = MBUF_SIZE;

  memset(&(g_virtio_net_device_ops), 0, sizeof(struct vhost_device_ops));
  memset(g_nat_socket, 0, MAX_PATH_LEN);
  memset(g_memid, 0, MAX_NAME_LEN);
  g_virtio_net_device_ops.new_device          = virtio_new_device;
  g_virtio_net_device_ops.destroy_device      = virtio_destroy_device;
  g_virtio_net_device_ops.vring_state_changed = virtio_vring_state_changed;
  strncpy(g_nat_socket, path, MAX_PATH_LEN-1);
  strncpy(g_memid, memid, MAX_NAME_LEN-1);
  g_enable = 0;
  g_rxtx_worker = 1;
  g_virtio_device_on = 0;
  dhcp_init();
  udp_init();
  tcp_init();
  tcp_llid_init();
  icmp_init();
  rxtx_init();
  sid = rte_lcore_to_socket_id(rte_get_main_lcore());
  if (sid < 0)
    KOUT(" ");
  err = rte_vhost_driver_register(path, flags);
  if (err)
    KOUT(" ");
  err = rte_vhost_driver_callback_register(path, &g_virtio_net_device_ops);
  if (err)
    KOUT(" ");
  err = rte_vhost_driver_disable_features(path, unsup_flags);
  if (err)
    KOUT(" ");
  err = rte_vhost_driver_start(path);
  if (err)
    KOUT(" ");
  g_mempool = rte_mempool_lookup(g_memid);
  if (g_mempool == NULL)
    g_mempool = rte_pktmbuf_pool_create(g_memid, mbufs, mcache, 0, msize, sid);
  if (!g_mempool)
    KOUT("%s %d", g_memid, rte_errno);
  i = rte_get_next_lcore(-1, 1, 0);
  j = rte_get_next_lcore(i, 1, 0);
  if (j < RTE_MAX_LCORE)
    g_running_lcore = j;
  else if (i < RTE_MAX_LCORE)
    g_running_lcore = i;
  else
    KOUT(" ");
  rte_eal_remote_launch(rxtx_worker, NULL, g_running_lcore);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_init(void)
{ 
  g_running_lcore = -1;
  g_mempool = NULL;
  g_enable = 0;
  memset(g_memid, 0, MAX_NAME_LEN);
  memset(g_nat_socket, 0, MAX_PATH_LEN);
  g_virtio_device_on = 0;
  g_rxtx_worker = 0;
  memset(&g_virtio_net_device_ops, 0, sizeof(struct vhost_device_ops));
}
/*--------------------------------------------------------------------------*/

