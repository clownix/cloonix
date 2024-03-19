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
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>

#include "mdl.h"
#include "wrap.h"
#include "debug.h"
#include "low_write.h"
#include "thread_x11.h"
#include "thread_spy.h"
#include "dialog_thread.h"


/*****************************************************************************/
typedef struct t_x11
{
  int sock_fd_ass;
  int x11_fd;
  int srv_idx;
  int cli_idx;
  int epfd;
  int diag_thread_fd;
  int diag_main_fd;
  int thread_on;
  int thread_waiting;
  int thread_terminating;
  int thread_terminated;
  uint32_t randid;
  int server_side;
  int  ptx;
  int  prx;
  long long btx;
  long long brx;
  t_msg *first_x11_msg;
  struct epoll_event *epev_x11_fd;
  struct epoll_event *epev_soc_fd;
  struct epoll_event *epev_diag_thread_fd;
  pthread_t thread_x11;
} t_x11;


static t_x11 *g_x11[MAX_FD_NUM];

/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void dialog_err(int sock_fd, int srv_idx, int cli_idx, char *buf)
{
  XOUT(" ");
}
/*--------------------------------------------------------------------------*/

static void dialog_killed(int sock_fd, int srv_idx, int cli_idx, char *buf)
{
  XOUT(" ");
}
/*--------------------------------------------------------------------------*/
static void dialog_stats(int sock_fd, int srv_idx, int cli_idx,
                         int txpkts, long long txbytes,
                         int rxpkts, long long rxbytes, char *buf)

{
  XOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void dialog_wake(int sock_fd, int srv_idx, int cli_idx, char *buf)
{
  t_x11 *x11;
  if ((sock_fd < 0) || (sock_fd >= MAX_FD_NUM))
    XOUT("%d", sock_fd);
  if (!(g_x11[sock_fd]))
    XERR("%d", sock_fd);
  else
    {
    x11 = g_x11[sock_fd];
    dialog_send_stats(x11->diag_thread_fd, x11->sock_fd_ass,
                      x11->srv_idx, x11->cli_idx,
                      x11->ptx, x11->btx, x11->prx, x11->brx);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void terminate_thread(t_x11 *x11, int line, int fd)
{
//  if (line)
//    XERR("LINE:%d (%d-%d) fd:%d", line, x11->srv_idx, x11->cli_idx, fd);
  x11->thread_on = 0;
  wrap_nonnonblock(x11->diag_thread_fd);
  dialog_send_killed(x11->diag_thread_fd, x11->sock_fd_ass,
                   x11->srv_idx, x11->cli_idx);
  dialog_tx_ready(x11->diag_thread_fd);
  wrap_nonblock(x11->diag_thread_fd);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void epev_soc_fd_set(t_x11 *x11)
{
  x11->epev_soc_fd->events = EPOLLIN | EPOLLRDHUP;
  if (low_write_not_empty(x11->sock_fd_ass))
    x11->epev_soc_fd->events |= EPOLLOUT;
  if (epoll_ctl(x11->epfd, EPOLL_CTL_MOD, x11->sock_fd_ass, x11->epev_soc_fd))
    terminate_thread(x11, __LINE__, x11->sock_fd_ass);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void epev_x11_fd_set(t_x11 *x11)
{
  x11->epev_x11_fd->events = EPOLLRDHUP;
  if (low_write_not_empty(x11->x11_fd))
    x11->epev_x11_fd->events |= EPOLLOUT;
  else
    {
    if (!low_write_not_empty(x11->sock_fd_ass))
      x11->epev_x11_fd->events |= EPOLLIN;
    }
  if (epoll_ctl(x11->epfd, EPOLL_CTL_MOD, x11->x11_fd, x11->epev_x11_fd))
    terminate_thread(x11, __LINE__, x11->x11_fd);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void epev_diag_thread_fd_set(t_x11 *x11)
{
  x11->epev_diag_thread_fd->events = EPOLLIN | EPOLLRDHUP;
  if (dialog_tx_queue_non_empty(x11->diag_thread_fd))
    x11->epev_diag_thread_fd->events |= EPOLLOUT;
  if (epoll_ctl(x11->epfd, EPOLL_CTL_MOD,
                x11->diag_thread_fd, x11->epev_diag_thread_fd))
    terminate_thread(x11, __LINE__, x11->diag_thread_fd);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void write_to_soc(t_x11 *x11, t_msg *msg)
{
  if (low_write_raw(x11->sock_fd_ass, msg, 0))
    terminate_thread(x11, __LINE__, x11->sock_fd_ass);
  else
    {
    x11->ptx += 1;
    x11->btx += msg->len;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int write_to_x11(t_x11 *x11, t_msg *msg)
{
  int wlen, result = -1;
  if (!low_write_raw(x11->x11_fd, msg, 0))
    {
    x11->prx += 1;
    x11->brx += msg->len;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int read_from_sock(t_x11 *x11)
{
  int result = -1;
  int len = g_msg_header_len + MAX_X11_MSG_LEN;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg, x11->randid, thread_type_x11,
                      fd_type_x11_rd_x11, x11->srv_idx, x11->cli_idx);
  len = wrap_read_x11_rd_soc(x11->sock_fd_ass, msg->buf, MAX_X11_MSG_LEN);
  if (len == 0)
    {
    XERR(" ");
    wrap_free(msg, __LINE__);
    }
  else if (len < 0)
    {
    if (errno != EINTR && errno != EAGAIN)
      XERR("%s", strerror(errno));
    else
      result = 0;
    wrap_free(msg, __LINE__);
    }
  else
    {
    msg->len = len;
    result = write_to_x11(x11, msg);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int read_from_x11(t_x11 *x11)
{
  int result = -1;
  int len = g_msg_header_len + MAX_X11_MSG_LEN;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg, x11->randid, thread_type_x11,
                      fd_type_x11_rd_x11, x11->srv_idx, x11->cli_idx);
  len = wrap_read_x11_rd_x11(x11->x11_fd, msg->buf, MAX_X11_MSG_LEN);
  if (len == 0)
    {
    XERR("%d %d X11 READ 0 READ READ 0 TOT:%lld",
         x11->sock_fd_ass,x11->x11_fd, x11->btx);
    terminate_thread(x11, 0, x11->x11_fd);
    wrap_free(msg, __LINE__);
    }
  else if (len < 0)
    {
    XERR("%s", strerror(errno));
    terminate_thread(x11, 0, x11->x11_fd);
    wrap_free(msg, __LINE__);
    }
  else
    {
    msg->len = len;
    write_to_soc(x11, msg);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void *thread_x11(void *arg)
{
  int i, fd, result, fd_ass;
  uint32_t evts;
  struct epoll_event events[MAX_EPOLL_EVENTS];
  t_x11 *x11 = (t_x11 *) arg;
  if (!x11)
    XOUT(" ");
  fd_ass = x11->sock_fd_ass;
  thread_spy_add(x11->sock_fd_ass, x11->x11_fd, x11->epfd, thread_type_x11);
  x11->epev_x11_fd = wrap_epoll_event_alloc(x11->epfd, x11->x11_fd, 4);
  x11->epev_soc_fd = wrap_epoll_event_alloc(x11->epfd, x11->sock_fd_ass, 5);
  mdl_open(x11->sock_fd_ass, fd_type_x11, wrap_write_x11_soc, wrap_read_kout);
  mdl_open(x11->x11_fd, fd_type_x11, wrap_write_x11_x11, wrap_read_kout);
  while(x11->thread_waiting)
    usleep(10000);
  if (x11->first_x11_msg)
    {
    write_to_soc(x11, x11->first_x11_msg);
    x11->first_x11_msg = NULL;
    }
  while (x11->thread_on)
    {
    epev_x11_fd_set(x11);
    epev_soc_fd_set(x11);
    epev_diag_thread_fd_set(x11);
    memset(events, 0, MAX_EPOLL_EVENTS * sizeof(struct epoll_event));
    result = epoll_wait(x11->epfd, events, MAX_EPOLL_EVENTS, 10);
    if (result < 0)
      {
      if (errno != EINTR)
        XOUT("%s\n ", strerror(errno));
      }
    for(i=0; (x11->thread_on) && (i<result); i++)
      {
      fd = events[i].data.fd;
      evts = events[i].events;

      if (evts & EPOLLRDHUP)
        terminate_thread(x11, __LINE__, fd);
      else if (evts & EPOLLHUP)
        terminate_thread(x11, __LINE__, fd);
      else if (evts & EPOLLERR)
        terminate_thread(x11, __LINE__, fd);
      else
        {
        if (evts & (~(EPOLLIN | EPOLLOUT)))
          {
          XERR("%X", evts);
          terminate_thread(x11, __LINE__, fd);
          }
        else if (fd == x11->x11_fd)
          {
          if (evts & EPOLLIN)
            {
            if (read_from_x11(x11))
              terminate_thread(x11, __LINE__, fd);
            else
              {
              if (low_write_fd(x11->sock_fd_ass))
                terminate_thread(x11, __LINE__, fd);
              }
            }
          if ((x11->thread_on) && (evts & EPOLLOUT))
            {
            if (low_write_fd(fd))
              terminate_thread(x11, __LINE__, fd);
            }
          }
        else if (fd == x11->sock_fd_ass) 
          {
          if (evts & EPOLLIN)
            {
            if (read_from_sock(x11))
              terminate_thread(x11, __LINE__, fd);
            else
              {
              if (low_write_fd(x11->x11_fd))
                terminate_thread(x11, __LINE__, fd);
              }
            }
          if ((x11->thread_on) && (evts & EPOLLOUT))
            {
            if (low_write_fd(fd))
              terminate_thread(x11, __LINE__, fd);
            }
          }
        else if (fd == x11->diag_thread_fd) 
          {
          if (evts & EPOLLIN)
            {
            if (dialog_recv_fd(fd, x11->sock_fd_ass,
                               x11->srv_idx, x11->cli_idx,
                               dialog_err, dialog_wake,
                               dialog_killed, dialog_stats))
              terminate_thread(x11, __LINE__, fd);
            }
          if (evts & EPOLLOUT)
            {
            if (dialog_tx_ready(fd))
              terminate_thread(x11, __LINE__, fd);
            }
          }
        }
      }
    }
  while(x11->thread_terminating)
    usleep(10000);
  mdl_close(x11->sock_fd_ass);
  mdl_close(x11->x11_fd);
  wrap_epoll_event_free(x11->epfd, x11->epev_x11_fd);
  wrap_epoll_event_free(x11->epfd, x11->epev_soc_fd);
  wrap_close(x11->epfd, __FUNCTION__);
  wrap_close(x11->x11_fd, __FUNCTION__);
  wrap_close(x11->sock_fd_ass, __FUNCTION__);
  thread_spy_del(x11->sock_fd_ass, thread_type_x11);
  x11->thread_terminated = 1;
  return NULL;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int thread_x11_open(uint32_t randid, int server_side, int sock_fd_ass,
                    int x11_fd, int srv_idx, int cli_idx, int epfd,
                    int diag_thread_fd, int diag_main_fd, t_msg *first_x11_msg)
{
  int result = -1;
  t_x11 *x11;
  if ((sock_fd_ass < 0) || (sock_fd_ass >= MAX_FD_NUM))
    XOUT("%d", sock_fd_ass);
  if ((x11_fd < 0) || (x11_fd >= MAX_FD_NUM))
    XOUT("%d", x11_fd);
  if (g_x11[sock_fd_ass])
    XOUT("%d", sock_fd_ass);
  x11 = (t_x11 *) wrap_malloc(sizeof(t_x11));
  memset(x11, 0, sizeof(t_x11));
  x11->sock_fd_ass        = sock_fd_ass;
  x11->x11_fd             = x11_fd;
  x11->srv_idx            = srv_idx;
  x11->cli_idx            = cli_idx;
  x11->server_side        = server_side;
  x11->randid             = randid;
  x11->epfd               = epfd;
  x11->diag_thread_fd      = diag_thread_fd;
  x11->diag_main_fd       = diag_main_fd;
  x11->thread_on = 1;
  x11->thread_waiting = 1;
  x11->thread_terminating = 1;
  x11->thread_terminated = 0;
  x11->first_x11_msg = first_x11_msg;

  x11->epev_diag_thread_fd = wrap_epoll_event_alloc(x11->epfd,
                                                    x11->diag_thread_fd, 6);
  dialog_open(x11->diag_thread_fd, wrap_write_dialog_thread,
                                    wrap_read_dialog_thread);
  dialog_open(x11->diag_main_fd, wrap_write_dialog_thread,
                                 wrap_read_dialog_thread);
  g_x11[sock_fd_ass] = x11;
  if (pthread_create(&x11->thread_x11, NULL, thread_x11, (void *) x11) != 0)
    {
    XERR("sock:%d srv:%d cli:%d",sock_fd_ass, x11->srv_idx, x11->cli_idx);
    g_x11[sock_fd_ass] = NULL;
    wrap_free(x11, __LINE__);
    }
  else
    result = 0;
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void thread_x11_start(int sock_fd_ass)
{
  t_x11 *x11;
  if ((sock_fd_ass < 0) || (sock_fd_ass >= MAX_FD_NUM))
    XOUT("%d", sock_fd_ass);
  if (!(g_x11[sock_fd_ass]))
    XOUT("%d", sock_fd_ass);
  x11 = g_x11[sock_fd_ass];
  x11->thread_waiting = 0;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void thread_x11_close(int sock_fd_ass)
{ 
  t_x11 *x11;
  wrap_mutex_lock();
  if ((sock_fd_ass < 0) || (sock_fd_ass >= MAX_FD_NUM))
    XOUT("%d", sock_fd_ass);
  if (g_x11[sock_fd_ass])
    {
    x11 = g_x11[sock_fd_ass];
    x11->thread_on = 0;
    x11->thread_waiting = 0;
    x11->thread_terminating = 0;
    while(x11->thread_terminated == 0)
      usleep(10000);
    dialog_close(x11->diag_main_fd);
    dialog_close(x11->diag_thread_fd);
    wrap_epoll_event_free(x11->epfd, x11->epev_diag_thread_fd);
    wrap_close(x11->diag_thread_fd, __FUNCTION__);
    wrap_close(x11->diag_main_fd, __FUNCTION__);
    g_x11[sock_fd_ass] = NULL;
    wrap_free(x11, __LINE__);
    }
  wrap_mutex_unlock();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int thread_x11_exists(int sock_fd_ass)
{
  t_x11 *x11;
  int result = 0;
  if ((sock_fd_ass < 0) || (sock_fd_ass >= MAX_FD_NUM))
    XOUT("%d", sock_fd_ass);
  if (g_x11[sock_fd_ass])
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void thread_x11_heartbeat(void)
{
  int i;
  t_x11 *x11;
  for (i=0; i<MAX_FD_NUM; i++)
    {
    x11 = g_x11[i];
    if (x11)
      dialog_send_wake(x11->diag_main_fd, x11->sock_fd_ass,
                       x11->srv_idx, x11->cli_idx);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void thread_x11_init(void)
{
  memset(g_x11, 0, sizeof(t_x11 *) * MAX_FD_NUM);
}
/*---------------------------------------------------------------------------*/

