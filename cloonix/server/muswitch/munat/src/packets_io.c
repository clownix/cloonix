/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "ioc.h"
#include "sock_fd.h"
#include "machine.h"
#include "bootp_input.h"
#include "clo_tcp.h"
#include "utils.h"
#include "main.h"
#include "packets_io.h"
#include "unix2inet.h"


/****************************************************************************/
static void unix2inet_or_gw_or_dns_arp_arrival(char *data, char *src_mac)
{
  int resp_len;
  char *arp_tip, *arp_sip, *resp_data;
  arp_tip = get_arp_tip(data);
  arp_sip = get_arp_sip(data);
  if ((!strcmp(arp_tip, get_gw_ip())) ||
      (!strcmp(arp_tip, get_dns_ip()))||
      (!strcmp(arp_tip, get_unix2inet_ip())))
    {
    resp_len = format_arp_resp(src_mac, arp_sip, arp_tip, &resp_data);
    packet_output_to_slirptux(resp_len, resp_data);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void unix2inet_arp_resp_arrival(char *data, char *src_mac)
{
  char *arp_tip, *arp_sip;
  arp_tip = get_arp_tip(data);
  arp_sip = get_arp_sip(data);
  if (!strcmp(arp_tip, get_unix2inet_ip()))
    {
    unix2inet_arp_resp(src_mac, arp_sip);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void packet_input_from_slirptux(int len, char *data)
{
  t_machine *machine;
  int proto;
  char *src_mac, *dst_mac;
  proto = get_proto(len, data);
  if (proto == proto_arp_resp)
    {
    dst_mac = get_dst_mac(data);
    src_mac = get_src_mac(data);
    if (!strcmp(dst_mac, OUR_MAC_CISCO))
      unix2inet_arp_resp_arrival(data, src_mac);
    }
  else if ((proto == proto_arp_req) || (proto == proto_ip))
    {
    dst_mac = get_dst_mac(data);
    src_mac = get_src_mac(data);
    if ((data[0] & 0x01) || 
        (!strcmp(dst_mac, OUR_MAC_GW)) ||
        (!strcmp(dst_mac, OUR_MAC_DNS)) || 
        (!strcmp(dst_mac, OUR_MAC_CISCO)))
      {
      if (proto == proto_arp_req)
        unix2inet_or_gw_or_dns_arp_arrival(data, src_mac);
      else
        {
        machine = look_for_machine_with_mac(src_mac);
        if (machine)
          packet_ip_input(machine, src_mac, dst_mac, len, data); 
        else 
          {
          if (add_machine(src_mac))
            KOUT("%s", src_mac);
          machine = look_for_machine_with_mac(src_mac);
          if (!machine)
            KOUT("%s", src_mac);
          packet_ip_input(machine, src_mac, dst_mac, len, data); 
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_packets_io(t_all_ctx *all_ctx, void *data)
{

  clownix_timeout_add(get_all_ctx(), 100, timer_packets_io, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void packet_output_to_slirptux(int len, char *data)
{
  t_blkd *blkd;
  blkd = blkd_create_tx_full_copy(len, data, 0, 0, 0);
  sock_fd_tx(get_all_ctx(), blkd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void packets_io_init(void)
{

  clownix_timeout_add(get_all_ctx(), 100, timer_packets_io, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/



