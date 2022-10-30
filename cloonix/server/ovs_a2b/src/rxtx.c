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
#include "utils.h"
#include "rxtx.h"


static uint8_t g_buf_tx[TOT_MSG_BUF_LEN];
static uint8_t g_buf_rx[TOT_MSG_BUF_LEN];
static uint8_t g_head_msg[HEADER_TAP_MSG];
static int g_fd_rx_from_tap0;
static int g_fd_tx_to_tap0;
static int g_fd_rx_from_tap1;
static int g_fd_tx_to_tap1;



/****************************************************************************/
static void rxtx_tx_enqueue0(int len, uint8_t *buf)
{
  int tx;
  g_buf_tx[0] = 0xCA;
  g_buf_tx[1] = 0xFE;
  g_buf_tx[2] = (len & 0xFF00) >> 8;
  g_buf_tx[3] =  len & 0xFF;
  memcpy(g_buf_tx + HEADER_TAP_MSG, buf, len);  
  tx = write(g_fd_tx_to_tap0, g_buf_tx, len + HEADER_TAP_MSG);
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rxtx_tx_enqueue1(int len, uint8_t *buf)
{
  int tx;
  g_buf_tx[0] = 0xCA;
  g_buf_tx[1] = 0xFE;
  g_buf_tx[2] = (len & 0xFF00) >> 8;
  g_buf_tx[3] =  len & 0xFF;
  memcpy(g_buf_tx + HEADER_TAP_MSG, buf, len);
  tx = write(g_fd_tx_to_tap1, g_buf_tx, len + HEADER_TAP_MSG);
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rxtx_packet_rx0(int len, uint8_t *buf)
{
  rxtx_tx_enqueue1(len, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rxtx_packet_rx1(int len, uint8_t *buf)
{
  rxtx_tx_enqueue0(len, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb0(int llid, int fd)
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
    rxtx_packet_rx0(len, g_buf_rx);
    }
  return (len+4);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb1(int llid, int fd)
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
    rxtx_packet_rx1(len, g_buf_rx);
    }
  return (len+4);
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
  utils_init();
  llid = msg_watch_fd(fd_rx_from_tap0, rx_cb0, rx_err0, "a2b0");
  if (llid == 0)
    KOUT("ERROR NAT");
  llid = msg_watch_fd(fd_rx_from_tap1, rx_cb1, rx_err1, "a2b1");
  if (llid == 0)
    KOUT("ERROR NAT");
}
/*--------------------------------------------------------------------------*/

