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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "tun_tap.h"
#include "utils.h"
#include "rxtx.h"
#include "ltcp.h"
#include "ludp.h"
#include "licmp.h"
#include "ldhcp.h"
#include "machine.h"
#include "tcp_flagseq.h"
#include "ssh_cisco_nat.h"

static uint8_t g_buf_rx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_buf_tx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static int g_fd_rx_from_tap;
static int g_fd_tx_to_tap;
static int g_llid_rxtx;



/****************************************************************************/
int get_llid_rxtx(void)
{
  return g_llid_rxtx;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rxtx_tx_enqueue(int len, uint8_t *buf)
{
  int tx;
  static uint16_t seqtap = 0;
  seqtap += 1;
/*
  char *spy;
  spy = utils_spy_on_tcp(len, buf);
  if (spy)
    KERR("%s", spy);
*/
  if ((len == 0) || (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KERR("ERROR LEN rxtx_tx_enqueue %d", len);
  fct_seqtap_tx(kind_seqtap_data, g_buf_tx, seqtap, len, buf);
  tx = write(g_fd_tx_to_tap, g_buf_tx, len + HEADER_TAP_MSG);
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rxtx_packet_rx(int len, uint8_t *buf)
{
  uint8_t *ipv4;
  uint8_t *icmp;
  uint8_t *udp;
  uint8_t *tcp;
  uint32_t sip;
  uint32_t dip;
  uint16_t src_port;
  uint16_t dst_port;
  int proto, ln, tcp_hdr_len;
  uint8_t smac[32];
  uint8_t dmac[32];
  memcpy(smac, &(buf[6]), 6);
  memcpy(dmac, &(buf[0]), 6);
  if (len <= ETHERNET_HEADER_LEN)
    {
    KERR("ERROR %d", len);
    return;
    }
  if ((buf[12] == 0x08) && (buf[13] == 0x06))
    {
    if (len < ETHERNET_HEADER_LEN + ARP_HEADER_LEN)
      {
      KERR("ERROR %d", len);
      return;
      }
    dhcp_arp_management(len, buf);
    } 
  else if ((buf[12] == 0x08) && (buf[13] == 0x00))
    {
    if (len <= ETHERNET_HEADER_LEN + IPV4_HEADER_LEN)
      {
      KERR("ERROR %d", len);
      return;
      }
    ipv4 = &(buf[ETHERNET_HEADER_LEN]);
    proto = ipv4[9] & 0xFF;
    if (proto == IPPROTO_ICMP)
      {
      if (len <= ETHERNET_HEADER_LEN + IPV4_HEADER_LEN + ICMP_HEADER_LEN)
        {
        KERR("ERROR %d", len);
        return;
        }
      icmp = &(ipv4[IPV4_HEADER_LEN]);
      if ((icmp[0] == ICMP_ECHO) && (icmp[1] == 0)) 
        {
        icmp_input(len, buf);
        }
      else
        KERR("PROTO ICMP %hhd NOT MANAGED len: %d", icmp[0], len);
      }
    else if (proto == IPPROTO_UDP)
      {
      if (len <= ETHERNET_HEADER_LEN + IPV4_HEADER_LEN + UDP_HEADER_LEN)
        {
        KERR("ERROR %d", len);
        return;
        }
      sip = (ipv4[12]<<24)+(ipv4[13]<<16)+(ipv4[14]<<8)+ipv4[15];
      dip = (ipv4[16]<<24)+(ipv4[17]<<16)+(ipv4[18]<<8)+ipv4[19];
      udp = &(ipv4[IPV4_HEADER_LEN]);
      src_port = (udp[0]<<8) + udp[1];
      dst_port = (udp[2]<<8) + udp[3];
      udp = &(ipv4[IPV4_HEADER_LEN+UDP_HEADER_LEN]);
      ln = len - ETHERNET_HEADER_LEN - IPV4_HEADER_LEN - UDP_HEADER_LEN;
      if ((src_port == BOOTP_CLIENT) && (dst_port == BOOTP_SERVER))
        {
        dhcp_input(smac, dmac, ln, udp);
        }
      else
        {
        udp_input(smac, dmac, sip, dip, src_port, dst_port, ln, udp);
        }
      }
    else if (proto == IPPROTO_TCP)
      {
      if (len < ETHERNET_HEADER_LEN + IPV4_HEADER_LEN + TCP_HEADER_LEN)
        {
        KERR("ERROR %d", len);
        return;
        }
      sip = (ipv4[12]<<24)+(ipv4[13]<<16)+(ipv4[14]<<8)+ipv4[15];
      dip = (ipv4[16]<<24)+(ipv4[17]<<16)+(ipv4[18]<<8)+ipv4[19];
      tcp = &(ipv4[IPV4_HEADER_LEN]);
      src_port = (tcp[0]<<8) + tcp[1];
      dst_port = (tcp[2]<<8) + tcp[3];
      tcp_hdr_len = ((tcp[12] & 0xf0) >> 2); 
      if (tcp_hdr_len < TCP_HEADER_LEN)
        KOUT("%d %d", tcp_hdr_len, TCP_HEADER_LEN); 
      ln = ETHERNET_HEADER_LEN + IPV4_HEADER_LEN + tcp_hdr_len;
      if (len < ln)
        KOUT("ERROR %d %d %d", len, tcp_hdr_len, ln); 

/*
  char *spy;
      spy = utils_spy_on_tcp(len, buf);
      if (spy)
        KERR("%s", spy);
*/

      tcp_input(smac,dmac,sip,dip,src_port,dst_port,tcp,len-ln,buf+ln);
      }
    else if (proto == IPPROTO_IGMP)
      {
      KERR("PROTO NOT MANAGED %X len: %d", proto, len);
      }
    else
      {
      KERR("PROTO NOT MANAGED %X len: %d", proto, len);
      }
    }
  else if (((buf[12] == 0x86) && (buf[13] == 0xdd)) ||
           ((buf[12] == 0x80) && (buf[13] == 0x35)))
    {
    }
  else
    {
    KERR("EHTERTYPE NOT MANAGED %hhx %hhx len: %d", buf[12], buf[13], len);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_cb(int llid, int fd)
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
    KERR("WARNING %hu %hu", seq, seqtap);
  seqtap = seq;
  rxtx_packet_rx(buf_len, buf);
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
void rxtx_init(int fd_rx_from_tap, int fd_tx_to_tap)
{
  int llid;
  g_fd_rx_from_tap = fd_rx_from_tap;
  g_fd_tx_to_tap = fd_tx_to_tap;
  machine_init();
  icmp_init();
  udp_init();
  tcp_init();
  utils_init();
  ssh_cisco_nat_init();
  dhcp_init();
  tcp_flagseq_init();
  llid = msg_watch_fd(fd_rx_from_tap, rx_cb, rx_err, "nat");
  if (llid == 0)
    KOUT("ERROR NAT");
  g_llid_rxtx = llid;
}
/*--------------------------------------------------------------------------*/

