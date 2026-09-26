/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/ioctl.h>
#ifndef __FreeBSD__
#include <asm/types.h>
#include <linux/sockios.h>
#endif
#include <sys/time.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "glob_common.h"
#include "rx_pktbuf.h"
#include "sock.h"
#include "commun.h"
#include "nonblock.h"
#include "cxp_wire.h"

static int  g_time_count;
static int  g_fd_virtio;
static int  g_rand_id;
static int  g_virtio_ready;
static int  g_virtio_full;
static int  g_fd_kerr = -1;
static int  g_in_forward;
static unsigned long g_kerr_lost;
static unsigned long g_stall_written;
static int  g_stall_sec;

t_rx_pktbuf g_rx_pktbuf;

static int get_biggest_fd(void)
{
  int res, result = 0;
  if (g_fd_virtio > result)
    result = g_fd_virtio;
  res = action_get_biggest_fd();
  if (res > result)
    result = res;
  if (wifi_get_fd() > result)
    result = wifi_get_fd();
  if (g_fd_kerr > result)
    result = g_fd_kerr;
  return result;
}

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

/* Past VIRTIO_TX_HIGH queued for the host the pipes are not read and the
   wifi frames are dropped, until under VIRTIO_TX_LOW. */
#define VIRTIO_TX_HIGH (8 * 1024 * 1024)
#define VIRTIO_TX_LOW  (1 * 1024 * 1024)
#define VIRTIO_STALL_SEC 10

int virtio_tx_high(void)
{
  return (nonblock_queued(g_fd_virtio) > VIRTIO_TX_HIGH);
}

/* A KERR line to the host, for the syslog of the net; one the port cannot
   take now is counted and said with the next one. */
static void kerr_to_host(const char *text)
{
  char buf[3072], note[80];
  int len, headsize = sock_header_get_size();
  if (g_in_forward)
    return;
  if ((!g_virtio_ready) || virtio_tx_high())
    {
    g_kerr_lost += 1;
    return;
    }
  g_in_forward = 1;
  note[0] = 0;
  if (g_kerr_lost)
    snprintf(note, sizeof(note), " [%lu line(s) before it not sent to the "
             "host]", g_kerr_lost);
  len = snprintf(buf + headsize, sizeof(buf) - headsize, "%s%s", text, note);
  if (len > (int) sizeof(buf) - headsize - 1)
    len = sizeof(buf) - headsize - 1;
  while ((len > 0) && (buf[headsize + len - 1] == '\n'))
    buf[headsize + --len] = 0;
  if (len > 0)
    {
    send_to_virtio(0, 0, 0, header_type_kerr, header_val_none, len + 1, buf);
    g_kerr_lost = 0;
    }
  g_in_forward = 0;
}

/* Called by cloonix_log: the line is in the syslog of the guest already. */
void cloonix_log_forward(const char *line)
{
  const char *p = line;
  int kill = !strncmp(p, "KERR KILL", 9);
  if (!strncmp(p, "KERR: ", 6))
    p += 6;
  if (!strncmp(p, "- ", 2))
    p += 2;
  kerr_to_host(p);
  if (kill && g_virtio_ready)
    nonblock_flush(g_fd_virtio, 1000);
}

static void kerr_sock_open(void)
{
  struct sockaddr_un addr;
  int fd;
  if ((mkdir("/cloonixmnt", 0755)) && (errno != EEXIST))
    KERR("WARNING mkdir /cloonixmnt: %s", strerror(errno));
  if ((mkdir("/cloonixmnt/tmp", 0755)) && (errno != EEXIST))
    KERR("WARNING mkdir /cloonixmnt/tmp: %s", strerror(errno));
  unlink(UNIX_KERR_SOCK);
  fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0)
    {
    KERR("WARNING socket for %s: %s", UNIX_KERR_SOCK, strerror(errno));
    return;
    }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, UNIX_KERR_SOCK, sizeof(addr.sun_path) - 1);
  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)))
    {
    KERR("WARNING bind %s: %s, the exec server lines stay in the guest",
         UNIX_KERR_SOCK, strerror(errno));
    close(fd);
    return;
    }
  g_fd_kerr = fd;
}

/* the lines of the exec server, already in the syslog of the guest */
static void kerr_sock_rx(void)
{
  char rx[1024], line[1100];
  int i, len;
  for (i = 0; i < 16; i++)
    {
    len = recv(g_fd_kerr, rx, sizeof(rx) - 1, 0);
    if (len <= 0)
      break;
    rx[len] = 0;
    snprintf(line, sizeof(line), "cloonix-cxp-srv: %s", rx);
    kerr_to_host(line);
    }
}

/* bytes waiting for the host while none was written for VIRTIO_STALL_SEC */
static void virtio_stall_check(void)
{
  int q = nonblock_queued(g_fd_virtio);
  unsigned long w = nonblock_written(g_fd_virtio);
  if ((q == 0) || (w != g_stall_written))
    g_stall_sec = 0;
  else
    {
    g_stall_sec += 1;
    if (g_stall_sec == VIRTIO_STALL_SEC)
      KERR("WARNING the virtio port took nothing for %d s, %d bytes queued "
           "for the host", VIRTIO_STALL_SEC, q);
    }
  g_stall_written = w;
}

static time_t my_second(void)
{
  static time_t offset = 0; 
  struct timeval tv;
  gettimeofday(&tv, NULL);
  if (offset == 0)
    offset = tv.tv_sec;
  return (tv.tv_sec - offset);
}

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

static void virtio_frame(void *ctx, t_rx_pktbuf *rp)
{
  action_rx_virtio(rp->dido_llid, rp->vwifi_base, rp->vwifi_cid,
                   rp->type, rp->val, rp->paylen, rp->payload);
}

static void virtio_desync(void *ctx)
{
  KERR("WARNING the stream from the host lost its sync, resynchronised");
}

static void events_from_virtio(int fd)
{
  char *rx;
  int len = process_fd_virtio(fd, &rx);
  if (len > 0)
    rx_pktbuf_feed(&g_rx_pktbuf, len, rx, NULL, virtio_frame, virtio_desync);
}

static int prepare_fd_set(fd_set *infd, fd_set *outfd)
{
  int result;
  FD_ZERO(infd);
  FD_ZERO(outfd);
  if ((!g_virtio_full) && virtio_tx_high())
    g_virtio_full = 1;
  else if ((g_virtio_full) &&
           (nonblock_queued(g_fd_virtio) < VIRTIO_TX_LOW))
    g_virtio_full = 0;
  FD_SET(g_fd_virtio, infd);
  nonblock_prepare_fd_set(g_fd_virtio, outfd);
  action_prepare_fd_set(infd, outfd, g_virtio_full);
  if (wifi_get_fd() >= 0)
    FD_SET(wifi_get_fd(), infd);
  if (g_fd_kerr >= 0)
    FD_SET(g_fd_kerr, infd);
  result = get_biggest_fd();
  return result;
}

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
    action_events(&infd);
    wifi_events(&infd);
    if ((g_fd_kerr >= 0) && FD_ISSET(g_fd_kerr, &infd))
      kerr_sock_rx();
    }
  cur_sec = my_second();
  if (cur_sec != g_time_count)
    {
    g_time_count = cur_sec;
    action_heartbeat(cur_sec);
    virtio_stall_check();
    }
}

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

/* a write to a closed socket fails with EPIPE, handled where it is done */
static void no_signal_pipe(void)
{
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &act, NULL);
}

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
  if (daemon(0,0))
    KOUT("ERROR daemon %s", strerror(errno));
  umask(0000);
  nonblock_init();
  action_init();
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
  purge();
  no_signal_pipe();
  nonblock_add_fd(g_fd_virtio);
  g_virtio_ready = 1;
  kerr_sock_open();
  for (;;)
    select_wait_and_switch();
  return 0;
}
