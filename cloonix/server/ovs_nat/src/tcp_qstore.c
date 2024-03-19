/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
  if (cur)
    {
    data = (uint8_t *) utils_malloc(flseq->offset + cur->data_len + 4);
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
  if (!cur)
    {
    if (flseq->tail_qstore != NULL)
      KOUT(" ");
    }
  else
    {
    data = (uint8_t *) utils_malloc(flseq->offset + cur->data_len + 4);
    memcpy(data + flseq->offset, cur->data, cur->data_len);
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
  return data;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_qstore_enqueue(t_flagseq *flseq, int data_len, uint8_t *data)
{
  t_qstore *cur = (t_qstore *) utils_malloc(sizeof(t_qstore));
  if (cur == NULL)
    KOUT("ERROR MALLOC");
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

