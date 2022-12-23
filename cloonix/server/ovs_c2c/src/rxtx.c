/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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


static uint8_t g_buf_tx[TOT_MSG_BUF_LEN];
static uint8_t g_buf_rx[TOT_MSG_BUF_LEN];
static uint8_t g_head_msg[HEADER_TAP_MSG];
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
  int tx;
  int udp2tap = 1;

  g_buf_tx[0] = 0xCA;
  g_buf_tx[1] = 0xFE;
  g_buf_tx[2] = (len & 0xFF00) >> 8;
  g_buf_tx[3] =  len & 0xFF;
  if (g_mac_mangle)
    packet_arp_mangle(udp2tap, len, buf);
  memcpy(g_buf_tx + HEADER_TAP_MSG, buf, len);  
  tx = write(g_fd_tx_to_tap, g_buf_tx, len + HEADER_TAP_MSG);
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rxtx_packet_rx(int len, uint8_t *buf)
{
  t_udp_burst *burst;
  int udp2tap = 0;
  int green_light = udp_get_traffic_mngt();
  if (green_light)
    {
    if (g_mac_mangle)
      packet_arp_mangle(udp2tap, len, buf);
    burst = get_udp_burst_tx();
    memcpy(burst[0].buf, buf, len);
    burst[0].len = len;
    if (udp_tx_burst(1, burst))
      KERR("ERROR %d", len);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb(int llid, int fd)
{
  int len, rx;
  len = read(fd, g_head_msg, HEADER_TAP_MSG);
  if (len != HEADER_TAP_MSG)
    KOUT("ERROR %d %d", len, errno);
  else
    {
    if ((g_head_msg[0] != 0xCA) || (g_head_msg[1] != 0xFE))
      KOUT("ERROR %d %d %hhx %hhx", len, errno, g_head_msg[0], g_head_msg[1]);
    len = ((g_head_msg[2] & 0xFF) << 8) + (g_head_msg[3] & 0xFF);
    if ((len == 0) || (len > MAX_TAP_BUF_LEN))
      KOUT("ERROR %d %hhx %hhx", len, g_head_msg[2], g_head_msg[3]);
    rx = read(fd, g_buf_rx, len);
    if (len != rx)
      KOUT("ERROR %d %d %d", len, rx, errno);
    rxtx_packet_rx(len, g_buf_rx);
    }
  return (len+4);
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

