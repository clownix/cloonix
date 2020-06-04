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

#define ALLOW_EXPERIMENTAL_API
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
#include "cirspy.h"

#define PKT_READ_SIZE 32
#define NB_QUEUE 1

static struct rte_mempool *g_mempool;
static int g_enable;
static void *g_jfs;
static char g_memid[MAX_NAME_LEN];
static char g_snf_socket[MAX_PATH_LEN];

static int g_virtio_device_on;
static int g_circle_worker;
static int g_circle_worker_ended[RTE_MAX_LCORE];
static struct vhost_device_ops g_virtio_net_device_ops;



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
  if (g_virtio_device_on)
    {
    if ((queue_id==0)||(queue_id==2)||(queue_id==4)||(queue_id==6))
      {
      if (enable)
        g_enable += 1;
      else
        {
        g_enable -= 1;
        KERR("RX DISABLE %d", queue_id);
        }
      }
    else if ((queue_id==1)||(queue_id==3)||(queue_id==5)||(queue_id==7))
      {
      }
    else
      KOUT("%d", queue_id);
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void fct_job_for_select(int ref_id, void *data)
{
  if (ref_id != 0xCCC)
    KOUT(" ");
  cirspy_run();
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int circle_worker(void *arg __rte_unused)
{
  struct rte_mbuf *pkts[PKT_READ_SIZE];
  int i, q, nb_rx, len;
  char *buf;
  long long usec;
  while(g_circle_worker)
    {
    usleep(1000);
    if (g_enable == NB_QUEUE)
      {
      for (q=0; q<2*NB_QUEUE; q++)
        {
        if ((q%2) == 1)
          {
          nb_rx = rte_vhost_dequeue_burst(0,q,g_mempool,pkts,PKT_READ_SIZE);
          if (nb_rx)
            {
            usec = cloonix_get_usec();
            for (i = 0; i < nb_rx; i++)
              {
              len = (int) (pkts[i]->pkt_len);
              buf = rte_pktmbuf_mtod(pkts[i], char *);
              if ((len <=0) || (len >= CIRC_MAX_LEN))
                KERR("BAD LEN PACKET: %d", len);
              else
                cirspy_put(usec, len, buf);
              }
            for (i = 0; i < nb_rx; i++)
              rte_pktmbuf_free(pkts[i]);
            job_for_select_request(g_jfs, fct_job_for_select, NULL);
            }
          }
        }
      }
    }
  i = rte_lcore_id();
  g_circle_worker_ended[i] = 1;
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_end(void *data)
{
  int err, i, ok_to_exit = 1;
  for (i=0; i < RTE_MAX_LCORE; i++)
    {
    if (g_circle_worker_ended[i] == 0)
      ok_to_exit = 0;
    }
  if (ok_to_exit)
    {
    rte_mempool_free(g_mempool);
    usleep(1000);
    err = rte_vhost_driver_unregister(g_snf_socket);
    if (err)
      KERR("ERROR UNREGISTER");
    exit(0);
    }
  else
    clownix_timeout_add(1, timeout_end, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_end_and_exit(void)
{
  g_circle_worker = 0;
  clownix_timeout_add(1, timeout_end, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void vhost_client_start(char *path, char *memid)
{
  uint64_t unsup_flags, flags = RTE_VHOST_USER_CLIENT;
  int i, err, llid1, llid2, sid;
  uint32_t mcache = 128;
  uint32_t mbufs = 2048;
  uint32_t msize = 2048;
  memset(&(g_virtio_net_device_ops), 0, sizeof(struct vhost_device_ops));
  memset(g_snf_socket, 0, MAX_PATH_LEN);
  memset(g_memid, 0, MAX_NAME_LEN);
  g_virtio_net_device_ops.new_device          = virtio_new_device;
  g_virtio_net_device_ops.destroy_device      = virtio_destroy_device;
  g_virtio_net_device_ops.vring_state_changed = virtio_vring_state_changed;
  strncpy(g_snf_socket, path, MAX_PATH_LEN-1);
  strncpy(g_memid, memid, MAX_NAME_LEN-1);
  g_enable = 0;
  g_circle_worker = 1;
  g_virtio_device_on = 0;
  job_for_select_init();
  g_jfs = job_for_select_alloc(0xCCC, &llid1, &llid2);
  sid = rte_lcore_to_socket_id(rte_get_master_lcore());
  if (sid < 0)
    KOUT(" ");
  err = rte_vhost_driver_register(path, flags);
  if (err)
    KOUT(" ");
  err = rte_vhost_driver_callback_register(path, &g_virtio_net_device_ops);
  if (err)
    KOUT(" ");
  unsup_flags = 1ULL << VIRTIO_NET_F_CSUM;
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
        KOUT(" ");
      }
    else
      KOUT(" ");
    }
  for (i=0; i < RTE_MAX_LCORE; i++)
    g_circle_worker_ended[i] = 1;
  for (i = rte_get_next_lcore(-1, 1, 0);
             i<RTE_MAX_LCORE;
             i = rte_get_next_lcore(i, 1, 0))
    {
    g_circle_worker_ended[i] = 0;
    rte_eal_remote_launch(circle_worker, NULL, i);
    }
}
/*--------------------------------------------------------------------------*/
