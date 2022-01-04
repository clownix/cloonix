/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <errno.h>


#include "mdl.h"
#include "wrap.h"
#include "io_clownix.h"
#include "doorways_sock.h"
#include "epoll_hooks.h"

#include "simu_tx.h"

#define MAX_SIMU_RX_LEN 1000000

/*****************************************************************************/
typedef struct t_simu_fd
{
  int sock_fd;
  int type;
  struct epoll_event *sock_fd_epev;
} t_simu_fd;
/*--------------------------------------------------------------------------*/

static t_fd_event g_conn_rx;
static int  g_tmp_type;

static t_simu_fd g_simu_fd[CLOWNIX_MAX_CHANNELS];
static struct timeval g_last_heartbeat_tv;
static int g_epfd;
static t_heartbeat_cb g_heartbeat;
static t_fct_before_epoll g_fct_before_epoll;
static t_fct_after_epoll g_fct_after_epoll;
static t_doorways_rx g_doorways_rx;
static t_doorways_end g_doorways_end;
static char g_buf[MAX_SIMU_RX_LEN];

/****************************************************************************/
static void sock_fd_read(int type, int llid, int fd)
{
  int len;
  len = read(fd, g_buf, MAX_SIMU_RX_LEN);
  if (len == 0)
    {
    KERR("read len is 0");
    g_doorways_end(llid);
    }
  else if (len < 0)
    {
    if ((errno != EAGAIN) && (errno != EINTR))
      {
      KERR("read error %s", strerror(errno));
      g_doorways_end(llid);
      }
    }
  else
    {
    g_doorways_rx(llid, llid+10, type, doors_val_xwy, len, g_buf);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int heartbeat_10th_second(struct timeval *cur)
{
  int result = 0;
  struct timeval *last = &g_last_heartbeat_tv;
  int delta = 0;
  delta = (cur->tv_sec - last->tv_sec)*1000;
  delta += cur->tv_usec/1000;
  delta -= last->tv_usec/1000;
  if (delta > 10)
    {
    gettimeofday(&g_last_heartbeat_tv, NULL);
    result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int msg_mngt_get_epfd(void)
{
  return g_epfd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void channel_add_epoll_hooks(t_fct_before_epoll bep, t_fct_after_epoll aep)
{
  g_fct_before_epoll = bep;
  g_fct_after_epoll = aep;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void time_doorways_rx(void *data)
{
  unsigned long ul_llid = (unsigned long) data;
  int llid = (int) ul_llid;
  g_doorways_rx(llid, llid+20, g_tmp_type, doors_val_link_ok, 3, "OK");
  g_tmp_type = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int doorways_tx(int llid, int tid, int type, int val, int len, char *buf)
{
  t_simu_fd *sfd;
  int sock_fd;
  unsigned long ul_llid = (unsigned long) llid;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid); 
  sfd = &(g_simu_fd[llid]);
  sock_fd = sfd->sock_fd;
  if (sock_fd == -1)
    KOUT("%d", llid); 
  if (type != sfd->type) 
    KOUT("%d %d", type, sfd->type);
  if (val == doors_val_init_link)
    clownix_timeout_add(1, time_doorways_rx, (void *) ul_llid, NULL, NULL);
  else if (val == doors_val_xwy)
    simu_tx_send(llid, len, buf);
  else
    KOUT("%d", val);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_mngt_loop(void)
{
  int i, j, fd, type, result;
  struct epoll_event events[MAX_EPOLL_EVENTS];
  uint32_t evts;
  struct timeval tv;
  t_simu_fd *sfd;

  for(;;)
    {
    for (i=1; i<CLOWNIX_MAX_CHANNELS; i++)
      {
      if (g_simu_fd[i].sock_fd != -1)
        {
        sfd = &(g_simu_fd[i]);
        sfd->sock_fd_epev->events = EPOLLIN | EPOLLRDHUP;
        simu_tx_fdset(0 + i);
        if (epoll_ctl(g_epfd, EPOLL_CTL_MOD, sfd->sock_fd, sfd->sock_fd_epev))
          KOUT(" ");
        }
      }
    if (g_fct_before_epoll)
      g_fct_before_epoll(g_epfd);
    memset(events, 0, MAX_EPOLL_EVENTS * sizeof(struct epoll_event));
    result = epoll_wait(g_epfd, events, MAX_EPOLL_EVENTS, 10);
    if (result < 0)
      {
      if (errno != EINTR)
        KOUT("%s\n ", strerror(errno));
      }
    if (g_fct_after_epoll)
      g_fct_after_epoll(result, events);
    for(i=0; i<result; i++)
      {
      fd = events[i].data.fd;
      evts = events[i].events;
      for (j=1; j<CLOWNIX_MAX_CHANNELS; j++)
        {
        if (g_simu_fd[j].sock_fd == fd)
          {
          type = g_simu_fd[j].type;
          if (evts & EPOLLIN)
            sock_fd_read(type, j, fd);
          if (evts & EPOLLOUT)
            simu_tx_fdiset(j);
          }
        }
      }
    gettimeofday(&tv, NULL);
    if (heartbeat_10th_second(&tv))
      {
      clownix_timer_beat();
      g_heartbeat(100);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void doorways_sock_client_inet_delete(int llid)
{
  t_simu_fd *sfd;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  sfd = &(g_simu_fd[llid]);
  if (sfd->sock_fd != -1)
    {
    simu_tx_close(llid);
    wrap_epoll_event_free(g_epfd, sfd->sock_fd_epev);
    wrap_close(sfd->sock_fd, __FUNCTION__);
    sfd->sock_fd = -1;
    sfd->sock_fd_epev = NULL;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void msg_mngt_heartbeat_init (t_heartbeat_cb heartbeat)
{
  g_heartbeat = heartbeat;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int doorways_sock_client_inet_end(int type,int llid,int sock_fd,char *passwd,
                                   t_doorways_end cb_end, t_doorways_rx cb_rx)
{
  t_simu_fd *sfd;
  unsigned long ul_llid;
  g_doorways_rx = cb_rx;
  g_doorways_end = cb_end;
  sfd = &(g_simu_fd[llid-20]);
  sfd->sock_fd = sock_fd-20;
  sfd->type = type;
  sfd->sock_fd_epev = wrap_epoll_event_alloc(g_epfd, sfd->sock_fd, 7);
  simu_tx_open(llid-20, sfd->sock_fd, sfd->sock_fd_epev);
  ul_llid = (unsigned long) llid;
  if (g_tmp_type)
    KOUT("%d", g_tmp_type);
  g_tmp_type = type;
  return (llid-20);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void time_connect(void *data)
{
  unsigned long ul_fd = (unsigned long) data;
  int llid, fd = (int) ul_fd; 
  llid = fd + 10;
  g_conn_rx(llid+20, fd+20);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int doorways_sock_client_inet_start(uint32_t ip, int port, t_fd_event conn_rx)
{
  int fd;
  unsigned long ul_fd;
  fd = wrap_socket_connect_inet(ip, port, fd_type_cli, "connect");
  if (fd == -1)
    KOUT("Cannot connect to: %s  port:%d", mdl_int_to_ip_string(ip), port);
  ul_fd = (unsigned long) fd;
  g_conn_rx = conn_rx; 
  clownix_timeout_add(1, time_connect, (void *) ul_fd, NULL, NULL);
  return (fd + 30);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doorways_sock_init(void)
{
  int i;
  gettimeofday(&g_last_heartbeat_tv, NULL);
  g_epfd = wrap_epoll_create(fd_type_cli, __FUNCTION__);
  g_fct_before_epoll = NULL;
  g_fct_after_epoll = NULL;
  g_tmp_type = 0;
  for (i=1; i<CLOWNIX_MAX_CHANNELS; i++)
    {
    g_simu_fd[i].sock_fd = -1;
    g_simu_fd[i].sock_fd_epev = NULL;
    }
  simu_tx_init();
}
/*---------------------------------------------------------------------------*/
