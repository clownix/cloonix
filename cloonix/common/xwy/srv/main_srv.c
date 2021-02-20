/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <pty.h>
#include <netinet/in.h>

#include "mdl.h"
#include "x11_fwd.h"
#include "scp_srv.h"
#include "low_write.h"
#include "wrap.h"
#include "debug.h"
#include "thread_tx.h"
#include "fd_spy.h"
#include "dialog_thread.h"
#include "pty_fork.h"
#include "cloonix.h"

#define MAX_FD_IDENT 100
#define MAX_FD_SIMULTANEOUS_ASSOSS 15

typedef struct t_cli_assos
{
  int sock_fd_ass;
  int srv_idx;
  int cli_idx;
} t_cli_assos;

typedef struct t_cli
{
  int sock_fd;
  int inhibited_associated;
  int inhibited_to_be_destroyed;
  int has_pty_fork;
  int scp_fd;
  int scp_begin;
  int srv_idx;
  uint32_t randid;
  t_cli_assos assos[MAX_FD_SIMULTANEOUS_ASSOSS];
  struct t_cli *prev;
  struct t_cli *next;
}t_cli;

static t_cli *g_cli_head;
static int g_nb_cli;
static int g_is_tcp;
static int g_listen_sock_fd;
static struct timeval g_timeout;
static struct timeval g_last_heartbeat_tv;

/****************************************************************************/
static void cli_alloc(int fd)
{
  int i;
  t_cli *cli = (t_cli *) wrap_malloc(sizeof(t_cli));
  memset(cli, 0, sizeof(t_cli));
  g_nb_cli += 1;
  cli->sock_fd = fd; 
  cli->scp_fd = -1;
  for (i=0; i<MAX_FD_SIMULTANEOUS_ASSOSS; i++)
    cli->assos[i].sock_fd_ass = -1;
  cli->scp_begin = 0;
  cli->inhibited_associated = 0;
  if (g_cli_head)
    g_cli_head->prev = cli;
  cli->next = g_cli_head;
  g_cli_head = cli;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cli_free(t_cli *cli)
{
  g_nb_cli -= 1;
  if (cli->srv_idx != 0)
    x11_free_display(cli->srv_idx);
  if (cli->prev)
    cli->prev->next = cli->next;
  if (cli->next)
    cli->next->prev = cli->prev;
  if (cli == g_cli_head)
    g_cli_head = cli->next;
  mdl_close(cli->sock_fd);
  if (cli->has_pty_fork) 
    {
    if (pty_fork_free_with_sock_fd(cli->sock_fd))
      KERR("%d", cli->sock_fd);
    }
  wrap_free(cli, __LINE__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_sock_fd_ass_free_idx(t_cli *cur)
{
  int i, result = -1;;
  for (i=0; i<MAX_FD_SIMULTANEOUS_ASSOSS; i++)
    {
    if (cur->assos[i].sock_fd_ass == -1)
      {
      result = i;
      cur->assos[i].srv_idx = 0;
      cur->assos[i].cli_idx = 0;
      break;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_sock_fd_ass_allocated_idx(t_cli *cur, int srv_idx, int cli_idx)
{
  int i, result = -1;;
  for (i=0; i<MAX_FD_SIMULTANEOUS_ASSOSS; i++)
    {
    if ((cur->assos[i].sock_fd_ass != -1) &&
        (cur->assos[i].srv_idx == srv_idx) &&
        (cur->assos[i].cli_idx == cli_idx))
      {
      result = i;
      break;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int cli_associate(t_cli *cli, uint32_t randid,
                         int sock_fd_ass, int srv_idx, int cli_idx)
{
  int idx, result =-1;
  t_cli *cur = g_cli_head;
  while(cur)
    {
    if ((cur->randid == randid) &&
        (cur->sock_fd != sock_fd_ass) &&
        (cur->inhibited_associated == 0))
      break;
    cur = cur->next;
    }
  if (!cur)
    {
    KERR("%08X", randid);
    DEBUG_EVT("ASSOCIATE PB %08X (%d-%d) %s", randid, __FUNCTION__,
                                              srv_idx, cli_idx);
    }
  else 
    {
    idx = get_sock_fd_ass_free_idx(cur);
    if (idx < 0)
      {
      DEBUG_EVT("NO ASSOCIATE SPACE FOR %08X %d  (%d-%d)",  randid,
                                         sock_fd_ass, srv_idx, cli_idx);
      KERR("NO ASSOCIATE SPACE FOR %08X %d  (%d-%d)",  randid,
                                         sock_fd_ass, srv_idx, cli_idx);
      }
    else
      {
      result = cur->sock_fd;
      DEBUG_EVT("RANDID ASSOCIATE %08X %d to %d  (%d-%d)", randid,
                                cur->sock_fd, sock_fd_ass, srv_idx, cli_idx);
      cur->assos[idx].sock_fd_ass = sock_fd_ass;
      cur->assos[idx].srv_idx = srv_idx;
      cur->assos[idx].cli_idx = cli_idx;
      cli->randid = randid; 
      cli->inhibited_associated = 1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_cli_association(int sock_fd, int srv_idx, int cli_idx)
{
  int idx, result = -1;
  t_cli *cur = g_cli_head;
  while(cur)
    {
    if ((cur->sock_fd == sock_fd) && 
        (cur->srv_idx == srv_idx))
      break;
    cur = cur->next;
    }
  if (!cur)
    {
    KERR("ASSOCIATE  PB %d (%d-%d)", sock_fd, srv_idx, cli_idx);
    DEBUG_EVT("ASSOCIATE PB %d (%d-%d) %s",sock_fd, srv_idx,
                                           cli_idx, __FUNCTION__);
    }
  else
    {
    idx = get_sock_fd_ass_allocated_idx(cur, srv_idx, cli_idx);
    if (idx < 0)
      {
      DEBUG_EVT("NO ASSOCIATE FOUND FOR %d (%d-%d)", sock_fd,
                                                     srv_idx, cli_idx);
      KERR("NO ASSOCIATE FOUND FOR %d (%d-%d)", sock_fd,
                                                srv_idx, cli_idx);
      }
    else
      {
      result = cur->assos[idx].sock_fd_ass;
      DEBUG_EVT("RANDID ASSOCIATED %08X %d to %d for (%d-%d)",
                cur->randid, sock_fd, result, srv_idx, cli_idx);
      cur->assos[idx].sock_fd_ass = -1;
      cur->assos[idx].srv_idx = 0;
      cur->assos[idx].cli_idx = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void listen_socket_action(void)
{
  int sock_fd, opt=1;
  sock_fd = wrap_accept(g_listen_sock_fd,fd_type_srv,g_is_tcp,__FUNCTION__); 
  if (sock_fd >= 0)
    {
    DEBUG_EVT("LISTEN ACCEPT fd_type_srv %d", sock_fd);
    mdl_open(sock_fd, fd_type_srv, wrap_write_srv, wrap_read_srv);
    cli_alloc(sock_fd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_inhibited_to_be_destroyed(t_cli *cli, int line)
{
//  if (line)
//    KERR("LINE:%d %d %d %d", line, cli->sock_fd, cli->sock_fd, cli->srv_idx);
  if (cli->has_pty_fork)
    {
    cli->has_pty_fork = 0;
    if (pty_fork_free_with_sock_fd(cli->sock_fd))
      KERR("%d", cli->sock_fd);
    }
  cli->inhibited_to_be_destroyed = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_err_cb (void *ptr, int llid, int fd, char *err)
{
  t_cli *cli = (t_cli *) ptr;
  set_inhibited_to_be_destroyed(cli, __LINE__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int send_msg_type_randid_associated_ack(uint32_t randid, int sock_fd, 
                                               int srv_idx, int cli_idx)
{
  int len = MAX_MSG_LEN+g_msg_header_len, result;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg, randid, msg_type_randid_associated_ack,
                      fd_type_srv, srv_idx, cli_idx);
  msg->len = 0;
  result = mdl_queue_write_msg(sock_fd, msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int send_data_end_ack(uint32_t randid, int sock_fd)
{
  int len = MAX_MSG_LEN+g_msg_header_len, result;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg, randid, msg_type_scp_data_end_ack, fd_type_srv, 0, 0);
  msg->len = 0;
  result = mdl_queue_write_msg(sock_fd, msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_msg_cb(void *ptr, int llid, int sock_fd, t_msg *msg)
{
  t_cli *cli = (t_cli *) ptr;
  int main_sock_fd, type, from, srv_idx, cli_idx, display_val;
  uint32_t randid;
  mdl_get_header_vals(msg, &randid, &type, &from, &srv_idx, &cli_idx);
  DEBUG_DUMP_RXMSG(msg);
  if (randid == 0)
      KERR(" ");
  else if (cli->randid != randid)
    {
    if (cli->randid != 0)
      KERR("%08X %08X", cli->randid, randid);
    else if (type == msg_type_randid)
      {
      cli->randid = randid; 
      thread_tx_start(sock_fd);
      DEBUG_EVT("RANDID %08X %d", randid, sock_fd);
      }
    else if (type == msg_type_randid_associated)
      {
      thread_tx_abort(sock_fd);
      main_sock_fd = cli_associate(cli, randid, 
                                   sock_fd, srv_idx, cli_idx);
      mdl_modify(sock_fd, fd_type_srv_ass);
      fd_spy_modify(sock_fd, fd_type_srv_ass);
      if (main_sock_fd != -1)
        {
        send_msg_type_randid_associated_ack(randid, main_sock_fd,
                                            srv_idx, cli_idx);
        }
      }
    else 
      KERR("%08X %d", randid, type);
    }
  else switch(type)
    {

    case msg_type_data_pty:
      pty_fork_msg_type_data_pty(sock_fd, msg);
      break;

    case msg_type_open_bash:
    case msg_type_open_dae:
    case msg_type_open_cmd:
      if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
        KERR("%d", srv_idx);
      else
        {
        display_val = x11_alloc_display(randid, srv_idx);
        cli->srv_idx = display_val;
        if (type == msg_type_open_bash)
          pty_fork_bin_bash(action_bash, randid, sock_fd, NULL, display_val);
        else if (type == msg_type_open_cmd)
          pty_fork_bin_bash(action_cmd, randid, sock_fd, msg->buf, display_val);
        else
          pty_fork_bin_bash(action_dae, randid, sock_fd, msg->buf, display_val);
        cli->has_pty_fork = 1;
        wrap_free(msg, __LINE__); 
        }
      break;

    case msg_type_win_size:
      pty_fork_msg_type_win_size(sock_fd, msg->len, msg->buf);
      wrap_free(msg, __LINE__); 
      break;

    case msg_type_x11_init:
      if (x11_init_cli_msg(randid, sock_fd, msg->buf))
        set_inhibited_to_be_destroyed(cli, __LINE__);
      wrap_free(msg, __LINE__); 
      break;

    case msg_type_x11_connect_ack:
      x11_connect_ack(srv_idx, cli_idx, msg->buf);
      wrap_free(msg, __LINE__); 
      break;

    case msg_type_x11_info_flow:
      x11_info_flow(randid, sock_fd, srv_idx, cli_idx, msg->buf);
      wrap_free(msg, __LINE__);
      break;

 
    case msg_type_scp_open_snd:
    case msg_type_scp_open_rcv:
      recv_scp_open(randid, type, sock_fd, &(cli->scp_fd), msg->buf);
      wrap_free(msg, __LINE__); 
    break;

    case msg_type_scp_data:
      if (recv_scp_data(cli->scp_fd, msg))
        set_inhibited_to_be_destroyed(cli, __LINE__);
      wrap_free(msg, __LINE__); 
      break;

    case msg_type_scp_data_end:
      if (recv_scp_data_end(randid, cli->scp_fd, sock_fd))
        set_inhibited_to_be_destroyed(cli, __LINE__);
      wrap_free(msg, __LINE__); 
      break;

    case msg_type_scp_data_begin:
      cli->scp_begin = 1;
      wrap_free(msg, __LINE__); 
      break;

    case msg_type_scp_data_end_ack:
      send_data_end_ack(randid, sock_fd);
      wrap_free(msg, __LINE__); 
      break;

    case msg_type_end_cli_pty_ack:
      set_inhibited_to_be_destroyed(cli, __LINE__);
    break;
    

    default :
      KOUT("%ld", type);

      }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_max(void)
{
  t_cli *cur = g_cli_head;
  int result = g_listen_sock_fd;
  result = pty_fork_get_max_fd(result);
  result = cloonix_get_max_fd(result);
  result = x11_get_max_fd(result);
  while(cur)
    {
    if (cur->sock_fd > result)
      result = cur->sock_fd;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void prepare_fd_set(fd_set *readfds, fd_set *writefds) 
{
  t_cli *cur, *next;
  int ret;
  FD_ZERO(readfds);
  FD_ZERO(writefds);
  FD_SET(g_listen_sock_fd, readfds);
  cur = g_cli_head;
  while(cur)
    {
    next = cur->next;
    if ((cur->inhibited_associated) || (cur->inhibited_to_be_destroyed == 1))
      {
      cli_free(cur);
      }
    else
      {
      if (cur->inhibited_to_be_destroyed == 0)
        FD_SET(cur->sock_fd, readfds);
      }
    cur = next;
    }
  x11_fdset(readfds, writefds);
  pty_fork_fdset(readfds, writefds);
  cloonix_fdset(readfds);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void server_loop(void)
{
  int max_fd, ret;
  t_cli *next = NULL, *cur = g_cli_head;
  fd_set readfds, writefds;

  while(cur)
    {
    if ((cur->scp_begin) && (cur->scp_fd != -1))
      {
      if (!low_write_levels_above_thresholds(cur->sock_fd))
        {
        ret = send_scp_to_cli(cur->randid, cur->scp_fd, cur->sock_fd);
        if (ret)
          {
          wrap_close(cur->scp_fd, __FUNCTION__);
          cur->scp_begin = 0;
          cur->scp_fd = -1;
          if (ret == -1)
            KERR("Error scp");
          }
        }
      }
    cur = next;
    }

  max_fd = get_max();
  prepare_fd_set(&readfds, &writefds);
  ret = select(max_fd + 1, &readfds, &writefds, NULL, &g_timeout);
  if (ret < 0)
    {
    if (errno != EINTR && errno != EAGAIN)
      KOUT("%s", strerror(errno));
    }
  if (ret == 0)
    {
    g_timeout.tv_sec = 0;
    g_timeout.tv_usec = 10000;
    }
  if (FD_ISSET(g_listen_sock_fd, &readfds))
    listen_socket_action();
  cur = g_cli_head;
  while(cur)
    {
    next = cur->next;
    thread_tx_purge_el(cur->sock_fd);
    if (cur->inhibited_associated)
      {
      cli_free(cur);
      }
    else 
      {
      if (cur->inhibited_to_be_destroyed == 0)
        {
        if (FD_ISSET(cur->sock_fd, &readfds))
          {
          mdl_read_fd((void *)cur, cur->sock_fd, rx_msg_cb, rx_err_cb);
          }
        }
     }
    cur = next;
    }
  pty_fork_fdisset(&readfds, &writefds);
  cloonix_fdisset(&readfds);
  x11_fdisset(&readfds, &writefds);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void usage(char *name)
{
  printf("\n%s <port> standalone", name);
  printf("\n%s <unix_path_xwy_cli> standalone", name);
  printf("\n%s <unix_path_xwy_cli> <unix_path_cloonix>", name);
  printf("\n\n");
  exit(1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void get_listen_sock_fd(int argc, char **argv)
{
  int port;
  port = mdl_parse_val(argv[1]);
  if (port > 0)
    {
    g_is_tcp = 1;
    g_listen_sock_fd = wrap_socket_listen_inet(INADDR_ANY, port,
                       fd_type_listen_inet_srv, __FUNCTION__);
    }
  else
    {
    g_is_tcp = 0;
    g_listen_sock_fd = wrap_socket_listen_unix(argv[1],
                       fd_type_listen_unix_srv, __FUNCTION__);
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
int main(int argc, char **argv)
{
  struct timeval tv;

  if(argc != 3)
    usage(argv[0]);

  daemon(0,0);
  DEBUG_INIT(1);
  g_nb_cli = 0;
  mdl_init();
  low_write_init();
  dialog_init();
  x11_init_display();
  get_listen_sock_fd(argc, argv);
  pty_fork_init();
  cloonix_init(argv[2]);
  gettimeofday(&g_last_heartbeat_tv, NULL);
  g_timeout.tv_sec = 0;
  g_timeout.tv_usec = 10000;
  while (1)
    {
    server_loop();
    gettimeofday(&tv, NULL);
    if (heartbeat_10th_second(&tv))
      {
      mdl_heartbeat();
      }
    cloonix_beat(&tv);
    }
  return 0;
}
/*--------------------------------------------------------------------------*/
