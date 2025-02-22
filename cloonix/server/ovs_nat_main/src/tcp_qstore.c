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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"
#include "tcp_flagseq.h"
#include "tcp_qstore.h"

#define TRIGGER_LEVEL_MIN 2000000
#define TRIGGER_LEVEL_MAX 3000000

char *get_nat_name(void);

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
uint8_t *tcp_qstore_get_backup(t_flagseq *flseq, int rank,
                               int *data_len, uint32_t *local_seq)
{
  t_qstore *cur = flseq->head_qstore_backup;
  uint8_t *data = NULL;
  int i, alloc_len;
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
  if (cur)
    {
    alloc_len = flseq->offset + cur->data_len + flseq->post_chk;
    data = (uint8_t *) utils_malloc(alloc_len);
    memcpy(data + flseq->offset, cur->data, cur->data_len);
    *data_len = cur->data_len;
    *local_seq = cur->local_seq;
    }
  return data;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
uint8_t *tcp_qstore_dequeue(t_flagseq *flseq, int *data_len, int local_seq)
{
  t_qstore *cur = flseq->head_qstore;
  uint8_t *data = NULL;
  uint64_t delta;
  int llid_prxy = get_llid_prxy();
  char sig_buf[2*MAX_PATH_LEN];
  int alloc_len;
  if (!cur)
    {
    if (flseq->tail_qstore != NULL)
      KOUT(" ");
    }
  else
    {
    alloc_len = flseq->offset + cur->data_len + flseq->post_chk;
    data = (uint8_t *) utils_malloc(alloc_len);
    memcpy(data + flseq->offset, cur->data, cur->data_len);
    *data_len = cur->data_len; 
    flseq->total_dequeue += (unsigned long long) *data_len;
    delta = (flseq->total_enqueue - flseq->total_dequeue);
    if ((delta < TRIGGER_LEVEL_MIN) && (flseq->total_stopped == 1))
      {
      flseq->total_stopped = 0;
      memset(sig_buf, 0, 2*MAX_PATH_LEN);
      snprintf(sig_buf, 2*MAX_PATH_LEN-1,
      "nat_proxy_tcp_go %s sip:%X dip:%X sport:%hu dport:%hu",
      get_nat_name(),flseq->sip,flseq->dip,flseq->sport,flseq->dport);
      rpct_send_sigdiag_msg(llid_prxy, 0, sig_buf);
      }
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
  return data;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_qstore_enqueue(t_flagseq *flseq, int data_len, uint8_t *data)
{
  t_qstore *cur = (t_qstore *) utils_malloc(sizeof(t_qstore));
  int llid_prxy = get_llid_prxy();
  uint64_t delta;
  char sig_buf[2*MAX_PATH_LEN];

  if (cur == NULL)
    KOUT("ERROR MALLOC");
  flseq->total_enqueue += (unsigned long long) data_len;
  delta = (flseq->total_enqueue - flseq->total_dequeue);
  if ((delta > TRIGGER_LEVEL_MAX) && (flseq->total_stopped == 0))
    { 
    flseq->total_stopped = 1;
    memset(sig_buf, 0, 2*MAX_PATH_LEN);
    snprintf(sig_buf, 2*MAX_PATH_LEN-1,
    "nat_proxy_tcp_stop %s sip:%X dip:%X sport:%hu dport:%hu",
    get_nat_name(),flseq->sip,flseq->dip,flseq->sport,flseq->dport);
    rpct_send_sigdiag_msg(llid_prxy, 0, sig_buf);
    }
  memset(cur, 0, sizeof(t_qstore));
  cur->data_len = data_len;
  cur->data     = (uint8_t *) utils_malloc(data_len); 
  memcpy(cur->data, data, data_len);
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
  flseq->offset = ETHERNET_HEADER_LEN +
                  IPV4_HEADER_LEN +
                  TCP_HEADER_LEN;
  flseq->post_chk = END_FRAME_ADDED_CHECK_LEN;
}
/*--------------------------------------------------------------------------*/

