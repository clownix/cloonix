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
#include "cloon.h"
#include "glob_common.h"

#define MAX_FD_IDENT 100
#define MAX_FD_SIMULTANEOUS_ASSOSS 15

typedef struct t_cli_assos
{
  int sock_ass_fd;
  int srv_idx;
  int cli_idx;
} t_cli_assos;

typedef struct t_cli
{
  int sock_idx_fd;
  int sock_val_fd;
  int inhibited_associated;
  int inhibited_to_be_destroyed;
  int has_pty_fork;
  int scp_fd;
  int scp_begin;
  int srv_idx;
  uint32_t randid;
  t_cli_assos assos[MAX_FD_SIMULTANEOUS_ASSOSS];
  char x11_path[MAX_TXT_LEN];
  struct t_cli *prev;
  struct t_cli *next;
}t_cli;

static t_cli *g_cli_head;
static int g_nb_cli;
static int g_sock_listen_fd;
static struct timeval g_timeout;
static struct timeval g_last_heartbeat_tv;
static char g_net_name[MAX_NAME_LEN];
static int g_net_rank;


/****************************************************************************/
static void cli_alloc(int fd)
{
  int i;
  t_cli *cli = (t_cli *) wrap_malloc(sizeof(t_cli));
  memset(cli, 0, sizeof(t_cli));
  g_nb_cli += 1;
  cli->sock_idx_fd = fd; 
  cli->sock_val_fd = fd; 
  cli->scp_fd = -1;
  for (i=0; i<MAX_FD_SIMULTANEOUS_ASSOSS; i++)
    cli->assos[i].sock_ass_fd = -1;
  cli->scp_begin = 0;
  cli->inhibited_associated = 0;
  cli->inhibited_to_be_destroyed = 0;
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
  if (strlen(cli->x11_path))
    unlink(cli->x11_path);
  if (cli->srv_idx != 0)
    x11_free_display(cli->srv_idx);
  if (cli->prev)
    cli->prev->next = cli->next;
  if (cli->next)
    cli->next->prev = cli->prev;
  if (cli == g_cli_head)
    g_cli_head = cli->next;
  mdl_close(cli->sock_idx_fd);
  if (cli->has_pty_fork) 
    {
    pty_fork_free_with_fd(cli->sock_idx_fd);
    }
  if (cli->inhibited_associated == 0)
    {
    if (cli->sock_val_fd >= 0)
      {
      wrap_close(cli->sock_val_fd, __FUNCTION__);
      cli->sock_val_fd = -1;
      }
    }
  wrap_free(cli, __LINE__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_sock_ass_fd_free_idx(t_cli *cur)
{
  int i, result = -1;;
  for (i=0; i<MAX_FD_SIMULTANEOUS_ASSOSS; i++)
    {
    if (cur->assos[i].sock_ass_fd == -1)
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
static int get_sock_ass_fd_allocated_idx(t_cli *cur, int srv_idx, int cli_idx)
{
  int i, result = -1;;
  for (i=0; i<MAX_FD_SIMULTANEOUS_ASSOSS; i++)
    {
    if ((cur->assos[i].sock_ass_fd != -1) &&
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
                         int sock_ass_fd, int srv_idx, int cli_idx)
{
  int idx, result =-1;
  t_cli *cur = g_cli_head;
  while(cur)
    {
    if ((cur->randid == randid) &&
        (cur->sock_idx_fd != sock_ass_fd) &&
        (cur->inhibited_associated == 0))
      break;
    cur = cur->next;
    }
  if (!cur)
    {
    KERR("ERROR %08X %d %d %d", randid, sock_ass_fd, srv_idx, cli_idx);
    }
  else 
    {
    idx = get_sock_ass_fd_free_idx(cur);
    if (idx < 0)
      {
      KERR("ERROR %08X %d %d %d", randid, sock_ass_fd, srv_idx, cli_idx);
      }
    else
      {
      result = cur->sock_idx_fd;
      cur->assos[idx].sock_ass_fd = sock_ass_fd;
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
int get_cli_association(int sock_idx_fd, int srv_idx, int cli_idx)
{
  int idx, result = -1;
  t_cli *cur = g_cli_head;
  while(cur)
    {
    if ((cur->sock_idx_fd == sock_idx_fd) && 
        (cur->srv_idx == srv_idx))
      break;
    cur = cur->next;
    }
  if (!cur)
    {
    KERR("ASSOCIATE  PB %d (%d-%d)", sock_idx_fd, srv_idx, cli_idx);
    }
  else
    {
    idx = get_sock_ass_fd_allocated_idx(cur, srv_idx, cli_idx);
    if (idx < 0)
      {
      KERR("NO ASSOCIATE FOUND FOR %d (%d-%d)", sock_idx_fd,
                                                srv_idx, cli_idx);
      }
    else
      {
      result = cur->assos[idx].sock_ass_fd;
      DEBUG_EVT("RANDID ASSOCIATED %08X %d to %d for (%d-%d)",
                cur->randid, sock_idx_fd, result, srv_idx, cli_idx);
      cur->assos[idx].sock_ass_fd = -1;
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
  int fd, opt=1;
  t_cli *cur = g_cli_head;
  fd = wrap_accept(g_sock_listen_fd, fd_type_srv, __FUNCTION__); 
  if (fd >= 0)
    {
    while(cur)
      {
      if (cur->sock_idx_fd == fd)
        {
        KERR("ERROR ERROR ERROR fd %d exists", fd);
        return;
        }
      cur = cur->next;
      }
    if (mdl_exists(fd))
      {
      KERR("ERROR ERROR ERROR fd %d exists in mdl", fd);
      return;
      }
    mdl_open(fd, fd_type_srv, wrap_write_srv, wrap_read_srv);
    cli_alloc(fd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_inhibited_to_be_destroyed(t_cli *cli, int line)
{
  if (cli->has_pty_fork)
    {
    cli->has_pty_fork = 0;
    if (pty_fork_free_with_fd(cli->sock_idx_fd))
      KERR("ERROR %d", cli->sock_idx_fd);
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
    KERR("ERROR");
  else if (cli->randid != randid)
    {
    if (cli->randid != 0)
      KERR("ERROR %08X %08X", cli->randid, randid);
    else if (type == msg_type_randid)
      {
      cli->randid = randid; 
      thread_tx_start(sock_fd);
      DEBUG_EVT("RANDID %08X %d", randid, sock_fd);
      }
    else if (type == msg_type_randid_associated)
      {
      DEBUG_EVT("MSG RANDID ASSOCIATED %08X %d", randid, sock_fd);
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
      KERR("ERROR %08X %d", randid, type);
    }
  else switch(type)
    {

    case msg_type_data_pty:
      pty_fork_msg_type_data_pty(sock_fd, msg);
      break;

    case msg_type_open_bash:
    case msg_type_open_dae:
    case msg_type_open_crun:
    case msg_type_open_cmd:
    case msg_type_open_ovs:
    case msg_type_open_slf:
      if ((srv_idx < X11_DISPLAY_XWY_MIN) || (srv_idx > X11_DISPLAY_XWY_MAX))
        KERR("ERROR %d", srv_idx);
      else
        {
        display_val = x11_alloc_display(randid, srv_idx);
        cli->srv_idx = display_val;
        if (type == msg_type_open_bash)
          {
          pty_fork_bin_bash(action_bash, randid, sock_fd, NULL,
                            g_net_rank, display_val);
          }
        else if (type == msg_type_open_slf)
          {
          pty_fork_bin_bash(action_slf, randid,sock_fd, msg->buf,
                            g_net_rank, display_val);
          }
        else if (type == msg_type_open_ovs)
          {
          pty_fork_bin_bash(action_ovs, randid, sock_fd, msg->buf,
                            g_net_rank, display_val);
          }
        else if (type == msg_type_open_cmd)
          {
          pty_fork_bin_bash(action_cmd, randid, sock_fd, msg->buf,
                            g_net_rank, display_val);
          }
        else if (type == msg_type_open_crun)
          {
          pty_fork_bin_bash(action_crun, randid, sock_fd, msg->buf,
                            g_net_rank, display_val);
          }
        else
          {
          pty_fork_bin_bash(action_dae, randid, sock_fd, msg->buf,
                            g_net_rank, display_val);
          }
        cli->has_pty_fork = 1;
        wrap_free(msg, __LINE__); 
        }
      break;

    case msg_type_win_size:
      pty_fork_msg_type_win_size(sock_fd, msg->len, msg->buf);
      wrap_free(msg, __LINE__); 
      break;

    case msg_type_x11_init:
      if (x11_init_cli_msg(randid, sock_fd, msg->buf,
                           g_net_rank, cli->x11_path))
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
  int result = g_sock_listen_fd;
  result = pty_fork_get_max_fd(result);
  result = cloonix_get_max_fd(result);
  result = x11_get_max_fd(result);
  while(cur)
    {
    if (cur->sock_idx_fd > result)
      result = cur->sock_idx_fd;
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
  if (!mdl_fd_is_valid(g_sock_listen_fd))
    KOUT("ERROR FD");
  FD_SET(g_sock_listen_fd, readfds);
  cur = g_cli_head;
  while(cur)
    {
    next = cur->next;
    if (cur->sock_val_fd >= 0)
      {
      if (cur->inhibited_associated)
        {
        cli_free(cur);
        }
      else if (cur->inhibited_to_be_destroyed)
        {
        cli_free(cur);
        }
      else if (!mdl_fd_is_valid(cur->sock_val_fd))
        {
        cli_free(cur);
        }
      else
        FD_SET(cur->sock_val_fd, readfds);
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
      if (!low_write_levels_above_thresholds(cur->sock_idx_fd))
        {
        ret = send_scp_to_cli(cur->randid, cur->scp_fd, cur->sock_idx_fd);
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
      KERR("ERROR %s", strerror(errno));
    }
  else
    {
    if (ret == 0)
      {
      g_timeout.tv_sec = 0;
      g_timeout.tv_usec = 10000;
      }
    if (FD_ISSET(g_sock_listen_fd, &readfds))
      listen_socket_action();
    cur = g_cli_head;
    while(cur)
      {
      next = cur->next;
      thread_tx_purge_el(cur->sock_idx_fd);
      if (FD_ISSET(cur->sock_val_fd, &readfds))
        mdl_read_fd((void *)cur, cur->sock_val_fd, rx_msg_cb, rx_err_cb);
      cur = next;
      }
    if (pty_fork_fdisset(&readfds, &writefds) == 0)
      {
      cloonix_fdisset(&readfds);
      x11_fdisset(&readfds, &writefds);
      }
    else
      KERR("WARNING CLOSED PTY");
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
static int check_and_set_uid(void)
{
  int result = -1;
  uid_t uid;
  if ((getuid() == 0) && getgid() == 0)
    {
    result = 0;
    }
  else
    {
    seteuid(0);
    setegid(0);
    uid = geteuid();
    if (uid == 0)
      {
      result = 0;
      umask(0000);
      if (setuid(0))
        KOUT(" ");
      if (setgid(0))
        KOUT(" ");
      }
    else
      KERR("ERROR uid %d", uid);
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int main(int argc, char **argv)
{
  struct timeval tv;
  char traffic_sock[MAX_PATH_LEN];
  char control_sock[MAX_PATH_LEN];

  if(argc != 3)
    KOUT("ERROR %d", argc);

  daemon(0,0);
  DEBUG_INIT(1);
  if (check_and_set_uid())
    KOUT("ERROR");
  g_nb_cli = 0;
  mdl_init();
  low_write_init();
  dialog_init();
  x11_init_display();
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(traffic_sock, 0, MAX_PATH_LEN);
  memset(control_sock, 0, MAX_PATH_LEN);
  snprintf(traffic_sock, MAX_PATH_LEN-1,
           "/var/lib/cloonix/%s/%s", argv[1], XWY_TRAFFIC_SOCK);
  snprintf(control_sock, MAX_PATH_LEN-1,
           "/var/lib/cloonix/%s/%s", argv[1], XWY_CONTROL_SOCK);
  snprintf(g_net_name, MAX_NAME_LEN-1, "%s", argv[1]);
  g_net_rank = atoi(argv[2]);
  g_sock_listen_fd = wrap_socket_listen_unix(traffic_sock,
                     fd_type_listen_unix_srv, __FUNCTION__);
  pty_fork_init(g_net_name, g_net_rank);
  cloonix_init(control_sock);
  gettimeofday(&g_last_heartbeat_tv, NULL);
  g_timeout.tv_sec = 0;
  g_timeout.tv_usec = 10000;
  while (1)
    {
    server_loop();
    gettimeofday(&tv, NULL);
    if (heartbeat_10th_second(&tv))
      {
      cloonix_timer_beat();
      mdl_heartbeat();
      }
    cloonix_beat(&tv);
    }
  return 0;
}
/*--------------------------------------------------------------------------*/
