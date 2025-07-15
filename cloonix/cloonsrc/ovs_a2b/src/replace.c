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
#include <sys/queue.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"
#include "circle.h"
#include "replace.h"

#define ETHERNET_HEADER_LEN     14
#define IPV4_HEADER_LEN         20
#define TCP_HEADER_LEN          20
#define UDP_HEADER_LEN           8
#define ICMP_HEADER_LEN          8
#define ARP_IPV4_LEN            20
#define ARP_HEADER_LEN          28


/*****************************************************************************/
typedef struct t_swap
{
  int dst_must_swap_ip;
  uint32_t dst_targ_ip;
  uint32_t dst_with_ip;
  int src_must_swap_ip;
  uint32_t src_targ_ip;
  uint32_t src_with_ip;
} t_swap;

static t_swap g_swap[2]; 
/*---------------------------------------------------------------------------*/
  
/*****************************************************************************/
static uint16_t utils_in_cksum(uint16_t *addr, unsigned int count)
{
  register uint32_t sum = 0;
  while (count > 1)
    {
    sum += * addr++;
    count -= 2;
    }
  if(count > 0)
    {
    sum += ((*addr)&htons(0xFF00));
    }
  while (sum>>16)
    sum = (sum & 0xffff) + (sum >> 16);
  sum = ~sum;
  return ((uint16_t) sum);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static uint16_t sum_calc(int len, uint8_t *buff, uint8_t *pseudo_header)
{
  int i, sum = 0;
  for (i=0; i<len; i=i+2)
    {
    if (i+1 == len)
      {
      sum += ((buff[i] << 8) & 0xFF00);
      break;
      }
    else
      sum += ((buff[i] << 8) & 0xFF00) + (buff[i+1] & 0xFF);
    }
  for (i=0; i<12; i=i+2)
    sum += ((pseudo_header[i] << 8) & 0xFF00) + (pseudo_header[i+1] & 0xFF);
  sum = (sum & 0xFFFF) + (sum >> 16);
  sum = (sum & 0xFFFF) + (sum >> 16);
  sum = ~sum;
  return ((uint16_t) sum);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static uint16_t tcp_checksum(int len, uint8_t *data, uint8_t *iph)
{
  int i;
  uint16_t chksum;
  uint8_t pseudo_header[12];
  for (i=0; i< 8; i++)
    pseudo_header[i] = iph[12+i];
  pseudo_header[8]  = 0;
  pseudo_header[9]  = IPPROTO_TCP;
  pseudo_header[10] = (len >> 8) & 0xFF;
  pseudo_header[11] = len & 0xFF;
  chksum = sum_calc(len, data, pseudo_header);
  return chksum;
} 
/*---------------------------------------------------------------------------*/

  
/*****************************************************************************/
static void utils_tcp_checksum(int buf_len, uint8_t *ebuf, int tcp_hdr_len)
{ 
  uint8_t *buf_ip = ebuf + ETHERNET_HEADER_LEN;
  uint8_t *buf = ebuf + ETHERNET_HEADER_LEN + IPV4_HEADER_LEN;
  uint16_t checksum;
  int data_len = buf_len - (ETHERNET_HEADER_LEN+IPV4_HEADER_LEN+tcp_hdr_len);
  buf[16] = 0;
  buf[17] = 0;
  buf[18] = 0;
  buf[19] = 0;
  checksum = tcp_checksum(tcp_hdr_len + data_len, buf, buf_ip);
  buf[16] =  (checksum >> 8) & 0xFF;
  buf[17] =  checksum & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void utils_fill_udp_csum(uint8_t  *buf_ip, uint32_t udp_len)
{
  int i;
  uint8_t pseudo_header[12];
  uint16_t checksum;
  uint8_t *buf = &(buf_ip[IPV4_HEADER_LEN]);
  buf[6]  = 0;
  buf[7]  = 0;
  for (i=0; i< 8; i++)
    pseudo_header[i] = buf_ip[12+i];
  pseudo_header[8]  = 0;
  pseudo_header[9]  = IPPROTO_UDP;
  pseudo_header[10] = (udp_len >> 8) & 0xFF;
  pseudo_header[11] = udp_len & 0xFF;
  checksum = sum_calc(udp_len, &(buf[0]), pseudo_header);
  buf[6]  =  (checksum >> 8) & 0xFF;
  buf[7]  =  checksum & 0xFF;
} 
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void swap_ip_src(uint8_t *data, uint32_t sip, int buf_len, uint8_t *buf)
{ 
  unsigned short checksum;
  data[12] = (sip >> 24) & 0xFF;
  data[13] = (sip >> 16) & 0xFF;
  data[14] = (sip >> 8)  & 0xFF;
  data[15] = sip & 0xFF;
  data[10] = 0;
  data[11] = 0;
  checksum = utils_in_cksum((u_short *)data, 20);
  data[10] = checksum & 0xFF;
  data[11] = (checksum >> 8) & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void swap_ip_dst(uint8_t *data, uint32_t dip, int buf_len, uint8_t *buf)
{
  unsigned short checksum;
  data[16] = (dip >> 24) & 0xFF;
  data[17] = (dip >> 16) & 0xFF;
  data[18] = (dip >> 8)  & 0xFF;
  data[19] =  dip & 0xFF;
  data[10] = 0;
  data[11] = 0;
  checksum = utils_in_cksum((u_short *)data, 20);
  data[10] = checksum & 0xFF;
  data[11] = (checksum >> 8) & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void udp_input(int dest, uint32_t targ_ip, uint32_t new_ip,
                      uint8_t *smac, uint8_t *dmac, uint8_t *ipv4,
                      uint32_t sip,  uint32_t dip,
                      uint16_t sport, uint16_t dport,
                      int buf_len, uint8_t *buf)
{
  int len = buf_len - (ETHERNET_HEADER_LEN + IPV4_HEADER_LEN);
  if ((dest) && (dip == targ_ip))
    {
    swap_ip_dst(ipv4, new_ip, buf_len, buf);
    utils_fill_udp_csum(ipv4, len);
    KERR("UDP LEN: %d swap dst %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
          buf_len, 
          ((dip >> 24) & 0xFF), ((dip >> 16) & 0xFF),
          ((dip >> 8)  & 0xFF), (dip & 0xFF),
          ((new_ip >> 24) & 0xFF), ((new_ip >> 16) & 0xFF),
          ((new_ip >> 8)  & 0xFF), (new_ip & 0xFF));
    }
  else if ((!dest) && (sip == targ_ip))
    {
    swap_ip_src(ipv4, new_ip, buf_len, buf);
    utils_fill_udp_csum(ipv4, len);
    KERR("UDP LEN: %d swap src %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
          buf_len, 
          ((sip >> 24) & 0xFF), ((sip >> 16) & 0xFF),
          ((sip >> 8)  & 0xFF), (sip & 0xFF),
          ((new_ip >> 24) & 0xFF), ((new_ip >> 16) & 0xFF),
          ((new_ip >> 8)  & 0xFF), (new_ip & 0xFF));
    }
  else
    KERR("UDPP NO SWAP dest=%d targ=%hhu.%hhu.%hhu.%hhu "
         " sip=%hhu.%hhu.%hhu.%hhu dip=%hhu.%hhu.%hhu.%hhu",
          dest,
          ((targ_ip >> 24) & 0xFF), ((targ_ip >> 16) & 0xFF),
          ((targ_ip >> 8)  & 0xFF), (targ_ip & 0xFF),
          ((sip >> 24) & 0xFF), ((sip >> 16) & 0xFF),
          ((sip >> 8)  & 0xFF), (sip & 0xFF),
          ((dip >> 24) & 0xFF), ((dip >> 16) & 0xFF),
          ((dip >> 8)  & 0xFF), (dip & 0xFF));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void tcp_input(int dest, uint32_t targ_ip, uint32_t new_ip,
                      uint8_t *smac, uint8_t *dmac, uint8_t *ipv4,
                      uint32_t sip, uint32_t dip,
                      uint16_t sport, uint16_t dport,
                      int buf_len, uint8_t *buf, int tcp_hdr_len)
{
  if ((dest) && (dip == targ_ip))
    {
    swap_ip_dst(ipv4, new_ip, buf_len, buf);
    utils_tcp_checksum(buf_len, buf, tcp_hdr_len);
    KERR("TCP %d swap dst %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
          buf_len,
          ((dip >> 24) & 0xFF), ((dip >> 16) & 0xFF),
          ((dip >> 8)  & 0xFF), (dip & 0xFF),
          ((new_ip >> 24) & 0xFF), ((new_ip >> 16) & 0xFF),
          ((new_ip >> 8)  & 0xFF), (new_ip & 0xFF));
    }
  else if ((!dest) && (sip == targ_ip))
    {
    swap_ip_src(ipv4, new_ip, buf_len, buf);
    utils_tcp_checksum(buf_len, buf, tcp_hdr_len);
    KERR("TCP %d swap src %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
          buf_len,
          ((sip >> 24) & 0xFF), ((sip >> 16) & 0xFF),
          ((sip >> 8)  & 0xFF), (sip & 0xFF),
          ((new_ip >> 24) & 0xFF), ((new_ip >> 16) & 0xFF),
          ((new_ip >> 8)  & 0xFF), (new_ip & 0xFF));
    }
  else
    KERR("TCP NO SWAP dest=%d targ=%hhu.%hhu.%hhu.%hhu "
         " sip=%hhu.%hhu.%hhu.%hhu dip=%hhu.%hhu.%hhu.%hhu",
          dest,
          ((targ_ip >> 24) & 0xFF), ((targ_ip >> 16) & 0xFF),
          ((targ_ip >> 8)  & 0xFF), (targ_ip & 0xFF),
          ((sip >> 24) & 0xFF), ((sip >> 16) & 0xFF),
          ((sip >> 8)  & 0xFF), (sip & 0xFF),
          ((dip >> 24) & 0xFF), ((dip >> 16) & 0xFF),
          ((dip >> 8)  & 0xFF), (dip & 0xFF));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void icmp_input(int dest, uint32_t targ_ip, uint32_t new_ip,
                       uint8_t *smac, uint8_t *dmac, uint8_t *ipv4,
                       uint32_t sip, uint32_t dip,
                       int buf_len, uint8_t *buf)
{
  if ((dest) && (dip == targ_ip))
    {
    swap_ip_dst(ipv4, new_ip, buf_len, buf);
    KERR("ICMP swap dst %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
          ((dip >> 24) & 0xFF), ((dip >> 16) & 0xFF),
          ((dip >> 8)  & 0xFF), (dip & 0xFF),
          ((new_ip >> 24) & 0xFF), ((new_ip >> 16) & 0xFF),
          ((new_ip >> 8)  & 0xFF), (new_ip & 0xFF));
    }
  else if ((!dest) && (sip == targ_ip))
    {
    swap_ip_src(ipv4, new_ip, buf_len, buf);
    KERR("ICMP swap src %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
          ((sip >> 24) & 0xFF), ((sip >> 16) & 0xFF),
          ((sip >> 8)  & 0xFF), (sip & 0xFF),
          ((new_ip >> 24) & 0xFF), ((new_ip >> 16) & 0xFF),
          ((new_ip >> 8)  & 0xFF), (new_ip & 0xFF));
    }
  else
    KERR("ICMP NO SWAP dest=%d targ=%hhu.%hhu.%hhu.%hhu "
         " sip=%hhu.%hhu.%hhu.%hhu dip=%hhu.%hhu.%hhu.%hhu",
          dest,
          ((targ_ip >> 24) & 0xFF), ((targ_ip >> 16) & 0xFF),
          ((targ_ip >> 8)  & 0xFF), (targ_ip & 0xFF),
          ((sip >> 24) & 0xFF), ((sip >> 16) & 0xFF),
          ((sip >> 8)  & 0xFF), (sip & 0xFF),
          ((dip >> 24) & 0xFF), ((dip >> 16) & 0xFF),
          ((dip >> 8)  & 0xFF), (dip & 0xFF));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void packet_rx(int len, uint8_t *buf,
                      int dest, uint32_t targ_ip, uint32_t new_ip)
{
  uint8_t *smac = &(buf[6]);
  uint8_t *dmac = &(buf[0]);
  uint8_t *ipv4;
  uint8_t *udp;
  uint8_t *tcp;
  uint32_t sip;
  uint32_t dip;
  uint16_t src_port;
  uint16_t dst_port;
  int proto, ln, tcp_hdr_len;

  if (len <= ETHERNET_HEADER_LEN)
    {
    KERR("ERROR HEADER LEN %d", len);
    return;
    }
  if ((buf[12] == 0x08) && (buf[13] == 0x06))
    {
    if (len < ETHERNET_HEADER_LEN + ARP_HEADER_LEN)
      {
      KERR("ERROR ARP LEN %d", len);
      return;
      }
    }
  else if ((buf[12] == 0x08) && (buf[13] == 0x00))
    {
    if (len <= ETHERNET_HEADER_LEN + IPV4_HEADER_LEN)
      {
      KERR("ERROR IP LEN %d", len);
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
      sip = (ipv4[12]<<24)+(ipv4[13]<<16)+(ipv4[14]<<8)+ipv4[15];
      dip = (ipv4[16]<<24)+(ipv4[17]<<16)+(ipv4[18]<<8)+ipv4[19];
      icmp_input(dest, targ_ip, new_ip, smac, dmac, ipv4,
                 sip, dip, len, buf);
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
      udp_input(dest, targ_ip, new_ip, smac, dmac, ipv4,
                sip, dip, src_port, dst_port, len, buf);
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
        KERR("%d %d", tcp_hdr_len, TCP_HEADER_LEN);
      else
        {
        ln = ETHERNET_HEADER_LEN + IPV4_HEADER_LEN + tcp_hdr_len;
        if (len < ln)
          KERR("ERROR %d %d %d", len, tcp_hdr_len, ln);
        else
          tcp_input(dest, targ_ip, new_ip, smac, dmac, ipv4,
                    sip, dip, src_port, dst_port, len, buf, tcp_hdr_len);
        }
      }
    else if (proto == IPPROTO_IGMP)
      {
      KERR("PROTO IGMP %X len: %d", proto, len);
      }
    else
      {
      KERR("PROTO NOT MANAGED %X len: %d", proto, len);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void replace_cnf(int dir, char *type, unsigned char t[4], unsigned char s[4])
{
  t_swap *swap;
  if ((dir != 0) && (dir != 1))
    KERR("%d %s", dir, type);
  else if (strcmp(type, "replace_ipdst") && strcmp(type, "replace_ipsrc") &&
           strcmp(type, "unreplace_ipdst") && strcmp(type, "unreplace_ipsrc"))
    KERR("%d %s", dir, type);
  else
    {
    swap = &(g_swap[dir]); 
    if (!strcmp(type, "replace_ipdst"))
      {
      KERR("dir:%d %s %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
            dir, type, t[0], t[1], t[2], t[3], s[0], s[1], s[2], s[3]);
      swap->dst_must_swap_ip = 1;
      swap->dst_targ_ip = (t[0]<<24)+(t[1]<<16)+(t[2]<<8)+t[3];
      swap->dst_with_ip = (s[0]<<24)+(s[1]<<16)+(s[2]<<8)+s[3];
      }
    else if (!strcmp(type, "replace_ipsrc"))
      {
      KERR("dir:%d %s %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
            dir, type, t[0], t[1], t[2], t[3], s[0], s[1], s[2], s[3]);
      swap->src_must_swap_ip = 1;
      swap->src_targ_ip = (t[0]<<24)+(t[1]<<16)+(t[2]<<8)+t[3];
      swap->src_with_ip = (s[0]<<24)+(s[1]<<16)+(s[2]<<8)+s[3];
      }
    else if (!strcmp(type, "unreplace_ipdst"))
      {
      KERR("dir:%d %s", dir, type);
      swap->dst_must_swap_ip = 0;
      swap->dst_targ_ip = 0;
      swap->dst_with_ip = 0;
      }
    else if (!strcmp(type, "unreplace_ipsrc"))
      {
      KERR("dir:%d %s", dir, type);
      swap->src_must_swap_ip = 0;
      swap->src_targ_ip = 0;
      swap->src_with_ip = 0;
      }
    else
      KOUT("ERROR %d %s", dir, type);
    KERR("CNF REPLACE dir:%d type:%s", dir, type);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void replace_packet(int dir, t_pkt *pbuf)
{
  t_swap *swap;
  int len = pbuf->len;
  uint8_t *pkt = pbuf->pkt;

  if ((dir == 0) || (dir == 1)) 
    {
    swap = &(g_swap[dir]); 
    if (swap->dst_must_swap_ip)
      {
      packet_rx(len, pkt, 1, swap->dst_targ_ip, swap->dst_with_ip);
      }
    if (swap->src_must_swap_ip)
      {
      packet_rx(len, pkt, 0, swap->src_targ_ip, swap->src_with_ip);
      }
    }
  else
    KERR("ERROR %d", dir);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void replace_init(char *name)
{
  uint32_t src_targ_ip0;
  uint32_t src_with_ip0;
  uint32_t dst_targ_ip0;
  uint32_t dst_with_ip0;
  uint32_t src_targ_ip1;
  uint32_t src_with_ip1;
  uint32_t dst_targ_ip1;
  uint32_t dst_with_ip1;

  memset(g_swap, 0, 2*(sizeof(t_swap)));
  g_swap[0].dst_must_swap_ip = 1;
  g_swap[0].src_must_swap_ip = 1;
  g_swap[1].dst_must_swap_ip = 1;
  g_swap[1].src_must_swap_ip = 1;

  if (strcmp(name, "swap1") == 0)
    {
    KERR("CONFIG swap1");
    ip_string_to_int(&src_targ_ip0, "10.0.11.1");
    ip_string_to_int(&dst_targ_ip0, "10.0.11.21");
    ip_string_to_int(&src_with_ip0, "198.168.101.160");
    ip_string_to_int(&dst_with_ip0, "198.168.102.160");
    ip_string_to_int(&dst_targ_ip1, "198.168.101.160");
    ip_string_to_int(&dst_with_ip1, "10.0.11.1");
    ip_string_to_int(&src_targ_ip1, "198.168.102.160");
    ip_string_to_int(&src_with_ip1, "10.0.11.21");
    }
  else if (strcmp(name, "swap2") == 0)
    {
    KERR("CONFIG swap2");
    ip_string_to_int(&src_targ_ip0, "10.0.11.21");
    ip_string_to_int(&dst_targ_ip0, "10.0.11.1");
    ip_string_to_int(&src_with_ip0, "198.168.102.160");
    ip_string_to_int(&dst_with_ip0, "198.168.101.160");
    ip_string_to_int(&dst_targ_ip1, "198.168.102.160");
    ip_string_to_int(&dst_with_ip1, "10.0.11.21");
    ip_string_to_int(&src_targ_ip1, "198.168.101.160");
    ip_string_to_int(&src_with_ip1, "10.0.11.1");
    }
  else
    KERR("ERROR %s", name);

  g_swap[0].dst_targ_ip = dst_targ_ip0;
  g_swap[0].dst_with_ip = dst_with_ip0;
  g_swap[0].src_targ_ip = src_targ_ip0;
  g_swap[0].src_with_ip = src_with_ip0;

  g_swap[1].dst_targ_ip = dst_targ_ip1;
  g_swap[1].dst_with_ip = dst_with_ip1;
  g_swap[1].src_targ_ip = src_targ_ip1;
  g_swap[1].src_with_ip = src_with_ip1;
}
/*--------------------------------------------------------------------------*/
