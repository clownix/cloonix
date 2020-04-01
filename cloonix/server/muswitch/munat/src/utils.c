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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#include "ioc.h"
#include "machine.h"
#include "clo_tcp.h"
#include "utils.h"

static struct timeval start_time;
static void put_ip(char *ip, char *buf);
static void put_src_mac(char *mac, char *data);
static void put_dst_mac(char *mac, char *data);


static char g_our_ip_dns[MAX_NAME_LEN];
static char g_our_ip_gw[MAX_NAME_LEN];
static char g_our_ip_unix2inet[MAX_NAME_LEN];

/*****************************************************************************/
int ip_string_to_int (uint32_t *inet_addr, char *ip_string)
{
  int result = -1;
  unsigned int part[4];
  if (strlen(ip_string) != 0)
    {
    if ((sscanf(ip_string,"%u.%u.%u.%u",
                          &part[0], &part[1], &part[2], &part[3]) == 4) &&
        (part[0]<=0xFF) && (part[1]<=0xFF) &&
        (part[2]<=0xFF) && (part[3]<=0xFF))
      {
      result = 0;
      *inet_addr = 0;
      *inet_addr += part[0]<<24;
      *inet_addr += part[1]<<16;
      *inet_addr += part[2]<<8;
      *inet_addr += part[3];
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void int_to_ip_string (uint32_t addr, char *ip_string)
{
  sprintf(ip_string, "%d.%d.%d.%d", (int) ((addr >> 24) & 0xff),
          (int)((addr >> 16) & 0xff), (int)((addr >> 8) & 0xff),
          (int) (addr & 0xff));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *flags_to_ascii(char flags)
{
  static char resp[100];
  memset(resp, 0, 100);
  if (flags&TH_FIN)
    strcat(resp, "FIN ");
  if (flags&TH_SYN)
    strcat(resp, "SYN ");
  if (flags&TH_RST)
    strcat(resp, "RST ");
  if (flags&TH_PUSH)
    strcat(resp, "PUSH ");
  if (flags&TH_ACK)
    strcat(resp, "ACK ");
  if (flags&TH_URG)
    strcat(resp, "URG ");
  return resp;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
char *get_dns_ip(void)
{
  return (g_our_ip_dns);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_gw_ip(void)
{
  return (g_our_ip_gw);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_unix2inet_ip(void)
{
  return (g_our_ip_unix2inet);
}
/*--------------------------------------------------------------------------*/






/*****************************************************************************/
char *debug_print_packet(int is_tx, int ilen, char *buf)
{
  static char bf[2000];
  struct timeval cur;
  int i, len, ln=0;
  if (ilen < 0)
    KOUT(" ");
  if (ilen > 2000)
    len = 2000;
  else
    len = ilen;
  gettimeofday(&cur, NULL);
  cur.tv_sec -= start_time.tv_sec;
  cur.tv_usec /= 1000;
  if (is_tx)
    printf("\nTX: %ds %dms len:%d\n", (int)cur.tv_sec, (int)cur.tv_usec, len);
  else
    printf("\nRX: %ds %dms len:%d\n", (int)cur.tv_sec, (int)cur.tv_usec, len);
  for (i=0; i<len; i++)
    {
    if (i%16 == 0)
      ln += sprintf(bf+ln, "\n\t");
    ln += sprintf(bf+ln, "%02X ", (unsigned int)(buf[i] & 0xFF));
    }
  ln += sprintf(bf+ln, "\n-------------------------------------------------------");
  return bf;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
unsigned short in_cksum(unsigned short *addr, int len)
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
int format_icmp_tx_buf(char *mac_dst_ascii, char *ip_dst_ascii,
                       int seq_num, char **result)
{
  static char tx_buf[ICMP_PACKET_LEN];
  unsigned short checksum;
  int ident = 0xCAFE;
  int lg = ICMP_LEN;
  int lg_tot = ICMP_PACKET_LEN;
  int i;
  *result = tx_buf;

  put_src_mac(OUR_MAC_GW, tx_buf);
  put_dst_mac(mac_dst_ascii, tx_buf);

  tx_buf[12] = 0x08;
  tx_buf[13] = 0x00;
  tx_buf[14] = 0x45;
  tx_buf[15] = 0x00;
  tx_buf[16] = (lg  >> 8) & 0xFF;
  tx_buf[17] =  lg & 0xFF;
  tx_buf[18] = 0x00;
  tx_buf[19] = 0x00;
  tx_buf[20] = 0x40;
  tx_buf[21] = 0x00;
  tx_buf[22] = 0x40;
  tx_buf[23] = 0x01;
  tx_buf[24] = 0x00;
  tx_buf[25] = 0x00;

  put_ip(get_gw_ip(), &(tx_buf[26]));
  put_ip(ip_dst_ascii, &(tx_buf[30]));

  checksum = in_cksum((u_short *) &(tx_buf[14]), 20);
  tx_buf[24] = checksum & 0xFF;
  tx_buf[25] = (checksum >> 8) & 0xFF;
  tx_buf[34] = 0x08;
  tx_buf[35] = 0x00;
  tx_buf[36] = 0;
  tx_buf[37] = 0;
  tx_buf[38] = (ident >> 8) & 0xFF;
  tx_buf[39] = (ident) & 0xFF;
  tx_buf[40] = (seq_num >> 8) & 0xFF;
  tx_buf[41] = (seq_num) & 0xFF;
  for (i=42; i<lg_tot; i++)
    tx_buf[i] = i & 0xFF;
  checksum = in_cksum((u_short *) &(tx_buf[34]), (lg_tot - 34));
  tx_buf[36] = checksum & 0xFF;
  tx_buf[37] = (checksum >> 8) & 0xFF;
  return lg_tot;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_proto(int len, char *data)
{
  int result = proto_none;
  if (len > MAC_HEADER)
    {
    if ((data[12] == 0x08) && (data[13] == 0x00))
      result = proto_ip;
    if ((data[12] == 0x08) && (data[13] == 0x06))
      {
      if (data[21] == 0x01)
        result = proto_arp_req;
      if (data[21] == 0x02)
        result = proto_arp_resp;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_src_mac(char *data)
{
  static char mac[MAX_NAME_LEN];
  sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
                data[6]&0xFF, data[7]&0xFF, data[8]&0xFF,
                data[9]&0xFF, data[10]&0xFF, data[11]&0xFF);
  return mac;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_src_mac_zero_end_byte(char *data)
{
  static char mac[MAX_NAME_LEN];
  sprintf(mac, "%02X:%02X:%02X:%02X:%02X:00",
                data[6]&0xFF, data[7]&0xFF, data[8]&0xFF,
                data[9]&0xFF, data[10]&0xFF);
  return mac;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
char *get_dst_mac(char *data)
{
  static char mac[MAX_NAME_LEN];
  sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", 
                data[0]&0xFF, data[1]&0xFF, data[2]&0xFF,
                data[3]&0xFF, data[4]&0xFF, data[5]&0xFF);
  return mac;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_arp_sip(char *data)
{
  static char ip[MAX_NAME_LEN];
  sprintf(ip, "%d.%d.%d.%d", (data[28]&0xFF),(data[29]&0xFF),
                             (data[30]&0xFF),(data[31]&0xFF)); 
  return ip;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_arp_tip(char *data)
{
  static char ip[MAX_NAME_LEN];
  sprintf(ip, "%d.%d.%d.%d", (data[38]&0xFF),(data[39]&0xFF),
                             (data[40]&0xFF),(data[41]&0xFF));
  return ip;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void put_src_mac(char *mac, char *data)
{
  int i, m[6];
  if (sscanf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", &(m[0]), &(m[1]),&(m[2]),
                                                   &(m[3]), &(m[4]),&(m[5])));
  for (i=0; i<6; i++)
    data[6+i] = m[i] & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void put_dst_mac(char *mac, char *data)
{
  int i, m[6];
  if (sscanf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", &(m[0]), &(m[1]),&(m[2]), 
                                                   &(m[3]), &(m[4]),&(m[5])));
  for (i=0; i<6; i++)
    data[i] = m[i] & 0xFF;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void put_ip(char *ip, char *buf)
{
  uint32_t inet_addr;
  if (ip_string_to_int (&inet_addr, ip))
    KOUT("%s", ip);
  buf[0] = (inet_addr >> 24) & 0xFF;
  buf[1] = (inet_addr >> 16) & 0xFF;
  buf[2] = (inet_addr >> 8) & 0xFF;
  buf[3] = inet_addr & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int format_arp_req(char *sip, char *tip, char **resp_data)
{
  static char resp[MAC_HEADER+ARP_HEADER];
  char *our_mac;
  int resp_len = MAC_HEADER+ARP_HEADER;
  *resp_data = resp;
  if (!strcmp(tip, get_gw_ip())) 
    our_mac = OUR_MAC_GW;
  else if (!strcmp(tip, get_dns_ip())) 
    our_mac = OUR_MAC_DNS;
  else
    our_mac = OUR_MAC_CISCO;
  memset(resp, 0, resp_len);
  put_src_mac(our_mac, resp);
  put_dst_mac("FF:FF:FF:FF:FF:FF", resp);
  resp[12] = 0x08;
  resp[13] = 0x06;
  resp[14] = 0x00;
  resp[15] = 0x01;
  resp[16] = 0x08;
  resp[17] = 0x00;
  resp[18] = 0x06;
  resp[19] = 0x04;
  resp[20] = 0x00;
  resp[21] = 0x01;
  put_dst_mac(our_mac, &(resp[22]));
  put_ip(sip, &(resp[28]));
  put_dst_mac("00:00:00:00:00:00", &(resp[32]));
  put_ip(tip, &(resp[38]));
  return resp_len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int format_arp_resp(char *mac, char *sip, char *tip, char **resp_data)
{
  static char resp[MAC_HEADER+ARP_HEADER];
  char *our_mac;
  int resp_len = MAC_HEADER+ARP_HEADER;
  *resp_data = resp;
  if (!strcmp(tip, get_gw_ip()))
    our_mac = OUR_MAC_GW;
  else if (!strcmp(tip, get_dns_ip()))
    our_mac = OUR_MAC_DNS;
  else if (!strcmp(tip, get_unix2inet_ip()))
    our_mac = OUR_MAC_CISCO;
  else
    KOUT("%s", tip);
  memset(resp, 0, resp_len);
  put_src_mac(our_mac, resp);
  put_dst_mac(mac, resp);
  resp[12] = 0x08;
  resp[13] = 0x06;
  resp[14] = 0x00;
  resp[15] = 0x01;
  resp[16] = 0x08;
  resp[17] = 0x00;
  resp[18] = 0x06;
  resp[19] = 0x04;
  resp[20] = 0x00;
  resp[21] = 0x02;
  put_dst_mac(our_mac, &(resp[22]));
  put_ip(tip, &(resp[28]));
  put_dst_mac(mac, &(resp[32]));
  put_ip(sip, &(resp[38]));
  return resp_len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void fill_mac_ip_header(char *data, char *sip, char *dmac)
{
  char *our_mac;
  if (!strcmp(sip, get_gw_ip()))
    our_mac = OUR_MAC_GW;
  else if (!strcmp(sip, get_dns_ip()))
    our_mac = OUR_MAC_DNS;
  else if (!strcmp(sip, get_unix2inet_ip()))
    our_mac = OUR_MAC_CISCO;
  else
    KOUT("%s", sip);
  put_src_mac(our_mac, data);
  put_dst_mac(dmac, data);
  data[12] = 0x08;
  data[13] = 0x00;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void fill_ip_ip_header(int len, char *data, char *sip, char *dip, int proto)
{
  unsigned short checksum;
  static unsigned short identification = 0;
  uint32_t src_ip, dst_ip;
  if (ip_string_to_int (&src_ip, sip))
    KOUT(" ");
  if (ip_string_to_int (&dst_ip, dip))
    KOUT(" ");
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
  data[9] = proto & 0xFF;
  data[10] = 0x00;
  data[11] = 0x00;
  data[12] = (src_ip >> 24) & 0xFF;
  data[13] = (src_ip >> 16) & 0xFF;
  data[14] = (src_ip >> 8)  & 0xFF;
  data[15] = src_ip & 0xFF;
  data[16] = (dst_ip >> 24) & 0xFF;
  data[17] = (dst_ip >> 16) & 0xFF;
  data[18] = (dst_ip >> 8)  & 0xFF;
  data[19] =  dst_ip & 0xFF;
  checksum = in_cksum((u_short *)data, 20);
  data[10] = checksum & 0xFF;
  data[11] = (checksum >> 8) & 0xFF;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void fill_udp_ip_header(int len, char *data, int sport, int dport)
{
  data[0] = (sport >> 8) & 0xFF;
  data[1] = sport & 0xFF;
  data[2] = (dport >> 8) & 0xFF;
  data[3] = dport & 0xFF;
  data[4] = (len >> 8) & 0xFF;
  data[5] = len & 0xFF;
  data[6] = 0x00;
  data[7] = 0x00;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void init_utils(char *our_ip_dns, char *our_ip_gw, char *our_ip_unix2inet)
{
  gettimeofday(&start_time, NULL);
  memset(g_our_ip_dns, 0, MAX_NAME_LEN);
  strncpy(g_our_ip_dns, our_ip_dns, MAX_NAME_LEN - 1);
  memset(g_our_ip_gw, 0, MAX_NAME_LEN);
  strncpy(g_our_ip_gw, our_ip_gw, MAX_NAME_LEN - 1);
  memset(g_our_ip_unix2inet, 0, MAX_NAME_LEN);
  strncpy(g_our_ip_unix2inet, our_ip_unix2inet, MAX_NAME_LEN - 1);
}
/*---------------------------------------------------------------------------*/
