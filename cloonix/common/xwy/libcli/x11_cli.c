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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>


#include "mdl.h"
#include "x11_init.h"
#include "cli_lib.h"
#include "x11_cli.h"
#include "getsock.h"
#include "low_write.h"
#include "wrap.h"
#include "debug.h"
#include "cli_lib.h"
#include "fd_spy.h"
#include "x11_tx.h"

typedef struct t_conn_cli_x11
{
  int srv_idx;
  int cli_idx;
  int x11_fd;
  int llid;
  int llid_ass;
  int tid;
  int type;
  struct epoll_event *x11_fd_epev;
} t_conn_cli_x11;

static t_conn_cli_x11 *g_conn[MAX_IDX_X11];

static t_sock_fd_ass_close g_sock_fd_ass_close;

/****************************************************************************/
void xcli_killed_x11(int cli_idx)
{
  static int in_zero = 0;
  int i;
  t_conn_cli_x11 *conn = g_conn[cli_idx];
  if (cli_idx == 0)
    {
    if (in_zero == 0)
      { 
      in_zero = 1;
      KERR("%d", cli_idx);
      for (i=1; i<MAX_IDX_X11; i++)
        {
        if (g_conn[i])
          xcli_killed_x11(i);
        }
      in_zero = 0;
      }
    }
  else if (!conn)
    {
    KERR("%d", cli_idx);
    }
  else
    {
    g_conn[cli_idx] = NULL;
    x11_tx_close(conn->x11_fd);
    wrap_epoll_event_free(xcli_get_main_epfd(), conn->x11_fd_epev);
    g_sock_fd_ass_close(conn->llid_ass);
    wrap_close(conn->x11_fd, __FUNCTION__);
    wrap_free(conn, __LINE__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void check_fd_unique(int x11_fd)
{
  int i;
  for (i=1; i<MAX_IDX_X11; i++)
    {
    if (g_conn[i])
      {
      if (x11_fd == g_conn[i]->x11_fd)
        KOUT("%d %d %d", i, x11_fd, g_conn[i]->x11_fd);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void conn_ack_ass(int llid, int cli_idx)
{
  char msg[MAX_TXT_LEN];
  t_conn_cli_x11 *conn;

  if (!g_conn[cli_idx])
    KERR("%d", cli_idx);
  else
    {
    conn = g_conn[cli_idx];
    memset(msg, 0, MAX_TXT_LEN);
    snprintf(msg, MAX_TXT_LEN-1, "OK");
    conn->llid = llid;
    xcli_send_msg_type_x11_connect_ack(cli_idx, msg);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_conn_and_ack(int srv_idx, int cli_idx, int x11_fd)
{
  int epfd, len = sizeof(t_conn_cli_x11);
  t_conn_cli_x11 *conn;

  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d %d", srv_idx, cli_idx);
  if (g_conn[cli_idx])
    KOUT("%d %d", srv_idx, cli_idx);
  if (x11_tx_open(x11_fd))
    KOUT("%d %d", srv_idx, cli_idx);
  conn = (t_conn_cli_x11 *) wrap_malloc(len);
  memset(conn, 0, sizeof(t_conn_cli_x11));
  conn->srv_idx       = srv_idx;
  conn->cli_idx       = cli_idx;
  conn->x11_fd        = x11_fd;
  conn->llid_ass      = 0;
  epfd = xcli_get_main_epfd();
  conn->x11_fd_epev = wrap_epoll_event_alloc(epfd, x11_fd, 3);
  fd_spy_set_info(conn->x11_fd, srv_idx, cli_idx); 
  g_conn[cli_idx] = conn;

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void sub_x11_conn(int srv_idx, int cli_idx, int x11_fd)
{
  int llid_ass, sock_fd_ass = -1;
  if (x11_fd < 0)
    {
    KERR("%s", strerror(errno));
    xcli_send_msg_type_x11_connect_ack(cli_idx, "KO");
    }
  else
    {
    xcli_send_randid_associated_begin(cli_idx);
    create_conn_and_ack(srv_idx, cli_idx, x11_fd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void x11_connect(int srv_idx, int cli_idx)
{
  unsigned long inet_addr;
  int x11_fd = -1;

  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d %d", srv_idx, cli_idx);
  if ((cli_idx <= 0) || (cli_idx >= MAX_IDX_X11))
    KOUT("%d %d", srv_idx, cli_idx);
  if (strlen(get_x11_path()))
    {
    x11_fd = wrap_socket_connect_unix(get_x11_path(), 
                                      fd_type_x11_connect, __FUNCTION__);
    check_fd_unique(x11_fd);
    sub_x11_conn(srv_idx, cli_idx, x11_fd);
    }
  else if (get_x11_port())
    {
    mdl_ip_string_to_int(&inet_addr, "127.0.0.1"); 
    x11_fd = wrap_socket_connect_inet(inet_addr, get_x11_port(),
                                      fd_type_x11_connect, __FUNCTION__);
    sub_x11_conn(srv_idx, cli_idx, x11_fd);
    }
  else
    {
    KERR("No display unix path");
    xcli_send_msg_type_x11_connect_ack(cli_idx, "KO");
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void x11_cli_write_to_x11(int cli_idx, int len, char *buf)
{
  t_conn_cli_x11 *conn;

  if (!g_conn[cli_idx])
    KOUT("%d", cli_idx);
  conn = g_conn[cli_idx];

  if (x11_tx_add_queue(conn->x11_fd, len, buf))
    KERR("%d", cli_idx);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int x11_cli_get_srv_idx(int cli_idx)
{
  int result = -1;
  t_conn_cli_x11 *conn;
  if (g_conn[cli_idx])
    {
    conn = g_conn[cli_idx];
    result = conn->srv_idx;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void x11_cli_set_params(int cli_idx, int llid_ass, int tid, int type)
{
  t_conn_cli_x11 *conn = g_conn[cli_idx];
  if (!conn)
    KOUT("%d", cli_idx); 
  conn->llid_ass = llid_ass;
  conn->tid = tid;
  conn->type = type;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int read_from_x11(t_conn_cli_x11 *conn, int fd)
{
  int result = -1;
  char buf[MAX_X11_MSG_LEN];
  int len = g_msg_header_len + MAX_X11_MSG_LEN;
  len = read(fd, buf, MAX_X11_MSG_LEN);
  if (len == 0)
    {
    KERR(" ");
    }
  else if (len < 0)
    {
    if (errno != EINTR && errno != EAGAIN)
      KERR("%s", strerror(errno));
    else
      result = 0;
    }
  else
    {
    xcli_x11_x11_rx(conn->llid_ass, conn->tid, conn->type, len, buf);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_fd_epollin_epollout_action(uint32_t evts, int fd)
{
  int i, result = 0;
  t_conn_cli_x11 *conn;
  int epfd = xcli_get_main_epfd();
  for (i=1; i<MAX_IDX_X11; i++)
    {
    conn = g_conn[i];
    if (conn)
      {
      if (conn->x11_fd == fd)
        {
        if (evts & EPOLLIN) 
          {
          result += 1;
          if (read_from_x11(conn, fd))
            {
            KERR("%d", i);
            xcli_killed_x11(i);
            }
          }
        if (evts & EPOLLOUT)
          {
          result += 1;
          if (x11_tx_ready(fd))
            {
            KERR("%d", i);
            xcli_killed_x11(i);
            }
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_fd_epollin_epollout_setup(void)
{
  int i;
  t_conn_cli_x11 *conn;
  int epfd = xcli_get_main_epfd();
  for (i=1; i<MAX_IDX_X11; i++)
    {
    conn = g_conn[i];
    if (conn)
      {
      conn->x11_fd_epev->events = EPOLLIN;
      if (x11_tx_queue_non_empty(conn->x11_fd))
        conn->x11_fd_epev->events |= EPOLLOUT;
      if (epoll_ctl(epfd, EPOLL_CTL_MOD, conn->x11_fd, 
                                         conn->x11_fd_epev))
        KOUT(" ");
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rx_x11_msg_cb(uint32_t randid, int llid, int type,
                   int srv_idx, int cli_idx, t_msg *msg)
{ 
  int len, srv, cli, ptx, prx;
  long long tx, rx;
  t_conn_cli_x11 *conn = g_conn[cli_idx];
  switch(type)
    {

    case msg_type_x11_connect:
      x11_connect(srv_idx, cli_idx);
      wrap_free(msg, __LINE__);
      break;

    case msg_type_x11_info_flow:
      if (conn)
        {
        }
      wrap_free(msg, __LINE__);
      break;

    case msg_type_randid_associated_ack:
      conn_ack_ass(llid, cli_idx);
      wrap_free(msg, __LINE__);
      break;

   default:
      KOUT("%d", type);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_cli_init(t_sock_fd_ass_close sock_fd_ass_close)
{
  g_sock_fd_ass_close = sock_fd_ass_close;
  memset(g_conn, 0, MAX_IDX_X11 * sizeof(t_conn_cli_x11 *));
}
/*--------------------------------------------------------------------------*/
