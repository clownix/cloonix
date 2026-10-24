/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/ioctl.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "util_sock.h"
#include "rxtx.h"
#include "udp.h"

static char g_proxy_unix_dgram_rx[MAX_PATH_LEN];
static char g_proxy_unix_dgram_tx[MAX_PATH_LEN];
static int g_llid_proxymous;
static int g_fd_proxymous;
static uint8_t g_rx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_tx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static int g_traffic_mngt;
void reply_probe_udp(uint8_t probe_idx);

char *get_net_name(void);
char *get_c2c_name(void);
 
static int g_fd_dgram_tx = -1;
static unsigned long g_nb_tx_full;

/* One line per ten seconds for a repeating condition. */
static int say_now(uint64_t *last)
{
  uint64_t now = cloonix_get_msec();
  if (now - *last < 10000)
    return 0;
  *last = now;
  return 1;
}

/* Non blocking unix datagrams (blocking deadlocked both ends): a full queue
   drops the frame, as udp does. */
static void dgram_buffers(int fd)
{
  int sz = 4 * 1024 * 1024;
  nonblock_fd(fd);
  setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz));
  setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz));
}

/* sig: a probe or reply must pass, so a bounded wait; data frames keep
   being dropped. */
static int transmit_unix_dgram_tx(char *dgram_tx, int len, char *data, int sig)
{
  int tries = 0;
  struct sockaddr_un name;
  struct sockaddr *pname = (struct sockaddr *)&name;
  int len_tx, len_un = sizeof(struct sockaddr_un);
  int result=0;
  char *c2c = get_c2c_name();
  if (len >= MAX_DGRAM_LEN)
    KOUT("ERROR SOCK_DGRAM LEN %s %d %s", c2c, len, dgram_tx);
  if (g_fd_dgram_tx == -1)
    {
    g_fd_dgram_tx = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (g_fd_dgram_tx < 0)
      KOUT("ERROR SOCK_DGRAM %s %s", c2c, dgram_tx);
    dgram_buffers(g_fd_dgram_tx);
    }
  memset(&name, 0, sizeof(struct sockaddr_un));
  name.sun_family = AF_UNIX;
  strncpy(name.sun_path, dgram_tx, 107);
  len_tx = sendto(g_fd_dgram_tx, data, len, 0, pname, len_un);
  while (sig && (len_tx == -1) && ((errno == EAGAIN) || (errno == ENOBUFS))
         && (tries++ < 500))
    {
    usleep(1000);
    len_tx = sendto(g_fd_dgram_tx, data, len, 0, pname, len_un);
    }
  if ((len_tx == -1) && ((errno == EAGAIN) || (errno == ENOBUFS)))
    {
    static uint64_t last;
    g_nb_tx_full += 1;
    if (say_now(&last))
      KERR("WARNING %s: proxymous is not taking frames, %lu dropped so "
           "far at its socket", c2c, g_nb_tx_full);
    return -1;
    }
  if (len_tx != len)
    {
    KERR("ERROR  %s %d %d %d %s", c2c, len_tx, len, errno, dgram_tx);
    result = -1;
    }
  return result;
}

int udp_tx_sig_send(int len, uint8_t *tx)
{
  int result = 0;
  char *c2c = get_c2c_name();
  if ((len <= 0) || (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR SEND %s  %d", c2c, len);
  fct_seqtap_tx(kind_seqtap_sig_hello, g_tx, 0, len, tx);
  if (g_llid_proxymous)
    {
    if (transmit_unix_dgram_tx(g_proxy_unix_dgram_tx, 
                               len+HEADER_TAP_MSG, (char *)g_tx, 1))
      {
      KERR("ERROR %s: a signalling frame of %d bytes could not be "
           "given to proxymous in 500 ms", c2c, len);
      result = -1;
      } 
    }
  else
    {
    KERR("ERROR %s  %d", c2c, len);
    result = -1;
    }
  return result;
}

int udp_tx_traf_send(int len, uint8_t *buf)
{
  static uint16_t seqtap=0;
  int result = 0;
  char *c2c = get_c2c_name();
  seqtap += 1;
  if ((len <= 0) || (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR SEND %s  %d", c2c, len);
  fct_seqtap_tx(kind_seqtap_data, g_tx, seqtap, len, buf);
  if (g_llid_proxymous)
    {
    if (transmit_unix_dgram_tx(g_proxy_unix_dgram_tx,
                               len+HEADER_TAP_MSG,  (char *) g_tx, 0))
      result = -1;
    }
  else
    {
    KERR("ERROR %s %d", c2c, len);
    result = -1;
    }
  return result;
}

static void err_cb(int llid, int err, int from)
{
  char *c2c = get_c2c_name();
  KERR("ERROR UDP  %s %d %d", c2c,  err, from);
  udp_close();
}

void udp_close(void)
{
  if (msg_exist_channel(g_llid_proxymous))
    msg_delete_channel(g_llid_proxymous);
  if (g_fd_proxymous != -1) 
    close(g_fd_proxymous);
  if (g_fd_dgram_tx != -1)
    close(g_fd_dgram_tx);
  g_fd_dgram_tx = -1;
  g_llid_proxymous = 0;
  g_fd_proxymous = -1;
  unlink(g_proxy_unix_dgram_rx);
  unlink(g_proxy_unix_dgram_tx);
}

int udp_get_traffic_mngt(void)
{
  return g_traffic_mngt;
}

void udp_enter_traffic_mngt(void)
{
  g_traffic_mngt = 1;
}

/* Where a gap happened: a queue near the receive buffer means this process
   fell behind, not the network. */
static void socket_fill(int fd, int *queued, int *rcvbuf)
{
  socklen_t len = sizeof(int);
  if (ioctl(fd, FIONREAD, queued))
    *queued = -1;
  if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, rcvbuf, &len))
    *rcvbuf = -1;
}

static int rx_dgram_one(int fd)
{
  static uint16_t seqtap=0;
  static int first_frame_seen;
  uint16_t seq;
  int len, buf_len, result;
  uint8_t *buf;
  char *c2c = get_c2c_name();
  uint8_t probe_idx;
  char probe[32];
  int max_len = HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN;

  len = read(fd, g_rx, max_len);
  if ((len < 0) && ((errno == EAGAIN) || (errno == EINTR)))
    return 0;
  if (len <= 0)
    KOUT("ERROR READ %s %d %d", c2c, len, errno);
  result = fct_seqtap_rx_udp(len, fd, g_rx, &seq, &buf_len, &buf);
  if (result == kind_seqtap_sig_hello)
    {
    memset(probe, 0, sizeof(probe));
    memcpy(probe, buf, (buf_len < 31) ? buf_len : 31);
    if (sscanf(probe, "cloonix_udp_probe %hhu", &probe_idx) == 1)
      reply_probe_udp(probe_idx);
    else
      result = kind_seqtap_error;
    }
  if (result == kind_seqtap_data)
    {
    /* Say what a sequence gap IS, and do not print one per packet. */
    if (g_traffic_mngt == 0)
      {
      static unsigned long nb_no_mngt;
      nb_no_mngt += 1;
      if ((nb_no_mngt % 1000) == 1)
        KERR("WARNING %s: data arrived before the enter-traffic message "
             "did, forwarding it anyway (%lu frames so far)", c2c, nb_no_mngt);
      }
    if (seq != ((seqtap+1)&0xFFFF))
      {
      static unsigned long nb_gap;
      int gap = (int)((seq - seqtap - 1) & 0xFFFF);
      if (!first_frame_seen)
        {
        /* The first data frame of the tunnel, and it is not number 1. */
        KTRC("%s: the tunnel opens at sequence %d, %d frame(s) dropped by "
             "the far end before it knew this end's address", c2c, seq, gap);
        }
      else
        {
        static uint64_t last;
        nb_gap += 1;
        if (say_now(&last))
          {
          int queued, rcvbuf;
          socket_fill(fd, &queued, &rcvbuf);
          KERR("WARNING %s udp sequence gap: got %d after %d, %d "
               "frame(s) lost or reordered (%lu gaps so far); %d of %d "
               "bytes queued on this socket%s", c2c, seq, seqtap, gap,
               nb_gap, queued, rcvbuf, (2 * queued > rcvbuf) ?
               ": this process fell behind" : "");
          }
        }
      }
    first_frame_seen = 1;
    seqtap = seq;
    rxtx_tx_enqueue(buf_len, buf);
    }
  else if (result != kind_seqtap_sig_hello)
    {
    /* Anybody can send to this port: a bad datagram is dropped, not fatal. */
    static uint64_t last_bad;
    static unsigned long nb_bad;
    nb_bad += 1;
    if (say_now(&last_bad))
      {
      char hex[16];
      int i, n = 0;
      memset(hex, 0, sizeof(hex));
      for (i=0; (i<len) && (i<4); i++)
        n += snprintf(hex+n, sizeof(hex)-n, " %02X", g_rx[i] & 0xFF);
      KERR("WARNING %s: %lu udp datagram(s) that are not from the peer "
           "dropped so far, the last of %d bytes starting%s",
           c2c, nb_bad, len, n ? hex : " with nothing");
      }
    result = 1;
    }
  return result;
}

/* Up to 64 datagrams per wakeup: the queue in front of this socket holds
   ten (net.unix.max_dgram_qlen), one read per wakeup left it full. */
static int rx_dgram_cb(int llid, int fd)
{
  int i, result = 0, r;
  for (i=0; i<64; i++)
    {
    r = rx_dgram_one(fd);
    if (r <= 0)
      break;
    result = r;
    }
  return result;
}

int udp_init(uint16_t udp_port)
{
  int fd, result = -1;
  char *net = get_net_name();
  char *c2c = get_c2c_name();
  g_llid_proxymous = 0;
  g_fd_proxymous = -1;
  g_traffic_mngt = 0;
  if (udp_port == 0)
    KOUT("ERROR No port udp");
  memset(g_proxy_unix_dgram_rx, 0, MAX_PATH_LEN-1);
  memset(g_proxy_unix_dgram_tx, 0, MAX_PATH_LEN-1);
  snprintf(g_proxy_unix_dgram_tx, MAX_PATH_LEN-1,
           "%s_%s/dgram_c2c_%hu_tx.sock", PROXYSHARE_IN, net, udp_port);
  snprintf(g_proxy_unix_dgram_rx, MAX_PATH_LEN-1,
           "%s_%s/dgram_c2c_%hu_rx.sock", PROXYSHARE_IN, net, udp_port);
  if (strlen(g_proxy_unix_dgram_tx) >= 108)
    KOUT("ERROR PATH LEN %lu", strlen(g_proxy_unix_dgram_tx));
  if (strlen(g_proxy_unix_dgram_rx) >= 108)
    KOUT("ERROR PATH LEN %lu", strlen(g_proxy_unix_dgram_rx));
  if (!access(g_proxy_unix_dgram_rx, R_OK))
    unlink(g_proxy_unix_dgram_rx);
  fd = util_socket_unix_dgram(g_proxy_unix_dgram_rx);
  if (fd < 0)
    KERR("ERROR %s %s", c2c, g_proxy_unix_dgram_rx);
  else
    {
    dgram_buffers(fd);
    g_llid_proxymous = msg_watch_fd(fd, rx_dgram_cb, err_cb, "udpd2d");
    if (g_llid_proxymous == 0)
      {
      KERR("ERROR %s %s", c2c, g_proxy_unix_dgram_rx);
      close(fd);
      }
    else
      {
      g_fd_proxymous = fd;
      result = 0;
      }
    }
  return result;
}

