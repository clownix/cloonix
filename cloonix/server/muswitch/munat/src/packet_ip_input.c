/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include <stdint.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/types.h>

#include "ioc.h"
#include "clo_tcp.h"
#include "sock_fd.h"
#include "main.h"
#include "machine.h"
#include "utils.h"
#include "bootp_input.h"
#include "packets_io.h"
#include "llid_slirptux.h"



/****************************************************************************/
static int tst_ip_input(int len, char *data, int *real_len)
{
  int result = -1;
  int ip_len = ((data[2] << 8) & 0xFF00) + (data[3] & 0xFF);
  *real_len = 0;
  if ((len > IP_HEADER) && (data[0] == 0x45))
    {
    if (ip_len <= len)
      {
      *real_len = ip_len;
      if (!in_cksum((u_short *) data, IP_HEADER))
        {
        if (((data[6] & (~0x40)) == 0) && (data[7] == 0))
          result = 0;
        else
          DOUT(get_all_ctx(), FLAG_HOP_DIAG, "DROP!!!! IP Fragment %02X %02X\n", 
                                      data[6], data[7]);
        }
      else
        DOUT(get_all_ctx(), FLAG_HOP_DIAG, "DROP!!!! IP checksum\n");
      }
    else
      DOUT(get_all_ctx(), FLAG_HOP_DIAG, "DROP!!!! IP len:%d real rx:%d\n",
                                 ip_len, len);
    }
  else
    DOUT(get_all_ctx(), FLAG_HOP_DIAG, "DROP!!!! IP 0x45\n");
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int ip_input_get_params(char *data, char *sip, char *dip)
{
  int proto = data[9] & 0xFF;
  sprintf(sip, "%d.%d.%d.%d", data[12]&0xFF, data[13]&0xFF,
                              data[14]&0xFF, data[15]&0xFF); 
  sprintf(dip, "%d.%d.%d.%d", data[16]&0xFF, data[17]&0xFF,
                              data[18]&0xFF, data[19]&0xFF); 
  return proto;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void packet_ip_input(t_machine *machine, char *src_mac, char *dst_mac,
                     int mac_len, char *mac_data)
{
  int proto, len;
  char sip[MAX_NAME_LEN];
  char dip[MAX_NAME_LEN];
  int in_len = mac_len-MAC_HEADER;
  char *data = mac_data+MAC_HEADER;
  if (!tst_ip_input(in_len, data, &len))
    {
    proto = ip_input_get_params(data, sip, dip);
    if (proto == IPPROTO_ICMP)
      {
      machine->tot_rx_pkt++;
      packet_icmp_input(machine, src_mac, dst_mac, sip, dip, 
                        len - IP_HEADER, data + IP_HEADER);
      }
    else if (proto == IPPROTO_UDP)
      {
      packet_udp_input(machine, src_mac, dst_mac, sip, dip, 
                       len - IP_HEADER, data + IP_HEADER);
      }
    else if (proto == IPPROTO_TCP)
      {
      llid_slirptux_tcp_rx_from_slirptux(mac_len, mac_data);
      }
    else 
      DOUT(get_all_ctx(), FLAG_HOP_DIAG, 
                  "DROP!!! VM:%s mac_src:%s mac_dst:%s len:%d\n"
                  "ip_src:%s ip_dst:%s proto:%d\n",
                   machine->name, src_mac, dst_mac, len, sip, dip, proto);
    }
}
/*---------------------------------------------------------------------------*/

