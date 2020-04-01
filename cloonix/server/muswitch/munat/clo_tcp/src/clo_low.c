/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <asm/types.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "ioc.h"
#include "clo_tcp.h"
/*---------------------------------------------------------------------------*/

#define TH_FIN  0x01
#define TH_SYN  0x02
#define TH_RST  0x04
#define TH_PUSH 0x08
#define TH_ACK  0x10
#define TH_URG  0x20

///*****************************************************************************/
//static char *loc_flags_to_ascii(char flags)
//{
//  static char resp[100];
//  memset(resp, 0, 100);
//  if (flags&TH_FIN)
//    strcat(resp, "FIN ");
//  if (flags&TH_SYN)
//    strcat(resp, "SYN ");
//  if (flags&TH_RST)
//    strcat(resp, "RST ");
//  if (flags&TH_PUSH)
//    strcat(resp, "PUSH ");
//  if (flags&TH_ACK)
//    strcat(resp, "ACK ");
//  if (flags&TH_URG)
//    strcat(resp, "URG ");
//  return resp;
//}
///*---------------------------------------------------------------------------*/

/*****************************************************************************/
static u16_t in_cksum(u16_t *addr, int len)
{
  int nleft = len;
  u16_t *w = addr;
  u32_t sum = 0;
  u16_t answer = 0;
  while (nleft > 1)
    {
    sum += *w++;
    nleft -= 2;
    }
  if (nleft == 1)
    {
    answer=0;
    *(u8_t *)(&answer) = *(u8_t *)w ;
    sum += answer;
    }
  sum = (sum >> 16) + (sum & 0xffff);
  sum += (sum >> 16);
  answer = ~sum;
  return(answer);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static u16_t tcp_sum_calc(int len, u8_t *buff, u8_t *pseudo_header)
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
  return ((u16_t) sum);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static u16_t tcp_checksum(int len, u8_t *data, u8_t *iph)
{
  int i;
  u16_t chksum;
  u8_t pseudo_header[12];
  for (i=0; i< 8; i++)
    pseudo_header[i] = iph[12+i];
  pseudo_header[8]  = 0;
  pseudo_header[9]  = IPPROTO_TCP;
  pseudo_header[10] = (len >> 8) & 0xFF;
  pseudo_header[11] = len & 0xFF;
  chksum = tcp_sum_calc(len, data, pseudo_header);
  return chksum;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void mac_get_src_dest(u8_t *mac_data, u8_t *mac_src, u8_t *mac_dest)
{
  int i;
  for (i=0; i<MAC_ADDR_LEN; i++)
    {
    mac_dest[i] = mac_data[i];
    mac_src[i]  = mac_data[i + MAC_ADDR_LEN];
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void ip_get_src_dest(u8_t *ip_data, u32_t *ip_src, u32_t *ip_dest)
{
  *ip_src = 0;
  *ip_src += ip_data[12] << 24;
  *ip_src += ip_data[13] << 16;
  *ip_src += ip_data[14] << 8;
  *ip_src += ip_data[15];
  *ip_dest = 0;
  *ip_dest += ip_data[16] << 24;
  *ip_dest += ip_data[17] << 16;
  *ip_dest += ip_data[18] << 8;
  *ip_dest += ip_data[19];
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_tcp_packet2low(int len, u8_t *mac_data, t_low **low)
{
  u8_t header_len;
  u8_t *ip_data = mac_data + MAC_HLEN;
  u8_t *data = mac_data + MAC_HLEN + IP_HLEN;
  *low = (t_low *) malloc(sizeof(t_low));
  memset((*low), 0, sizeof(t_low));
  mac_get_src_dest(mac_data, (*low)->mac_src, (*low)->mac_dest);
  ip_get_src_dest(ip_data, &((*low)->ip_src), &((*low)->ip_dest));
  (*low)->tcp_src  = ((data[0] << 8) & 0xFF00) + (data[1] & 0xFF);
  (*low)->tcp_dest = ((data[2] << 8) & 0xFF00) + (data[3] & 0xFF);
  (*low)->seqno     = 0;
  (*low)->seqno    += (data[4] << 24) & 0xFF000000;
  (*low)->seqno    += (data[5] << 16) & 0x00FF0000;
  (*low)->seqno    += (data[6] << 8 ) & 0x0000FF00;
  (*low)->seqno    += data[7]         & 0x000000FF;
  (*low)->ackno     = 0;
  (*low)->ackno    += (data[8] << 24) & 0xFF000000;
  (*low)->ackno    += (data[9] << 16) & 0x00FF0000;
  (*low)->ackno    += (data[10]<< 8 ) & 0x0000FF00;
  (*low)->ackno    +=  data[11]        & 0x000000FF;
  header_len        = (data[12]&0xF0)  >> 2;
  (*low)->flags     =  data[13] & 0xFF;
  (*low)->wnd       = ((data[14] << 8) & 0xFF00) + (data[15] & 0xFF);
  (*low)->datalen   = len - header_len;
  (*low)->tcplen    = (*low)->datalen;
  if ((*low)->flags & (TH_FIN | TH_SYN))
    (*low)->tcplen += 1;
  if ((*low)->datalen == 0)
    (*low)->data      = NULL;
  else
    {
    (*low)->data  = (u8_t *) malloc((*low)->datalen);
    memcpy((*low)->data, data + header_len, (*low)->datalen);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_mac_header(u8_t *data, u8_t *local, u8_t *remote)
{
  int i;
  for (i=0; i<MAC_ADDR_LEN; i++)
    {
    data[MAC_ADDR_LEN + i] = local[i] & 0xFF;
    data[i] = remote[i] & 0xFF;
    }
  data[12] = 0x08;
  data[13] = 0x00;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
static void fill_ip_header(int len, u8_t *data, u32_t local, u32_t remote)
{
  u16_t checksum;
  static u16_t identification = 0;
  identification += 1;
  data[0] = 0x45;
  data[1] = 0;
  data[2] = (len >> 8) & 0xFF;
  data[3] = len & 0xFF;
  data[4] =  (identification >> 8) & 0xFF;
  data[5] =  identification & 0xFF;
  data[6] = 0x40;
  data[7] = 0x00;
  data[8] = 0x40;
  data[9] = IPPROTO_TCP & 0xFF;
  data[10] = 0x00;
  data[11] = 0x00;
  data[12] = (local >> 24) & 0xFF;
  data[13] = (local >> 16) & 0xFF;
  data[14] = (local >> 8)  & 0xFF;
  data[15] = local & 0xFF;
  data[16] = (remote >> 24) & 0xFF;
  data[17] = (remote >> 16) & 0xFF;
  data[18] = (remote >> 8)  & 0xFF;
  data[19] =  remote & 0xFF;
  checksum = in_cksum((u16_t *)data, 20);
  data[10] = checksum & 0xFF;
  data[11] = (checksum >> 8) & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fill_tcp(u8_t *ip_data, t_low *low)
{
  u16_t checksum;

  ip_data[IP_HLEN + 0] = (low->tcp_src >> 8) & 0xFF;
  ip_data[IP_HLEN + 1] =  low->tcp_src & 0xFF;
  ip_data[IP_HLEN + 2] = (low->tcp_dest >> 8) & 0xFF;
  ip_data[IP_HLEN + 3] =  low->tcp_dest & 0xFF;

  ip_data[IP_HLEN + 4] = (low->seqno >> 24) & 0xFF;
  ip_data[IP_HLEN + 5] = (low->seqno >> 16) & 0xFF;
  ip_data[IP_HLEN + 6] = (low->seqno >> 8) & 0xFF;
  ip_data[IP_HLEN + 7] =  low->seqno & 0xFF;

  if (low->flags & TH_ACK)
    {
    ip_data[IP_HLEN + 8]  =  (low->ackno  >> 24) & 0xFF;
    ip_data[IP_HLEN + 9]  =  (low->ackno  >> 16) & 0xFF;
    ip_data[IP_HLEN + 10] = (low->ackno  >> 8)  & 0xFF;
    ip_data[IP_HLEN + 11] =  low->ackno  & 0xFF;
    }
  else
    {
    ip_data[IP_HLEN + 8]  = 0;
    ip_data[IP_HLEN + 9]  = 0;
    ip_data[IP_HLEN + 10] = 0;
    ip_data[IP_HLEN + 11] = 0;
    }

  ip_data[IP_HLEN + 12] = (5 << 4) & 0xF0;
  ip_data[IP_HLEN + 13] = low->flags;
  ip_data[IP_HLEN + 14] = (low->wnd >> 8) & 0xFF;
  ip_data[IP_HLEN + 15] = low->wnd & 0xFF;

  ip_data[IP_HLEN + 16] = 0;
  ip_data[IP_HLEN + 17] = 0;
  ip_data[IP_HLEN + 18] = 0;
  ip_data[IP_HLEN + 19] = 0;

  if (low->datalen)
    memcpy(&(ip_data[IP_HLEN + TCP_HLEN]), low->data, low->datalen);

  checksum=tcp_checksum(TCP_HLEN+low->datalen,&(ip_data[IP_HLEN]),ip_data);
  ip_data[IP_HLEN + 16] =  (checksum >> 8) & 0xFF;
  ip_data[IP_HLEN + 17] =  checksum & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void tcp_low2packet(int *mac_len, u8_t **mac_data, t_low *low)
{
  *mac_len =  MAC_HLEN + IP_HLEN + TCP_HLEN + low->datalen;
  *mac_data = (u8_t *) malloc(*mac_len);
  memset((*mac_data), 0, *mac_len); 
  fill_mac_header(*mac_data, low->mac_src, low->mac_dest);
  fill_ip_header((IP_HLEN + TCP_HLEN + low->datalen), 
                  (*mac_data) + MAC_HLEN , 
                  low->ip_src, low->ip_dest);
  fill_tcp((*mac_data) + MAC_HLEN, low);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int tcp_packet2low(int mac_len, u8_t *mac_data, t_low **low)
{
  int result = -1;
  int len;
  u8_t *ip_data = mac_data + MAC_HLEN;
  u8_t *data = mac_data + MAC_HLEN + IP_HLEN;
  len  = 0;
  *low = NULL;
  len  += (ip_data[2] << 8 ) & 0x0000FF00;
  len  += ip_data[3]         & 0x000000FF;
  len  -= IP_HLEN;

  if (mac_len != (len+34))
     KERR("DIFFERENCE !!!!!!!!!!!!!! %d %d", mac_len, len+34);

  if (len < 0)
    KERR("DROP!!!! TCP BAD LEN");
  else if (tcp_checksum(len, data, ip_data))
    {
    if (len != 20)
      KERR("BUGTOLOOKINTO %d  TCP CHECKSUM", len);
    else
      {
      //KERR("IGNORING BAD CHKSUM");
      local_tcp_packet2low(len, mac_data, low);
      result = 0;
      }
    }
  else
    {
    local_tcp_packet2low(len, mac_data, low);
    result = 0;
    }
//  if (result == 0)
//    KERR("LOW %s %d %d %lu %lu", loc_flags_to_ascii((char) ((*low)->flags & 0xFF)),
//                                 (*low)->tcp_src, (*low)->tcp_dest,
//                                 (*low)->seqno, (*low)->ackno);
  return result;
}
/*---------------------------------------------------------------------------*/
