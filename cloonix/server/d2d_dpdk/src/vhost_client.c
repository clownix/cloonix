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
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_meter.h>
#include <rte_pci.h>
#include <rte_version.h>
#include <rte_vhost.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "vhost_client.h"
#include "udp.h"



static struct rte_mempool *g_mempool;
static int g_enable;
static char g_memid[MAX_NAME_LEN];
static char g_d2d_socket[MAX_PATH_LEN];

static int g_virtio_device_on;
static int g_rxtx_worker;
static struct vhost_device_ops g_virtio_net_device_ops;

static uint32_t volatile g_lock;
static int g_running_lcore;

void end_clean_unlink(void);
char *get_net_name(void);
char *get_d2d_name(void);


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
      {
      g_enable = 1;
      }
    else
      {
      g_enable = 0;
      }
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
  struct rte_mbuf *pkts_tx[MAX_PKT_BURST];
  int i, nb_tx_offst, nb, nb_rx,  nb_tx = 0;
  while(g_rxtx_worker)
    {
    usleep(10);
    vhost_lock_acquire();
    if ((g_rxtx_worker) && (g_enable))
      {
      if (nb_tx == 0)
        {
        udp_rx_burst(&nb_tx, pkts_tx);
        nb_tx_offst = 0;
        }
      if (nb_tx != 0)
        {
        nb = rte_vhost_enqueue_burst(0, 0,
                                     &(pkts_tx[nb_tx_offst]),
                                     nb_tx - nb_tx_offst);
        for (i=0; i<nb; i++)
          rte_pktmbuf_free(pkts_tx[nb_tx_offst+i]);
        nb_tx_offst += nb;
        if (nb_tx_offst == nb_tx)
          nb_tx = 0;
        }
      nb_rx = rte_vhost_dequeue_burst(0,1,g_mempool,pkts_rx,MAX_PKT_BURST);
      while(nb_rx)
        {
        if (udp_tx_burst(nb_rx, &(pkts_rx[0])))
          break;
        nb_rx = rte_vhost_dequeue_burst(0,1,g_mempool,pkts_rx,MAX_PKT_BURST);
        }
      }
    vhost_lock_release();
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_stop(void)
{
  int err;
  vhost_lock_acquire();
  g_enable = 0;
  g_virtio_device_on = 0;
  g_rxtx_worker = 0;
  vhost_lock_release();
  if (g_running_lcore == -1)
    KERR("ERROR STOP %s %s", get_net_name(), get_d2d_name());
  else
    rte_eal_wait_lcore(g_running_lcore);
  g_running_lcore = -1;
  err = rte_vhost_driver_unregister(g_d2d_socket);
  if (err)
    KERR("ERROR UNREGISTER");
  rte_mempool_free(g_mempool);
  g_mempool = NULL;
  end_clean_unlink();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_start(char *path, char *memid)
{
  uint64_t flags = 0;
  uint64_t unsup_flags = (1ULL << VIRTIO_NET_F_STATUS);
  int i, j,  err, sid;
  uint32_t mcache = 128;
  uint32_t mbufs = 2048;
  uint32_t msize = 2048;

  g_lock = 0;
  memset(&(g_virtio_net_device_ops), 0, sizeof(struct vhost_device_ops));
  memset(g_d2d_socket, 0, MAX_PATH_LEN);
  memset(g_memid, 0, MAX_NAME_LEN);
  g_virtio_net_device_ops.new_device          = virtio_new_device;
  g_virtio_net_device_ops.destroy_device      = virtio_destroy_device;
  g_virtio_net_device_ops.vring_state_changed = virtio_vring_state_changed;
  strncpy(g_d2d_socket, path, MAX_PATH_LEN-1);
  strncpy(g_memid, memid, MAX_NAME_LEN-1);
  g_enable = 0;
  g_rxtx_worker = 1;
  g_virtio_device_on = 0;
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
  g_mempool = rte_pktmbuf_pool_create(g_memid, mbufs, mcache, 0, msize, sid);
  if (!g_mempool)
    {
    if (rte_errno == EEXIST)
      {
      g_mempool = rte_mempool_lookup(g_memid);
      if (!g_mempool)
        KOUT("%s %d", g_memid, rte_errno);
      }
    else
      KOUT(" ");
    }
  i = rte_get_next_lcore(-1, 1, 0);
  j = rte_get_next_lcore(i, 1, 0);
  if (j < RTE_MAX_LCORE)
    g_running_lcore = j;
  else if (i < RTE_MAX_LCORE)
    g_running_lcore = i;
  else
    KOUT(" ");
  rte_eal_remote_launch(rxtx_worker, NULL, g_running_lcore);
  udp_enter_traffic_mngt();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_init(void)
{
  g_mempool = NULL;
  g_enable = 0;
  memset(g_memid, 0, MAX_NAME_LEN);
  memset(g_d2d_socket, 0, MAX_PATH_LEN);
  g_virtio_device_on = 0;
  g_rxtx_worker = 0;
  g_running_lcore = -1;
  memset(&g_virtio_net_device_ops, 0, sizeof(struct vhost_device_ops));
}
/*--------------------------------------------------------------------------*/

