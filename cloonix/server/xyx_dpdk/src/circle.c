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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <stdint.h>

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
#include "circle.h"
#include "packet_arp_mangle.h"
#include "pcap_record.h"
#include "eventfull.h"

static int g_cnf_mac_mangle;

#define MEMORY_BARRIER()  asm volatile ("": : :"memory")

#define CIRCLE_MASK (0xFFFF)

/*****************************************************************************/
typedef struct t_circle_elem
{
  uint64_t len;
  uint64_t usec;
  struct rte_mbuf *mbuf;
} t_circle_elem;
/*---------------------------------------------------------------------------*/
typedef struct t_circle_set
{
  uint32_t volatile head;
  uint32_t volatile tail;
  uint32_t volatile lock;
  t_circle_elem *elem[CIRCLE_MASK+1];
} t_circle_set;
/*---------------------------------------------------------------------------*/
typedef struct t_circle
{
  t_circle_set rxcq_set;
  t_circle_set txcq_set;
  t_circle_set xxxx_set;
  t_circle_set free_set;
} t_circle;
/*---------------------------------------------------------------------------*/

static t_circle_elem g_elem[2][CIRCLE_MASK+1];
static t_circle g_circle[2];


/*****************************************************************************/
static int utils_to_endp(uint64_t usec, struct rte_mbuf *mbuf)
{
  uint8_t *data;
  int len, result;;
  len = (int) (mbuf->pkt_len);
  data = rte_pktmbuf_mtod_offset(mbuf, uint8_t *, 0);
  result = pcap_record_rx_packet(usec, len, data);
  eventfull_hook_spy(0, len, data);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int utils_from_endp(uint64_t usec, struct rte_mbuf *mbuf)
{
  uint8_t *data;
  int len, result;;
  len = (int) (mbuf->pkt_len);
  data = rte_pktmbuf_mtod_offset(mbuf, uint8_t *, 0);
  result = pcap_record_rx_packet(usec, len, data);
  eventfull_hook_spy(1, len, data);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void elem_transform(int id, t_circle_elem *elem)
{
  int len;
  uint8_t *data;
  len = (int) (elem->mbuf->pkt_len);
  data = rte_pktmbuf_mtod_offset(elem->mbuf, uint8_t *, 0);
  if (packet_arp_mangle(id, len, data))
    KERR("%d", id);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static inline uint32_t ccnt(uint32_t volatile head, uint32_t volatile tail)
{
  return ((head - tail) & CIRCLE_MASK);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static inline uint32_t cspace(uint32_t volatile head, uint32_t volatile tail)
{
  return ((tail - (head+1)) & CIRCLE_MASK);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void elem_put(t_circle_set *cset, t_circle_elem *elem)
{
  uint32_t head, tail, new_head;
  while (__sync_lock_test_and_set(&(cset->lock), 1));
  head = (uint32_t) cset->head;
  tail = (uint32_t) cset->tail;
  if (cspace(head, tail) == 0)
    KOUT(" ");
  if (cset->elem[head] != NULL)
    KOUT(" ");
  cset->elem[head] = elem;
  new_head = (head + 1) & CIRCLE_MASK;
  MEMORY_BARRIER();
  if (__sync_val_compare_and_swap(&(cset->head), head, new_head) != head)
    KOUT("%d %d %d", new_head, head, cset->head);
  __sync_lock_release(&(cset->lock));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int elem_nb(t_circle_set *cset)
{
  int result;
  uint32_t head, tail;
  while (__sync_lock_test_and_set(&(cset->lock), 1));
  head = (uint32_t) cset->head;
  tail = (uint32_t) cset->tail;
  result = ccnt(head, tail);
  __sync_lock_release(&(cset->lock));
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_circle_elem *elem_peek(t_circle_set *cset)
{
  void *elem = NULL;
  uint32_t head, tail;
  while (__sync_lock_test_and_set(&(cset->lock), 1));
  head = (uint32_t) cset->head;
  tail = (uint32_t) cset->tail;
  if (ccnt(head, tail) >= 1)
    {
    elem = cset->elem[tail];
    if (!elem)
      KOUT(" ");
    }
  __sync_lock_release(&(cset->lock));
  return elem;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_circle_elem *elem_get(t_circle_set *cset)
{
  void *elem = NULL;
  uint32_t head, tail, new_tail;
  while (__sync_lock_test_and_set(&(cset->lock), 1));
  head = (uint32_t) cset->head;
  tail = (uint32_t) cset->tail;
  if (ccnt(head, tail) >= 1) 
    {
    elem = cset->elem[tail];
    if (!elem)
      KOUT(" ");
    cset->elem[tail] = NULL;
    new_tail = (tail + 1) & CIRCLE_MASK;
    MEMORY_BARRIER();
    if (__sync_val_compare_and_swap(&(cset->tail), tail, new_tail) != tail)
      KOUT("%d %d %d", new_tail, tail, cset->tail);
    }
  __sync_lock_release(&(cset->lock));
  return elem;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int circle_nb(int id)
{
  int result;
  result = elem_nb(&(g_circle[id].rxcq_set));
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int circle_peek(int id, uint64_t *len, uint64_t *usec, struct rte_mbuf **mbuf)
{
  int result = -1;
  t_circle_elem *elem = elem_peek(&(g_circle[id].rxcq_set));
  if (elem)
    {
    *len = elem->len;
    *usec = elem->usec;
    *mbuf = elem->mbuf;
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int circle_swap(int id, int pcap_nok)
{
  int result = pcap_nok;
  t_circle_elem *elem = elem_get(&(g_circle[id].rxcq_set));
  if (!elem)
    KOUT(" ");
  if (g_cnf_mac_mangle == 1)
    elem_transform(id, elem);
  if (result == 0)
    {
    if (id == 1)
      result = utils_to_endp(elem->usec, elem->mbuf);
    else
      result = utils_from_endp(elem->usec, elem->mbuf);
    }
  elem_put(&(g_circle[id].txcq_set), elem);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void circle_put(int id, uint64_t len, uint64_t usec, struct rte_mbuf *mbuf)
{
  t_circle_elem *elem;

  elem = elem_get(&(g_circle[id].free_set));
  if (!elem)
    KOUT(" ");
  elem->len = len;
  elem->usec = usec;
  elem->mbuf = mbuf;
  elem_put(&(g_circle[id].rxcq_set), elem);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
struct rte_mbuf *circle_get(int id)
{
  struct rte_mbuf *result = NULL;
  t_circle_elem *elem = elem_get(&(g_circle[id].txcq_set));
  if (elem)
    {
    elem_put(&(g_circle[id].xxxx_set), elem);
    result = elem->mbuf;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void circle_clean(int id)
{
  t_circle_elem *elem;
  elem = elem_get(&(g_circle[id].xxxx_set));
  while(elem)
    {
    rte_pktmbuf_free(elem->mbuf);
    elem_put(&(g_circle[id].free_set), elem);
    elem = elem_get(&(g_circle[id].xxxx_set));
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void circle_flush(int id)
{
  t_circle_elem *elem;
  elem = elem_get(&(g_circle[id].rxcq_set));
  while(elem)
    {
    rte_pktmbuf_free(elem->mbuf);
    elem = elem_get(&(g_circle[id].rxcq_set));
    }
  elem = elem_get(&(g_circle[id].txcq_set));
  while(elem)
    {
    rte_pktmbuf_free(elem->mbuf);
    elem = elem_get(&(g_circle[id].txcq_set));
    }
  elem = elem_get(&(g_circle[id].xxxx_set));
  while(elem)
    {
    rte_pktmbuf_free(elem->mbuf);
    elem = elem_get(&(g_circle[id].xxxx_set));
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void circle_cnf(int type, uint8_t *mac)
{

  if (type == xyx_type_mac_mangle)
    {
    set_arp_mangle_mac_hwaddr(mac);
    g_cnf_mac_mangle = 1;
    KERR("CNF mac: %hhX:%hhX:%hhX:%hhX:%hhX:%hhX",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
  else
    KERR("ERROR CNF mac: %hhX:%hhX:%hhX:%hhX:%hhX:%hhX",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void  circle_init(void)
{
  int i;
  g_cnf_mac_mangle = 0;
  memset(&g_circle, 0, 2 * sizeof(t_circle));
  for (i=0; i<CIRCLE_MASK; i++)
    {
    elem_put(&(g_circle[0].free_set), &(g_elem[0][i]));
    elem_put(&(g_circle[1].free_set), &(g_elem[1][i]));
    }
}
/*--------------------------------------------------------------------------*/
