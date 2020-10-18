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
#include <sys/time.h>

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
#include "sched.h"
#include "circle.h"
#include "utils.h"


#define VIRTIO_PMD_SUPPORTED_GUEST_FEATURES     \
        (1u << VIRTIO_NET_F_GUEST_CSUM     |    \
         1u << VIRTIO_NET_F_GUEST_TSO4     |    \
         1u << VIRTIO_NET_F_GUEST_TSO6     |    \
         1u << VIRTIO_NET_F_CSUM           |    \
         1u << VIRTIO_NET_F_HOST_TSO4      |    \
         1u << VIRTIO_NET_F_HOST_TSO6)


#define MAX_PKT_BURST 32

static int g_enable[2];
static char g_memid0[MAX_NAME_LEN];
static char g_memid1[MAX_NAME_LEN];
static char g_a2b0_socket[MAX_PATH_LEN];
static char g_a2b1_socket[MAX_PATH_LEN];
static struct rte_mempool *g_mempool0;
static struct rte_mempool *g_mempool1;
static int g_rxtx_worker;
static struct vhost_device_ops g_virtio_net_device_ops0;
static struct vhost_device_ops g_virtio_net_device_ops1;
static uint32_t volatile g_lock;
static int g_running_lcore;


void end_clean_unlink(void);

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
static uint64_t get_usec(void)
{
  uint64_t usec;
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  usec = (ts.tv_sec * 1000000);
  usec += (ts.tv_nsec / 1000);
  return usec;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int virtio_new_device(int vid)
{
  rte_vhost_enable_guest_notification(vid, 0, 0);
  rte_vhost_enable_guest_notification(vid, 1, 0);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int virtio_features_changed(int vid, uint64_t features)
{
KERR("%lu", features);
return features; 
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void virtio_destroy_device(int vid)
{
  KERR("%d", vid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int virtio_vring_state_changed(int vid, uint16_t queue_id, int enable)
{
  if ((vid != 0) && (vid != 1))
    KOUT("%d %d %d", vid, queue_id, enable);
  if (queue_id==0)
    {
    if (enable)
      {
      g_enable[vid] = 1;
      }
    else
      {
      g_enable[vid] = 0;
      }
    }
  else if (queue_id != 1)
    KOUT("%d", queue_id);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void enqueue(int id, uint64_t usec, int nb, struct rte_mbuf **pkts)
{
  int i;
  for (i=0; i<nb; i++)
    { 
    if (sched_can_enqueue(id, pkts[i]->pkt_len))
      {
      checksum_compute(pkts[i]);
      circle_put(id, pkts[i]->pkt_len, usec, pkts[i]);
      sched_inc_queue_stored(id, pkts[i]->pkt_len);
      }
    else
      rte_pktmbuf_free(pkts[i]);
    }
}   
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void flush_tx_circle(int id)
{
  struct rte_mbuf *pkts_tx[MAX_PKT_BURST];
  int i, nb_tx_todo;
  int result = 0;
  while (result == 0)
    {
    result = -1;
    nb_tx_todo = 0;
    pkts_tx[nb_tx_todo] = circle_get(id);
    while(pkts_tx[nb_tx_todo])
      {
      nb_tx_todo += 1;
      if (nb_tx_todo == MAX_PKT_BURST)
        {
        result = 0;
        break;
        }
      pkts_tx[nb_tx_todo] = circle_get(id);
      }
    if (id == 0)
      {
      rte_vhost_enqueue_burst(1, 0, pkts_tx, nb_tx_todo);
      }
    else
      {
      rte_vhost_enqueue_burst(0, 0, pkts_tx, nb_tx_todo);
      }
    for (i=0; i<nb_tx_todo; i++)
      rte_pktmbuf_free(pkts_tx[i]);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int store_rx_circle(int id, uint64_t arrival_usec)
{
  struct rte_mbuf *pkts_rx[MAX_PKT_BURST];
  int nb_rx, result = -1;
  if (id == 0)
    {
    nb_rx = rte_vhost_dequeue_burst(0, 1, g_mempool0, pkts_rx, MAX_PKT_BURST);
    }
  else
    {
    nb_rx = rte_vhost_dequeue_burst(1, 1, g_mempool1, pkts_rx, MAX_PKT_BURST);
    }
  if (nb_rx)
    {
    enqueue(id, arrival_usec, nb_rx, pkts_rx);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rxtx_worker(void *unused)
{
  int i, fast = 0, loop_is_on = 0;
  uint64_t arrival_usec, delta_usec, last_usec;
  (void) unused;
  while(g_rxtx_worker)
    {
    if (!(g_enable[0] && g_enable[1]))
      usleep(500);
    else if (loop_is_on == 0)
      {
      last_usec = get_usec();
      loop_is_on = 1;
      }
    else
      {
      rte_pause();
      if (!fast)
        {
        usleep(60);
        }
      vhost_lock_acquire();
      fast = 0;
      arrival_usec = get_usec();
      delta_usec = arrival_usec - last_usec;
      last_usec = arrival_usec;
      for (i=0; i<2; i++)
        { 
        if (store_rx_circle(i, arrival_usec) == 0)
          fast = 1;
        sched_mngt(i, arrival_usec, delta_usec);
        flush_tx_circle(i);
        }
      vhost_lock_release();
      }
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_end_and_exit(void)
{
  int err;
KERR("STOP A2B +");
  vhost_lock_acquire();
  g_rxtx_worker = 0;
  vhost_lock_release();
  if (g_running_lcore == -1) 
    KERR("ERROR A2B STOP");
  else
    rte_eal_wait_lcore(g_running_lcore);
  g_running_lcore = -1;
  circle_flush();
  rte_mempool_free(g_mempool0);
  rte_mempool_free(g_mempool1);
  g_mempool0 = NULL;
  g_mempool1 = NULL;
  err = rte_vhost_driver_unregister(g_a2b0_socket);
  if (err)
    KERR("ERROR UNREGISTER");
  err = rte_vhost_driver_unregister(g_a2b1_socket);
  if (err)
    KERR("ERROR UNREGISTER");
  end_clean_unlink();
KERR("STOP A2B -");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_start(char *path0, char *memid0, char *path1, char *memid1)
{
  uint64_t flags = RTE_VHOST_USER_CLIENT | RTE_VHOST_USER_EXTBUF_SUPPORT;
  int i, err, sid;
  uint32_t mcache = 128;
  uint32_t mbufs = 512;
  uint32_t msize = 65500;

  sid = rte_lcore_to_socket_id(rte_get_master_lcore());
  if (sid < 0)
    KOUT(" ");
  strncpy(g_a2b0_socket, path0, MAX_PATH_LEN-1);
  strncpy(g_a2b1_socket, path1, MAX_PATH_LEN-1);
  strncpy(g_memid0, memid0, MAX_NAME_LEN-1);
  strncpy(g_memid1, memid1, MAX_NAME_LEN-1);
  g_mempool0 = rte_pktmbuf_pool_create(g_memid0,mbufs,mcache,0,msize,sid);
  g_mempool1 = rte_pktmbuf_pool_create(g_memid1,mbufs,mcache,0,msize,sid);
  if (!g_mempool0)
    KOUT(" ");
  if (!g_mempool1)
    KOUT(" ");

  g_virtio_net_device_ops0.new_device          = virtio_new_device;
  g_virtio_net_device_ops0.destroy_device      = virtio_destroy_device;
  g_virtio_net_device_ops0.vring_state_changed = virtio_vring_state_changed;
  g_virtio_net_device_ops0.features_changed    = virtio_features_changed;

  g_virtio_net_device_ops1.new_device          = virtio_new_device;
  g_virtio_net_device_ops1.destroy_device      = virtio_destroy_device;
  g_virtio_net_device_ops1.vring_state_changed = virtio_vring_state_changed;
  g_virtio_net_device_ops1.features_changed    = virtio_features_changed;

  g_rxtx_worker = 1;
  err = rte_vhost_driver_register(path0, flags);
  if (err)
    KOUT(" ");
  err = rte_vhost_driver_register(path1, flags);
  if (err)
    KOUT(" ");

  err = rte_vhost_driver_callback_register(path0, &g_virtio_net_device_ops0);
  if (err)
    KOUT(" ");
  err = rte_vhost_driver_callback_register(path1, &g_virtio_net_device_ops1);
  if (err)
    KOUT(" ");

  err = rte_vhost_driver_start(path0);
  if (err)
    KOUT(" ");
  err = rte_vhost_driver_start(path1);
  if (err)
    KOUT(" ");
  i = rte_get_next_lcore(-1, 1, 0);
  if (i >= RTE_MAX_LCORE)
    KOUT(" ");
  g_running_lcore = i;
  rte_eal_remote_launch(rxtx_worker, NULL, g_running_lcore);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_init(void)
{
  g_enable[0] = 0;
  g_enable[1] = 0;
  g_mempool0 = NULL;
  g_mempool1 = NULL;
  g_running_lcore = -1;
  memset(g_memid0, 0, MAX_NAME_LEN);
  memset(g_memid1, 0, MAX_NAME_LEN);
  memset(g_a2b0_socket, 0, MAX_PATH_LEN);
  memset(g_a2b1_socket, 0, MAX_PATH_LEN);
  g_rxtx_worker = 0;
  memset(&g_virtio_net_device_ops0, 0, sizeof(struct vhost_device_ops));
  memset(&g_virtio_net_device_ops1, 0, sizeof(struct vhost_device_ops));
  circle_init();
  sched_init();
}
/*--------------------------------------------------------------------------*/

