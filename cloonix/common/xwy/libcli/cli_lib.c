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
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pty.h>
#include <unistd.h>
#include <termios.h>
#include <sys/epoll.h>
#include <sys/time.h>


#include "mdl.h"
#include "low_write.h"
#include "wrap.h"
#include "debug.h"
#include "x11_init.h"
#include "cli_lib.h"
#include "x11_cli.h"
#include "getsock.h"
#include "scp.h"
#include "x11_tx.h"
#include "glob_common.h"


static struct epoll_event *g_zero_fd_epev;
static struct epoll_event *g_win_chg_fd_epev;
static struct termios g_orig_term;
static struct termios g_cur_term;
static int g_win_chg_write_fd;
static int g_action;
static int g_main_epfd;
static char *g_bash_cmd;
static t_sock_fd_tx g_sock_fd_tx;
static t_sock_fd_ass_open g_sock_fd_ass_open;
static t_sock_fd_ass_close g_sock_fd_ass_close;
static uint16_t g_write_seqnum;
static int g_llid;
static int g_tid;
static int g_type;
static uint32_t g_randid;


/****************************************************************************/
static uint16_t get_write_seqnum(void)
{
  uint16_t result = g_write_seqnum;
  g_write_seqnum += 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void win_size_chg(int sig)
{
  char buf[1];
  buf[0] = 'w';
  if (write(g_win_chg_write_fd, buf, 1) == -1)
    KERR("Unable to write to g_win_chg_write_fd: %s", strerror(errno));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void xcli_send_msg_type_x11_connect_ack(int cli_idx, char *txt)
{
  int len = MAX_MSG_LEN+g_msg_header_len;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  int srv_idx = x11_cli_get_srv_idx(cli_idx);
  if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
    KOUT("%d %d", srv_idx, cli_idx);
  mdl_set_header_vals(msg, g_randid, msg_type_x11_connect_ack,
                      fd_type_cli, srv_idx, cli_idx);
  msg->len = sprintf(msg->buf, "%s", txt) + 1;
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_x11_init(uint32_t randid, char *magic)
{
  int len = MAX_MSG_LEN+g_msg_header_len;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg, randid, msg_type_x11_init, fd_type_cli, 0, 0);
  msg->len = sprintf(msg->buf, "%s", magic) + 1;
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_open_pty(int action, uint32_t randid,
                                   int srv_idx, char *cmd)
{
  int len = MAX_MSG_LEN+g_msg_header_len;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  int type;
  if (cmd)
    {
    if (strlen(cmd) > MAX_MSG_LEN - 1)
      KOUT("%d", strlen(cmd));
    if (action == action_crun)
      type = msg_type_open_crun;
    else if (action == action_cmd)
      type = msg_type_open_cmd;
    else
      type = msg_type_open_dae;
    mdl_set_header_vals(msg, randid, type, fd_type_cli, srv_idx, 0);
    msg->len = sprintf(msg->buf, "%s", cmd) + 1;
    }
  else
    {
    mdl_set_header_vals(msg, randid, msg_type_open_bash, fd_type_cli,
                        srv_idx, 0);
    msg->len = 0;
    }
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_win_size(uint32_t randid)
{
  int len = MAX_MSG_LEN+g_msg_header_len;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg, randid, msg_type_win_size, fd_type_cli, 0, 0);
  msg->len = sizeof(struct winsize);
  ioctl(0, TIOCGWINSZ, msg->buf);
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_randid(uint32_t randid)
{
  int len = g_msg_header_len;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg, randid, msg_type_randid, fd_type_cli, 0, 0);
  msg->len = 0;
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_end_ack(uint32_t randid)
{
  int len = MAX_MSG_LEN+g_msg_header_len;
  t_msg *msg;
  msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg,randid,msg_type_end_cli_pty_ack,fd_type_srv,0,0);
  msg->len = 0;
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void restore_term(void)
{
  tcsetattr(0, TCSADRAIN, &g_orig_term);
  printf("\033[?25h\r\n");
  fflush(stdout);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void config_term(void)
{
  if (atexit(restore_term))
    KOUT("%s", strerror(errno));
  memset(&g_cur_term, 0, sizeof(struct termios));
  g_cur_term.c_iflag |= IGNPAR;
  g_cur_term.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
  g_cur_term.c_iflag &= ~(IXON|IXOFF|IXANY);
  g_cur_term.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ISIG|ICANON|IEXTEN);
  g_cur_term.c_cflag &= ~(CSIZE|PARENB);
  g_cur_term.c_cflag |= CS8;
  g_cur_term.c_oflag &= ~(OPOST);
  g_cur_term.c_cc[VMIN] = 1;
  g_cur_term.c_cc[VTIME] = 0;
  tcsetattr(0, TCSADRAIN, &g_cur_term);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_bash_err_cb (void *ptr, int llid, int fd, char *err)
{
  (void) ptr;
  (void) fd;
  KOUT("ERROR %s", err);
  exit(0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timout_end_connect(void *data)
{
  exit(0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_bash_msg_cb(void *ptr, int llid, int fd, t_msg *msg)
{
  (void) ptr;
  int type, from, srv_idx, cli_idx;
  uint32_t randid;
  mdl_get_header_vals(msg, &randid, &type, &from, &srv_idx, &cli_idx);
  DEBUG_DUMP_RXMSG(msg);
  if (randid != xcli_get_randid())
    KERR("%08X %08X", randid, xcli_get_randid());
  else switch(type)
    {

    case msg_type_data_cli_pty:
      if (low_write_raw(1, msg, 0))
        KOUT(" ");
      break;

    case msg_type_end_cli_pty:
      low_write_fd(1);
      send_msg_type_end_ack(g_randid);
      cloonix_timeout_add(50, timout_end_connect, NULL);
      break;

    case msg_type_x11_init:
      if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
        KOUT("%d %s", srv_idx, msg->buf);
      x11_init_resp(srv_idx, msg);
      send_msg_type_open_pty(g_action, g_randid, srv_idx, g_bash_cmd);
      if ((g_action == action_bash) || 
          (g_action == action_dae)  ||
          (g_action == action_crun)  ||
          (g_action == action_cmd))
        send_msg_type_win_size(g_randid);
      else
        KOUT("%d", g_action);
      break;

    case msg_type_x11_info_flow:
    case msg_type_x11_connect:
    case msg_type_randid_associated_ack:
      if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
        KOUT("%d %d", srv_idx, cli_idx);
      rx_x11_msg_cb(randid, llid, type, srv_idx, cli_idx, msg);
      break;

    default:
      KOUT("%ld", type);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void zero_input_rx(void)
{
  int len;
  t_msg *msg;
  len = MAX_MSG_LEN+g_msg_header_len;
  msg = (t_msg *) wrap_malloc(len);
  len = wrap_read_zero(0, msg->buf, MAX_MSG_LEN);
  if (len <= 0)
    KOUT(" ");
  mdl_set_header_vals(msg, g_randid, msg_type_data_pty, fd_type_pty,0,0);
  msg->len = len;
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void win_chg_input_rx(int win_chg_fd)
{
  char buf[16];
  if (read(win_chg_fd, buf, sizeof(buf)) == -1)
    KERR("Could not read from win_chg_fd: %s", strerror(errno));
  send_msg_type_win_size(g_randid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cli_warn(int sig)
{
  KERR("%d", sig);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int setup_win_pipe(int *win_chg_fd)
{
  int pipe_fd[2], result = -1;
  if (wrap_pipe(pipe_fd, fd_type_pipe_win_chg, __FUNCTION__) < 0)
    printf("Error pipe\n");
  else if (tcgetattr(0, &g_orig_term) < 0)
    printf("Error No pty\n");
  else
    {
    g_win_chg_write_fd = pipe_fd[1];
    *win_chg_fd = pipe_fd[0];
    config_term();

    if (signal(SIGPIPE, cli_warn) == SIG_ERR)
      KERR("%s", strerror(errno));

    if (signal(SIGWINCH, win_size_chg) == SIG_ERR)
      KERR("%s", strerror(errno));
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
uint32_t xcli_get_randid(void)
{
  return g_randid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int xcli_get_main_epfd(void)
{
  return g_main_epfd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void xcli_send_randid_associated_begin(int cli_idx)
{
  g_sock_fd_ass_open(cli_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void xcli_send_randid_associated_end(int llid, int tid, int type, int cli_idx)
{
  int len = g_msg_header_len;
  t_msg *msg;
  int srv_idx = x11_cli_get_srv_idx(cli_idx);
  if (srv_idx < 0)
    {
    KERR("%d", cli_idx);
    g_sock_fd_ass_close(cli_idx);
    }
  else
    {
    x11_cli_set_params(cli_idx, llid, tid, type);
    msg = (t_msg *) wrap_malloc(len);
    mdl_set_header_vals(msg, g_randid, msg_type_randid_associated,
                        fd_type_cli, srv_idx, cli_idx);
    msg->len = 0;
    mdl_prepare_header_msg(0, msg);
    g_sock_fd_tx(llid, tid, type, len, (char *) msg);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void xcli_fct_before_epoll(int epfd)
{
  int win_chg_fd, zero_fd;

  if ((g_action == action_put) && (scp_get_fd() >= 0))
    {
    scp_send_data();
    }
  if ((g_action == action_bash) ||
      (g_action == action_dae)  ||
      (g_action == action_crun)  ||
      (g_action == action_cmd))
    {
    if (g_action != action_dae)
      {
      zero_fd = g_zero_fd_epev->data.fd;
      win_chg_fd = g_win_chg_fd_epev->data.fd;
      g_zero_fd_epev->events = EPOLLIN;
      if (epoll_ctl(epfd, EPOLL_CTL_MOD, zero_fd, g_zero_fd_epev))
        KOUT(" ");
      g_win_chg_fd_epev->events = EPOLLIN;
      if (epoll_ctl(epfd, EPOLL_CTL_MOD, win_chg_fd, g_win_chg_fd_epev))
        KOUT(" ");
      }
    x11_fd_epollin_epollout_setup();
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void xcli_x11_x11_rx(int llid_ass, int tid, int type, int len, char *buf)
{
  g_sock_fd_tx(llid_ass, tid, type, len, buf);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void xcli_x11_doors_rx(int cli_idx, int len, char *buf)
{
  x11_cli_write_to_x11(cli_idx, len, buf);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void xcli_traf_doors_rx(int llid, int len, char *buf)
{
  if ((g_action == action_bash) ||
      (g_action == action_crun)  ||
      (g_action == action_dae)  ||
      (g_action == action_cmd))
    mdl_read_data(NULL, llid, 0, len, buf, rx_bash_msg_cb, rx_bash_err_cb);
  else
    mdl_read_data(NULL, llid, 0, len, buf, rx_scp_msg_cb, rx_scp_err_cb);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int xcli_fct_after_epoll(int nb, struct epoll_event *events)
{
  int i, fd, result = 0;
  uint32_t evts; 

  if ((g_action == action_bash) ||
      (g_action == action_crun)  ||
      (g_action == action_dae)  ||
      (g_action == action_cmd))
    {
    for(i=0; i<nb; i++)
      {
      fd = events[i].data.fd;
      evts = events[i].events;
      if (evts & EPOLLIN)
        {
        if (g_action != action_dae)
          {
          if (fd == g_win_chg_fd_epev->data.fd)
            win_chg_input_rx(fd);
            result += 1;
          if (fd == g_zero_fd_epev->data.fd)
            zero_input_rx();
            result += 1;
          }
        }
      result += x11_fd_epollin_epollout_action(evts, fd);
      }
    if (low_write_fd(1))
      KOUT(" ");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int is_valid_fd(int fd)
{
  return fcntl(fd, F_GETFL) != -1 || errno != EBADF;
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
void xcli_init(int epfd, int llid, int tid, int type,
               t_sock_fd_tx soc_tx, t_sock_fd_ass_open sock_fd_ass_open,
               t_sock_fd_ass_close sock_fd_ass_close, int action,
               char *src, char *dst, char *cmd)
{
  struct timeval tv;
  int win_chg_fd;

  if (!is_valid_fd(0))
    KERR("ERROR 0 is NOT VALID");

  if (!is_valid_fd(epfd))
    KERR("ERROR %d is NOT VALID", epfd);
  g_zero_fd_epev = NULL;
  g_win_chg_fd_epev = NULL;
  g_llid = llid;
  g_tid = tid;
  g_type = type;
  g_write_seqnum = 0;
  g_sock_fd_tx = soc_tx;
  g_sock_fd_ass_open = sock_fd_ass_open;
  g_sock_fd_ass_close = sock_fd_ass_close;
  g_action = action;
  g_bash_cmd = cmd;
  g_main_epfd = epfd;
  mdl_init();
  low_write_init();
  if (action != action_dae)
    {
    g_zero_fd_epev = wrap_epoll_event_alloc(epfd, 0, 1);
    if (g_zero_fd_epev == NULL)
      KOUT(" ");
    }
  gettimeofday(&tv, NULL);
  srand((int)tv.tv_usec);
  g_randid = (uint32_t) rand();
  x11_cli_init(sock_fd_ass_close);
  x11_init_magic();
  x11_tx_init();
  mdl_open(0, fd_type_cli, NULL, NULL);
  mdl_open(1, fd_type_one, wrap_write_one, wrap_read_kout);
  send_msg_type_randid(g_randid);
  if ((action == action_bash) ||
      (action == action_dae)  ||
      (action == action_crun)  ||
      (action == action_cmd))
    {
    if (action == action_dae)
      {
      daemon(0, 0);
      send_msg_type_x11_init(g_randid, get_x11_magic());
      }
    else
      {
      send_msg_type_x11_init(g_randid, get_x11_magic());
      if (setup_win_pipe(&win_chg_fd))
        KOUT(" ");
      g_win_chg_fd_epev = wrap_epoll_event_alloc(epfd, win_chg_fd, 2);
      if (g_win_chg_fd_epev == NULL)
        KOUT(" ");
      }
    }
  else if (action == action_get)
    {
    scp_send_get(llid, tid, type, g_sock_fd_tx, src, dst);
    }
  else if (action == action_put)
    {
    scp_send_put(llid, tid, type, g_sock_fd_tx, src, dst);
    }
  else
    KOUT("%d", action);
}
/*--------------------------------------------------------------------------*/



