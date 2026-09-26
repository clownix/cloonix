/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _GNU_SOURCE
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
#include <sys/socket.h>
#include <sys/resource.h>

#include "io_clownix.h"
#include "channel.h"
#include "util_sock.h"
#include "msg_layer.h"
#include "stall_detect.h"

#define HEARTBEAT_DEFAULT_TIMEOUT 5
#define MAX_EPOLL_EVENTS_PER_RUN 50
#define PERSEC_COUNTER_INSANE 3000000

static unsigned long g_new_second_arrival_count;
static struct timeval last_heartbeat;
static int channel_modification_occurence = 0;
static int g_epfd;

static int g_count_eintr;

typedef struct t_io_channel
  {
  int fd;
  int epollhup_count;
  int zero_count_read_total;
  int zero_count_read;
  int zero_count_write_total;
  int zero_count_write;
  int must_reactivate_read;
  int must_reactivate_write;
  int red_to_stop_reading;
  int rdhup_seen;
  int red_to_stop_writing;
  t_fd_event rx_cb;
  t_fd_event tx_cb;
  t_fd_error err_cb;
  int kind;
  char little_name[MAX_NAME_LEN];
  struct epoll_event epev;
  } t_io_channel;

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

static void zero_channel(int idx)
{
  memset(&(g_channel[idx]), 0, sizeof(t_io_channel));
  g_channel[idx].fd = -1;
  g_channel[idx].epev.data.fd = -1;
}

static void my_gettimeofday(struct timeval *tv)
{ 
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  tv->tv_sec = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec/1000;
}

int get_fd_with_cidx(int cidx)
{
  return g_channel[cidx].fd; 
}

int get_clownix_channels_nb(void)
{
  return current_max_channels;
}

static int get_llid(int cidx)
{
return (cidx_2_llid[cidx]);
}

int channel_get_llid_with_cidx(int cidx)
{
  return get_llid(cidx);
}

static int get_cidx(int llid)
{
return (llid_2_cidx[llid]);
}

char *channel_get_little_name(int llid)
{
  int cidx = get_cidx(llid);
  return(g_channel[cidx].little_name);
}

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

static void set_llid_cidx(int llid, int cidx)
{
  cidx_2_llid[cidx] = llid;
  llid_2_cidx[llid] = cidx;
  current_nb_channels++;
}

static void unset_llid_cidx(int llid, int cidx)
{
  cidx_2_llid[cidx] = 0;
  llid_2_cidx[llid] = 0;
  current_nb_channels--;
}

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

static void release_cidx(int cidx)
{
  int i;
  channel_modification_occurence = 1;
  zero_channel(cidx);
  for (i=0; i<MAX_SELECT_CHANNELS; i++)
    if (g_channel[i].fd != -1)
      current_max_channels = i;
}

static void llid_pool_init(void)
{
  int i;
  for(i = 0; i < CLOWNIX_MAX_CHANNELS; i++)
    llid_pool.fifo_free_index[i] = i;
  llid_pool.read_idx = 1;
  llid_pool.write_idx =  CLOWNIX_MAX_CHANNELS - 1;
}

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

static void release_llid_cidx(int llid, int cidx)
{
  release_cidx(cidx);
  llid_pool_release(llid);
  unset_llid_cidx(llid, cidx);
}

static int delta_beat(struct timeval *cur, struct timeval *last)
{
  int delta = 0;
  delta = (cur->tv_sec - last->tv_sec)*10000;
  delta += cur->tv_usec/100;
  delta -= last->tv_usec/100;
  return delta;
}

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

void channel_heartbeat_ms_set (int heartbeat_ms)
{
  glob_heartbeat_ms_timeout = heartbeat_ms;
}

static void new_second_arrival(void)
{
  g_new_second_arrival_count = 0;
}

/* A beat is two heartbeat periods of the monotonic clock, whatever woke the
   loop: counted per wakeup it halved under traffic, and every timeout with
   it. */
int heartbeat_mngt(int type)
{
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
  if (type == 1)
    heart_count++;
  my_gettimeofday(&cur);
  current_time.tv_sec = cur.tv_sec;
  current_time.tv_usec = cur.tv_usec;
  if (cur.tv_sec > last_heartbeat.tv_sec)
    new_second_arrival();
  delta = delta_beat(&cur, &last_heartbeat);
  if (delta >= (glob_heartbeat_ms_timeout*20))
    {
    clownix_timer_beat();
    if (glob_heartbeat_ms_timeout < 50)
      {
      if (heart_count > 60000)
        {
        KERR("%d LONG SELECT", type);
        result = 1;
        }
      }
    heart_count=0;
    /* Stepped by one period so late wakeups do not add up; a loop late by
       more than a period starts again from now, no burst of beats. */
    if (delta >= (glob_heartbeat_ms_timeout*40))
      memcpy(&last_heartbeat, &cur, sizeof(struct timeval));
    else
      {
      last_heartbeat.tv_usec += glob_heartbeat_ms_timeout*2000;
      if (last_heartbeat.tv_usec >= 1000000)
        {
        last_heartbeat.tv_usec -= 1000000;
        last_heartbeat.tv_sec += 1;
        }
      }
    if (channel_beat)
      {
      channel_beat(delta);
      }
    }
  return result;
}

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

unsigned long channel_get_tx_queue_len(int llid)
{
  int cidx = channel_check_llid(llid, __FUNCTION__);
  int result = 0;
  if (cidx != 0) 
    result = get_tot_txq_size(cidx);
  return result;
}

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
        if (g_channel[cidx].err_cb)
          g_channel[cidx].err_cb(llid, errno, 1111);
        if (msg_exist_channel(llid))
          channel_delete(llid);
        }
      else
        KERR(" %s %d ", g_channel[cidx].little_name, errno);
      }
    }
}

static int check_fd_is_valid(int fd)
{
  return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}

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
        if (g_channel[cidx].err_cb)
          g_channel[cidx].err_cb(llid, errno, 514);
        channel_delete(llid);
        }
      else
        {
        evt = 0;
        /* EPOLLRDHUP must be asked for explicitly or a channel is blind to
           its peer going away. */
        if ((g_channel[cidx].kind != kind_server)                 &&
            (g_channel[cidx].kind != kind_server_doors)           &&
            (g_channel[cidx].kind != kind_server_proxy_traf_unix) &&
            (g_channel[cidx].kind != kind_simple_watch)           &&
            (g_channel[cidx].kind != kind_simple_watch_connect))
          {
          if (g_channel[cidx].rx_cb)
            evt |= EPOLLRDHUP;
          else
            {
            /* Counted, so the build is checkable and a channel stuck in setup
               does not go unnoticed. */
            static unsigned long nb_not_ready;
            nb_not_ready += 1;
            if ((nb_not_ready % 5000) == 1)
              KTRC("rdhup needs rx_cb: %lu event registrations skipped "
                   "on channels still being set up", nb_not_ready);
            }
          }
        if (g_channel[cidx].kind == kind_simple_watch_connect)
          evt |= EPOLLOUT;
        else
          {
          if (get_tot_txq_size(cidx))
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

int channel_is_connect_wait(int llid)
{
  int result = 0;
  if (msg_exist_channel(llid))
    result = (g_channel[get_cidx(llid)].kind == kind_simple_watch_connect);
  return result;
}

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
    /* Not inherited across exec */
    if (fcntl(fd, F_SETFD, FD_CLOEXEC))
      KERR("ERROR FD_CLOEXEC on fd %d (%s): %s", fd, little_name,
           strerror(errno));
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

void channel_beat_set (t_heartbeat_cb beat)
{
  channel_beat = beat;
}

static int handle_io_on_fd(int nb, struct epoll_event *events)
{
  int llid, cidx, len, k, result = -1;
  uint32_t evt;

  for(k=0; (!channel_modification_occurence) && (k<nb); k++)
    {
    if (events[k].data.fd == -1)
      continue;
    cidx = events[k].data.fd + 1;
    llid = get_llid(cidx);
    if ((g_channel[cidx].fd == -1) || (llid == 0))
      continue;
    evt = events[k].events;
    if (evt & EPOLLERR)
      {
      int so_err = 0;
      socklen_t so_len = sizeof(so_err);
      getsockopt(g_channel[cidx].fd, SOL_SOCKET, SO_ERROR, &so_err, &so_len);
      /* ENOBUFS: a netlink queue overflowed, messages lost, socket alive */
      if (so_err == ENOBUFS)
        KERR("WARNING llid %d fd %d kind %d: ENOBUFS, the kernel dropped "
             "messages", llid, g_channel[cidx].fd, g_channel[cidx].kind);
      else
        {
        /* a reset or a broken pipe is the peer closing: nominal */
        if ((so_err != ECONNRESET) && (so_err != EPIPE))
          KERR("WARNING llid %d fd %d kind %d: EPOLLERR (%s), closed", llid,
               g_channel[cidx].fd, g_channel[cidx].kind,
               so_err ? strerror(so_err) : "no socket error");
        if (g_channel[cidx].err_cb)
          g_channel[cidx].err_cb(llid, so_err, 5312);
        channel_delete(llid);
        continue;
        }
      }
    if (evt & EPOLLRDHUP)
      {
      /* Only if the channel is ready to be read. */
      if ((g_channel[cidx].red_to_stop_reading) &&
          (g_channel[cidx].rx_cb))
        {
        g_channel[cidx].red_to_stop_reading = 0;
        g_channel[cidx].rdhup_seen += 1;
        if (g_channel[cidx].rdhup_seen == 1)
          KTRC("llid=%d peer closed while reading was stopped, "
               "flow control lifted so the close can be seen", llid);
        }
      }
    if (evt & EPOLLHUP)
      {
      g_channel[cidx].epollhup_count += 1;
      if (g_channel[cidx].epollhup_count > 200)
        {
        KERR("WARNING llid %d fd %d kind %d: EPOLLHUP %d times, closed",
             llid, g_channel[cidx].fd, g_channel[cidx].kind,
             g_channel[cidx].epollhup_count);
        if (g_channel[cidx].err_cb)
          g_channel[cidx].err_cb(llid, errno, 5112);
        channel_delete(llid);
        }
      continue;
      }
    g_channel[cidx].epollhup_count = 0;
    }
  for(k=0; (!channel_modification_occurence) && (k<nb); k++) 
    {
    if (events[k].data.fd == -1)
      continue;
    cidx = events[k].data.fd + 1;
    llid = get_llid(cidx);
    if ((g_channel[cidx].fd == -1) || (llid == 0))
      continue;
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
            if (g_channel[cidx].err_cb)
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
            if (g_channel[cidx].err_cb)
              g_channel[cidx].err_cb(llid, errno, 1532);
            channel_delete(llid);
            }

          result = 0;
          }
        }
      }
    }

  for(k=0; (!channel_modification_occurence) && (k<nb); k++) 
    {
    if (events[k].data.fd == -1)
      continue;
    cidx = events[k].data.fd + 1;
    llid = get_llid(cidx);
    if ((g_channel[cidx].fd == -1) || (llid == 0))
      continue;
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
            if (g_channel[cidx].err_cb)
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
          /* A watch rx_cb must return >0 when it consumed something: eleven
             zeros in a row take the fd out of epoll until the next beat. */
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
              KERR("WARNING llid %d fd %d: 5000 empty reads in a row on a "
                   "proxy inet channel, closed", llid, g_channel[cidx].fd);
              if (g_channel[cidx].err_cb)
                g_channel[cidx].err_cb(llid, errno, 1533);
              channel_delete(llid);
              }
            }
          result = 0;
          }
        }
      }
    }
  return result;
}

/* Where a late loop lost its time, said once per window: waiting for the
   cpu after epoll_wait was due back, or inside one turn of callbacks, and
   then with the cpu of that turn: little cpu is a callback that blocked. */
#define TURN_SLOW_MS   2000
#define TURN_WINDOW_MS 10000
static long long g_turn_window_ms;
static int g_turn_nb, g_wait_sum, g_wait_worst;
static int g_work_sum, g_work_worst, g_work_worst_cpu;
static long g_work_worst_vol, g_work_worst_forced;

static long long turn_cpu_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return ((((long long) ts.tv_sec) * 1000) + (ts.tv_nsec / 1000000));
}

static void turn_account(int wait_ms, int work_ms, int work_cpu_ms,
                         long vol, long forced)
{
  long long now = stall_now_ms();
  g_turn_nb += 1;
  if (wait_ms > 0)
    g_wait_sum += wait_ms;
  if (wait_ms > g_wait_worst)
    g_wait_worst = wait_ms;
  g_work_sum += work_ms;
  if (work_ms > g_work_worst)
    {
    g_work_worst = work_ms;
    g_work_worst_cpu = work_cpu_ms;
    g_work_worst_vol = vol;
    g_work_worst_forced = forced;
    }
  if (g_turn_window_ms == 0)
    g_turn_window_ms = now;
  if (now - g_turn_window_ms < TURN_WINDOW_MS)
    return;
  if ((g_wait_worst > TURN_SLOW_MS) || (g_work_worst > TURN_SLOW_MS))
    KERR("WARNING late loop, %d turns in %lld ms: epoll_wait came back "
         "late by %d ms in all, %d ms at worst (the cpu was not given); "
         "the callbacks took %d ms in all, one turn %d ms at worst with "
         "%d ms of cpu, %ld voluntary and %ld forced switches (voluntary: "
         "it blocked in a call; forced only: preempted and not run again)",
         g_turn_nb, now - g_turn_window_ms, g_wait_sum, g_wait_worst,
         g_work_sum, g_work_worst, g_work_worst_cpu, g_work_worst_vol,
         g_work_worst_forced);
  g_turn_window_ms = now;
  g_turn_nb = 0;
  g_wait_sum = 0;
  g_wait_worst = 0;
  g_work_sum = 0;
  g_work_worst = 0;
  g_work_worst_cpu = 0;
}

void channel_loop(void)
{
  long long t_call, t_back = 0, cpu_back = 0;
  struct rusage ru, ru_back;
  int wait_ms = 0;
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
    memset(events, 0, MAX_EPOLL_EVENTS_PER_RUN * sizeof(struct epoll_event));
    t_call = stall_now_ms();
    if (t_back)
      {
      getrusage(RUSAGE_THREAD, &ru);
      turn_account(wait_ms, (int) (t_call - t_back), 
                   (int) (turn_cpu_ms() - cpu_back),
                   ru.ru_nvcsw - ru_back.ru_nvcsw,
                   ru.ru_nivcsw - ru_back.ru_nivcsw);
      }
    result = epoll_wait(g_epfd, events, 
                        MAX_EPOLL_EVENTS_PER_RUN, 
                        glob_heartbeat_ms_timeout);  
    t_back = stall_now_ms();
    cpu_back = turn_cpu_ms();
    getrusage(RUSAGE_THREAD, &ru_back);
    wait_ms = (int) (t_back - t_call) - glob_heartbeat_ms_timeout;
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

    channel_modification_occurence = 0;
    if (!handle_io_on_fd(result, events))
      slipery_select = 0;

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
    }
}

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
  current_max_channels = 0;
  current_nb_channels = 0;
  glob_heartbeat_ms_timeout = HEARTBEAT_DEFAULT_TIMEOUT;
  channel_beat = NULL;
  llid_pool_init();
  clownix_timer_init();
  g_epfd = epoll_create1(EPOLL_CLOEXEC);
  if (g_epfd < 0)
    KOUT("ERROR epoll_create1: %s", strerror(errno));
}

