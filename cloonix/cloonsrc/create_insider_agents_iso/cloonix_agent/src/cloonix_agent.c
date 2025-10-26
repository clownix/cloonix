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
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <asm/types.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <sys/time.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



#include "glob_common.h"
#include "sock.h"
#include "commun.h"
#include "x11_channels.h"
#include "nonblock.h"
#include "vwifi.h"

static int  g_time_count;
static int  g_vwifi_run;
static int  g_fd_virtio;
static int  g_fd_listen_vwifi_cli;
static int  g_fd_listen_vwifi_spy;
static int  g_fd_listen_vwifi_ctr;
static int  g_rand_id;



t_rx_pktbuf g_rx_pktbuf;


/****************************************************************************/
void my_mkdir(char *dst_dir)
{
  struct stat stat_file;
  if (mkdir(dst_dir, 0777))
    {
    if (errno != EEXIST)
      KOUT("%s, %d", dst_dir, errno);
    else
      {
      if (stat(dst_dir, &stat_file))
        KOUT("%s, %d", dst_dir, errno);
      if (!S_ISDIR(stat_file.st_mode))
        {
        unlink(dst_dir);
        if (mkdir(dst_dir, 0777))
          KOUT("%s, %d", dst_dir, errno);
        }
      }
    }
  sync();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_biggest_fd(void)
{
  int res, result = x11_get_biggest_fd();
  if (g_fd_virtio > result)
    result = g_fd_virtio;
  if (g_fd_listen_vwifi_cli > result)
    result = g_fd_listen_vwifi_cli;
  if (g_fd_listen_vwifi_spy > result)
    result = g_fd_listen_vwifi_spy;
  if (g_fd_listen_vwifi_ctr > result)
    result = g_fd_listen_vwifi_ctr;
  res = action_get_biggest_fd();
  if (res > result)
    result = res;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void send_to_virtio(int dido_llid, int vwifi_base, int vwifi_cid,
                    int type, int var, int len, char *buf)
{
  char *payload;
  int headsize = sock_header_get_size();
  if (len > MAX_A2D_LEN - headsize)
    KOUT("%d", len);
  sock_header_set_info(buf, dido_llid,
                       vwifi_base, vwifi_cid,
                       type, var, len, &payload);
  nonblock_send(g_fd_virtio, buf, len + headsize);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static time_t my_second(void)
{
  static time_t offset = 0; 
  struct timeval tv;
  gettimeofday(&tv, NULL);
  if (offset == 0)
    offset = tv.tv_sec;
  return (tv.tv_sec - offset);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int process_fd_virtio(int fd, char **msgrx)
{
  static char rx[MAX_A2D_LEN];
  int  headsize = sock_header_get_size();
  int len;
  *msgrx = rx;
  len = read(fd, rx, MAX_A2D_LEN-headsize-1);
  if (len == 0)
    KOUT(" ");
  if (len < 0)
    {
    if ((errno != EAGAIN) && (errno != EINTR))
      KOUT("%d", errno);
    }
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_pktbuf_fill(int *len, char  *buf, t_rx_pktbuf *rx_pktbuf)
{
  int result, headsize = sock_header_get_size();
  int len_chosen, len_desired, len_avail = *len;
  if (rx_pktbuf->offset < headsize)
    {
    len_desired = headsize - rx_pktbuf->offset;
    if (len_avail >= len_desired)
      {
      len_chosen = len_desired;
      result = 1;
      }
    else
      {
      len_chosen = len_avail;
      result = 2;
      }
    }
  else
    {
    if (rx_pktbuf->paylen <= 0)
      KOUT(" ");
    len_desired = headsize + rx_pktbuf->paylen - rx_pktbuf->offset;
    if (len_avail >= len_desired)
      {
      len_chosen = len_desired;
      result = 3;
      }
    else
      {
      len_chosen = len_avail;
      result = 2;
      }
    }
  if (len_chosen + rx_pktbuf->offset > MAX_A2D_LEN)
    KOUT("%d %d", len_chosen, rx_pktbuf->offset);
  memcpy(rx_pktbuf->rawbuf+rx_pktbuf->offset, buf, len_chosen);
  rx_pktbuf->offset += len_chosen;
  *len -= len_chosen;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_pktbuf_get_paylen(t_rx_pktbuf *rx_pktbuf)
{
  int result = 0;
  if (sock_header_get_info(rx_pktbuf->rawbuf, &(rx_pktbuf->dido_llid),
                           &(rx_pktbuf->vwifi_base), &(rx_pktbuf->vwifi_cid),
                           &(rx_pktbuf->type), &(rx_pktbuf->val),
                           &(rx_pktbuf->paylen), &(rx_pktbuf->payload)))
    {
    KERR("NOT IN SYNC");
    rx_pktbuf->offset = 0;
    rx_pktbuf->paylen = 0;
    rx_pktbuf->payload = NULL;
    rx_pktbuf->dido_llid = 0;
    rx_pktbuf->vwifi_base = 0;
    rx_pktbuf->vwifi_cid = 0;
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_pktbuf_process(t_rx_pktbuf *rx_pktbuf)
{
  if (!rx_pktbuf->payload)
    KOUT(" ");
  if (rx_pktbuf->type == header_type_x11)
    x11_write(rx_pktbuf->val, rx_pktbuf->paylen, rx_pktbuf->payload);
  else
    action_rx_virtio(rx_pktbuf->dido_llid,
                     rx_pktbuf->vwifi_base, rx_pktbuf->vwifi_cid,
                     rx_pktbuf->type, rx_pktbuf->val,
                     rx_pktbuf->paylen, rx_pktbuf->payload);
  rx_pktbuf->offset = 0;
  rx_pktbuf->paylen = 0;
  rx_pktbuf->payload = NULL;
  rx_pktbuf->dido_llid = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_pktbuf_virtio(int len, char  *buf)
{
  int res, len_done, len_left_to_do = len;
  while (len_left_to_do)
    {
    len_done = len - len_left_to_do;
    res = rx_pktbuf_fill(&len_left_to_do, buf + len_done, &(g_rx_pktbuf));
    if (res == 1)
      {
      if (rx_pktbuf_get_paylen(&(g_rx_pktbuf)))
        break;
      }
    else if (res == 2)
      {
      }
    else if (res == 3)
      {
      rx_pktbuf_process(&(g_rx_pktbuf));
      }
    else
      KOUT("%d", res);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void events_from_virtio(int fd)
{
  char *rx;
  int len = process_fd_virtio(fd, &rx);
  if (len > 0)
    rx_pktbuf_virtio(len, rx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int prepare_fd_set(fd_set *infd, fd_set *outfd)
{
  int result;
  FD_ZERO(infd);
  FD_ZERO(outfd);
  FD_SET(g_fd_virtio, infd);
  if (g_vwifi_run)
    {
    if (!vwifi_listen_locked(g_fd_listen_vwifi_cli))
      FD_SET(g_fd_listen_vwifi_cli, infd);
    if (!vwifi_listen_locked(g_fd_listen_vwifi_spy))
      FD_SET(g_fd_listen_vwifi_spy, infd);
    if (!vwifi_listen_locked(g_fd_listen_vwifi_ctr))
      FD_SET(g_fd_listen_vwifi_ctr, infd);
    }
  nonblock_prepare_fd_set(g_fd_virtio, outfd);
  x11_prepare_fd_set(infd, outfd);
  action_prepare_fd_set(infd, outfd);
  result = get_biggest_fd();
  if (g_vwifi_run)
    {
    result = vwifi_trafic_fd_set(infd, result);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void select_wait_and_switch(void)
{
  fd_set infd, outfd;
  int fd_max, result;
  time_t cur_sec;
  static struct timeval timeout;
  fd_max = prepare_fd_set(&infd, &outfd);
  result = select(fd_max + 1, &infd, &outfd, NULL, &timeout);
  if ( result < 0 )
    KOUT(" ");
  else if (result == 0) 
    {
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    }
  else 
    {
    if (g_vwifi_run)
      vwifi_trafic_fd_isset(&infd);
    nonblock_process_events(&outfd);
    if (FD_ISSET(g_fd_virtio, &infd))
      events_from_virtio(g_fd_virtio);
    x11_process_events(&infd);
    action_events(&infd);
    if (g_vwifi_run)
      {
      if (FD_ISSET(g_fd_listen_vwifi_cli, &infd))
        vwifi_client_syn_cli(g_rand_id, g_fd_listen_vwifi_cli);
      if (FD_ISSET(g_fd_listen_vwifi_spy, &infd))
        vwifi_client_syn_spy(g_rand_id, g_fd_listen_vwifi_spy);
      if (FD_ISSET(g_fd_listen_vwifi_ctr, &infd))
        vwifi_client_syn_ctr(g_rand_id, g_fd_listen_vwifi_ctr);
      }
    }
  cur_sec = my_second();
  if (cur_sec != g_time_count)
    {
    g_time_count = cur_sec;
    action_heartbeat(cur_sec);
    vwifi_heartbeat(cur_sec);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void purge(void)
{
  char *rx;
  int result;
  fd_set infd;
  struct timeval timeout;
  for (;;)
    {
    timeout.tv_sec = 0;
    timeout.tv_usec = 5000;
    FD_ZERO(&infd);
    FD_SET(g_fd_virtio, &infd);
    result = select(g_fd_virtio + 1, &infd, NULL, NULL, &timeout);
    if ( result < 0 )
      KOUT(" ");
    else if (result == 0)
      break;
    else if (FD_ISSET(g_fd_virtio, &infd))
      {
      process_fd_virtio(g_fd_virtio, &rx);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void signal_pipe(int no_use)
{
  KERR("PIPE");
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
static void no_signal_pipe(void)
{
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  sigfillset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = signal_pipe;
  sigaction(SIGPIPE, &act, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int is_a_crun(void)
{
  struct stat sb;
  int result = 0;
  if (lstat("/cloonixmnt/mountbear", &sb) != -1)
    {
    if ((sb.st_mode & S_IFMT) == S_IFDIR)
      result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void vwifi_start_ok(void)
{
  g_vwifi_run = 1;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{ 
  int fd_listen;
  fd_set infd;
  struct timeval last_tv;
  gettimeofday(&last_tv, NULL);
  srand((int) (last_tv.tv_usec & 0xFFFF));
  g_rand_id = 0;
  while (g_rand_id < 0x100)
    g_rand_id = rand() & 0xF00;
  g_vwifi_run = 0;
  daemon(0,0);
  umask(0000);
  nonblock_init();
  action_init();
  x11_init();
  g_time_count = 0;
  if (is_a_crun())
    {
    fd_listen = util_socket_listen_unix("/cloonixmnt/mountbear/sock");
    if (fd_listen < 0)
      KOUT("Bad crun socket");
    FD_ZERO(&infd);
    FD_SET(fd_listen, &infd); 
    if (select(fd_listen + 1, &infd, NULL, NULL, NULL) <= 0)
      KOUT("Bad crun socket");
    if (!FD_ISSET(fd_listen, &infd))
      KOUT("Bad crun socket");
    g_fd_virtio = sock_fd_accept(fd_listen);
    if (g_fd_virtio < 0)
      KOUT("Bad Virtio port %s", VIRTIOPORT);
    }
  else
    {
    g_fd_virtio = sock_open_virtio_port(VIRTIOPORT);
    if (g_fd_virtio < 0)
      KOUT("Bad Virtio port %s", VIRTIOPORT);
    }
  my_mkdir(X11_DISPLAY_DIR);
  purge();
  no_signal_pipe();
  nonblock_add_fd(g_fd_virtio);
  vwifi_init(g_rand_id);
  unlink(UNIX_VWIFI_CLI);
  unlink(UNIX_VWIFI_SPY);
  unlink(UNIX_VWIFI_CTR);
  g_fd_listen_vwifi_cli = util_socket_listen_unix(UNIX_VWIFI_CLI);
  if (g_fd_listen_vwifi_cli == -1)
    KERR("ERROR LISTEN %s", UNIX_VWIFI_CLI);
  g_fd_listen_vwifi_spy = util_socket_listen_unix(UNIX_VWIFI_SPY);
  if (g_fd_listen_vwifi_spy == -1)
    KERR("ERROR LISTEN %s", UNIX_VWIFI_SPY);
  g_fd_listen_vwifi_ctr = util_socket_listen_unix(UNIX_VWIFI_CTR);
  if (g_fd_listen_vwifi_ctr == -1)
    KERR("ERROR LISTEN %s", UNIX_VWIFI_CTR);
  for (;;)
    select_wait_and_switch();
  return 0;
}
/*--------------------------------------------------------------------------*/
