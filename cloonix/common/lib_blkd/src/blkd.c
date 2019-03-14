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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>

#include "ioc_blkd.h"
#include "blkd.h"
#include "sock_unix.h"




/****************************************************************************/
static t_blkd_ctx *g_blkd_ctx;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_blkd_ctx *get_blkd_ctx(void *ptr)
{
  t_all_ctx_head *ctx_head;
  t_blkd_ctx *ctx;
  if (!ptr)
    {
    ctx = g_blkd_ctx;
    }
  else
    {
    ctx_head = (t_all_ctx_head *) ptr;
    ctx = (t_blkd_ctx *) ctx_head->blkd_ctx;
    }
  return ctx;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int *get_llid_blkd_list(void *ptr)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  return(ctx->llid_list);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_llid_blkd_list_max(void *ptr)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  return(ctx->llid_list_max);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int *get_llid_blkd_list_max_ptr(void *ptr)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  return(&(ctx->llid_list_max));
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void add_to_llid_list(int llid, int *llid_list, int *llid_list_max)
{
  int i;
  for (i=0; i<MAX_SELECT_CHANNELS; i++)
    {
    if (llid_list[i] == 0)
      break;
    }
  if (i == MAX_SELECT_CHANNELS)
    KOUT("must increase MAX_SELECT_CHANNELS");
  llid_list[i] = llid;
  if ((i+1) > *llid_list_max)
    *llid_list_max = i+1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void del_from_llid_list(int llid, int *llid_list, int llid_list_max)
{
  int i;
  for (i=0; i<llid_list_max; i++)
    {
    if (llid_list[i] == llid)
      break;
    }
  if (i == llid_list_max)
    KOUT("NOT FOUND %d", llid);
  llid_list[i] = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_llid_blkd *get_llid2blkd(void *ptr, int llid)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  t_llid_blkd *result = ctx->llid2blkd[llid];
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_llid2blkd(void *ptr, int llid, t_llid_blkd *val)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  ctx->llid2blkd[llid] = val;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static t_llid_blkd *alloc_llid_blkd(void *ptr, int llid, int fd, 
                                    char *sock_type, char *sock)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  t_llid_blkd *result = NULL;
  int *llid_list = get_llid_blkd_list(ptr);
  int *llid_list_max = get_llid_blkd_list_max_ptr(ptr);
  if ((llid <=0) || (llid>=CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  if (get_llid2blkd(ptr, llid))
    KERR("%d", llid);
  else
    {
    result = (t_llid_blkd *) malloc(sizeof(t_llid_blkd));
    memset(result, 0, sizeof(t_llid_blkd));
    result->llid = llid;
    result->fd = fd;
    snprintf(result->sock, MAX_PATH_LEN-1, "%s", sock);
    snprintf(result->sock_type, MAX_NAME_LEN-1, "%s", sock_type);
    blkd_tx_init(&(result->fifo_tx));
    blkd_rx_init(&(result->fifo_rx));
    set_llid2blkd(ptr, llid, result);
    add_to_llid_list(llid, llid_list, llid_list_max);
    strncpy(result->report_item.rank_name, "undefined", MAX_NAME_LEN);
    result->report_item.rank_num = -1;
    result->report_item.rank_tidx = -1;
    result->report_item.pid = ctx->g_pid;
    result->report_item.llid = result->llid;
    result->report_item.fd = result->fd;
    snprintf(result->report_item.sock,MAX_PATH_LEN-1,"%s:",result->sock_type);
    strncat(result->report_item.sock, result->sock, MAX_PATH_LEN-1);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_llid_blkd(void *ptr, int llid)
{
  t_llid_blkd *llid2blkd;
  int *llid_list = get_llid_blkd_list(ptr);
  int *llid_list_max = get_llid_blkd_list_max_ptr(ptr);
  if ((llid <=0) || (llid>=CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  llid2blkd = get_llid2blkd(ptr, llid);
  if (!llid2blkd)
    KERR("%d", llid);
  else if (llid2blkd->llid != llid)
    KERR("%d %d", llid, llid2blkd->llid);
  else
    {
    blkd_fd_event_purge_tx(ptr, &(llid2blkd->fifo_tx));
    del_from_llid_list(llid, llid_list, *llid_list_max);
    free(llid2blkd);
    set_llid2blkd(ptr, llid, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_llid_blkd *find_llid_blk(void *ptr, int llid)
{
  t_llid_blkd *cur = get_llid2blkd(ptr, llid);
  if (cur)
    {
    if (cur->llid != llid)
      {
      KERR("%d %d", cur->llid, llid);
      cur = NULL;
      }
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_blkd_channel(void *ptr, int llid, int err, int from)
{
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (!cur)
    KERR(" ");
  else if (!cur->blkd_error_event)
    {
    free_llid_blkd(ptr, llid);
    KERR(" ");
    }
  else
    {
    cur->blkd_error_event(ptr, llid, err, from);
    if (find_llid_blk(ptr, llid))
      free_llid_blkd(ptr, llid);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_blkd_channel(void *ptr, int llid, int fd)
{
  int result = -1;
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (!cur)
    KERR(" ");
  else if (cur->fd != fd)
    KERR(" ");
  else
    {
    cur->fifo_rx.slot_sel[cur->fifo_rx.current_slot] += 1;
    result = blkd_fd_event_rx(ptr, llid, fd,
                              &(cur->fifo_rx),
                              cur->blkd_rx_ready);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int tx_blkd_channel(void *ptr, int llid, int fd)
{
  int result = -1;
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (!cur)
    KERR(" ");
  else if (cur->fd != fd)
    KERR(" ");
  else
    {
    cur->fifo_tx.slot_sel[cur->fifo_tx.current_slot] += 1;
    result = blkd_fd_event_tx(ptr, fd, &(cur->fifo_tx)); 
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void default_err_kill(void *ptr, int llid, int err, int from)
{
  KOUT("%d %d %d\n", llid, err, from);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int new_connect_from_client(void *ptr, int listen_llid, int fd)
{
  int result = -1;
  t_llid_blkd *cur, *listen_cur;
  int fd_new, llid;
  listen_cur = find_llid_blk(ptr, listen_llid);
  if (!listen_cur)
    KERR("%d", listen_llid);
  else if (listen_cur->fd != fd)
    KERR("%d %d", listen_cur->fd, fd);
  else if (!listen_cur->blkd_connect_event)
    KERR(" ");
  else
    {
    sock_unix_fd_accept(fd, &fd_new);
    if (fd_new >= 0)
      {
      llid = blkd_channel_create(ptr, fd_new, 
                                 rx_blkd_channel, 
                                 tx_blkd_channel, 
                                 err_blkd_channel,
                                 (char *) __FUNCTION__);
      if (llid > 0)
        {
        result = 0;
        cur = alloc_llid_blkd(ptr, llid, fd_new, "Server", listen_cur->sock); 
        if (!cur)
          KOUT(" ");
        cur->fd = fd_new;
        listen_cur->blkd_connect_event(ptr, listen_llid, llid);
        }
      else
        KERR("%d", llid);
      }
    else
      KERR("%d %d", fd_new, errno);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_server_listen(void *ptr, char *sock, t_fd_connect connect_cb) 
{
  t_llid_blkd *listen_cur;
  int llid=0, listen_fd;
  listen_fd = sock_listen_unix(sock);
  if (listen_fd > 0)
    {
    llid = blkd_channel_create(ptr, listen_fd, 
                               new_connect_from_client,
                               NULL, default_err_kill,
                               (char *) __FUNCTION__);
    if (llid > 0)
      {
      listen_cur = alloc_llid_blkd(ptr, llid, listen_fd, "Listen", sock);
      if (!listen_cur)
        KOUT(" ");
      listen_cur->fd = listen_fd;
      listen_cur->blkd_connect_event = connect_cb;
      }
    else
      KERR("%d", llid);
    }
  else
    KERR(" ");
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_server_set_callbacks(void *ptr, int llid,
                                    t_blkd_rx_cb rx_cb,
                                    t_fd_error err_cb)
{
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (!cur)
    KERR(" ");
  else
    {
    if (!rx_cb)
      KOUT(" ");
    if (!err_cb)
      KOUT(" ");
    cur->blkd_rx_ready = rx_cb;
    cur->blkd_error_event = err_cb; 
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_client_connect(void *ptr, char *sock,
                             t_blkd_rx_cb rx_cb,
                             t_fd_error err_cb)
{
  t_llid_blkd *cur;
  int fd, llid=0;
  if (!sock_client_unix(sock, &fd))
    {
    llid = blkd_channel_create(ptr, fd, 
                               rx_blkd_channel, 
                               tx_blkd_channel, 
                               err_blkd_channel,
                               (char *) __FUNCTION__);
    if (llid > 0)
      {
      cur = alloc_llid_blkd(ptr, llid, fd, "Client", sock); 
      if (!cur)
        KOUT(" ");
      if (!rx_cb)
        KOUT(" ");
      if (!err_cb)
        KOUT(" ");
      cur->fd = fd;
      cur->blkd_rx_ready = rx_cb;
      cur->blkd_error_event = err_cb; 
      }
    else
      KERR("%d", llid);
    }
  else
    KERR("%s", sock);
  return (llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_blkd *blkd_get_rx(void *ptr, int llid)
{
  t_blkd *blkd = NULL;
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (!cur)
    KERR(" ");
  else
    {
    blkd = blkd_rx_fifo_get(&(cur->fifo_rx));
    }
  return blkd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_put_tx(void *ptr, int nb, int *llid, t_blkd *blkd)
{
  int i;
  t_llid_blkd *cur;
  if (blkd->payload_len > PAYLOAD_BLKD_SIZE)
    KOUT("%d %d", (int) PAYLOAD_BLKD_SIZE, blkd->payload_len); 
  if (blkd->payload_len <=0)
    KOUT("%d", blkd->payload_len); 
  if (blkd->count_reference != 0)
    KERR("ABNORMAL");
  __sync_fetch_and_add(&(blkd->count_reference), 1);
  for (i=0; i<nb; i++)
    {
    cur = find_llid_blk(ptr, llid[i]);
    if (cur)
      {
      if (blkd_tx_fifo_alloc(ptr, &(cur->fifo_tx), blkd))
        {
        cur->report_item.drop_tx += blkd->payload_len;
        }
      }
    } 
  __sync_fetch_and_sub(&(blkd->count_reference), 1);
  if (blkd->count_reference == 0)
    {
    blkd_free(ptr, blkd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_delete(void *ptr, int llid)
{
  int result = -1;
  if (find_llid_blk(ptr, llid))
    {
    free_llid_blkd(ptr, llid);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_get_tx_rx_queues(void *ptr, int llid, int *tx_queued, int *rx_queued)
{
  int result = -1;
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (cur)
    {
    result = 0;
    *tx_queued = cur->fifo_tx.tx_queued_bytes;    
    *rx_queued = cur->fifo_rx.rx_queued_bytes;    
    }
  else
    {
    *tx_queued = 0;
    *rx_queued = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_last_half_sec_rx(t_llid_blkd *cur)
{
  int i, idx, bandwidth_rx = 0;
  for (i=0; i<STORED_SLOTS_IN_HALF_SEC; i++)
    {
    idx = cur->fifo_tx.current_slot - i;
    if (idx < 0)
      idx += STORED_SLOTS;
    bandwidth_rx += cur->fifo_rx.slot_bandwidth[idx];
    }
  return bandwidth_rx;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_get_max_rx_flow_llid(void *ptr, t_blkd_fifo_rx **pool)
{
  int i, llid, rxq, rxa, result_llid = 0, sum, result_sum = 0;
  int *llid_list = get_llid_blkd_list(ptr);
  int llid_list_max = get_llid_blkd_list_max(ptr);
  t_llid_blkd *cur;
  *pool = NULL;
  for (i=0; i < llid_list_max; i++)
    {
    llid = llid_list[i];
    if (llid)
      {
      cur = find_llid_blk(ptr, llid);
      if (cur)
        {
        rxq = cur->fifo_rx.rx_queued_bytes;    
        rxa = get_last_half_sec_rx(cur);
        sum = rxq + rxa;
        if (sum > result_sum)
          {
          result_sum = sum;
          result_llid = llid;
          *pool = &(cur->fifo_rx);
          }
        }
      }
    }
  return result_llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_blkd *blkd_create_tx_empty(int llid, int type, int val)
{
  t_blkd_group *blkd_group = malloc(sizeof(t_blkd_group)); 
  t_blkd *cur = malloc(sizeof(t_blkd));
  memset(blkd_group, 0, sizeof(t_blkd_group));
  memset(cur, 0, sizeof(t_blkd));
  __sync_fetch_and_add(&(blkd_group->count_blkd_tied), 1);
  blkd_group->head_data = (char *) malloc(MAX_TOTAL_BLKD_SIZE);
  cur->qemu_group_rank = 0;
  cur->usec = cloonix_get_usec();
  cur->header_blkd_len = HEADER_BLKD_SIZE;
  cur->header_blkd = blkd_group->head_data;
  cur->payload_blkd = cur->header_blkd + cur->header_blkd_len;
  cur->group = blkd_group;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_blkd *blkd_create_tx_qemu_group(t_blkd_group **ptr_group,
                                   t_qemu_group qemu_group_cb,
                                   void *data,
                                   int len, char *buf,
                                   int llid, int type, int val)
{
  t_blkd_group *blkd_group = *ptr_group;
  t_blkd *cur = malloc(sizeof(t_blkd));
  memset(cur, 0, sizeof(t_blkd));
  if (blkd_group == NULL)
    {
    cur->header_blkd_len = HEADER_BLKD_SIZE;
    (*ptr_group) = (t_blkd_group *) malloc(sizeof(t_blkd_group));
    blkd_group = *ptr_group;
    memset(blkd_group, 0, sizeof(t_blkd_group));
    blkd_group->head_data = (char *) malloc(cur->header_blkd_len);
    if ((!qemu_group_cb) || (!data))
      KOUT(" ");
    blkd_group->qemu_group_cb = qemu_group_cb;
    blkd_group->data = data;
    cur->header_blkd = blkd_group->head_data;
    blkd_group->qemu_total_payload_len = len;
    }
  else
    {
    if ((qemu_group_cb) || (data))
      KOUT(" ");
    cur->header_blkd = NULL;
    cur->header_blkd_len = 0;
    blkd_group->qemu_total_payload_len += len;
    }
  __sync_fetch_and_add(&(blkd_group->count_blkd_tied), 1);
  if (blkd_group->count_blkd_tied >= MAX_QEMU_BLKD_IN_GROUP)
    KOUT("%d %d", blkd_group->count_blkd_tied, len);
  if (len > PAYLOAD_BLKD_SIZE)
    KOUT("%d %d", (int) PAYLOAD_BLKD_SIZE, len);
  if (len <=0)
    KOUT("%d", len);
  cur->qemu_group_rank = blkd_group->count_blkd_tied;
  cur->usec = cloonix_get_usec();
  cur->payload_len = len;
  cur->payload_blkd = buf;
  cur->group = blkd_group;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_blkd *blkd_create_tx_full_copy(int len, char *buf, 
                                 int llid, int type, int val)
{
  t_blkd_group *blkd_group = malloc(sizeof(t_blkd_group));
  t_blkd *cur = malloc(sizeof(t_blkd));
  memset(blkd_group, 0, sizeof(t_blkd_group));
  memset(cur, 0, sizeof(t_blkd));
  __sync_fetch_and_add(&(blkd_group->count_blkd_tied), 1);
  blkd_group->head_data = (char *) malloc(MAX_TOTAL_BLKD_SIZE);
  if (len > PAYLOAD_BLKD_SIZE)
    KOUT("%d %d", (int) PAYLOAD_BLKD_SIZE, len); 
  if (len <=0)
    KOUT("%d", len); 
  cur->qemu_group_rank = 0;
  cur->usec = cloonix_get_usec();
  cur->payload_len = len;
  cur->header_blkd_len = HEADER_BLKD_SIZE;
  cur->header_blkd = blkd_group->head_data;
  cur->payload_blkd = cur->header_blkd + cur->header_blkd_len;
  cur->group = blkd_group;
  memcpy(cur->payload_blkd, buf, len);
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_free(void *ptr, t_blkd *blkd)
{
  t_blkd_group *blkd_group;
  if (!blkd)
    KOUT(" ");
  blkd_group = blkd->group;
  if (!blkd_group)
    KOUT(" ");
  if (blkd_group->count_blkd_tied <= 0)
    KOUT("%d", blkd_group->count_blkd_tied);
  __sync_fetch_and_sub(&(blkd_group->count_blkd_tied), 1);
  if (blkd_group->count_blkd_tied == 0) 
    {
    if (blkd_group->qemu_group_cb)
      {
      if (!ptr)
        KOUT(" ");
      blkd_group->qemu_group_cb(ptr, blkd_group->data);
      }
    free(blkd_group->head_data);
    memset(blkd_group, 0, sizeof(t_blkd_group));
    free(blkd_group);
    }
  memset(blkd, 0, sizeof(t_blkd));
  free(blkd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_watch_fd(void *ptr, char *strident, int fd,
                  t_blkd_rx_cb rx_cb,
                  t_fd_error err_cb)
{
  t_llid_blkd *cur;
  int llid;
  if (!strident)
    KOUT(" ");
  if ((strlen(strident) < 2) || (strlen(strident) >= MAX_PATH_LEN))
    KOUT("%s", strident);
  llid = blkd_channel_create(ptr, fd,
                             rx_blkd_channel,
                             tx_blkd_channel,
                             err_blkd_channel,
                             (char *) __FUNCTION__);
  if (llid > 0)
    {
    cur = alloc_llid_blkd(ptr, llid, fd, "Watch_fd", strident);
    if (!cur)
      KOUT(" ");
    if (!rx_cb)
      KOUT(" ");
    if (!err_cb)
      KOUT(" ");
    cur->fd = fd;
    cur->blkd_rx_ready = rx_cb;
    cur->blkd_error_event = err_cb;
    }
  else
    KERR("%d", llid);
  return (llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_header_rx_extract(t_blkd_record *rec)
{
  char *head = rec->blkd->header_blkd;
  if (((head[0] & 0xFF) != 0xCA) ||
      ((head[1] & 0xFF) != 0xFE) ||
      ((head[2] & 0xFF) != 0xBE) ||
      ((head[3] & 0xFF) != 0xEF))
    KOUT("%02X %02X %02X %02X", head[0]&0xFF, head[1]&0xFF,
                                head[2]&0xFF, head[3]&0xFF);
  rec->len_to_do = ((head[4] & 0xFF) << 8);
  rec->len_to_do += (head[5] & 0xFF);
  if(rec->len_to_do <= rec->blkd->header_blkd_len)
    KOUT("%d %d", rec->len_to_do, rec->blkd->header_blkd_len);
  rec->blkd->payload_len = rec->len_to_do - rec->blkd->header_blkd_len;
  rec->blkd->llid = ((head[6] & 0xFF) << 8);
  rec->blkd->llid += (head[7] & 0xFF);
  rec->blkd->type = ((head[8] & 0xFF) << 8);
  rec->blkd->type += (head[9] & 0xFF);
  rec->blkd->val = ((head[10] & 0xFF) << 8);
  rec->blkd->val += (head[11] & 0xFF);
  memcpy(&(rec->blkd->usec), &(head[12]), sizeof(long long));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void blkd_header_tx_setup(t_blkd_record *rec)
{
  int llid, type, val;
  long long usec;
  char *head;
  int len;
  if (rec->blkd->header_blkd_len)
    {
    rec->len_to_do = rec->blkd->payload_len + rec->blkd->header_blkd_len;
    if (!rec->blkd->header_blkd)
      KOUT(" ");
    llid = rec->blkd->llid;
    type = rec->blkd->type;
    val =  rec->blkd->val;
    usec = rec->blkd->usec;
    head = rec->blkd->header_blkd;
    if (rec->blkd->qemu_group_rank)
      len=rec->blkd->group->qemu_total_payload_len+rec->blkd->header_blkd_len;
    else
      len = rec->blkd->payload_len + rec->blkd->header_blkd_len;
    head[0]  = 0xCA;
    head[1]  = 0xFE;
    head[2]  = 0xBE;
    head[3]  = 0xEF;
    head[4]  = ((len & 0xFF00) >> 8) & 0xFF;
    head[5]  = len & 0xFF;
    head[6]  = ((llid & 0xFF00) >> 8) & 0xFF;
    head[7]  = llid & 0xFF;
    head[8]  = ((type & 0xFF00) >> 8) & 0xFF;
    head[9]  = type & 0xFF;
    head[10] = ((val & 0xFF00) >> 8) & 0xFF;
    head[11] = val & 0xFF;
    memcpy(&(head[12]), &(usec), sizeof(long long));
    }
  else
    rec->len_to_do = rec->blkd->payload_len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int blkd_get_our_mutype(void *ptr)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  int result = ctx->g_our_mutype;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void blkd_set_our_mutype(void *ptr, int mutype)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  ctx->g_our_mutype = mutype;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void update_report(t_llid_blkd *cur)
{
  int i, idx, bandwidth_tx = 0, bandwidth_rx = 0;
  int sel_tx = 0, sel_rx = 0;
  int queue_tx = 0, queue_rx = 0;
  int qty_tx = 0, qty_rx = 0;
  int stop_tx = 0, stop_rx = 0;
  int dist_flow_ctrl_tx = 0, dist_flow_ctrl_rx = 0;
  for (i=0; i<STORED_SLOTS_IN_ONE_SEC; i++)
    {
    idx = cur->fifo_tx.current_slot - i;
    if (idx < 0)
      idx += STORED_SLOTS;
    sel_tx += cur->fifo_tx.slot_sel[idx];
    bandwidth_tx += cur->fifo_tx.slot_bandwidth[idx];
    stop_tx += cur->fifo_tx.slot_stop[idx];
    dist_flow_ctrl_tx += cur->fifo_tx.slot_dist_flow_ctrl[idx];
    if (queue_tx < cur->fifo_tx.slot_queue[idx])
      queue_tx = cur->fifo_tx.slot_queue[idx];
    if (qty_tx < cur->fifo_tx.slot_qty[idx])
      qty_tx = cur->fifo_tx.slot_qty[idx];
    }
  for (i=0; i<STORED_SLOTS_IN_ONE_SEC; i++)
    {
    idx = cur->fifo_rx.current_slot - i;
    if (idx < 0)
      idx += STORED_SLOTS;
    sel_rx += cur->fifo_rx.slot_sel[idx];
    bandwidth_rx += cur->fifo_rx.slot_bandwidth[idx];
    stop_rx += cur->fifo_rx.slot_stop[idx];
    dist_flow_ctrl_rx += cur->fifo_rx.slot_dist_flow_ctrl[idx];
    if (queue_rx < cur->fifo_rx.slot_queue[idx])
      queue_rx = cur->fifo_rx.slot_queue[idx];
    if (qty_rx < cur->fifo_rx.slot_qty[idx])
      qty_rx = cur->fifo_rx.slot_qty[idx];
    }
  cur->report_item.sel_tx = sel_tx;
  cur->report_item.bandwidth_tx = bandwidth_tx;
  cur->report_item.stop_tx = stop_tx;
  cur->report_item.queue_tx = queue_tx;
  cur->report_item.fifo_tx = qty_tx;
  cur->report_item.dist_flow_ctrl_tx = dist_flow_ctrl_tx;
  cur->report_item.sel_rx = sel_rx;
  cur->report_item.bandwidth_rx = bandwidth_rx;
  cur->report_item.stop_rx = stop_rx;
  cur->report_item.queue_rx = queue_rx;
  cur->report_item.fifo_rx = qty_rx;
  cur->report_item.dist_flow_ctrl_rx = dist_flow_ctrl_rx;
}
/*---------------------------------------------------------------------------*/
 
/****************************************************************************/
static void increment_current_slot(t_llid_blkd *cur)
{
  int idx;
  cur->heartbeat = 0;
  cur->fifo_tx.current_slot += 1;
  if (cur->fifo_tx.current_slot >= STORED_SLOTS)
    cur->fifo_tx.current_slot = 0;
  cur->fifo_rx.current_slot += 1;
  if (cur->fifo_rx.current_slot >= STORED_SLOTS)
    cur->fifo_rx.current_slot = 0;
  idx = cur->fifo_tx.current_slot;
  cur->fifo_tx.slot_sel[idx] = 0;
  cur->fifo_tx.slot_qty[idx] = 0;
  cur->fifo_tx.slot_queue[idx] = 0;
  cur->fifo_tx.slot_bandwidth[idx] = 0;
  cur->fifo_tx.slot_stop[idx] = 0;
  cur->fifo_tx.slot_dist_flow_ctrl[idx] = 0;
  idx = cur->fifo_rx.current_slot;
  cur->fifo_rx.slot_sel[idx] = 0;
  cur->fifo_rx.slot_qty[idx] = 0;
  cur->fifo_rx.slot_queue[idx] = 0;
  cur->fifo_rx.slot_bandwidth[idx] = 0;
  cur->fifo_rx.slot_stop[idx] = 0;
  cur->fifo_rx.slot_dist_flow_ctrl[idx] = 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_set_cloonix_llid(void *ptr, int llid)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  ctx->g_cloonix_llid = llid;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_get_cloonix_llid(void *ptr)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  return (ctx->g_cloonix_llid);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_get_llid_with_rank(void *ptr, int rank)
{
  t_llid_blkd *cur;
  int result, llid, i, *llid_list = get_llid_blkd_list(ptr);
  int llid_list_max = get_llid_blkd_list_max(ptr);
  for (i=0; i < llid_list_max; i++)
    {
    llid = llid_list[i];
    if (llid)
      {
      cur = find_llid_blk(ptr, llid);
      if ((cur) && (cur->report_item.rank == rank))
        {
        result = llid;
        break;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_set_rank(void *ptr, int llid, int rank, 
                   char *name, int num, int tidx)
{
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (!cur)
    KERR(" ");
  else
    {
    memset(cur->report_item.rank_name, 0, MAX_NAME_LEN);
    strncpy(cur->report_item.rank_name, name, MAX_NAME_LEN-1);
    cur->report_item.rank_num = num;
    cur->report_item.rank_tidx = tidx;
    cur->report_item.rank = rank;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int blkd_get_rank(void *ptr, int llid, char *name, int *num, int *tidx)
{
  int result = 0;
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (!cur)
    KERR(" ");
  else if ((cur->report_item.rank_num == -1) || 
           (cur->report_item.rank_tidx == -1))
    KERR(" ");
  else 
    {
    memset(name, 0, MAX_NAME_LEN);
    strncpy(name, cur->report_item.rank_name, MAX_NAME_LEN-1);
    *num = cur->report_item.rank_num;
    *tidx = cur->report_item.rank_tidx;
    result = cur->report_item.rank;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_stop_tx_counter_increment(void *ptr, int llid)
{
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (cur)
    {
    cur->fifo_tx.slot_stop[cur->fifo_tx.current_slot] += 1;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_stop_rx_counter_increment(void *ptr, int llid)
{
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (!cur)
    KERR(" ");
  else
    {
    cur->fifo_rx.slot_stop[cur->fifo_rx.current_slot] += 1;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_drop_rx_counter_increment(void *ptr, int llid, int val)
{
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (!cur)
    KERR(" ");
  else
    {
    cur->report_item.drop_rx += val;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_heartbeat(void *ptr)
{
  t_llid_blkd *cur;
  int tidx, num, rank, llid, i, *llid_list = get_llid_blkd_list(ptr);
  int llid_list_max = get_llid_blkd_list_max(ptr);
  char name[MAX_NAME_LEN];
  for (i=0; i < llid_list_max; i++)
    {
    llid = llid_list[i];
    if (llid)
      {
      cur = find_llid_blk(ptr, llid);
      if (cur)
        {
        if (cur->fifo_rx.dist_flow_control_count > 0)
          {
          cur->fifo_rx.dist_flow_control_count -= 1;
          if (cur->fifo_rx.dist_flow_control_count == 0) 
            {
            rank = blkd_get_rank(ptr, llid, name, &num, &tidx);
            if ((rank == 0) || (num == -1))
              KERR("%d %d", rank, num);
            else
              {
              cur->fifo_rx.dist_flow_control_on = 0;
              blkd_rx_dist_flow_control(ptr, name, num, tidx, rank, 0);
              }
            }
          }

        cur->heartbeat += 1;
        if (cur->heartbeat == 5)
          {
          cur->heartbeat = 0;
          update_report(cur);
          increment_current_slot(cur);
          }
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_blkd_item *get_llid_blkd_report_item(void *ptr, int llid)
{
  t_blkd_item *result = NULL;
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (cur)
    result = &(cur->report_item);
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_rx_local_flow_control(void *ptr, int llid, int stop)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  ctx->local_flow_ctrl_rx(ptr, llid, stop);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_tx_local_flow_control(void *ptr, int llid, int stop)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  t_llid_blkd *cur = find_llid_blk(ptr, llid);
  if (stop) 
    cur->fifo_tx.slot_dist_flow_ctrl[cur->fifo_tx.current_slot] += 1;
  ctx->local_flow_ctrl_tx(ptr, llid, stop);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_rx_dist_flow_control(void *ptr, char *name, int num, int tidx, 
                               int rank, int stop)
{
  t_blkd_ctx *ctx = get_blkd_ctx(ptr);
  ctx->dist_flow_ctrl(ptr, ctx->g_cloonix_llid, name, num, tidx, rank, stop);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void blkd_init(void *ptr, t_fd_local_flow_ctrl lfc_tx,
                          t_fd_local_flow_ctrl lfc_rx,
                          t_fd_dist_flow_ctrl dfc)
{
  t_all_ctx_head *ctx_head;
  t_blkd_ctx *ctx;
  if (!ptr) 
    {
    g_blkd_ctx = (t_blkd_ctx *) malloc(sizeof(t_blkd_ctx));
    ctx = g_blkd_ctx;
    }
  else
    {
    ctx_head = (t_all_ctx_head *) ptr;
    ctx_head->blkd_ctx = (t_blkd_ctx *) malloc(sizeof(t_blkd_ctx));
    ctx = ctx_head->blkd_ctx;
    }
  memset(ctx, 0, sizeof(t_blkd_ctx));
  ctx->g_pid = (int) getpid();
  ctx->local_flow_ctrl_tx = lfc_tx;
  ctx->local_flow_ctrl_rx = lfc_rx;
  ctx->dist_flow_ctrl = dfc;
}
/*--------------------------------------------------------------------------*/
