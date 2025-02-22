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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <arpa/inet.h>


#include "io_clownix.h"


#define MAC_HEADER 14
#define IP_HEADER 20 



static uint8_t g_wif_hwaddr[6];


enum
{
  proto_none,
  proto_arp_req,
  proto_arp_resp,
  proto_ip,
  proto_ipv6,
};

typedef struct t_association
{
  int ip;
  uint8_t mac[6];
  struct t_association *prev;
  struct t_association *next;
} t_association;


static t_association *head_association;


/*****************************************************************************/
static uint8_t *get_wif_hwaddr(void)
{
  return g_wif_hwaddr;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *get_mac_string(uint8_t *mac)
{
  static char result[80];
  sprintf(result, "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0]&0xff,mac[1]&0xff,mac[2]&0xff,
                 mac[3]&0xff,mac[4]&0xff,mac[5]&0xff);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *get_ip_string(int addr)
{
  static char result[80];
  sprintf(result, "%d.%d.%d.%d", (int) ((addr >> 24) & 0xff),
          (int)((addr >> 16) & 0xff), (int)((addr >> 8) & 0xff),
          (int) (addr & 0xff));
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static unsigned short in_cksum(unsigned short *addr, int len)
{
  int nleft = len;
  unsigned short *w = addr;
  unsigned long sum = 0;
  unsigned short answer = 0;
  while (nleft > 1)
    {
    sum += *w++;
    nleft -= 2;
    }
  if (nleft == 1)
    {
    answer=0;
    *(unsigned char *)(&answer) = *(unsigned char *)w ;
    sum += answer;
    }
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  answer = ~sum;
  return(answer);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static inline int dst_mac_is_unicast(uint8_t *data)
{
  return (!(data[0] & 0x1));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static inline int src_mac_is_unicast(uint8_t *data)
{
  return (!(data[6] & 0x1));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static uint8_t *get_arp_sender_mac(uint8_t *data)
{
  static uint8_t mac[6];
  int i;
  for (i=0; i<6; i++)
    mac[i] = data[22 + i]&0xFF;
  return mac;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void get_arp_sender_ip(uint8_t *data, int *ip)
{
  *ip = 0;
  *ip += (data[28]<<24) & 0xFF000000;
  *ip += (data[29]<<16) & 0x00FF0000;
  *ip += (data[30]<<8)  & 0x0000FF00;
  *ip += (data[31])     & 0x000000FF;  
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void get_arp_target_ip(uint8_t *data, int *ip)
{
  *ip = 0;
  *ip += (data[38]<<24) & 0xFF000000;
  *ip += (data[39]<<16) & 0x00FF0000;
  *ip += (data[40]<<8)  & 0x0000FF00;
  *ip += (data[41])     & 0x000000FF;  
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static inline uint8_t *get_src_mac(uint8_t *data)
{
  return (&(data[6]));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static inline uint8_t *get_dst_mac(uint8_t *data)
{
  return (&(data[0]));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void put_src_mac(uint8_t *mac, uint8_t *data)
{
  int i;
  for (i=0; i<6; i++)
    data[6+i] = mac[i] & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void put_dst_mac(uint8_t *mac, uint8_t *data)
{
  int i;
  for (i=0; i<6; i++)
    data[i] = mac[i] & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void put_arp_sender_mac(uint8_t *mac, uint8_t *data)
{
  int i;
  for (i=0; i<6; i++)
    data[22+i] = mac[i] & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void put_arp_target_mac(uint8_t *mac, uint8_t *data)
{
  int i;
  for (i=0; i<6; i++)
    data[32+i] = mac[i] & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_proto(int len, uint8_t *data)
{
//  int i;
  int result;
  if ((data[12] == 0x08) && (data[13] == 0x00))
    {
    result = proto_ip;
    }
  else if ((data[12] == 0x08) && (data[13] == 0x06))
    {
    if (data[21] == 0x01)
      result = proto_arp_req;
    if (data[21] == 0x02)
      result = proto_arp_resp;
    }
  else if ((data[12] == 0x86) && (data[13] == 0xdd))
    {
    result = proto_ipv6;
    }
  else
    {
    result = proto_none;
/*
    for (i=0; i<5; i++)
      KERR("%02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX "
           "%02hX %02hX %02hX %02hX %02hX %02hX %02hX %02hX",
           data[0+(16*i)], data[1+(16*i)], data[2+(16*i)], data[3+(16*i)],
           data[4+(16*i)], data[5+(16*i)], data[6+(16*i)], data[7+(16*i)],
           data[8+(16*i)], data[9+(16*i)], data[10+(16*i)], data[11+(16*i)],
           data[12+(16*i)], data[13+(16*i)], data[14+(16*i)], data[15+(16*i)]);
*/
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void associate_privates(int ip, uint8_t *mac)
{
  t_association *cur = head_association;
  t_association *next;
  int already_there = 0;
  while (cur)
    {
    next = cur->next;
    if (cur->ip == ip)
      {
      if (!memcmp(cur->mac, mac, 6))
        already_there = 1;
      else
        {
        if (cur->next)
          cur->next->prev = cur->prev;
        if (cur->prev)
          cur->prev->next = cur->next;
        if (cur == head_association)
          head_association = cur->next;
        free(cur);
        }
      break;
      }
    cur = next;
    }
  if (already_there == 0)
    {
    cur = (t_association *) malloc(sizeof(t_association));
    memset(cur, 0, sizeof(t_association));
    cur->ip = ip;
    memcpy(cur->mac, mac, 6);
    if (head_association)
      head_association->prev = cur;
    cur->next = head_association;
    head_association = cur;
    }
}
/*---------------------------------------------------------------------------*/
 
/*****************************************************************************/
static uint8_t *get_association(int ip)
{
  t_association *cur = head_association;
  uint8_t *mac = NULL;
  while (cur)
    {
    if (cur->ip == ip)
      {
      mac = cur->mac;
      break;
      } 
    cur = cur->next;
    }
  return mac;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int tst_ip_input(int len, uint8_t *data)
{
  int result = -1;
  int ip_len = ((data[2] << 8) & 0xFF00) + (data[3] & 0xFF);
  if ((len > IP_HEADER) && (data[0] == 0x45))
    {
    if ((ip_len <= len) ||
        (ip_len == len + MAC_HEADER) ||
        (ip_len == len + MAC_HEADER - END_FRAME_ADDED_CHECK_LEN))
      {
      if (!in_cksum((u_short *) data, IP_HEADER))
        {
        if (((data[6] & (~0x40)) == 0) && (data[7] == 0))
          result = 0;
        else
          KERR("BAD!!!! IP Fragment %02X %02X\n", data[6], data[7]);
        }
      else
        KERR("BAD!!!! IP checksum\n");
      }
    else
      KERR("BAD IP LENGTH !!!! IP len:%d real rx:%d\n", ip_len, len);
    }
  else
    KERR("BAD!!!! IP 0x45 %d %0hX\n", len, data[0]);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void packet_arp_mangle(int udp2tap, int len, uint8_t *data)
{
  int proto;
  int private_net_ip;
  uint8_t *private_net_mac;
  if (!data)
    KOUT(" ");
  if (len < 40)
    { 
    KERR("TOO SHORT PACKET %d", len);
    return;
    }
  proto = get_proto(len, data);
  switch (proto)
    {
    case proto_arp_req:
    case proto_arp_resp:
      if (udp2tap)
        {
        get_arp_sender_ip(data, &private_net_ip);
        associate_privates(private_net_ip, get_arp_sender_mac(data));
/*        KERR("ARPTX IP-MAC ASSOCIATION: %s %s", 
                    get_ip_string(private_net_ip), 
                    get_mac_string(get_arp_sender_mac(data))); */
        if (src_mac_is_unicast(data))
          put_src_mac(get_wif_hwaddr(), data);
        put_arp_sender_mac(get_wif_hwaddr(), data);
        }
      else
        {
        get_arp_target_ip(data, &private_net_ip);
        private_net_mac = get_association(private_net_ip);
        if (private_net_mac)
          {
/*          KERR("RXGETARP: %s %s", get_ip_string(private_net_ip), 
                                  get_mac_string(private_net_mac)); */
          if (dst_mac_is_unicast(data))
            put_dst_mac(private_net_mac, data);
          put_arp_target_mac(private_net_mac, data);
          }
        }
      break;
    case proto_ip:
      if (!tst_ip_input(len-MAC_HEADER, data+MAC_HEADER))
        {
        private_net_ip = 0;
        if (!udp2tap)
          {
          private_net_ip += (data[MAC_HEADER+16]<<24) & 0xFF000000;
          private_net_ip += (data[MAC_HEADER+17]<<16) & 0x00FF0000;
          private_net_ip += (data[MAC_HEADER+18]<<8)  & 0x0000FF00;
          private_net_ip += (data[MAC_HEADER+19])     & 0x000000FF;  
          private_net_mac = get_association(private_net_ip);
          if (private_net_mac)
            {
            if ((dst_mac_is_unicast(data)) &&
                (!memcmp(get_wif_hwaddr(), get_dst_mac(data), 6)))
              put_dst_mac(private_net_mac, data);
            }
          }
        else
          {
          private_net_ip += (data[MAC_HEADER+12]<<24) & 0xFF000000;
          private_net_ip += (data[MAC_HEADER+13]<<16) & 0x00FF0000;
          private_net_ip += (data[MAC_HEADER+14]<<8)  & 0x0000FF00;
          private_net_ip += (data[MAC_HEADER+15])     & 0x000000FF;  
          private_net_mac = get_association(private_net_ip);
          if (src_mac_is_unicast(data)) 
            {
            if ((!private_net_mac) || 
                (memcmp(private_net_mac, get_src_mac(data), 6)))
              {
              private_net_mac = get_src_mac(data);
              associate_privates(private_net_ip, private_net_mac);
              KERR("IPTX IP-MAC ASSOCIATION: %s %s", 
                          get_ip_string(private_net_ip), 
                          get_mac_string(private_net_mac));
              }
            if (private_net_mac)
              {
              put_src_mac(get_wif_hwaddr(), data);
              }
            }
          }
        }
      break;
    case proto_ipv6:
      break;
    case proto_none:
/*      KERR("NOT SUPPORTED PROTO %hhx %hhx", data[12], data[13]); */
      break;
    default:
      KOUT("%d", proto);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void set_arp_mangle_mac_hwaddr(uint8_t *mac)
{
  int i;
  for (i=0; i<6; i++)
    g_wif_hwaddr[i] = mac[i];
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void init_packet_arp_mangle(void)
{
  int i;
  for (i=0; i<6; i++)
    g_wif_hwaddr[i] = 0;
  head_association = NULL;
}
/*---------------------------------------------------------------------------*/


