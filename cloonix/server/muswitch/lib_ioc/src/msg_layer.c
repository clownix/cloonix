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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>


#include "ioc.h"
#include "ioc_ctx.h"
#include "channel.h"
#include "util_sock.h"
#include "msg_layer.h"
#include "chunk.h"

#define MAX_TOT_LEN_QSIG         1000000
#define MAX_TOT_LEN_QDAT         20000000
/*---------------------------------------------------------------------------*/

void init_wake_out_epoll(t_all_ctx *all_ctx);

void linker_helper1_fct(void);

/*****************************************************************************/
t_data_channel *get_dchan(t_ioc_ctx *ioc_ctx, int cidx)
{
  return (&(ioc_ctx->g_dchan[cidx]));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_bigbuf(t_all_ctx *all_ctx)
{
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  return ioc_ctx->g_bigsndbuf;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void default_err_kill(void *ptr, int llid, int err, int from)
{
  KOUT("%d %d %d\n", llid, err, from);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void default_rx_callback(t_all_ctx *all_ctx, int llid, 
                                int len, char *str_rx)
{
  KOUT("%d %d", llid, len);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void err_dchan_cb(void *ptr, int llid, int err, int from)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  int is_blkd, cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
  if (cidx)
    {
    if (is_blkd)
      KOUT(" ");
    if (!ioc_ctx->g_dchan[cidx].rx_callback)
      KOUT("%d %d\n", err, from);
    if (!ioc_ctx->g_dchan[cidx].error_callback)
      KOUT("%d %d\n", err, from);
    ioc_ctx->g_dchan[cidx].error_callback(ptr, llid, err, from);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rx_dchan_cb(void *ptr, int llid, int fd)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int is_blkd, len, cidx, correct_recv;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  cidx = channel_check_llid(all_ctx, llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
  if (!llid || (ioc_ctx->g_dchan[cidx].llid != llid))
    KOUT(" ");
  if (ioc_ctx->g_dchan[cidx].fd != fd)
    KOUT(" ");
  ioc_ctx->g_first_rx_buf = 
  (char *)malloc(ioc_ctx->g_first_rx_buf_max+1);
  len = util_read(ioc_ctx->g_first_rx_buf, 
                  ioc_ctx->g_first_rx_buf_max, 
                  ioc_ctx->g_channel[cidx].fd);
  if (len < 0)
    {
    free(ioc_ctx->g_first_rx_buf);
    err_dchan_cb(all_ctx, llid, errno, 2);
    correct_recv = 0;
    }
  else
    {
    correct_recv = len;
    ioc_ctx->g_first_rx_buf[len] = 0;
    if (ioc_ctx->g_dchan[cidx].decoding_state == rx_type_rawdata)
      {
      ioc_ctx->g_dchan[cidx].rx_callback(all_ctx, llid, len, 
                                         ioc_ctx->g_first_rx_buf);
      free(ioc_ctx->g_first_rx_buf);
      }
    else
      {  
      new_rx_to_process(all_ctx, &(ioc_ctx->g_dchan[cidx]), 
                        len, ioc_ctx->g_first_rx_buf);
      }
    }
  return correct_recv;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int tx_write(t_all_ctx *all_ctx, char *msg, int len, int fd)
{
  int tx_len = 0;
  tx_len = write (fd, (unsigned char *) msg, len);
  if (tx_len < 0)
    {
    tx_len = 0;
    if (!((errno == EAGAIN) || (errno == EINTR)))
      KOUT(" ");
    }
  else if (tx_len == 0)
    KOUT(" ");
  else
    {
    if (tx_len != len)
      KERR("%d %d", tx_len, len);
    }
  return tx_len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int tx_try_send_chunk(t_all_ctx *all_ctx, t_data_channel *dchan, 
                             int cidx, int *correct_send, t_fd_error err_cb)
{
  int len, fstr_len, result;
  char *fstr;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  fstr_len = dchan->tx->len - dchan->tx->len_done;
  fstr = dchan->tx->chunk + dchan->tx->len_done;
  len = tx_write(all_ctx, fstr, fstr_len, ioc_ctx->g_channel[cidx].fd);
  *correct_send = 0;
  if (len > 0)
    {
    *correct_send = len;
    if (dchan->tot_txq_size < (unsigned int) len)
      KOUT("%lu %d", dchan->tot_txq_size, len);
    dchan->tot_txq_size -= len;
    if (len == fstr_len)
      {
      first_elem_delete(&(dchan->tx), &(dchan->last_tx));
      if (dchan->tx)
        result = 1;
      else
        result = 0;
      }
    else
      {
      dchan->tx->len_done += len;
      result = 2;
      }
    }
  else if (len == 0)
    {
    result = 0;
    }
  else if (len < 0)
    {
    dchan->tot_txq_size = 0;
    chain_delete(&(dchan->tx), &(dchan->last_tx));
    if (!err_cb)
      KOUT(" ");
    err_cb(all_ctx, dchan->llid, errno, 3);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int tx_dchan_cb(void *ptr, int llid, int fd)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int is_blkd, cidx, result, correct_send, total_correct_send = 0;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  cidx = channel_check_llid(all_ctx, llid, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
  if (ioc_ctx->g_dchan[cidx].llid !=  llid)
    KOUT(" ");
  if (ioc_ctx->g_dchan[cidx].fd !=  fd)
    KOUT(" ");
  if (ioc_ctx->g_dchan[cidx].tot_txq_size)
    {
    if (!ioc_ctx->g_dchan[cidx].tx)
      KOUT(" ");
    do
      {
      result = tx_try_send_chunk(all_ctx, &(ioc_ctx->g_dchan[cidx]), 
                                 cidx, &correct_send, err_dchan_cb);
      total_correct_send += correct_send;
      } while (result == 1);
    }
  return total_correct_send;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int server_has_new_connect_from_client(void *ptr, int id, int fd)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int is_blkd, fd_new, llid, cidx, serv_cidx;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  serv_cidx = channel_check_llid(all_ctx, id, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
  util_fd_accept(fd, &fd_new, __FUNCTION__);
  if (fd_new >= 0)
    {
    if (fd_new >= MAX_SELECT_CHANNELS-1)
      KOUT("%d", fd_new);
    llid = channel_create(all_ctx, 0, fd_new, rx_dchan_cb, 
                          tx_dchan_cb, err_dchan_cb, (char *) __FUNCTION__);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(all_ctx, llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
    if (!ioc_ctx->g_dchan[serv_cidx].server_connect_callback)
      KOUT(" ");
    memset(&ioc_ctx->g_dchan[cidx], 0, sizeof(t_data_channel));
    if (ioc_ctx->g_dchan[serv_cidx].decoding_state == rx_type_rawdata)
      ioc_ctx->g_dchan[cidx].decoding_state = rx_type_rawdata;
    else
      ioc_ctx->g_dchan[cidx].decoding_state = rx_type_ascii_start;
    ioc_ctx->g_dchan[cidx].llid = llid;
    ioc_ctx->g_dchan[cidx].fd = fd_new;
    ioc_ctx->g_dchan[serv_cidx].server_connect_callback(all_ctx, id, llid);
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void  msg_mngt_set_callbacks (t_all_ctx *all_ctx, int llid, 
                              t_fd_error err_cb, t_msg_rx_cb rx_cb)
{
  int is_blkd, cidx;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  cidx = channel_check_llid(all_ctx, llid, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
  if (ioc_ctx->g_dchan[cidx].llid !=  llid)
    KOUT(" ");
  if (!err_cb)
    KOUT(" ");
  if (!rx_cb)
    KOUT(" ");
  ioc_ctx->g_dchan[cidx].error_callback = err_cb;
  ioc_ctx->g_dchan[cidx].rx_callback = rx_cb;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void msg_delete_channel(t_all_ctx *all_ctx, int llid)
{
  int is_blkd, cidx;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  cidx = channel_check_llid(all_ctx, llid, &is_blkd, __FUNCTION__);
  channel_delete(all_ctx, llid);
  if (is_blkd)
    {
    if (blkd_delete((void *) all_ctx, llid))
      KERR(" ");
    }
  else
    {
    chain_delete(&(ioc_ctx->g_dchan[cidx].tx), 
                 &(ioc_ctx->g_dchan[cidx].last_tx));
    chunk_chain_delete(&(ioc_ctx->g_dchan[cidx]));
    memset(&(ioc_ctx->g_dchan[cidx]), 0, sizeof(t_data_channel));
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_mngt_heartbeat_init(t_all_ctx *all_ctx, t_heartbeat_cb heartbeat)
{
  channel_beat_set (all_ctx, heartbeat);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int msg_watch_fd(t_all_ctx *all_ctx, int fd, 
                 t_fd_event rx_data, t_fd_error err)
{
  int is_blkd, llid, cidx;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  if (fd < 0)
    KOUT(" ");
  if (!err)
    KOUT(" ");
  if ((fd < 0) || (fd >= MAX_SELECT_CHANNELS-1))
    KOUT("%d", fd);
  llid = channel_create(all_ctx, 0, fd, rx_data, 
                        tx_dchan_cb, err_dchan_cb, (char *) __FUNCTION__);
  if (!llid)
    KOUT(" ");
  cidx = channel_check_llid(all_ctx, llid, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
  memset(&ioc_ctx->g_dchan[cidx], 0, sizeof(t_data_channel));
  ioc_ctx->g_dchan[cidx].decoding_state = rx_type_watch;
  ioc_ctx->g_dchan[cidx].rx_callback = default_rx_callback;
  ioc_ctx->g_dchan[cidx].error_callback = err;
  ioc_ctx->g_dchan[cidx].llid = llid;
  ioc_ctx->g_dchan[cidx].fd = fd;
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int local_string_server_unix(t_all_ctx *all_ctx, char *pname, 
                                    int type_raw, t_fd_connect connect_cb)
{
  int is_blkd, llid=0, cidx, listen_fd;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  listen_fd = utilx_socket_listen_unix(pname);
  if (listen_fd >= 0)
    {
    if (listen_fd >= MAX_SELECT_CHANNELS-1)
      KOUT("%d", listen_fd);
    llid = channel_create(all_ctx, 0, listen_fd,
                          server_has_new_connect_from_client,
                          NULL, default_err_kill, (char *) __FUNCTION__);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(all_ctx, llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
    memset(&ioc_ctx->g_dchan[cidx], 0, sizeof(t_data_channel));
    if (type_raw)
      ioc_ctx->g_dchan[cidx].decoding_state = rx_type_rawdata;
    else
      ioc_ctx->g_dchan[cidx].decoding_state = rx_type_ascii_start;
    ioc_ctx->g_dchan[cidx].server_connect_callback = connect_cb;
    }
  return llid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int string_server_unix(t_all_ctx *all_ctx, char *pname, 
                       t_fd_connect connect_cb) 
{
  return local_string_server_unix(all_ctx, pname, 0, connect_cb); 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int rawdata_server_unix(t_all_ctx *all_ctx, char *pname, 
                        t_fd_connect connect_cb)
{ 
  return local_string_server_unix(all_ctx, pname, 1, connect_cb);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int  string_client_unix(t_all_ctx *all_ctx, char *pname, 
                        t_fd_error err_cb, t_msg_rx_cb rx_cb)
{
  int is_blkd, fd, llid=0, cidx;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  if (!util_client_socket_unix(pname, &fd))
    {
    if (!err_cb)
      KOUT(" ");
    if (!rx_cb)
      KOUT(" ");
    if ((fd < 0) || (fd >= MAX_SELECT_CHANNELS-1))
      KOUT("%d", fd);
    llid = channel_create(all_ctx, 0, fd, rx_dchan_cb,
                          tx_dchan_cb, err_dchan_cb, (char *) __FUNCTION__);
    if (!llid)
      KOUT(" ");
    cidx = channel_check_llid(all_ctx, llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
    memset(&ioc_ctx->g_dchan[cidx], 0, sizeof(t_data_channel));
    ioc_ctx->g_dchan[cidx].decoding_state = rx_type_ascii_start;
    ioc_ctx->g_dchan[cidx].llid = llid;
    ioc_ctx->g_dchan[cidx].fd = fd;
    ioc_ctx->g_dchan[cidx].error_callback = err_cb;
    ioc_ctx->g_dchan[cidx].rx_callback = rx_cb;
    }
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void string_tx(t_all_ctx *all_ctx, int llid, int len, char *str_tx)
{
  char *ntx;
  int is_blkd, cidx;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  if ((len<=0) || (len > MAX_TOT_LEN_QSIG))
    KOUT("%d", len);
  cidx = channel_check_llid(all_ctx, llid, &is_blkd, __FUNCTION__);
  if (is_blkd)
    KOUT(" ");
  if (ioc_ctx->g_dchan[cidx].llid != llid)
    KOUT(" ");
  if ((ioc_ctx->g_dchan[cidx].decoding_state != rx_type_ascii_start) && 
      (ioc_ctx->g_dchan[cidx].decoding_state != rx_type_open_bound_found)) 
    KOUT(" ");
  if (ioc_ctx->g_dchan[cidx].tot_txq_size < MAX_TOT_LEN_QSIG)
    {
    ioc_ctx->g_dchan[cidx].tot_txq_size += len;
    if (ioc_ctx->g_peak_queue_len[cidx] < ioc_ctx->g_dchan[cidx].tot_txq_size)
      ioc_ctx->g_peak_queue_len[cidx] = ioc_ctx->g_dchan[cidx].tot_txq_size;
    ntx = (char *)malloc(len);
    memcpy (ntx, str_tx, len);
    chain_append_tx(&(ioc_ctx->g_dchan[cidx].tx), 
                    &(ioc_ctx->g_dchan[cidx].last_tx), len, ntx);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void ptr_string_tx(void *ptr, int llid, int len, char *str_tx)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  int is_blkd, cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
  if (cidx)
    {
    string_tx(all_ctx, llid, len, str_tx);
    channel_sync(ptr, llid);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_mngt_loop(t_all_ctx *all_ctx)
{
  channel_loop(all_ctx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int msg_mngt_get_tx_queue_len(t_all_ctx *all_ctx, int llid)
{
  int tx_queued, rx_queued;
  channel_get_tx_queue_len(all_ctx, llid, &tx_queued, &rx_queued);
  return (tx_queued);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void signal_pipe(int no_use)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void data_tx(t_all_ctx *all_ctx, int llid, int len, char *str_tx)
{
  char *ntx;
  int cidx, is_blkd;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  if (len)
    {
    if ((len<0) || (len > MAX_TOT_LEN_QDAT))
      KOUT("%d", len);
    cidx = channel_check_llid(all_ctx, llid, &is_blkd, __FUNCTION__);
    if (is_blkd)
      KOUT(" ");
    if (ioc_ctx->g_dchan[cidx].llid != llid)
      KOUT(" ");
    if ((ioc_ctx->g_dchan[cidx].decoding_state != rx_type_watch) &&
        (ioc_ctx->g_dchan[cidx].decoding_state != rx_type_rawdata))
      KOUT(" ");
    if (ioc_ctx->g_dchan[cidx].tot_txq_size < MAX_TOT_LEN_QDAT)
      {
      ioc_ctx->g_dchan[cidx].tot_txq_size += len;
      if (ioc_ctx->g_peak_queue_len[cidx] < ioc_ctx->g_dchan[cidx].tot_txq_size)
        ioc_ctx->g_peak_queue_len[cidx] = ioc_ctx->g_dchan[cidx].tot_txq_size;
      ntx = (char *)malloc(len);
      memcpy (ntx, str_tx, len);
      chain_append_tx(&(ioc_ctx->g_dchan[cidx].tx),
                      &(ioc_ctx->g_dchan[cidx].last_tx), len, ntx);
      }
    else
      KERR("%d", (int) ioc_ctx->g_dchan[cidx].tot_txq_size);
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
t_all_ctx *msg_mngt_init (char *name, int num, int max_len_per_read)
{
  t_all_ctx *all_ctx = ioc_ctx_init(name, num, max_len_per_read);
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  sigfillset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = signal_pipe;
  sigaction(SIGPIPE, &act, NULL);
  channel_init(all_ctx);
  init_wake_out_epoll(all_ctx);
  blkd_init(all_ctx, channel_tx_local_flow_ctrl,
                     channel_rx_local_flow_ctrl,
                     rpct_send_peer_flow_control);
  rpct_init(all_ctx, ptr_string_tx);
  linker_helper1_fct();
  return (all_ctx);
} 
/*---------------------------------------------------------------------------*/

