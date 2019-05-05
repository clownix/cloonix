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
#include <rte_ether.h>
#include <rte_mbuf.h>
#include <unistd.h>
#include <pthread.h>

#include "ioc.h"
#include "pcap_fifo.h"
#include "ring_client.h"

#define MAX_THREAD_RING 100
#define PKT_READ_SIZE  ((uint16_t)32)

static int pool_fifo_free_idx[MAX_THREAD_RING];
static int pool_read_idx;
static int pool_write_idx;

typedef struct t_dpdkr_ring
{
  int idx;
  int ring;
  struct rte_ring *rx_ring;
  struct t_dpdkr_ring *prev;
  struct t_dpdkr_ring *next;
} t_dpdkr_ring;

static t_all_ctx *g_all_ctx;
static t_dpdkr_ring *g_dpdkr_ring_head;
static pthread_t g_thread;

static uint32_t volatile g_sync_lock;
static int g_rte_eal_init_done;


/****************************************************************************/
static int mutex_test_and_lock(void)
{
   return (__sync_lock_test_and_set(&g_sync_lock, 1));
}
/*--------------------------------------------------------------------------*/

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

/****************************************************************************/
static void pool_init(void)
{
  int i;
  for(i = 0; i < MAX_THREAD_RING; i++)
    pool_fifo_free_idx[i] = i+1;
  pool_read_idx = 0;
  pool_write_idx = MAX_THREAD_RING - 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int pool_alloc(void)
{
  int idx = 0;
  if(pool_read_idx != pool_write_idx)
    {
    idx = pool_fifo_free_idx[pool_read_idx];
    pool_read_idx = (pool_read_idx + 1)%MAX_THREAD_RING;
    }
  return idx;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void pool_free(int idx)
{
  pool_fifo_free_idx[pool_write_idx] =  idx;
  pool_write_idx=(pool_write_idx + 1)%MAX_THREAD_RING;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dpdkr_ring_alloc(int idx, int ring, struct rte_ring *rx_ring)
{
  t_dpdkr_ring *cur = (t_dpdkr_ring *) malloc(sizeof(t_dpdkr_ring));
  memset(cur, 0, sizeof(t_dpdkr_ring));
  cur->idx = idx;
  cur->ring = ring;
  cur->rx_ring = rx_ring;
  if (g_dpdkr_ring_head)
    g_dpdkr_ring_head->prev = cur;
  cur->next = g_dpdkr_ring_head;
  g_dpdkr_ring_head = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dpdkr_ring_free(t_dpdkr_ring *cur)
{
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur == g_dpdkr_ring_head)
    g_dpdkr_ring_head = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dpdkr_ring *dpdkr_ring_find(int ring)
{
  t_dpdkr_ring *cur = g_dpdkr_ring_head;
  while(cur)
    {
    if (cur->ring == ring)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static inline const char *get_rx_queue_name(unsigned int id)
{
  static char buffer[RTE_RING_NAMESIZE];
  snprintf(buffer, sizeof(buffer), "dpdkr%u_tx", id);
  return buffer;
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
static void my_process_bulk(int nb_pkts, struct rte_mbuf *pkts[])
{
  struct rte_mbuf  *mb;
  int i, len;
  char *buf;
  long long usec = cloonix_get_usec();
  for (i = 0; i < nb_pkts; i++)
    {
    mb = pkts[i];
    len = (int) mb->pkt_len;
    buf = rte_pktmbuf_mtod(mb, char *);
    pcap_fifo_rx_packet(g_all_ctx, usec, len, buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void *dpdkr_monitor(void *arg)
{
  int i, rx_pkts, retval;
  struct rte_mbuf *pkts[PKT_READ_SIZE];
  struct rte_mbuf *m;
  t_dpdkr_ring *cur;
  char *dpdk_dir = (char *) arg;
  char *argv[] = {"dpdkr_monitor", "--proc-type", "secondary", "--", NULL};
  usleep(100000);
  KERR("dpdkr_monitor STARTING %s", dpdk_dir);
  if (setenv("XDG_RUNTIME_DIR", dpdk_dir, 1))
    KERR("ERROR SETENV XDG_RUNTIME_DIR=%s", dpdk_dir);
  if ((rte_eal_init(4, argv)) < 0)
    KERR("dpdkr_monitor");
  KERR("dpdkr_monitor STARTED");
  g_rte_eal_init_done = 1;
  for (;;)
    {
    if (mutex_test_and_lock())
      usleep(1);
    else
      {
      cur = g_dpdkr_ring_head;
      while (cur)
        {
        rx_pkts = (int) RTE_MIN(rte_ring_count(cur->rx_ring),PKT_READ_SIZE);
        retval=rte_ring_dequeue_bulk(cur->rx_ring,(void *)pkts,rx_pkts,NULL);
        if (retval>0)
          {
          my_process_bulk(retval, pkts);
          for (i = 0; i < retval; i++)
            { 
            m = pkts[i];
            rte_pktmbuf_free(m);
            }
          }
        cur = cur->next;
        }
      mutex_unlock();
      }
    }
  KERR("THREAD EXIT");
  pthread_exit(NULL);
  return NULL;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int ring_add_dpdkr(int ring)
{
  struct rte_ring *rx_ring;
  int idx, result;
  t_dpdkr_ring *cur;
  if (g_rte_eal_init_done == 0)
    { 
    KERR("%s %d", __FUNCTION__, ring);
    result = -1; 
    }
  else
    {
    mutex_lock();
    cur = dpdkr_ring_find(ring);
    rx_ring = rte_ring_lookup(get_rx_queue_name(ring));
    if (cur)
      { 
      KERR("%s %d", __FUNCTION__, ring);
      result = -1; 
      }
    else if (rx_ring == NULL)
      { 
      KERR("%s %d", __FUNCTION__, ring);
      result = -1; 
      }
    else
      {
      idx = pool_alloc();
      if (!idx) 
        { 
        KERR("%s %d", __FUNCTION__, ring);
        result = -1; 
        }
      else
        {
        dpdkr_ring_alloc(idx, ring, rx_ring);
        result = 0; 
        }
      }
    mutex_unlock();
    }
KERR("%s %d", __FUNCTION__, ring);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_rte_eal_init_done(void)
{
return 1;
  return g_rte_eal_init_done;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ring_del_dpdkr(int ring)
{
  int result = -1;
  t_dpdkr_ring *cur = dpdkr_ring_find(ring);
  if (cur)
    {
    mutex_lock();
    pool_free(cur->idx);
    dpdkr_ring_free(cur);
    mutex_unlock();
    result = 0; 
    }
KERR("%s %d", __FUNCTION__, ring);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ring_dpdkr_init(t_all_ctx *all_ctx, char *dpdk_dir)
{
  int result = 0;
  g_rte_eal_init_done = 0;
  pool_init();
  mutex_init();
  g_dpdkr_ring_head = NULL;
  if (pthread_create(&g_thread, NULL, dpdkr_monitor, (void *) dpdk_dir) != 0)
    {
    KERR("THREAD BAD START");
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/
