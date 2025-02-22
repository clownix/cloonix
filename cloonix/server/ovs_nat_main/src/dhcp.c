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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "rxtx.h"

#include "dhcp.h"
#include "utils.h"
#include "ssh_cisco_nat.h"
#include "machine.h"

#define BOOTP_REQUEST   1
#define BOOTP_REPLY     2
#define DHCP_OPT_LEN            312

#define RFC1533_COOKIE          99, 130, 83, 99
#define RFC1533_PAD             0
#define RFC1533_END             255
#define RFC2132_MSG_TYPE        53
#define RFC2132_REQ_ADDR        50
#define RFC2132_LEASE_TIME      51
#define RFC1533_DNS             6
#define RFC1533_GATEWAY         3
#define RFC1533_END             255
#define RFC2132_SRV_ID          54
#define RFC1533_NETMASK         1


#define LEASE_TIME (0x00FFF)
#define DHCPDISCOVER            1
#define DHCPOFFER               2
#define DHCPREQUEST             3
#define DHCPACK                 5
#define DHCPNACK                6
#define DHCPRELEASE             7


struct bootp_t {
    uint8_t  bp_op;
    uint8_t  bp_htype;
    uint8_t  bp_hlen;
    uint8_t  bp_hops;
    uint32_t bp_xid;
    uint16_t bp_secs;
    uint16_t unused;
    uint32_t bp_ciaddr;
    uint32_t bp_yiaddr;
    uint32_t bp_siaddr;
    uint32_t bp_giaddr;
    uint8_t  bp_hwaddr[16];
    char     bp_sname[64];
    char     bp_file[128];
    uint8_t  bp_vend[DHCP_OPT_LEN];
};

static char rfc1533_cookie[] = { RFC1533_COOKIE };

static uint32_t g_offset;

/*****************************************************************************/
static void dhcp_decode(int len, uint8_t *data, int *type, uint32_t *addr)
{
  uint8_t *p, *p_end;
  int lng, tag;
  *type = 0;
  *addr = 0;
  p = data;
  p_end = data + len;
  if ((len >= 5) && (memcmp(p, rfc1533_cookie, 4) == 0))
    {
    p += 4;
    while (p < p_end)
      {
      tag = p[0];
      if (tag == RFC1533_PAD)
        p++;
      else if (tag == RFC1533_END)
        break;
      else
        {
        p++;
        if (p >= p_end)
          break;
        lng = *p++;
        lng &= 0xFF;
        switch(tag)
          {
          case RFC2132_MSG_TYPE:
            if (lng >= 1)
              *type = p[0] & 0xFF;
            break;
          case RFC2132_REQ_ADDR:
            if (lng == 4)
              {
              *addr += (p[0]<<24) & 0xFF;
              *addr += (p[1]<<16) & 0xFF;
              *addr += (p[2]<<8 ) & 0xFF;
              *addr += p[3] & 0xFF;
              }
            break;
          default:
            break;
          }
        p += lng;
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int format_rbp(struct bootp_t *bp, struct bootp_t *rbp, 
                      uint32_t addr, int type)
{
  uint32_t our_ip_gw, our_ip_dns, our_default_route;
  int len;
  uint8_t *q;
  our_ip_gw = utils_get_gw_ip();
  our_ip_dns = utils_get_dns_ip();
  our_default_route = utils_get_gw_ip();
  rbp->bp_op = BOOTP_REPLY;
  rbp->bp_xid = bp->bp_xid;
  rbp->bp_htype = 1;
  rbp->bp_hlen = 6;
  memcpy(rbp->bp_hwaddr, bp->bp_hwaddr, 6);
  rbp->bp_yiaddr = htonl(addr);
  rbp->bp_siaddr = htonl(our_ip_gw);
  strcpy(rbp->bp_sname, "slirpserverhost");
  strcpy(rbp->bp_file, "boot_file_name");
  q = rbp->bp_vend;
  memcpy(q, rfc1533_cookie, 4);
  q += 4;
  *q++ = RFC2132_MSG_TYPE;
  *q++ = 1;
  *q++ = (type==DHCPDISCOVER) ?DHCPOFFER:DHCPACK;
  *q++ = RFC2132_SRV_ID;
  *q++ = 4;
  *q++ = (our_ip_gw >> 24) & 0xFF;
  *q++ = (our_ip_gw >> 16) & 0xFF;
  *q++ = (our_ip_gw >> 8) & 0xFF;
  *q++ = our_ip_gw & 0xFF;
  *q++ = RFC1533_NETMASK;
  *q++ = 4;
  *q++ = 0xff;
  *q++ = 0xff;
  *q++ = 0xff;
  *q++ = 0x00;
  *q++ = RFC1533_GATEWAY;
  *q++ = 4;
  *q++ = (our_default_route >> 24) & 0xFF;
  *q++ = (our_default_route >> 16) & 0xFF;
  *q++ = (our_default_route >> 8) & 0xFF;
  *q++ = our_default_route & 0xFF;
  *q++ = RFC1533_DNS;
  *q++ = 4;
  *q++ = (our_ip_dns >> 24) & 0xFF;
  *q++ = (our_ip_dns >> 16) & 0xFF;
  *q++ = (our_ip_dns >> 8) & 0xFF;
  *q++ = our_ip_dns & 0xFF;
  *q++ = RFC2132_LEASE_TIME;
  *q++ = 4;
  *q++ = (LEASE_TIME >> 24) & 0xFF;
  *q++ = (LEASE_TIME >> 16) & 0xFF;
  *q++ = (LEASE_TIME >> 8) & 0xFF;
  *q++ = LEASE_TIME & 0xFF;
  *q++ = RFC1533_END;
  len = q - (uint8_t *) rbp;
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int bootp_format_reply(int len, uint8_t *src_mac, uint32_t *addr,
                              struct bootp_t *bp, struct bootp_t *rbp)
{
  int result = -1;
  int type;
  uint32_t addr_in_msg;
  dhcp_decode(len, bp->bp_vend, &type, &addr_in_msg);
  if ((type == DHCPDISCOVER) || (type == DHCPREQUEST)) 
    {
    if (memcmp(src_mac, bp->bp_hwaddr, 6))
      KERR("ERROR");
    *addr = machine_dhcp(src_mac);
    if (!(*addr))
      KERR("ERROR");
    result =  format_rbp(bp, rbp, *addr, type);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void dhcp_input(uint8_t *src_mac, uint8_t *dst_mac, int len, uint8_t *data)
{
  struct bootp_t *bp = (struct bootp_t *)data;
  uint8_t resp[MAX_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
  uint32_t addr;
  int resp_len;
  struct bootp_t *rbp;
  uint8_t our_mac_gw[6];
  memset(resp, 0, MAX_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN);
  rbp = (struct bootp_t *) &(resp[g_offset]);
  if (bp->bp_op == BOOTP_REQUEST)
    {
    resp_len = bootp_format_reply(len, src_mac, &addr, bp, rbp);
    if ((resp_len > 0) && addr)
      {
      utils_get_mac(NAT_MAC_GW, our_mac_gw);
      utils_fill_udp_packet(resp, resp_len,
                            our_mac_gw, src_mac,
                            utils_get_gw_ip(), 0xFFFFFFFF,
                            BOOTP_SERVER, BOOTP_CLIENT);
      resp_len += g_offset; 
      rxtx_tx_enqueue(resp_len, resp);
      }
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void dhcp_arp_management(int len, uint8_t *buf)
{
  uint32_t our_cisco_ip = utils_get_cisco_ip();
  uint32_t our_gw_ip = utils_get_gw_ip();
  uint32_t our_dns_ip = utils_get_dns_ip();
  uint32_t sip, tip;
  uint8_t *mc, mac[6];
  uint8_t *arp_header = buf + ETHERNET_HEADER_LEN;
  uint8_t *arp_opcode = arp_header + 6;
  uint8_t *arp_sha = arp_header + 8;
  uint8_t *arp_sip = arp_header + 14;
  uint8_t *arp_tha = arp_header + 18;
  uint8_t *arp_tip = arp_header + 24;
  uint8_t tmp_tip[4];
  int ok = 1;

  sip = (arp_sip[0]<<24) + (arp_sip[1]<<16) + (arp_sip[2]<<8) + arp_sip[3]; 
  tip = (arp_tip[0]<<24) + (arp_tip[1]<<16) + (arp_tip[2]<<8) + arp_tip[3]; 
  if ((arp_opcode[0] == 0) && (arp_opcode[1] == ARP_OP_REPLY))
    {
    if (tip == our_cisco_ip)
      {
      utils_get_mac(NAT_MAC_CISCO, mac);
      mc = buf;
      if (memcmp(mc, mac, 6))
        KERR("ERROR %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
              mc[0],mc[1],mc[2],mc[3],mc[4],mc[5]); 
      else
        ssh_cisco_nat_arp_resp(&(buf[6]), sip);
      }
    else
      {
      }
    }
  else if ((arp_opcode[0] == 0) && (arp_opcode[1] == ARP_OP_REQUEST))
    {
    if (tip == our_cisco_ip)
      utils_get_mac(NAT_MAC_CISCO, mac);
    else if (tip == our_gw_ip)
      utils_get_mac(NAT_MAC_GW, mac);
    else if (tip == our_dns_ip)
      utils_get_mac(NAT_MAC_DNS, mac);
    else
      ok = 0;
    if (ok == 1)
      {
      memcpy(buf, &(buf[6]), 6);
      memcpy(&(buf[6]), mac, 6);
      arp_opcode[1] = ARP_OP_REPLY;
      memcpy(tmp_tip, arp_tip, 4);
      memcpy(arp_tip, arp_sip, 4);
      memcpy(arp_sip, tmp_tip, 4);
      memcpy(arp_tha, arp_sha, 6);
      memcpy(arp_sha, mac, 6);
      rxtx_tx_enqueue(len, buf);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dhcp_init(void)
{
  g_offset = ETHERNET_HEADER_LEN +
             IPV4_HEADER_LEN +
             UDP_HEADER_LEN;
}
/*---------------------------------------------------------------------------*/


