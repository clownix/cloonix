/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "tun_tap.h"
#include "udp.h"
#include "packet_arp_mangle.h"

static uint8_t g_buf_tx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_buf_rx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static int g_fd_rx_from_tap;
static int g_fd_tx_to_tap;
static int g_mac_mangle;
char *get_net_name(void);
char *get_c2c_name(void);

void rxtx_tx_enqueue(int len, uint8_t *buf)
{
  static uint16_t seqtap = 0;
  int tx;
  int udp2tap = 1;
  seqtap += 1;
  if (g_mac_mangle)
    packet_arp_mangle(udp2tap, len, buf);
  if ((len <= 0) || (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR SEND  %d", len);
  fct_seqtap_tx(kind_seqtap_data, g_buf_tx, seqtap, len, buf);
  tx = write(g_fd_tx_to_tap, g_buf_tx, len + HEADER_TAP_MSG);
  if ((tx < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
    {
    static unsigned long dropped, last;
    unsigned long now = (unsigned long) time(NULL);
    dropped += 1;
    if (now - last >= 10)
      {
      last = now;
      KERR("WARNING c2c %s: the tap is not taking frames, %lu dropped so "
           "far at its pipe", get_c2c_name(), dropped);
      }
    return;
    }
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
}


static void rx_err(int llid, int err, int from);

/* One frame per turn, not a batch: the next hop, the unix datagram queue of
   proxymous, holds ten, and 32 at once overflowed it 26 times as often. */
static int rx_cb(int llid, int fd)
{
  int udp2tap = 0;
  uint16_t seq;
  int len, buf_len, result;
  uint8_t *buf;
  int green_light = udp_get_traffic_mngt();
  len = read(fd, g_buf_rx, HEADER_TAP_MSG);
  if (len == 0)
    rx_err(llid, 0, 0);
  if (len != HEADER_TAP_MSG)
    KOUT("ERROR READ %d %d", len, errno);
  result = fct_seqtap_rx(fd, g_buf_rx, &seq, &buf_len, &buf);
  if (result != kind_seqtap_data)
    KOUT("ERROR %d", result);
  /* a gap in seq is a frame the tap child dropped at its pipe: it says so */
  if (green_light)
    {
    if (g_mac_mangle)
      packet_arp_mangle(udp2tap, buf_len, buf);
    udp_tx_traf_send(buf_len, buf);
    }
  /* >0: a frame was consumed. 0 eleven times in a row stops the read for a beat (channel.c). */
  return 1;
}

static void rx_err(int llid, int err, int from)
{
  KERR("ERROR c2c %s: a tap child is gone, its pipe closed (llid %d err %d "
       "from %d), stopping", get_c2c_name(), llid, err, from);
  ctrl_link_stop();
}

void rxtx_mac_mangle(uint8_t *mac)
{
  g_mac_mangle = 1;
  set_arp_mangle_mac_hwaddr(mac);
}

void rxtx_init(int fd_rx_from_tap, int fd_tx_to_tap)
{
  int llid;
  g_mac_mangle = 0;
  g_fd_rx_from_tap = fd_rx_from_tap;
  g_fd_tx_to_tap = fd_tx_to_tap;
  llid = msg_watch_fd(fd_rx_from_tap, rx_cb, rx_err, "c2c");
  if (llid == 0)
    KOUT("ERROR C2C");
}

