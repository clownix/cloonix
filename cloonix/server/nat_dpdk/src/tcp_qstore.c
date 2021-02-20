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
#include "tcp_flagseq.h"
#include "tcp_qstore.h"
#include "utils.h"


/****************************************************************************/
static void free_head_backup(t_flagseq *flseq)
{
  t_qstore *cur = flseq->head_qstore_backup;
  if (flseq->head_qstore_backup == flseq->tail_qstore_backup)
    {
    flseq->head_qstore_backup = NULL;
    flseq->tail_qstore_backup = NULL;
    }
  else
    {
    flseq->head_qstore_backup = flseq->head_qstore_backup->next;
    }
  utils_free(cur->data);
  utils_free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void tcp_qstore_enqueue_backup(t_flagseq *flseq, t_qstore *cur)
{
  if (flseq->head_qstore_backup == NULL)
    {
    if (flseq->tail_qstore_backup != NULL)
      KOUT(" ");
    flseq->head_qstore_backup = cur;
    flseq->tail_qstore_backup = cur;
    }
  else
    {
    if (flseq->tail_qstore_backup == NULL)
      KOUT(" ");
    if (flseq->tail_qstore_backup->next != NULL)
      KOUT(" ");
    flseq->tail_qstore_backup->next = cur;
    flseq->tail_qstore_backup = cur;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
struct rte_mbuf *tcp_qstore_get_backup(t_flagseq *flseq, int rank,
                                       int *data_len, uint32_t *local_seq)
{
  t_qstore *cur = flseq->head_qstore_backup;
  uint8_t *data;
  struct rte_mbuf *result = NULL;
  int i;
  if (cur)
    {
    for (i=0; i<rank; i++)
      {
      if (cur)
        cur = cur->next;
      else
        break; 
      }
    }
  if (!cur)
    {
    result = NULL;
    }
  else
    {
    result = utils_alloc_pktmbuf(flseq->offset + cur->data_len + 4 + EMPTY_HEAD);
    data = rte_pktmbuf_mtod(result, uint8_t *);
    memset(data, 0, EMPTY_HEAD);
    data = rte_pktmbuf_mtod_offset(result, uint8_t *, flseq->offset + EMPTY_HEAD);
    memcpy(data, cur->data, cur->data_len);
    *data_len = cur->data_len;
    *local_seq = cur->local_seq;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
struct rte_mbuf *tcp_qstore_dequeue(t_flagseq *flseq,
                                    int *data_len, int local_seq)
{
  t_qstore *cur = flseq->head_qstore;
  uint8_t *data;
  struct rte_mbuf *result;
  if (!cur)
    {
    if (flseq->tail_qstore != NULL)
      KOUT(" ");
    result = NULL;
    }
  else
    {
    result = utils_alloc_pktmbuf(flseq->offset + cur->data_len + 4 + EMPTY_HEAD);
    data = rte_pktmbuf_mtod(result, uint8_t *);
    memset(data, 0, EMPTY_HEAD);
    data = rte_pktmbuf_mtod_offset(result, uint8_t *, flseq->offset + EMPTY_HEAD);
    memcpy(data, cur->data, cur->data_len);
    *data_len = cur->data_len; 
    if (flseq->head_qstore == flseq->tail_qstore)
      {
      flseq->head_qstore = NULL;
      flseq->tail_qstore = NULL;
      }
    else
      {
      flseq->head_qstore = flseq->head_qstore->next;
      }
    flseq->nb_qstore -= 1;
    flseq->nb_qstore_backup += 1;
    cur->next = NULL;
    cur->local_seq = local_seq;
    tcp_qstore_enqueue_backup(flseq, cur);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_qstore_enqueue(t_flagseq *flseq, int data_len, uint8_t *data)
{
  t_qstore *cur = (t_qstore *) utils_malloc(sizeof(t_qstore));
  if (cur == NULL)
    KERR(" ");
  else
    {
    memset(cur, 0, sizeof(t_qstore));
    cur->data_len = data_len;
    cur->data     = data; 
    flseq->nb_qstore += 1;
    if (flseq->head_qstore == NULL)
      {
      if (flseq->tail_qstore != NULL)
        KOUT(" ");
      flseq->head_qstore = cur;
      flseq->tail_qstore = cur;
      }
    else
      {
      if (flseq->tail_qstore == NULL)
        KOUT(" ");
      if (flseq->tail_qstore->next != NULL)
        KOUT(" ");
      flseq->tail_qstore->next = cur;
      flseq->tail_qstore = cur;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_qstore_flush_backup_seq(t_flagseq *flseq, uint32_t local_seq)
{
  while ((flseq->head_qstore_backup) &&
         (flseq->head_qstore_backup->local_seq < local_seq))
    {
    free_head_backup(flseq);
    flseq->nb_qstore_backup -= 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int tcp_qstore_qty(t_flagseq *flseq)
{
  int result = 0; 
  t_qstore *cur = flseq->head_qstore;
  while (cur)
    {
    result += 1;
    cur = cur->next;
    }
  cur = flseq->head_qstore_backup;
  while (cur)
    {
    result += 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int tcp_qstore_flush(t_flagseq *flseq)
{
  int result = 0;
  t_qstore *next, *cur = flseq->head_qstore;
  while (cur)
    {
    next = cur->next;
    result += 1;
    flseq->nb_qstore -= 1;
    utils_free(cur->data);
    utils_free(cur);
    cur = next;
    }
  flseq->head_qstore = NULL;
  flseq->tail_qstore = NULL;
  cur = flseq->head_qstore_backup;
  while (cur)
    {
    next = cur->next;
    result += 1;
    flseq->nb_qstore_backup -= 1;
    utils_free(cur->data);
    utils_free(cur);
    cur = next;
    }
  flseq->head_qstore_backup = NULL;
  flseq->tail_qstore_backup = NULL;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_qstore_init(t_flagseq *flseq)
{
  flseq->offset = sizeof(struct rte_ether_hdr) +
                  sizeof(struct rte_ipv4_hdr) +
                  sizeof(struct rte_tcp_hdr);
}
/*--------------------------------------------------------------------------*/

