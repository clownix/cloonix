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
#define _GNU_SOURCE 
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mdl.h"
#include "x11_fwd.h"
#include "low_write.h"
#include "wrap.h"
#include "debug.h"
#include "thread_x11.h"
#include "dialog_thread.h"
#include "tools.h"
#include "fd_spy.h"
#include "pty_fork.h"
#include "first_x11_magic.h"


typedef struct t_conn_x11
{
  int cli_idx; 
  int x11_fd; 
  int sock_fd_ass;
  int threads_on; 
  int diag_main_fd;
  t_msg *first_x11_msg;
} t_conn_x11;

typedef struct t_display_x11
{
  uint32_t randid;
  int sock_fd;
  int srv_idx;
  int srv_idx_acked;
  int x11_listen_fd;
  char magic_cookie[2*MAGIC_COOKIE_LEN+1];
  int pool_fifo[MAX_IDX_X11-1];
  int pool_read;
  int pool_write;
  t_conn_x11 *conn[MAX_IDX_X11];
  struct t_display_x11 *prev;
  struct t_display_x11 *next;
} t_display_x11;


int get_cli_association(int sock_fd, int srv_idx, int cli_idx);

static t_display_x11 *g_display[SRV_IDX_MAX - SRV_IDX_MIN + 1];
static t_display_x11 *g_display_head;

static void disconnect_cli_idx(t_display_x11 *disp, int cli_idx);


/****************************************************************************/
static void print_peer_info_tx(int srv, int cli, int ptx, long long tx)
{
  DEBUG_EVT("PEER INFO TX: srv:%d cli:%d ptx:%d tx:%lld", srv, cli, ptx, tx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void print_peer_info_rx(int srv, int cli, int prx, long long rx)
{
  DEBUG_EVT("PEER INFO RX: srv:%d cli:%d prx:%d rx:%lld", srv, cli, prx, rx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int send_msg_type_x11_info_flow(uint32_t randid, int sock_fd,
                                        int srv_idx, int cli_idx, char *txt)
{
  int len = MAX_MSG_LEN+g_msg_header_len, result;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d", srv_idx);
  mdl_set_header_vals(msg, randid, msg_type_x11_info_flow,
                      fd_type_srv, srv_idx, cli_idx);
  msg->len = sprintf(msg->buf, "%s", txt) + 1;
  result = mdl_queue_write_msg(sock_fd, msg);
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void dialog_err(int sock_fd, int srv_idx, int cli_idx, char *buf)
{
  KERR("%d %d %d %s", sock_fd, srv_idx, cli_idx, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dialog_wake(int sock_fd, int srv_idx, int cli_idx, char *buf)
{
  KERR("%d %d %d %s", sock_fd, srv_idx, cli_idx, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dialog_stats(int sock_fd, int srv_idx, int cli_idx,
                         int txpkts, long long txbytes,
                         int rxpkts, long long rxbytes, char *buf)
{
  t_display_x11 *disp = g_display[srv_idx-SRV_IDX_MIN];
  t_conn_x11 *conn;
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d", srv_idx);
  if (!disp)
    KERR("%d %d %d %s", sock_fd, srv_idx, cli_idx, buf);
  else
    {
    conn = disp->conn[cli_idx];
    if (!conn)
      KOUT("%d %d", disp->sock_fd, cli_idx);
    DEBUG_EVT("(%d-%d) txpkts:%d txbytes:%lld rxpkts:%d rxbytes:%lld",
               srv_idx, cli_idx, txpkts, txbytes, rxpkts, rxbytes);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dialog_killed(int sock_fd, int srv_idx, int cli_idx, char *buf)
{
  t_display_x11 *disp = g_display[srv_idx-SRV_IDX_MIN];
  t_conn_x11 *conn;
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d", srv_idx);
  if (!disp)
    KERR("%d %d %d %s", sock_fd, srv_idx, cli_idx, buf);
  else
    {
    conn = disp->conn[cli_idx];
    if (!conn)
      KOUT("%d %d", disp->sock_fd, cli_idx);
    if (!conn->threads_on)
      KOUT("%d %d", disp->sock_fd, cli_idx);
    conn->threads_on = 0;
    conn->x11_fd = -1;
    disconnect_cli_idx(disp, cli_idx);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_fd_is_unique(int x11_fd)
{
  int  j, srv_idx, cli_idx, result = 1;
  t_display_x11 *cur = g_display_head;

  if (!fd_spy_get_type(x11_fd, &srv_idx, &cli_idx))
    KERR("EXISTS: %d %d %d", x11_fd, srv_idx, cli_idx);

  while(cur) 
    {
    for (j=1; j<MAX_IDX_X11; j++)
      {
      if ((cur->conn[j]) && (cur->conn[j]->x11_fd == x11_fd))
        {
        KERR("%d %d", j, x11_fd);
        result = 0;
        }
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_display_x11 *find_disp(int srv_idx)
{
  t_display_x11 *cur = NULL;
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d", srv_idx);
  cur = g_display_head;
  while(cur)
    {
    if (cur->srv_idx == srv_idx)
      break;
    cur = cur->next;
    }
  if (cur != g_display[srv_idx-SRV_IDX_MIN])
    KOUT("%d %p %p", srv_idx, cur, g_display[srv_idx-SRV_IDX_MIN]);
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_display_x11 *find_empy(int srv_idx)
{
  t_display_x11 *cur = NULL;
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d", srv_idx);
  if (g_display[srv_idx-SRV_IDX_MIN])
    KOUT("%d", srv_idx);
  cur = (t_display_x11 *) wrap_malloc(sizeof(t_display_x11));
  memset(cur, 0, sizeof(t_display_x11));
  cur->srv_idx = srv_idx;
  g_display[srv_idx-SRV_IDX_MIN] = cur;
  cur->next = g_display_head;
  if (g_display_head)
    g_display_head->prev = cur;
  g_display_head = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void init_pool(t_display_x11 *disp)
{
  int i;
  for(i = 0; i < MAX_IDX_X11 - 1; i++)
    disp->pool_fifo[i] = i+1;
  disp->pool_read = 0;
  disp->pool_write =  MAX_IDX_X11 - 2;
  memset(disp->conn, 0, (sizeof(t_conn_x11 *)) * MAX_IDX_X11);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int alloc_pool_idx(t_display_x11 *disp)
{
  int idx = 0, len = sizeof(t_conn_x11);
  t_conn_x11 *conn;
  if(disp->pool_read != disp->pool_write)
    {
    idx = disp->pool_fifo[disp->pool_read];
    disp->pool_read = (disp->pool_read + 1) % (MAX_IDX_X11 - 1);
    if (disp->conn[idx])
      KOUT(" ");
    disp->conn[idx] = (t_conn_x11 *)wrap_malloc(len);
    memset(disp->conn[idx], 0, sizeof(t_conn_x11));
    conn = disp->conn[idx];
    conn->cli_idx = idx;
    conn->sock_fd_ass = -1;
    conn->diag_main_fd =-1;
    conn->x11_fd = -1;
    }
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_pool_idx(t_display_x11 *disp, int idx)
{
  if (!disp->conn[idx])
    KERR("%d", idx);
  else
    {
    disp->pool_fifo[disp->pool_write] =  idx;
    disp->pool_write = (disp->pool_write + 1) % (MAX_IDX_X11 - 1);
    wrap_free(disp->conn[idx], __LINE__);
    disp->conn[idx] = NULL;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int send_msg_type_x11_connect(uint32_t randid, int sock_fd,
                                     int srv_idx, int cli_idx)
{
  int len = MAX_MSG_LEN+g_msg_header_len, result;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d", srv_idx);
  mdl_set_header_vals(msg, randid, msg_type_x11_connect,
                      fd_type_srv, srv_idx, cli_idx);
  msg->len = 0;
  result = mdl_queue_write_msg(sock_fd, msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int send_msg_type_x11_init(uint32_t randid, int sock_fd, 
                                  int srv_idx, char *txt)
{
  int len = MAX_MSG_LEN+g_msg_header_len, result;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg, randid, msg_type_x11_init, fd_type_srv,srv_idx,0);
  msg->len = sprintf(msg->buf, "%s", txt) + 1;
  result = mdl_queue_write_msg(sock_fd, msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void disconnect_cli_idx(t_display_x11 *disp, int cli_idx)
{
  t_conn_x11 *conn = disp->conn[cli_idx];
  if (!conn)
    KOUT("%d %d", disp->sock_fd, cli_idx);
  if (conn->sock_fd_ass == -1)
    KERR("%d %d", disp->sock_fd, cli_idx);
  else
    thread_x11_close(conn->sock_fd_ass);
  if (conn->x11_fd != -1)
    wrap_close (conn->x11_fd, __FUNCTION__);
  if (conn->first_x11_msg)
    wrap_free(conn->first_x11_msg, __LINE__);
  free_pool_idx(disp, cli_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void begin_x11_listen_action(t_display_x11 *disp)
{
  int x11_fd, cli_idx;
  t_msg *first_x11_msg;

  x11_fd = wrap_accept(disp->x11_listen_fd,
                       fd_type_x11_accept, 1, __FUNCTION__);
  if (x11_fd < 0)
    KERR("%s", strerror(errno));
  else if (!check_fd_is_unique(x11_fd))
    {
    KERR("Duplicate x11_fd");
    wrap_close (x11_fd, __FUNCTION__);
    }
  else
    {
    cli_idx = alloc_pool_idx(disp);
    if (cli_idx == 0)
      {
      KERR("No space for new idx");
      wrap_close (x11_fd, __FUNCTION__);
      }
   else
      {
      first_x11_msg = first_read_magic(disp->randid, x11_fd, 
                                       disp->srv_idx, cli_idx,
                                       disp->magic_cookie);
      if (first_x11_msg == NULL)
        {
        KERR("Failed first read idx");
        wrap_close (x11_fd, __FUNCTION__);
        }
      else
        {
        disp->conn[cli_idx]->first_x11_msg = first_x11_msg;
        disp->conn[cli_idx]->x11_fd = x11_fd;
        disp->conn[cli_idx]->sock_fd_ass = -1;
        if (send_msg_type_x11_connect(disp->randid, disp->sock_fd,
                                      disp->srv_idx, cli_idx))
          {
          KERR("%d %d", disp->sock_fd, cli_idx);
KERR("%d", disp->srv_idx);
          disconnect_cli_idx(disp, cli_idx);
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void terminate_x11(int p_fd[2], 
                          int sock_fd_ass, int epfd,
                          t_display_x11 *disp, int cli_idx)
{
  if (p_fd[0] != -1)    
    wrap_close(p_fd[0], __FUNCTION__);
  if (p_fd[1] != -1)    
    wrap_close(p_fd[1], __FUNCTION__);
  if (sock_fd_ass != -1)
    wrap_close(sock_fd_ass, __FUNCTION__);
  if (epfd != -1)
    wrap_close(epfd, __FUNCTION__);
KERR("%d", disp->srv_idx);
  disconnect_cli_idx(disp, cli_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void helper_spy_set_info(t_display_x11 *disp, t_conn_x11 *conn)
{
  int srv_idx = disp->srv_idx;
  int cli_idx = conn->cli_idx;
  fd_spy_set_info(disp->sock_fd,             srv_idx, cli_idx);
  fd_spy_set_info(conn->x11_fd,              srv_idx, cli_idx);
  fd_spy_set_info(conn->sock_fd_ass,         srv_idx, cli_idx);
  fd_spy_set_info(conn->diag_main_fd,    srv_idx, cli_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void end_x11_listen_action(t_display_x11 *disp,
                                  t_conn_x11 *conn, int cli_idx)
{
  int p_fd[2];
  int sock_fd_ass, epfd;
  p_fd[0] = -1;
  p_fd[1] = -1;
  sock_fd_ass = get_cli_association(disp->sock_fd, disp->srv_idx, cli_idx);
  epfd = wrap_epoll_create(fd_type_x11_epoll, __FUNCTION__);
  if ((epfd == -1) || (sock_fd_ass == -1) ||
      (wrap_socketpair(p_fd, fd_type_dialog_thread, __FUNCTION__)  == -1))
    {
    KERR(" ");
    terminate_x11(p_fd, sock_fd_ass, epfd, disp, cli_idx);
    }
  else if (thread_x11_open(disp->randid, 1, sock_fd_ass, conn->x11_fd,
                           disp->srv_idx, cli_idx, epfd, p_fd[1],  p_fd[0],
                           conn->first_x11_msg))
    {
    KERR(" ");
    terminate_x11( p_fd, sock_fd_ass, epfd, disp, cli_idx);
    }
  else
    {
    conn->first_x11_msg = NULL;
    DEBUG_DUMP_THREAD(sock_fd_ass, conn->x11_fd, disp->srv_idx, cli_idx);
    conn->threads_on = 1;
    conn->sock_fd_ass = sock_fd_ass;
    conn->diag_main_fd = p_fd[0];
    helper_spy_set_info(disp, conn);
    thread_x11_start(sock_fd_ass);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_connect_ack(int srv_idx, int cli_idx, char *txt)
{
  t_display_x11 *disp = g_display[srv_idx-SRV_IDX_MIN];
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d", srv_idx);
  else if ((cli_idx <= 0) || (cli_idx >= MAX_IDX_X11))
    KERR("%d", cli_idx);
  else if (!disp)
    KERR("%d %d", srv_idx, cli_idx);
  else if (!(disp->conn[cli_idx]))
    KERR("%d %d", srv_idx, cli_idx);
  else
    {
    if (strcmp(txt, "OK"))
      KERR("%s", txt);
    else
      end_x11_listen_action(disp, disp->conn[cli_idx], cli_idx);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int init_alloc_display(uint32_t randid, int sock_fd, int srv_idx)
{
  int fd, port, result = -1;
  t_display_x11 *disp;
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d %d", sock_fd, srv_idx);
  disp = find_disp(srv_idx);
  if (disp)
    KERR("%d", srv_idx);
  else
    {
    disp = find_empy(srv_idx);
    port = X11_OFFSET_PORT + srv_idx;
    if (!disp)
      KERR("%d", srv_idx);
    else
      {
      fd = wrap_socket_listen_inet(INADDR_LOOPBACK, port,
                                   fd_type_x11_listen, __FUNCTION__);
      if (fd < 0)
        KERR("%d %s", port, strerror(errno));
      else
        {
        disp->randid = randid;
        disp->sock_fd = sock_fd;
        disp->x11_listen_fd = fd;
        init_pool(disp);
        result = 0;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_init_cli_msg(uint32_t randid, int sock_fd, char *magic_cookie)
{
  int srv_idx, result = 0;
  srv_idx = SRV_IDX_MIN;
  while (tools_port_already_in_use(X11_OFFSET_PORT + srv_idx))
    {
    srv_idx += 1;
    if (srv_idx > SRV_IDX_MAX)
      {
      KERR("%d", srv_idx);
      send_msg_type_x11_init(randid, sock_fd, 0, "KO");
      result = -1;
      break;
      }
    }
  if (result == 0)
    {
    if (init_alloc_display(randid, sock_fd, srv_idx))
      {
      KERR("%d", srv_idx);
      if (send_msg_type_x11_init(randid, sock_fd, 0, "KO"))
        result = -1;
      }
    else
      {
      if (strlen(magic_cookie) != 2*MAGIC_COOKIE_LEN)
        KOUT("%s", magic_cookie);
      else if (pty_fork_xauth_add_magic_cookie(srv_idx, magic_cookie))
        {
        KERR("%s", magic_cookie);
        if (send_msg_type_x11_init(randid, sock_fd, 0, "KO"))
          result = -1;
        }
      else
        {
        strcpy(g_display[srv_idx-SRV_IDX_MIN]->magic_cookie, magic_cookie);
        if (send_msg_type_x11_init(randid, sock_fd, srv_idx, "OK"))
          result = -1;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_alloc_display(uint32_t randid, int srv_idx)
{
  int fd, port, result = 0;
  t_display_x11 *disp;
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d", srv_idx);
  disp = find_disp(srv_idx);
  if (!disp)
    KERR("%d", srv_idx);
  else
    {
    disp->srv_idx_acked = 1;
    result = srv_idx;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_free_display(int srv_idx)
{
  int i;
  t_display_x11 *disp;
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d", srv_idx);
  disp = g_display[srv_idx-SRV_IDX_MIN];
  if (disp)
    {
    if (disp->x11_listen_fd != -1)
      wrap_close(disp->x11_listen_fd, __FUNCTION__);
    for (i=1; i<MAX_IDX_X11; i++)
      {
      if (disp->conn[i])
        {
KERR("%d", disp->srv_idx);
        disconnect_cli_idx(disp, i);
        }
      }
    if (disp->next)
      disp->next->prev = disp->prev;
    if (disp->prev)
      disp->prev->next = disp->next;
    if (disp == g_display_head)
      g_display_head = disp->next;
    g_display[srv_idx-SRV_IDX_MIN] = NULL;
    wrap_free(disp, __LINE__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_init_display(void)
{
  memset(g_display, 0, (SRV_IDX_MAX-SRV_IDX_MIN) * sizeof(t_display_x11 *));
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void x11_fdset(fd_set *readfds, fd_set *writefds)
{
  int j;
  t_display_x11 *cur, *next;
  t_conn_x11 *conn;
  int sock_fd, x11_fd;
  cur = g_display_head;
  while(cur)
    {
    next = cur->next;
    if (cur->x11_listen_fd < 0)
      {
      KERR(" ");
      x11_free_display(cur->srv_idx);
      }
    else
      {
      FD_SET(cur->x11_listen_fd, readfds);
      for (j=1; j<MAX_IDX_X11; j++)
        {
        conn = cur->conn[j];
        if ((conn) && (conn->threads_on) && (conn->sock_fd_ass != -1))
          {
          FD_SET(conn->diag_main_fd, readfds);
          if (dialog_tx_queue_non_empty(conn->diag_main_fd))
            FD_SET(conn->diag_main_fd, writefds);
          }
        }
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_fdisset(fd_set *readfds, fd_set *writefds)
{
  int j, sock_fd, x11_fd, srv_idx;
  t_display_x11 *cur, *next;
  t_conn_x11 *conn;
  cur = g_display_head;
  while(cur)
    { 
    next = cur->next;
    if (cur->x11_listen_fd < 0)
      KERR(" ");
    else
      {
      sock_fd = cur->sock_fd;
      srv_idx = cur->srv_idx;
      if (FD_ISSET(cur->x11_listen_fd, readfds))
        begin_x11_listen_action(cur);

      for (j=1; j<MAX_IDX_X11; j++)
        {
        conn = g_display[srv_idx-SRV_IDX_MIN]->conn[j];
        if ((conn) && (conn->threads_on) && (conn->sock_fd_ass != -1)) 
          {
          if (FD_ISSET(conn->diag_main_fd, readfds))
            {
            if (dialog_recv_fd(conn->diag_main_fd, sock_fd,
                               srv_idx, j, dialog_err, dialog_wake,
                               dialog_killed, dialog_stats))
              {
              KERR(" ");
KERR("%d", cur->srv_idx);
              disconnect_cli_idx(cur, j);
              }
            }
          if (FD_ISSET(conn->diag_main_fd, writefds))
            {
            if (dialog_tx_ready(conn->diag_main_fd))
              {
              KERR(" ");
KERR("%d", cur->srv_idx);
              disconnect_cli_idx(cur, j);
              }
            }
          }
        }
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int x11_get_max_fd(int max)
{
  int j, result = max;
  t_display_x11 *cur;
  t_conn_x11 *conn;
  cur = g_display_head;
  while(cur)
    {
    if (cur->x11_listen_fd > result)
      result = cur->x11_listen_fd;
    for (j=1; j<MAX_IDX_X11; j++)
      {
      conn = cur->conn[j];
      if (conn)
        {
        if (conn->diag_main_fd > result)
          result = conn->diag_main_fd;
        }
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_info_flow(uint32_t randid, int sock_fd, int srv_idx, int cli_idx,
                   char *buf)
{
  t_display_x11 *disp;
  t_conn_x11 *conn;
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d", srv_idx);
  disp = g_display[srv_idx-SRV_IDX_MIN];
  if (!disp)
    KERR("%d %d %d", sock_fd, srv_idx, cli_idx);
  else if (disp->conn[cli_idx])
    {
    conn = disp->conn[cli_idx];
    }
}
/*--------------------------------------------------------------------------*/

