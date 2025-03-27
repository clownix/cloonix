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


#include "io_clownix.h"
#include "channel.h"
#include "util_sock.h"
#include "epoll_hooks.h"
#include "msg_layer.h"

#define HEARTBEAT_DEFAULT_TIMEOUT 5
#define MAX_EPOLL_EVENTS_PER_RUN 50
#define PERSEC_COUNTER_INSANE 3000000


static unsigned long g_new_second_arrival_count;
static struct timeval last_heartbeat;
static unsigned long long channel_total_recv;
static unsigned long long channel_total_send;
static int channel_modification_occurence = 0;
static int g_epfd;

static t_fct_before_epoll g_fct_before_epoll;
static t_fct_after_epoll  g_fct_after_epoll;

static int g_count_eintr;

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
typedef struct t_io_channel
  {
  int fd;
  int waked_count_in;
  int waked_count_out;
  int waked_count_err;
  int epollhup_count;
  int zero_count_read_total;
  int zero_count_read;
  int zero_count_write_total;
  int zero_count_write;
  int out_bytes;
  int in_bytes;
  int must_reactivate_read;
  int must_reactivate_write;
  int red_to_stop_reading;
  int red_to_stop_writing;
  t_fd_event rx_cb;
  t_fd_event tx_cb;
  t_fd_error err_cb;
  int kind;
  char little_name[MAX_NAME_LEN];
  struct epoll_event epev;
  } t_io_channel;
/*---------------------------------------------------------------------------*/

typedef struct t_llid_pool
  {
  int fifo_free_index[CLOWNIX_MAX_CHANNELS];
  int read_idx;
  int write_idx;
  } t_llid_pool;
static int slipery_select;
static int counter_select_speed;
static t_io_channel g_channel[MAX_SELECT_CHANNELS];
static int current_max_channels;
static int current_nb_channels;
static t_heartbeat_cb channel_beat;
static struct timeval current_time;
static int llid_2_cidx[CLOWNIX_MAX_CHANNELS];
static int cidx_2_llid[MAX_SELECT_CHANNELS];
static t_llid_pool llid_pool;
static int glob_heartbeat_ms_timeout;

/*****************************************************************************/

/*****************************************************************************/
int channel_get_kind(int cidx)
{
  return (g_channel[cidx].kind);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void zero_channel(int idx)
{
  memset(&(g_channel[idx]), 0, sizeof(t_io_channel));
  g_channel[idx].fd = -1;
  g_channel[idx].epev.data.fd = -1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void my_gettimeofday(struct timeval *tv)
{ 
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  tv->tv_sec = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec/1000;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_fd_with_cidx(int cidx)
{
  return g_channel[cidx].fd; 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_waked_in_with_cidx(int cidx)
{
  int result = g_channel[cidx].waked_count_in;
  g_channel[cidx].waked_count_in = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_waked_out_with_cidx(int cidx)
{
  int result = g_channel[cidx].waked_count_out;
  g_channel[cidx].waked_count_out = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_waked_err_with_cidx(int cidx)
{
  int result = g_channel[cidx].waked_count_err;
  g_channel[cidx].waked_count_err = 0;
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int get_in_bytes_with_cidx(int cidx)
{
  int result = g_channel[cidx].in_bytes;
  g_channel[cidx].in_bytes = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_out_bytes_with_cidx(int cidx)
{
  int result = g_channel[cidx].out_bytes;
  g_channel[cidx].out_bytes = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_clownix_channels_nb(void)
{
  return current_max_channels;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_clownix_channels_max(void)
{
  return current_max_channels;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
unsigned long long get_channel_total_recv(void)
{
  return (channel_total_recv);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
unsigned long long get_channel_total_send(void)
{
  return (channel_total_send);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_channel_cur_recv(void)
{
  static unsigned long long old_channel_total_recv;
  unsigned long long result = channel_total_recv - old_channel_total_recv;
  old_channel_total_recv = channel_total_recv;
  return ((int) result);
} 
/*---------------------------------------------------------------------------*/
int get_channel_cur_send(void)
{ 
  static unsigned long long old_channel_total_send;
  unsigned long long result = channel_total_send - old_channel_total_send;
  old_channel_total_send = channel_total_send;
  return ((int) result);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_llid(int cidx)
{
return (cidx_2_llid[cidx]);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int channel_get_llid_with_cidx(int cidx)
{
  return get_llid(cidx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_cidx(int llid)
{
return (llid_2_cidx[llid]);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *channel_get_little_name(int llid)
{
  int cidx = get_cidx(llid);
  return(g_channel[cidx].little_name);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_fd_with_llid(int llid)
{
  int cidx, result = -1;
  if (msg_exist_channel(llid))
    {
    cidx = get_cidx(llid);
    result = get_fd_with_cidx(cidx);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void set_llid_cidx(int llid, int cidx)
{
  cidx_2_llid[cidx] = llid;
  llid_2_cidx[llid] = cidx;
  current_nb_channels++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void unset_llid_cidx(int llid, int cidx)
{
  cidx_2_llid[cidx] = 0;
  llid_2_cidx[llid] = 0;
  current_nb_channels--;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int msg_exist_channel(int llid)
{
  int result = 0;
  int cidx;
  if (llid < 0)
    KOUT(" ");
  if (llid >= CLOWNIX_MAX_CHANNELS)
    KOUT(" ");
  if (llid != 0)
    {
    cidx = get_cidx (llid);
    if ((cidx > 0) && (cidx <= current_max_channels) && 
        (get_llid(cidx) == llid) && (g_channel[cidx].fd != -1))
      result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int channel_check_llid(int llid, const char *fct)
{
  int cidx = 0;
  if (llid <= 0) 
    KOUT("%s %d",fct, llid);
  if (llid >= CLOWNIX_MAX_CHANNELS)
    KOUT("%s",fct);
  cidx = get_cidx (llid);
  if (cidx <= 0) 
    KERR("%s",fct);
  else
    {
    if (cidx > current_max_channels)
      KOUT("%s",fct);
    if (get_llid(cidx) != llid)
      KOUT("%s",fct);
    if (g_channel[cidx].fd == -1)
      KOUT("%s",fct);
    }
  return (cidx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int alloc_cidx(int fd)
{
  int cidx;
  channel_modification_occurence = 1;
  if (fd < 0)
    KOUT(" ");
  if (fd >= MAX_SELECT_CHANNELS-1)
    KOUT("%d", fd);
  cidx = fd + 1;
  if (cidx > current_max_channels)
    current_max_channels = cidx;
  g_channel[cidx].fd = fd;
  return cidx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void release_cidx(int cidx)
{
  int i;
  channel_modification_occurence = 1;
  zero_channel(cidx);
  for (i=0; i<MAX_SELECT_CHANNELS; i++)
    if (g_channel[i].fd != -1)
      current_max_channels = i;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void llid_pool_init(void)
{
  int i;
  for(i = 0; i < CLOWNIX_MAX_CHANNELS; i++)
    llid_pool.fifo_free_index[i] = i;
  llid_pool.read_idx = 1;
  llid_pool.write_idx =  CLOWNIX_MAX_CHANNELS - 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int llid_pool_alloc(void)
{
  int llid;
  if(llid_pool.read_idx != llid_pool.write_idx)
    {
    llid = llid_pool.fifo_free_index[llid_pool.read_idx];
    llid_pool.read_idx = (llid_pool.read_idx + 1) % CLOWNIX_MAX_CHANNELS;
    if (llid_pool.read_idx == 0)
      llid_pool.read_idx = 1;
    }
  else
    KOUT(" ");
  return llid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void llid_pool_release(int llid)
{
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  llid_pool.fifo_free_index[llid_pool.write_idx] =  llid;
  llid_pool.write_idx = (llid_pool.write_idx + 1) % CLOWNIX_MAX_CHANNELS;
  if (llid_pool.write_idx == 0)
    llid_pool.write_idx = 1;
  if (!i_am_a_clone())
    called_from_channel_free_llid(llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_llid_cidx(int *llid, int *cidx, int fd)
{
  *cidx = alloc_cidx(fd);
  *llid = llid_pool_alloc();
  if (*llid <= 0)
    KOUT("%d", *llid);
  if (*llid >= CLOWNIX_MAX_CHANNELS)
    KOUT("%d", *llid);
  if (*cidx <= 0)
    KOUT("oo %d", *cidx);
  if (*cidx >= MAX_SELECT_CHANNELS)
    KOUT("%d", *cidx);
  set_llid_cidx(*llid, *cidx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void release_llid_cidx(int llid, int cidx)
{
  release_cidx(cidx);
  llid_pool_release(llid);
  unset_llid_cidx(llid, cidx);
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
struct timeval *channel_get_current_time(void)
{
  return (&current_time);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*
void channel_rx_slow_down(int llid)
{
  int cidx;
  if (msg_exist_channel(llid))
    {
    cidx = get_cidx(llid);
    g_channel[cidx].red_to_stop_reading = 1;
    g_channel[cidx].must_reactivate = 1;
    }
}
*/
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void channel_rx_local_flow_ctrl(int llid, int stop)
{
  int cidx;
  if (msg_exist_channel(llid))
    {
    cidx = get_cidx(llid);
    if (stop)
      g_channel[cidx].red_to_stop_reading = 1;
    else
      g_channel[cidx].red_to_stop_reading = 0;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_tx_local_flow_ctrl(int llid, int stop)
{
  int cidx;
  if (msg_exist_channel(llid))
    {
    cidx = get_cidx(llid);
    if (stop)
      g_channel[cidx].red_to_stop_writing = 1;
    else
      g_channel[cidx].red_to_stop_writing = 0;
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void channel_heartbeat_ms_set (int heartbeat_ms)
{
  glob_heartbeat_ms_timeout = heartbeat_ms;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void new_second_arrival(void)
{
  g_new_second_arrival_count = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int heartbeat_mngt(int type)
{
  static int even_for_10ms=0;
  static int heart_count=0;
  struct timeval cur;
  int cidx, delta, result = 0;
  for (cidx=1; cidx<=current_max_channels; cidx++)
    {
    if (g_channel[cidx].must_reactivate_write)
      {
      g_channel[cidx].red_to_stop_writing = 0;
      g_channel[cidx].must_reactivate_write = 0;
      }
    if (g_channel[cidx].must_reactivate_read)
      {
      g_channel[cidx].red_to_stop_reading = 0;
      g_channel[cidx].must_reactivate_read = 0;
      }
    }
  even_for_10ms += 1;
  if (even_for_10ms == 2)
    {
    even_for_10ms = 0;
  if (type == 1)
    heart_count++;
  my_gettimeofday(&cur);
  current_time.tv_sec = cur.tv_sec;
  current_time.tv_usec = cur.tv_usec;
  if (cur.tv_sec > last_heartbeat.tv_sec)
    new_second_arrival();
  delta = delta_beat(&cur, &last_heartbeat);
  if ((type == 0) || (delta >= (glob_heartbeat_ms_timeout*10)))
    {
    clownix_timer_beat();
    if (glob_heartbeat_ms_timeout < 50)
      {
      if (heart_count > 15000)
        {
        KERR("%d LONG SELECT", type);
        result = 1;
        }
      }
    heart_count=0;
    memcpy(&last_heartbeat, &cur, sizeof(struct timeval));
    if (channel_beat)
      {
      channel_beat(delta);
      }
    }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fd_check(int fd, int line, char *little_name)
{
  int flags;
  flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    KERR(" %d %s ", line, little_name);
  flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    KERR(" %d %s ", line, little_name);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
unsigned long channel_get_tx_queue_len(int llid)
{
  int cidx = channel_check_llid(llid, __FUNCTION__);
  int result = 0;
  if (cidx != 0) 
    result = get_tot_txq_size(cidx);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void apply_epoll_ctl(int llid, int cidx, uint32_t evt)
{
  int res, fd;
  g_channel[cidx].epev.events = evt;
  fd = g_channel[cidx].fd;
  res = epoll_ctl(g_epfd, EPOLL_CTL_MOD, fd, &(g_channel[cidx].epev));
  if (res)
    {
    if (errno == ENOENT)
      {
      if (epoll_ctl(g_epfd, EPOLL_CTL_ADD, fd, &(g_channel[cidx].epev)))
        KERR(" %s %d ", g_channel[cidx].little_name, errno);
      }
    else
      {
      if (errno == EBADF)
        {
        KERR(" %s %d (bad fd) ", g_channel[cidx].little_name, errno);
        g_channel[cidx].err_cb(llid, errno, 1111);
        if (msg_exist_channel(llid))
          channel_delete(llid);
        }
      else
        KERR(" %s %d ", g_channel[cidx].little_name, errno);
      }
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int check_fd_is_valid(int fd)
{
  return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void prepare_rx_tx_events(void)
{
  int llid, cidx, fd;
  uint32_t evt;
  for (cidx=1; cidx<=current_max_channels; cidx++)
    {
    fd = g_channel[cidx].fd;
    if ((fd != -1) && (g_channel[cidx].kind != kind_glib_managed))
      {
      llid = get_llid(cidx);
      if (!llid)
        KOUT("%d %d %d", cidx, fd, g_channel[cidx].kind);
      if ((fd + 1) != cidx)
        KOUT(" %d %d ", fd, cidx);
      if (!check_fd_is_valid(fd))
        {
        KERR("ERROR FD %d %d", fd, g_channel[cidx].kind);
        g_channel[cidx].err_cb(llid, errno, 514);
        channel_delete(llid);
        }
      else
        {
        evt = 0;
        if (g_channel[cidx].kind == kind_simple_watch_connect)
          evt |= EPOLLOUT;
        else
          {
          if (channel_get_tx_queue_len(get_llid(cidx)))
            {
            if (!g_channel[cidx].red_to_stop_reading)
              evt |= EPOLLIN;
            if (!g_channel[cidx].red_to_stop_writing)
              evt |= EPOLLOUT;
            }
          else 
            {
            if (!g_channel[cidx].red_to_stop_reading)
              evt |= EPOLLIN;
            }
          }
        apply_epoll_ctl(llid, cidx, evt);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_current_max_channels(void)
{
return current_max_channels;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_delete(int llid)
{
  int cidx, fd;
  if (msg_exist_channel(llid))
    {
    cidx = get_cidx(llid);
    fd = g_channel[cidx].fd;
    epoll_ctl(g_epfd, EPOLL_CTL_DEL, fd, NULL);
    
    if (g_channel[cidx].kind != kind_simple_watch_connect)
      util_free_fd(fd);
    release_llid_cidx(llid, cidx);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int channel_create(int fd, int kind, char *little_name, 
                   t_fd_event rx_cb, t_fd_event tx_cb, t_fd_error err_cb)
{
  int llid = 0, cidx;
  if (fd < 0)
    KERR("ERROR NEGATIVE fd");
  else if (g_channel[fd].fd == fd)
    KERR("ERROR fd exists");
  else
    {
    alloc_llid_cidx(&llid, &cidx, fd);
    if (cidx != fd+1)
      KOUT(" %d %d ", cidx, fd);
    g_channel[cidx].kind       = kind;
    g_channel[cidx].rx_cb      = rx_cb;
    g_channel[cidx].tx_cb      = tx_cb;
    g_channel[cidx].err_cb     = err_cb;
    g_channel[cidx].epev.events = 0;
    g_channel[cidx].epev.data.fd = fd;
    fd_check(fd, __LINE__, little_name);
    if (epoll_ctl(g_epfd, EPOLL_CTL_ADD, fd, &(g_channel[cidx].epev)))
      {
      KERR("ERROR epoll_ctl %s %d", little_name, errno);
      release_llid_cidx(llid, cidx);
      llid = 0;
      }
    else
      strncpy(g_channel[cidx].little_name, little_name, MAX_NAME_LEN-1);
    }
  return (llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_counter_select_speed(void)
{
  int result = counter_select_speed;
  counter_select_speed = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_beat_set (t_heartbeat_cb beat)
{
  channel_beat = beat;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void waked_count_update(int nb, struct epoll_event *events)
{
  int  k, cidx, llid;
  uint32_t evt;
  for(k=0; k<nb; k++)
    {
    if (events[k].data.fd >= 0)
      {
      cidx = events[k].data.fd + 1;
      llid = get_llid(cidx);
      if ((g_channel[cidx].fd != -1) && (llid > 0))
        {
        evt = events[k].events;
        if (evt)
          {
          if (evt & EPOLLIN)
            g_channel[cidx].waked_count_in += 1;
          if (evt & EPOLLOUT)
            g_channel[cidx].waked_count_out += 1;
          if ((evt & EPOLLERR) || (evt & EPOLLHUP))
            g_channel[cidx].waked_count_err += 1;
          }
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int handle_io_on_fd(int nb, struct epoll_event *events)
{
  int llid, cidx, len, k, result = -1;
  uint32_t evt;

  for(k=0; (!channel_modification_occurence) && (k<nb); k++)
    {
    /*---------------------------------------------------------*/
    if (events[k].data.fd == -1)
      continue;
    cidx = events[k].data.fd + 1;
    llid = get_llid(cidx);
    if ((g_channel[cidx].fd == -1) || (llid == 0))
      continue;
    /*---------------------------------------------------------*/
    evt = events[k].events;
    if (evt & EPOLLERR)
      {
      g_channel[cidx].err_cb(llid, errno, 5312);
      channel_delete(llid);
      continue;
      }
    if (evt & EPOLLHUP)
      {
      g_channel[cidx].epollhup_count += 1;
      if (g_channel[cidx].epollhup_count > 200)
        {
        g_channel[cidx].err_cb(llid, errno, 5112);
        channel_delete(llid);
        }
      continue;
      }
    g_channel[cidx].epollhup_count = 0;
    }
  for(k=0; (!channel_modification_occurence) && (k<nb); k++) 
    {
    /*---------------------------------------------------------*/
    if (events[k].data.fd == -1)
      continue;
    cidx = events[k].data.fd + 1;
    llid = get_llid(cidx);
    if ((g_channel[cidx].fd == -1) || (llid == 0))
      continue;
    /*---------------------------------------------------------*/
    evt = events[k].events;
    if ((g_channel[cidx].kind == kind_simple_watch_connect)  ||
        (g_channel[cidx].kind == kind_simple_watch)          ||
        (g_channel[cidx].kind == kind_server)                || 
        (g_channel[cidx].kind == kind_server_doors)          || 
        (g_channel[cidx].kind == kind_server_proxy_traf_unix) || 
        (g_channel[cidx].kind == kind_server_proxy_traf_inet) || 
        (g_channel[cidx].kind == kind_server_proxy_sig)      || 
        (g_channel[cidx].kind == kind_client))
      {
      if (evt & EPOLLOUT)
        {
        len = g_channel[cidx].tx_cb(llid, g_channel[cidx].fd);
        if (len < 0)
          {
          if (g_channel[cidx].fd != -1)
            {
            g_channel[cidx].err_cb(llid, errno, 512);
            channel_delete(llid);
            }
          }
        else
          {
          if (len == 0)
            {
            g_channel[cidx].zero_count_write += 1;
            g_channel[cidx].zero_count_write_total += 1;
            }
          else  
            {
            g_channel[cidx].zero_count_write = 0;
            g_channel[cidx].zero_count_write_total = 0;
            }
          if (g_channel[cidx].zero_count_write > 10)
            {
            g_channel[cidx].red_to_stop_writing = 1;
            g_channel[cidx].must_reactivate_write = 1;
            KERR("WARNING WRITE %d %d %d %d", llid, cidx, g_channel[cidx].fd,
                                            g_channel[cidx].kind);
            }
          if (g_channel[cidx].zero_count_write_total > 5000)
            {
            KERR("ERROR WRITE %d %d %d %d", llid, cidx, g_channel[cidx].fd,
                                            g_channel[cidx].kind);
            g_channel[cidx].err_cb(llid, errno, 1532);
            channel_delete(llid);
            }

          g_channel[cidx].out_bytes += len;
          channel_total_send += len;
          result = 0;
          }
        }
      }
    }

  for(k=0; (!channel_modification_occurence) && (k<nb); k++) 
    {
    /*---------------------------------------------------------*/
    if (events[k].data.fd == -1)
      continue;
    cidx = events[k].data.fd + 1;
    llid = get_llid(cidx);
    if ((g_channel[cidx].fd == -1) || (llid == 0))
      continue;
    /*---------------------------------------------------------*/
    evt = events[k].events;
    if ((g_channel[cidx].kind == kind_simple_watch)          ||
        (g_channel[cidx].kind == kind_server)                || 
        (g_channel[cidx].kind == kind_server_doors)          || 
        (g_channel[cidx].kind == kind_server_proxy_traf_unix) || 
        (g_channel[cidx].kind == kind_server_proxy_traf_inet) || 
        (g_channel[cidx].kind == kind_server_proxy_sig)      || 
        (g_channel[cidx].kind == kind_client))
      {
      if (evt & EPOLLIN)
        {
        len = g_channel[cidx].rx_cb(llid, g_channel[cidx].fd);
        if (len < 0)
          {
          if (g_channel[cidx].fd != -1)
            {
            g_channel[cidx].err_cb(llid, errno, 513);
            channel_delete(llid);
            }
          }
        else
          {
          if (len == 0)
            {
            g_channel[cidx].zero_count_read += 1;
            g_channel[cidx].zero_count_read_total += 1;
            } 
          else  
            {
            g_channel[cidx].zero_count_read = 0;
            g_channel[cidx].zero_count_read_total = 0;
            }
          if (g_channel[cidx].zero_count_read > 10)
            {
            g_channel[cidx].zero_count_read = 0;
            g_channel[cidx].red_to_stop_reading = 1;
            g_channel[cidx].must_reactivate_read = 1;
            }
          if (g_channel[cidx].zero_count_read_total > 5000)
            {
            g_channel[cidx].zero_count_read_total = 0;
            if (g_channel[cidx].kind == kind_server_proxy_traf_inet)
              {
              g_channel[cidx].err_cb(llid, errno, 1533);
              channel_delete(llid);
              }
            }
          g_channel[cidx].in_bytes += len;
          channel_total_recv += len;
          result = 0;
          }
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_loop(int once)
{
  int cidx, k, result, pb = 0;
  uint32_t evt;
  static struct epoll_event events[MAX_EPOLL_EVENTS_PER_RUN];
  my_gettimeofday(&last_heartbeat);
  for(;;)
    {
    if ((current_max_channels<=0) || 
        (current_max_channels >= MAX_SELECT_CHANNELS))
      KOUT(" %d  %d", current_max_channels, MAX_SELECT_CHANNELS); 
    g_new_second_arrival_count += 1;
    prepare_rx_tx_events();
    if (g_fct_before_epoll)
      g_fct_before_epoll(g_epfd);
    memset(events, 0, MAX_EPOLL_EVENTS_PER_RUN * sizeof(struct epoll_event));
    result = epoll_wait(g_epfd, events, 
                        MAX_EPOLL_EVENTS_PER_RUN, 
                        glob_heartbeat_ms_timeout);  
    if ( result < 0 )
      {
if (g_new_second_arrival_count > PERSEC_COUNTER_INSANE)
KERR("ERROR");
      if (errno == EINTR)
        {
        g_count_eintr += 1;
        if (g_count_eintr > 1000)
          KOUT(" errno: %d\n ", errno);
        continue;
        }
      KOUT(" errno: %d\n ", errno);
      }
    g_count_eintr = 0;
    if (result == 0)
      {
if (g_new_second_arrival_count > PERSEC_COUNTER_INSANE)
KERR("ERROR");
      heartbeat_mngt(0);
      continue;
      }
    else
      pb = heartbeat_mngt(1);
if (g_new_second_arrival_count > PERSEC_COUNTER_INSANE)
KERR("ERROR %d %d", slipery_select, counter_select_speed);
    slipery_select++;
    counter_select_speed++;
    waked_count_update(result, events);

    channel_modification_occurence = 0;
    if (!handle_io_on_fd(result, events))
      slipery_select = 0;

    if (g_fct_after_epoll)
      {
      if (g_fct_after_epoll(result, events))
        slipery_select = 0;
      }

    if (slipery_select >= 1000)
        KERR( "WARNING slip:%d", slipery_select);
    if (pb || (slipery_select >= 2000))
      {
      for(k=0; k<result; k++) 
        {
        cidx = events[k].data.fd + 1;
        evt = events[k].events;
        KERR( "pb: %d slip:%d %d %d %d %d %08X %d", pb, slipery_select,
                               g_channel[cidx].kind, cidx, (evt & EPOLLOUT),
                               (evt & EPOLLIN), evt, get_llid(cidx));
        }
      KOUT(" %d %d", result, channel_modification_occurence);
      }
    if (once)
      break;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_add_epoll_hooks(t_fct_before_epoll beopll,
                             t_fct_after_epoll aepoll)
{
  g_fct_before_epoll = beopll;
  g_fct_after_epoll  = aepoll;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int channel_get_epfd(void)
{
  return g_epfd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void channel_init(void)
{
  int i;
  g_count_eintr = 0;
  g_new_second_arrival_count = 0;
  my_gettimeofday(&last_heartbeat);
  for (i=0; i<MAX_SELECT_CHANNELS; i++)
    {
    zero_channel(i);
    cidx_2_llid[i] = 0;
    }
  for (i=0; i<CLOWNIX_MAX_CHANNELS; i++)
    llid_2_cidx[i] = 0;
  channel_total_recv = 0;
  channel_total_send = 0;
  current_max_channels = 0;
  current_nb_channels = 0;
  glob_heartbeat_ms_timeout = HEARTBEAT_DEFAULT_TIMEOUT;
  channel_beat = NULL;
  llid_pool_init();
  clownix_timer_init();
  g_epfd = epoll_create(EPOLL_CLOEXEC);
  g_fct_before_epoll = NULL;
  g_fct_after_epoll = NULL;
}
/*---------------------------------------------------------------------------*/


