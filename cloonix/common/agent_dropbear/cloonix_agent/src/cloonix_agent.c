/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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


#include "sock.h"
#include "commun.h"
#include "x11_channels.h"
#include "nonblock.h"

static int  g_time_count;
static int  g_fd_virtio;
static char g_buf[MAX_A2D_LEN];




typedef struct t_rx_pktbuf
{
  char buf[MAX_A2D_LEN];
  int  offset;
  int  paylen;
  int  dido_llid;
  int  type;
  int  val;
  char *payload;
} t_rx_pktbuf;

t_rx_pktbuf g_rx_pktbuf;


char *get_g_buf(void)
{
  return g_buf;
}


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
  res = action_get_biggest_fd();
  if (res > result)
    result = res;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void send_to_virtio(int dido_llid, int len, int type, int var, char *buf)
{
  char *payload;
  int headsize = sock_header_get_size();
  if (len > MAX_A2D_LEN - headsize)
    KOUT("%d", len);
  sock_header_set_info(g_buf, dido_llid, len, type, var, &payload);
  if (g_buf != buf)
    KOUT("%p %p", g_buf, buf);
  nonblock_send(g_fd_virtio, g_buf, len + headsize);
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
  memcpy(rx_pktbuf->buf+rx_pktbuf->offset, buf, len_chosen);
  rx_pktbuf->offset += len_chosen;
  *len -= len_chosen;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_pktbuf_get_paylen(t_rx_pktbuf *rx_pktbuf)
{
  int result = 0;
  if (sock_header_get_info(rx_pktbuf->buf,
                           &(rx_pktbuf->dido_llid), &(rx_pktbuf->paylen),
                           &(rx_pktbuf->type), &(rx_pktbuf->val),
                           &(rx_pktbuf->payload)))
    {
    KERR("NOT IN SYNC");
    rx_pktbuf->offset = 0;
    rx_pktbuf->paylen = 0;
    rx_pktbuf->payload = NULL;
    rx_pktbuf->dido_llid = 0;
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
    action_rx_virtio(rx_pktbuf->dido_llid, rx_pktbuf->paylen,
                     rx_pktbuf->type, rx_pktbuf->val, rx_pktbuf->payload);
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
  nonblock_prepare_fd_set(g_fd_virtio, outfd);
  x11_prepare_fd_set(infd, outfd);
  action_prepare_fd_set(infd, outfd);
  result = get_biggest_fd();
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
    nonblock_process_events(&outfd);
    if (FD_ISSET(g_fd_virtio, &infd))
      events_from_virtio(g_fd_virtio);
    x11_process_events(&infd);
    action_events(&infd);
    }
  cur_sec = my_second();
  if (cur_sec != g_time_count)
    {
    g_time_count = cur_sec;
    action_heartbeat(cur_sec);
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
int main(int argc, char *argv[])
{ 
  daemon(0,0);
  nonblock_init();
  action_init();
  x11_init();
  g_time_count = 0;
  g_fd_virtio = sock_open_virtio_port(VIRTIOPORT);
  if (g_fd_virtio < 0)
    KOUT("Bad Virtio port %s", VIRTIOPORT);
  my_mkdir(UNIX_X11_SOCKET_DIR);
  purge();
  no_signal_pipe();
  nonblock_add_fd(g_fd_virtio);
  for (;;)
    select_wait_and_switch();
  return 0;
}
/*--------------------------------------------------------------------------*/
