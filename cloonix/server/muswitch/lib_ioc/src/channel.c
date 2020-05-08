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
#include <sys/select.h>
#include <time.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <asm/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>


#include "ioc.h"
#include "ioc_ctx.h"
#include "channel.h"
#include "util_sock.h"
#include "msg_layer.h"

#define HEARTBEAT_DEFAULT_TIMEOUT 10


void clownix_timer_beat(t_all_ctx *all_ctx);
void clownix_timer_init(t_all_ctx *all_ctx);


/*****************************************************************************/
static void fd_check(t_all_ctx *all_ctx, int fd, int line)
{
  int flags;
  flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    KERR(" %d %d", line, fd);
  flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    KERR(" %d %d", line, fd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void my_gettimeofday(struct timeval *tv)
{ 
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  tv->tv_sec = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec/1000;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_clownix_channels_nb(t_ioc_ctx *ioc_ctx)
{
  return (ioc_ctx->g_current_max_channels);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_clownix_channels_max(t_ioc_ctx *ioc_ctx)
{
  return (ioc_ctx->g_current_max_channels);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_llid(t_ioc_ctx *ioc_ctx, int cidx)
{
  int llid = 0;
  if ((cidx <= 0) || (cidx >= MAX_SELECT_CHANNELS))
    KERR("%d", cidx);
  else if (cidx > ioc_ctx->g_current_max_channels)
    KERR("%d %d", cidx, ioc_ctx->g_current_max_channels);
  else
    {
    llid = ioc_ctx->g_cidx_2_llid[cidx]; 
    if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
      {
      KERR("%d", llid);
      llid = 0;
      }
    else
      {
      if (cidx != ioc_ctx->g_llid_2_cidx[llid])
        {
        KERR("%d %d %d", llid, cidx, ioc_ctx->g_llid_2_cidx[llid]);
        llid = 0;
        }
      }
    }
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int channel_get_llid_with_cidx(t_ioc_ctx *ioc_ctx, int cidx)
{
  int llid = 0;
  if (ioc_ctx->g_cidx_2_llid[cidx]) 
    {
    llid = get_llid(ioc_ctx, cidx);
    if (!llid)
      KOUT("%d", cidx);
    } 
  return llid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_cidx(t_ioc_ctx *ioc_ctx, int llid)
{
  int cidx;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  cidx = ioc_ctx->g_llid_2_cidx[llid];
  if ((cidx <= 0) || (cidx >= MAX_SELECT_CHANNELS))
    KOUT("%d", cidx);
  if (cidx > ioc_ctx->g_current_max_channels)
    KOUT("%d %d", cidx, ioc_ctx->g_current_max_channels);
  if (llid != ioc_ctx->g_cidx_2_llid[cidx])
    KOUT("%d %d %d", llid, cidx, ioc_ctx->g_cidx_2_llid[cidx]);
  if (ioc_ctx->g_channel[cidx].fd != (cidx - 1))
    KOUT("%d %d", cidx, ioc_ctx->g_channel[cidx].fd);
  return (cidx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int channel_check_llid(t_all_ctx *all_ctx, int llid,
                       int *is_blkd, const char *fct)
{
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  int cidx;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d %s", llid, fct);
  cidx = ioc_ctx->g_llid_2_cidx[llid];
  if ((cidx <= 0) || (cidx >= MAX_SELECT_CHANNELS))
    KOUT("%d %d %s", llid, cidx, fct);
  if (llid != ioc_ctx->g_cidx_2_llid[cidx])
    KOUT("%d %d %d %s", llid, cidx, ioc_ctx->g_cidx_2_llid[cidx], fct);
  if (cidx > ioc_ctx->g_current_max_channels)
    KOUT("%s %d %d",fct, cidx, ioc_ctx->g_current_max_channels);
  *is_blkd = ioc_ctx->g_channel[cidx].is_blkd;
  return (cidx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void set_llid_cidx(t_ioc_ctx *ioc_ctx, int llid, int cidx)
{
  ioc_ctx->g_cidx_2_llid[cidx] = llid;
  ioc_ctx->g_llid_2_cidx[llid] = cidx;
  ioc_ctx->g_current_nb_channels++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void unset_llid_cidx(t_ioc_ctx *ioc_ctx, int llid, int cidx)
{
  ioc_ctx->g_cidx_2_llid[cidx] = 0;
  ioc_ctx->g_llid_2_cidx[llid] = 0;
  ioc_ctx->g_current_nb_channels--;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int msg_exist_channel(t_all_ctx *all_ctx, int llid, 
                      int *is_blkd, const char *fct)
{
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  int cidx = 0;
  *is_blkd = 0;
  if (llid != 0)
    {
    if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
      KOUT("%d %s", llid, fct);
    cidx = ioc_ctx->g_llid_2_cidx[llid];
    if (cidx)
      {
      channel_check_llid(all_ctx, llid, is_blkd, fct);
      }
    }
  return cidx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int channel_get_tx_queue_len(t_all_ctx *all_ctx, int llid,
                             int *tx_queued, int *rx_queued)
{
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  int is_blkd, cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
  if (cidx)
    {
    if (is_blkd)
      {
      if (blkd_get_tx_rx_queues((void *) all_ctx, llid, tx_queued, rx_queued))
        KERR(" ");
      }
    else
      {
      *tx_queued = (int) ioc_ctx->g_dchan[cidx].tot_txq_size;
      *rx_queued = 0;
      }
    }
  else
    {
    *tx_queued = 0;
    *rx_queued = 0;
    }
  return is_blkd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_fd_with_llid(t_all_ctx *all_ctx, int llid)
{
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  int fd = -1;
  int is_blkd, cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
  if (cidx)
    {
    fd = ioc_ctx->g_channel[cidx].fd; 
    }
  return (fd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_rx_local_flow_ctrl(void *ptr, int llid, int stop)
{
  int cidx;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  cidx = ioc_ctx->g_llid_2_cidx[llid];
  if (cidx)
    {
    if (stop)
      {
      ioc_ctx->g_channel[cidx].red_to_stop_reading = 1;
      }
    else
      {
      ioc_ctx->g_channel[cidx].red_to_stop_reading = 0;
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_set_red_to_stop_reading(t_all_ctx *all_ctx, int llid)
{
  channel_rx_local_flow_ctrl((void *) all_ctx, llid, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_unset_red_to_stop_reading(t_all_ctx *all_ctx, int llid)
{
  channel_rx_local_flow_ctrl((void *) all_ctx, llid, 0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_tx_local_flow_ctrl(void *ptr, int llid, int stop)
{
  int cidx;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  cidx = ioc_ctx->g_llid_2_cidx[llid];
  if (cidx)
    {
    if (stop)
      {
      ioc_ctx->g_channel[cidx].red_to_stop_writing = 1;
      }
    else
      {
      ioc_ctx->g_channel[cidx].red_to_stop_writing = 0;
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_sync(void *ptr, int llid)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  int is_blkd, cidx = msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__);
  if (cidx)
    {
    fsync(ioc_ctx->g_channel[cidx].fd);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int alloc_cidx(t_all_ctx *all_ctx, int fd)
{
  int cidx;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  ioc_ctx->g_channel_modification_occurence = 1;
  cidx = fd + 1;
  if (ioc_ctx->g_cidx_2_llid[cidx])
    KOUT("%d %d %d", fd, cidx, ioc_ctx->g_cidx_2_llid[cidx]);
  if (ioc_ctx->g_channel[cidx].fd == fd)
    KOUT(" "); 
  if (cidx > ioc_ctx->g_current_max_channels)
    ioc_ctx->g_current_max_channels = cidx;
  ioc_ctx->g_channel[cidx].fd = fd;
  return cidx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void zero_channel(t_ioc_ctx *ioc_ctx, int cidx)
{
  memset (&(ioc_ctx->g_channel[cidx]), 0, sizeof(t_io_channel));
  ioc_ctx->g_channel[cidx].fd = -1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void release_cidx(t_ioc_ctx *ioc_ctx, int cidx)
{
  int i;
  ioc_ctx->g_channel_modification_occurence = 1;
  zero_channel(ioc_ctx, cidx);
  for (i=0; i<MAX_SELECT_CHANNELS; i++)
    if (ioc_ctx->g_channel[i].fd != -1)
      ioc_ctx->g_current_max_channels = i;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void llid_pool_init(t_ioc_ctx *ioc_ctx)
{
  int i;
  for(i = 0; i < CLOWNIX_MAX_CHANNELS; i++)
    ioc_ctx->g_llid_pool.fifo_free_index[i] = i;
  ioc_ctx->g_llid_pool.read_idx = 1;
  ioc_ctx->g_llid_pool.write_idx =  CLOWNIX_MAX_CHANNELS - 1;
  ioc_ctx->g_llid_pool.used_idx = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int llid_pool_alloc(t_all_ctx *all_ctx, char *from)
{
  int llid = 0;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  if(ioc_ctx->g_llid_pool.read_idx != ioc_ctx->g_llid_pool.write_idx)
    {
    llid = ioc_ctx->g_llid_pool.fifo_free_index[ioc_ctx->g_llid_pool.read_idx];
    ioc_ctx->g_llid_pool.read_idx = 
    (ioc_ctx->g_llid_pool.read_idx + 1) % CLOWNIX_MAX_CHANNELS;
    if (ioc_ctx->g_llid_pool.read_idx == 0)
      ioc_ctx->g_llid_pool.read_idx = 1;
    ioc_ctx->g_llid_pool.used_idx += 1;
    if (ioc_ctx->g_llid_pool.used_idx > CLOWNIX_MAX_CHANNELS/2)
      KERR("%d %s", ioc_ctx->g_llid_pool.used_idx, from);
    }
  else
    KOUT("%s", from);
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  return llid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void llid_pool_release(t_ioc_ctx *ioc_ctx, int llid)
{
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  ioc_ctx->g_llid_pool.fifo_free_index[ioc_ctx->g_llid_pool.write_idx] =  llid;
  ioc_ctx->g_llid_pool.write_idx = 
  (ioc_ctx->g_llid_pool.write_idx + 1) % CLOWNIX_MAX_CHANNELS;
  if (ioc_ctx->g_llid_pool.write_idx == 0)
    ioc_ctx->g_llid_pool.write_idx = 1;
  ioc_ctx->g_llid_pool.used_idx -= 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_llid_cidx(t_all_ctx *all_ctx, int *llid, 
                            int *cidx, int fd, char *from)
{
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  *cidx = alloc_cidx(all_ctx, fd);
  *llid = llid_pool_alloc(all_ctx, from);
  if (*cidx < 0)
    KOUT("%d", *cidx);
  if (*cidx >= MAX_SELECT_CHANNELS)
    KOUT("%d", *cidx);
  set_llid_cidx(ioc_ctx, *llid, *cidx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void release_llid_cidx(t_all_ctx *all_ctx, int llid, int cidx)
{
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  release_cidx(ioc_ctx, cidx);
  llid_pool_release(ioc_ctx, llid);
  unset_llid_cidx(ioc_ctx, llid, cidx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int delta_beat(struct timeval *cur, struct timeval *last)
{
  int delta = 0;
  delta = (cur->tv_sec - last->tv_sec)*10000;
  delta += cur->tv_usec/100;
  delta -= last->tv_usec/100;
  return delta;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
struct timeval *channel_get_current_time(t_ioc_ctx *ioc_ctx)
{
  return (&ioc_ctx->g_current_time);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int heartbeat_mngt(t_all_ctx *all_ctx, int type)
{
  struct timeval cur;
  int  delta;  
  int result = 0;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  if (type == 1)
    ioc_ctx->g_heart_count++;
  my_gettimeofday(&cur);
  ioc_ctx->g_current_time.tv_sec = cur.tv_sec;
  ioc_ctx->g_current_time.tv_usec = cur.tv_usec;
  delta = delta_beat(&cur, &ioc_ctx->g_last_heartbeat);
  if ((type == 0) || (delta >= (ioc_ctx->g_heartbeat_ms_timeout*10)))
    {
    clownix_timer_beat(all_ctx);
    if (ioc_ctx->g_heartbeat_ms_timeout < 50)
      {
      if (ioc_ctx->g_heart_count > 15000)
        KERR(" ");
      }
    ioc_ctx->g_heart_count=0;
    memcpy(&ioc_ctx->g_last_heartbeat, &cur, sizeof(struct timeval));
    if (ioc_ctx->g_channel_beat)
      {
      ioc_ctx->g_channel_beat(all_ctx, delta);
      result = 1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void get_max_tx_rx_queues(t_all_ctx *all_ctx, int ref_llid, 
                                 int *tx_max, int *rx_max)
{
  int llid, tx_queued, rx_queued, cidx, fd;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  *tx_max = 0;
  *rx_max = 0;
  if (!all_ctx->qemu_mueth_state)
    {
    for (cidx=0; cidx<=ioc_ctx->g_current_max_channels; cidx++)
      {
      fd = ioc_ctx->g_channel[cidx].fd;
      if (fd != -1)
        {
        llid = get_llid(ioc_ctx, cidx);
        if (!llid)
          KOUT("%d %d", cidx, fd);
        if (ioc_ctx->g_channel[cidx].is_blkd)
          {
          if (llid == ref_llid)
            {
            if (blkd_get_tx_rx_queues((void *)all_ctx, llid,
                                      &tx_queued, &rx_queued))
              KERR(" ");
            if (rx_queued > *rx_max)
              *rx_max = rx_queued;
            }
          else
            {
            if (blkd_get_tx_rx_queues((void *)all_ctx, llid,
                                      &tx_queued, &rx_queued))
              KERR(" ");
            if (tx_queued > *tx_max)
              *tx_max = tx_queued;
            }
          }
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void apply_epoll_ctl(t_all_ctx *all_ctx, int llid,
                            int cidx, uint32_t evt)
{
  int res, fd, is_blkd;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  fd = ioc_ctx->g_channel[cidx].fd;
  ioc_ctx->g_channel[cidx].epev.events = evt;
  res = epoll_ctl(ioc_ctx->g_epfd, EPOLL_CTL_MOD, fd,
                  &(ioc_ctx->g_channel[cidx].epev));
  if (res)
    {
    if (errno == ENOENT)
      {
      if (epoll_ctl(ioc_ctx->g_epfd, EPOLL_CTL_ADD, fd,
                    &(ioc_ctx->g_channel[cidx].epev)))
        KERR("%d", errno);
      }
    else
      {
      if (errno == EBADF)
        {
        if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
          channel_delete(all_ctx, llid);
        KERR("%d (bad fd) ", errno);
        }
      else
        KERR("%d ", errno);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void evt_set_blkd_epoll(t_all_ctx *all_ctx, int cidx, 
                               int llid, uint32_t *evt)
{
  int our_mutype, max_tx_queued = 0, max_rx_queued = 0;
  int tx_queued, rx_queued;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  our_mutype = blkd_get_our_mutype((void *)all_ctx);
  if (blkd_get_tx_rx_queues((void *)all_ctx, llid, &tx_queued, &rx_queued))
    KERR(" ");
  if (tx_queued)
    {
    if (ioc_ctx->g_channel[cidx].red_to_stop_writing == 0)
      (*evt) |= EPOLLOUT; 
    else
      blkd_stop_tx_counter_increment((void *) all_ctx, llid);
    }
  switch (our_mutype)
    {
    case mulan_type:
    case endp_type_tap:
    case endp_type_wif:
    case endp_type_phy:
    case endp_type_pci:
    case endp_type_snf:
    case endp_type_c2c:
    case endp_type_nat:
    case endp_type_a2b:
      get_max_tx_rx_queues(all_ctx, llid, &max_tx_queued, &max_rx_queued);
      if ((max_tx_queued < MAX_GLOB_BLKD_QUEUED_BYTES/2) && 
          (ioc_ctx->g_channel[cidx].red_to_stop_reading == 0))
        (*evt) |= EPOLLIN;
      else
        blkd_stop_rx_counter_increment((void *) all_ctx, llid);
      break;
    case endp_type_kvm_sock:
    case endp_type_kvm_dpdk:
    case endp_type_kvm_vhost:
    case endp_type_kvm_wlan:
    case endp_type_mtcp:
      if (ioc_ctx->g_channel[cidx].red_to_stop_reading == 0)
        (*evt) |= EPOLLIN;
      else
        blkd_stop_rx_counter_increment((void *) all_ctx, llid);
      break;
    default:
      KOUT("%d", our_mutype);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void prepare_rx_tx_events(t_all_ctx *all_ctx)
{
  int cidx, llid, fd;
  uint32_t evt;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  for (cidx=1; cidx<=ioc_ctx->g_current_max_channels; cidx++)
    {
    fd = ioc_ctx->g_channel[cidx].fd;
    if (fd != -1)
      {
      evt = 0;
      llid = get_llid(ioc_ctx, cidx);
      if (!llid)
        KOUT("%d %d", cidx, fd);
      if ((fd + 1) != cidx)
        KOUT(" %d %d ", fd, cidx);
      if (ioc_ctx->g_channel[cidx].is_blkd)
        {
        evt_set_blkd_epoll(all_ctx, cidx, llid, &evt);
        }
      else
        {
        if (ioc_ctx->g_dchan[cidx].tot_txq_size)
          evt |= EPOLLOUT;
        evt |= EPOLLIN;
        }
      apply_epoll_ctl(all_ctx, llid, cidx, evt);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_current_max_channels(t_ioc_ctx *ioc_ctx)
{
return ioc_ctx->g_current_max_channels;
}
/*---------------------------------------------------------------------------*/






/*****************************************************************************/
void channel_delete(t_all_ctx *all_ctx, int llid)
{
  t_ioc_ctx *ioc_ctx;
  int cidx, fd;
  ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  cidx = get_cidx(ioc_ctx, llid);
  fd = ioc_ctx->g_channel[cidx].fd;
  epoll_ctl(ioc_ctx->g_epfd, EPOLL_CTL_DEL, fd, NULL);
  util_free_fd(fd);
  release_llid_cidx(all_ctx, llid, cidx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int channel_create(t_all_ctx *all_ctx, int is_blkd, int fd, 
                   t_fd_event rx_cb, 
                   t_fd_event tx_cb, 
                   t_fd_error err_cb,
                   char *from)
{
  int llid, cidx;
  t_ioc_ctx *ioc_ctx;
  ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  if (ioc_ctx->g_channel[fd].fd == fd)
    KERR("fd exists");
  else
    {
    alloc_llid_cidx(all_ctx, &llid, &cidx, fd, from);
    if (cidx != fd+1)
      KOUT(" %d %d ", cidx, fd);
    ioc_ctx->g_channel[cidx].is_blkd    = is_blkd;
    ioc_ctx->g_channel[cidx].rx_cb      = rx_cb;
    ioc_ctx->g_channel[cidx].tx_cb      = tx_cb;
    ioc_ctx->g_channel[cidx].err_cb     = err_cb;
    ioc_ctx->g_channel[cidx].epev.events = 0;
    ioc_ctx->g_channel[cidx].epev.data.fd = fd;
    ioc_ctx->g_channel[cidx].red_to_stop_reading = 0;
    ioc_ctx->g_channel[cidx].red_to_stop_writing = 0;
    fd_check(all_ctx, fd, __LINE__);
    if (epoll_ctl(ioc_ctx->g_epfd, EPOLL_CTL_ADD, fd, 
                  &(ioc_ctx->g_channel[cidx].epev)))
      KOUT(" ");
    }
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int blkd_channel_create(void *ptr, int fd, 
                        t_fd_event rx, 
                        t_fd_event tx,
                        t_fd_error err, 
                        char *from)
{
  if ((fd < 0) || (fd >= MAX_SELECT_CHANNELS-1))
    KOUT("%d", fd);
  return channel_create((t_all_ctx *)ptr, 1, fd, rx, tx, err, from); 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_counter_select_speed(t_ioc_ctx *ioc_ctx)
{
  int result = ioc_ctx->g_counter_select_speed;
  ioc_ctx->g_counter_select_speed = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_beat_set (t_all_ctx *all_ctx, t_heartbeat_cb beat)
{
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  ioc_ctx->g_channel_beat = beat;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int handle_io_on_fd(t_all_ctx *all_ctx, int nb, 
                           struct epoll_event *events)
{
  int is_blkd, llid, fd, cidx, k, result = -1;
  uint32_t evt;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;

  for(k=0; (!ioc_ctx->g_channel_modification_occurence) && (k<nb); k++)
    {
    cidx = events[k].data.fd + 1;
    fd = ioc_ctx->g_channel[cidx].fd;
    if (fd == -1)
      continue;
    if (fd != events[k].data.fd)
      KOUT("%d %d %d", cidx, fd, events[k].data.fd);
    llid = get_llid(ioc_ctx, cidx);
    if (!llid)
      KOUT("%d %d", cidx, fd);
    evt = events[k].events;
    if ((evt & EPOLLHUP) || (evt & EPOLLERR))
      {
      ioc_ctx->g_channel[cidx].err_cb(all_ctx, llid, errno, 112); 
      if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
        channel_delete(all_ctx, llid);
      result = 0;
      break;
      }
    else
      {
      if (evt & EPOLLOUT)
        {
        if (ioc_ctx->g_channel[cidx].tx_cb(all_ctx, llid, fd)  < 0)
          {
          if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
            channel_delete(all_ctx, llid);
          result = 0;
          break;
          }
        result = 0;
        }
      if (evt & EPOLLIN)
        {
        if (ioc_ctx->g_channel[cidx].rx_cb(all_ctx, llid, fd)  < 0) 
          {
          if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
            channel_delete(all_ctx, llid);
          result = 0;
          break;
          }
        result = 0;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_loop(t_all_ctx *all_ctx)
{
  int cidx, k, result;
  uint32_t evt;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  my_gettimeofday(&ioc_ctx->g_last_heartbeat);

  for(;;)
    {
    if ((ioc_ctx->g_current_max_channels<0) || 
        (ioc_ctx->g_current_max_channels >= MAX_SELECT_CHANNELS))
      KOUT(" %d  %d", ioc_ctx->g_current_max_channels, MAX_SELECT_CHANNELS); 
    prepare_rx_tx_events(all_ctx);
    memset(ioc_ctx->g_events, 0, 
           MAX_EPOLL_EVENTS_PER_RUN * sizeof(struct epoll_event));
    result = epoll_wait(ioc_ctx->g_epfd, ioc_ctx->g_events, 
                        MAX_EPOLL_EVENTS_PER_RUN, 
                        ioc_ctx->g_heartbeat_ms_timeout);  
    if ( result < 0 )
      {
      if (errno == EINTR)
        {
        continue;
        }
      KOUT(" errno: %d\n ", errno);
      }
    if (result == 0)
      {
      heartbeat_mngt(all_ctx, 0);
      continue;
      }
    else
      heartbeat_mngt(all_ctx, 1);
    ioc_ctx->g_slipery_select++;
    ioc_ctx->g_counter_select_speed++;


    ioc_ctx->g_channel_modification_occurence = 0;
    if (!handle_io_on_fd(all_ctx, result, ioc_ctx->g_events))
      ioc_ctx->g_slipery_select = 0;

    if (ioc_ctx->g_slipery_select >= 5)
      {
      for(k=0; k<result; k++) 
          {
          cidx = ioc_ctx->g_events[k].data.fd + 1;
          evt = ioc_ctx->g_events[k].events;
          KERR("%d %d %d %08X", cidx, (evt & EPOLLOUT), 
                                      (evt & EPOLLIN), evt);
          }
      KOUT(" %d %d", result, ioc_ctx->g_channel_modification_occurence);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_init(t_all_ctx *all_ctx)
{
  int i;
  t_ioc_ctx *ioc_ctx = all_ctx->ctx_head.ioc_ctx;
  clownix_timer_init(all_ctx);
  my_gettimeofday(&ioc_ctx->g_last_heartbeat);
  for (i=0; i<MAX_SELECT_CHANNELS; i++)
    {
    zero_channel(ioc_ctx, i);
    ioc_ctx->g_cidx_2_llid[i] = 0;
    }
  for (i=0; i<CLOWNIX_MAX_CHANNELS; i++)
    ioc_ctx->g_llid_2_cidx[i] = 0;
  ioc_ctx->g_current_max_channels = 0;
  ioc_ctx->g_current_nb_channels = 0;
  ioc_ctx->g_heartbeat_ms_timeout = HEARTBEAT_DEFAULT_TIMEOUT;
  ioc_ctx->g_channel_beat = NULL;
  llid_pool_init(ioc_ctx);
  ioc_ctx->g_epfd = epoll_create(EPOLL_CLOEXEC);
}
/*---------------------------------------------------------------------------*/
 


