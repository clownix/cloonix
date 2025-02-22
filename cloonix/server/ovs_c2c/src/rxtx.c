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
#include <linux/icmp.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "tun_tap.h"
#include "udp.h"
#include "packet_arp_mangle.h"


static uint8_t g_buf_tx[MAX_TAP_BUF_LEN+HEADER_TAP_MSG+END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_buf_rx[MAX_TAP_BUF_LEN+HEADER_TAP_MSG+END_FRAME_ADDED_CHECK_LEN];
static int g_fd_rx_from_tap;
static int g_fd_tx_to_tap;
static int g_mac_mangle;
char *get_net_name(void);


int get_fd_tx_to_tap(void)
{
  return g_fd_tx_to_tap;
}

/****************************************************************************/
void rxtx_tx_enqueue(int len, uint8_t *buf)
{
  static uint16_t seqtap = 0;
  int tx;
  int udp2tap = 1;
  seqtap += 1;
  if (g_mac_mangle)
    packet_arp_mangle(udp2tap, len, buf);
  if ((len <= 0) || (len > MAX_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR SEND  %d", len);
  fct_seqtap_tx(kind_seqtap_data, g_buf_tx, seqtap, len, buf);
  tx = write(g_fd_tx_to_tap, g_buf_tx, len + HEADER_TAP_MSG);
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb(int llid, int fd)
{
  static uint16_t seqtap=0;
  int udp2tap = 0;
  uint16_t seq;
  int len, buf_len, result;
  uint8_t *buf;
  int green_light = udp_get_traffic_mngt();
  len = read(fd, g_buf_rx, HEADER_TAP_MSG);
  if (len <= 0)
    KOUT("ERROR READ %d %d", len, errno);
  result = fct_seqtap_rx(0, 0, fd, g_buf_rx, &seq, &buf_len, &buf);
  if (result != kind_seqtap_data)
    KOUT("ERROR %d", result);
  if (seq != ((seqtap+1)&0xFFFF))
    KERR("WARNING %d %d", seq, seqtap);
  seqtap = seq;
  if (green_light)
    {
    if (g_mac_mangle)
      packet_arp_mangle(udp2tap, buf_len, buf);
    if (udp_tx_traf_send(buf_len, buf))
      KERR("ERROR %d %hhX %hhX %hhX %hhX %hhX %hhX %hhX %hhX",
           buf_len, g_buf_rx[0], g_buf_rx[1], g_buf_rx[2], g_buf_rx[3],
           g_buf_rx[4], g_buf_rx[5], g_buf_rx[6], g_buf_rx[7]);
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_err(int llid, int err, int from)
{
  KOUT("ERROR %d %d %d", llid, err, from);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rxtx_mac_mangle(uint8_t *mac)
{
  g_mac_mangle = 1;
  set_arp_mangle_mac_hwaddr(mac);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

