/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "tun_tap.h"
#include "utils.h"
#include "rxtx.h"
#include "sched.h"
#include "circle.h"
#include "replace.h"

char *get_a2b_name(void);

static uint64_t g_prev_usec;
static uint8_t g_buf_rx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_buf_tx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static int g_fd_rx_from_tap0;
static int g_fd_tx_to_tap0;
static int g_fd_rx_from_tap1;
static int g_fd_tx_to_tap1;

static uint64_t get_usec(void)
{
  uint64_t usec;
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  usec = (ts.tv_sec * 1000000);
  usec += (ts.tv_nsec / 1000);
  return usec;
}

static void packet_tx(int id, int len, uint8_t *buf)
{
  static uint16_t seqtap0 = 0;
  static uint16_t seqtap1 = 0;
  int tx;
  if ((len <= 0) || (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR TX %d", len);
  if (id == 0)
    {
    seqtap0 += 1;
    fct_seqtap_tx(kind_seqtap_data, g_buf_tx, seqtap0, len, buf);
    tx = write(g_fd_tx_to_tap0, g_buf_tx, len + HEADER_TAP_MSG);
    }
  else if (id == 1)
    {
    seqtap1 += 1;
    fct_seqtap_tx(kind_seqtap_data, g_buf_tx, seqtap1, len, buf);
    tx = write(g_fd_tx_to_tap1, g_buf_tx, len + HEADER_TAP_MSG);
    }
  else
    KOUT("ERROR %d", id);
  
  if ((tx < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
    {
    static unsigned long dropped[2], last[2];
    unsigned long now = (unsigned long) time(NULL);
    dropped[id] += 1;
    if (now - last[id] >= 10)
      {
      last[id] = now;
      KERR("WARNING a2b %s tap %d is not taking frames, %lu dropped so far "
           "at its pipe", get_a2b_name(), id, dropped[id]);
      }
    return;
    }
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
}

static void trigger_schedule(uint64_t usec, uint64_t delta)
{
  int id;
  t_pkt *packet;
  for (id=0; id<2; id++)
    {
    sched_mngt(id, usec, delta);
    packet = circle_get(id);
    while(packet)
      {
      if (id == 0)
        packet_tx(1, packet->len, packet->pkt);
      else
        packet_tx(0, packet->len, packet->pkt);
      free(packet->pkt);
      free(packet);
      packet = circle_get(id);
      }
    }
}

static void packet_rx(int id, int len, uint8_t *buf)
{
  uint64_t usec = get_usec();
  uint64_t blen = (uint64_t) len;
  uint8_t *pkt;
  t_pkt *pbuf;
  if (sched_can_enqueue(id, blen, buf))
    {
    pkt = (uint8_t *) malloc(len);
    memcpy(pkt, buf, len);
    pbuf = (t_pkt *) malloc(sizeof(t_pkt));
    pbuf->len = len;
    pbuf->pkt = pkt;
    replace_packet(id, pbuf);
    circle_put(id, blen, usec, pbuf);
    sched_inc_queue_stored(id, blen);
    trigger_schedule(usec, 0);
    }
  else
    KTRC("SCHED DROP %d DSCP %02X len:%d", id, buf[15], len);
}

static void timeout_schedule(void *data)
{
  uint64_t usec = get_usec();
  uint64_t delta = usec - g_prev_usec;
  g_prev_usec = usec;
  trigger_schedule(usec, delta);
  clownix_timeout_add(1, timeout_schedule, NULL, NULL, NULL);
}


static int rx_side(int id, int fd)
{
  uint16_t seq;
  int len, buf_len, result, nb = 0;
  uint8_t *buf;
  do
    {
    len = read(fd, g_buf_rx, HEADER_TAP_MSG);
    if (len != HEADER_TAP_MSG)
      KOUT("ERROR READ %d %d", len, errno);
    result = fct_seqtap_rx(fd, g_buf_rx, &seq, &buf_len, &buf);
    if (result != kind_seqtap_data)
      KOUT("ERROR %d", result);
    /* a gap in seq is a frame the tap child dropped at its pipe: it says so */
    packet_rx(id, buf_len, buf);
    nb += 1;
    } while ((nb < SEQTAP_PIPE_BATCH) && fct_seqtap_pipe_more(fd));
  return nb;
}

static int rx_cb0(int llid, int fd)
{
  (void) llid;
  return rx_side(0, fd);
}

static int rx_cb1(int llid, int fd)
{
  (void) llid;
  return rx_side(1, fd);
}

static void rx_err0(int llid, int err, int from)
{
  KOUT("ERROR %d %d %d", llid, err, from);
}

static void rx_err1(int llid, int err, int from)
{
  KOUT("ERROR %d %d %d", llid, err, from);
}

void rxtx_init(int fd_rx_from_tap0, int fd_tx_to_tap0,
               int fd_rx_from_tap1, int fd_tx_to_tap1)
{
  int llid;
  g_fd_rx_from_tap0 = fd_rx_from_tap0;
  g_fd_tx_to_tap0 = fd_tx_to_tap0;
  g_fd_rx_from_tap1 = fd_rx_from_tap1;
  g_fd_tx_to_tap1 = fd_tx_to_tap1;
  sched_init();
  circle_init();
  utils_init();
  llid = msg_watch_fd(fd_rx_from_tap0, rx_cb0, rx_err0, "a2b0");
  if (llid == 0)
    KOUT("ERROR NAT");
  llid = msg_watch_fd(fd_rx_from_tap1, rx_cb1, rx_err1, "a2b1");
  if (llid == 0)
    KOUT("ERROR NAT");
  g_prev_usec = get_usec();
  clownix_timeout_add(1, timeout_schedule, NULL, NULL, NULL);
}

