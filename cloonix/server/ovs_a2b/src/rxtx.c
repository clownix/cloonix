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


static uint64_t g_prev_usec;
static uint8_t g_buf_rx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_buf_tx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static int g_fd_rx_from_tap0;
static int g_fd_tx_to_tap0;
static int g_fd_rx_from_tap1;
static int g_fd_tx_to_tap1;


/****************************************************************************/
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
/*---------------------------------------------------------------------------*/


/****************************************************************************/
static void packet_tx(int id, int len, uint8_t *buf)
{
  static uint16_t seqtap = 0;
  int tx;
  seqtap += 1;
  if ((len <= 0) || (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR TX %d", len);
  fct_seqtap_tx(kind_seqtap_data, g_buf_tx, seqtap, len, buf);
  if (id == 0)
    tx = write(g_fd_tx_to_tap0, g_buf_tx, len + HEADER_TAP_MSG);
  else if (id == 1)
    tx = write(g_fd_tx_to_tap1, g_buf_tx, len + HEADER_TAP_MSG);
  else
    KOUT("ERROR %d", id);
  
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
    KERR("SCHED DROP %d DSCP %02X len:%d", id, buf[15], len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_schedule(void *data)
{
  uint64_t usec = get_usec();
  uint64_t delta = usec - g_prev_usec;
  g_prev_usec = usec;
  trigger_schedule(usec, delta);
  clownix_timeout_add(1, timeout_schedule, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb0(int llid, int fd)
{
  static uint16_t seqtap=0;
  uint16_t seq;
  int len, buf_len, result;
  uint8_t *buf;
  len = read(fd, g_buf_rx, HEADER_TAP_MSG);
  if (len != HEADER_TAP_MSG)
    KOUT("ERROR READ %d %d", len, errno);
  result = fct_seqtap_rx(0, 0, fd, g_buf_rx, &seq, &buf_len, &buf);
  if (result != kind_seqtap_data)
    KOUT("ERROR %d", result);
  if (seq != ((seqtap+1)&0xFFFF)) 
    KERR("WARNING %d %d", seq, seqtap);
  seqtap = seq;
  packet_rx(0, buf_len, buf);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb1(int llid, int fd)
{
  static uint16_t seqtap=0;
  uint16_t seq;
  int len, buf_len, result;
  uint8_t *buf;
  len = read(fd, g_buf_rx, HEADER_TAP_MSG);
  if (len != HEADER_TAP_MSG)
    KOUT("ERROR READ %d %d", len, errno);
  result = fct_seqtap_rx(0, 0, fd, g_buf_rx, &seq, &buf_len, &buf);
  if (result != kind_seqtap_data)
    KOUT("ERROR %d", result);
  if (seq != ((seqtap+1)&0xFFFF)) 
    KERR("WARNING %d %d", seq, seqtap);
  seqtap = seq;
  packet_rx(1, buf_len, buf);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_err0(int llid, int err, int from)
{
  KOUT("ERROR %d %d %d", llid, err, from);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_err1(int llid, int err, int from)
{
  KOUT("ERROR %d %d %d", llid, err, from);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

