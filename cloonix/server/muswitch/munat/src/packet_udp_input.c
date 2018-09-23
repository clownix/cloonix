/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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

#include "ioc.h"
#include "clo_tcp.h"
#include "sock_fd.h"
#include "main.h"
#include "machine.h"
#include "utils.h"
#include "bootp_input.h"
#include "packets_io.h"
#include "llid_slirptux.h"

/*****************************************************************************/
static void transfert_onto_udp(t_machine *machine, 
                               char *src_mac, char *dst_mac,
                               char *sip, char *dip, 
                               short sport, short dport,
                               char *real_dip,
                               int len, char *data)
{
  t_machine_sock *udp_sock;
  uint32_t dst_addr;
  struct sockaddr_in addr;
  if (ip_string_to_int (&dst_addr, dip))
    KOUT(" ");
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(dst_addr);
  addr.sin_port = htons(dport); 
  udp_sock = machine_get_udp_sock(machine, real_dip, dport, sip, sport);
  if (udp_sock)
    {
    sendto(udp_sock->fd, data, len, 0,
           (struct sockaddr *)&addr, sizeof (struct sockaddr));
    }
  else
    DOUT(get_all_ctx(), FLAG_HOP_DIAG, 
                "DROP UDP !!! VM:%s mac_src:%s mac_dst:%s len:%d\n"
                "ip_src:%s ip_dst:%s UDP sport:%d dport:%d\n",
                 machine->name, src_mac, dst_mac, len,
                 sip, dip, sport, dport);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void packet_udp_input(t_machine *machine, char *src_mac, char *dst_mac,
                      char *sip, char *dip, int len, char *data)
{
  int udp_len;
  uint32_t dst_addr, our_addr;
  short sport, dport;
  sport = ((data[0] << 8) & 0xFF00) + (data[1] & 0xFF);
  dport = ((data[2] << 8) & 0xFF00) + (data[3] & 0xFF);
  udp_len = ((data[4] << 8) & 0xFF00) + (data[5] & 0xFF);
  if (udp_len == len)
    {
    if ((dport == BOOTP_SERVER) && (sport == BOOTP_CLIENT)) 
      {
      packet_bootp_input(machine, src_mac, dst_mac, 
                         len - UDP_HEADER, data + UDP_HEADER);
      }
    else
      {
      if (ip_string_to_int (&dst_addr, dip))
        KOUT(" ");
      if (ip_string_to_int (&our_addr, get_gw_ip()))
        KOUT(" ");
      if ((dst_addr & 0xFFFFFF00) == (our_addr & 0xFFFFFF00))
        {
        if (!strcmp(dip, get_dns_ip()))
          transfert_onto_udp(machine, src_mac, dst_mac, sip, 
                             get_dns_from_resolv(), sport, dport, dip,
                             len - UDP_HEADER, data + UDP_HEADER);
        if (!strcmp(dip, get_gw_ip()))
          {
          if (!strcmp(get_dns_from_resolv(), get_dns_ip()))
            transfert_onto_udp(machine, src_mac, dst_mac, sip, 
                               get_gw_ip(), sport, dport, dip, 
                               len - UDP_HEADER, data + UDP_HEADER);
          else
            transfert_onto_udp(machine, src_mac, dst_mac, sip, 
                               "127.0.0.1", sport, dport, dip, 
                               len - UDP_HEADER, data + UDP_HEADER);
          }
        }
      else
        transfert_onto_udp(machine, src_mac, dst_mac, sip, dip, 
                           sport, dport, dip, 
                           len - UDP_HEADER, data + UDP_HEADER);
      }
    }
  else
    DOUT(get_all_ctx(), FLAG_HOP_DIAG, "DROP!!! VM:%s mac_src:%s mac_dst:%s len:%d\n"
                "ip_src:%s ip_dst:%s UDP sport:%d dport:%d udp_len:%d\n",
                 machine->name, src_mac, dst_mac, len, sip, dip, 
                 sport, dport, udp_len);
}
/*---------------------------------------------------------------------------*/

