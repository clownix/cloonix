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


#define ICMP_ECHO               0x08  
#define ICMP_ECHOREPLY          0x00  
#define ICMP_UNREACH            0x03       

int get_glob_uml_cloonix_switch_llid(void);



/*****************************************************************************/
static void packet_icmp_echoreply(char *src_mac, char *dst_mac,
                                  char *sip, char *dip,
                                  int len, char *data)
{
  int tot_len = MAC_HEADER + IP_HEADER + len;
  u_short checksum;
  char *resp;
  resp =  (char *) malloc(tot_len);
  memset(resp, 0, tot_len);
  memcpy(&(resp[MAC_HEADER + IP_HEADER]), data, len);
  resp[MAC_HEADER + IP_HEADER] = ICMP_ECHOREPLY;
  resp[MAC_HEADER + IP_HEADER + 1] = 0;
  resp[MAC_HEADER + IP_HEADER + 2] = 0;
  resp[MAC_HEADER + IP_HEADER + 3] = 0;
  checksum = in_cksum((u_short *) &(resp[MAC_HEADER + IP_HEADER]), len);
  resp[MAC_HEADER + IP_HEADER + 2] = checksum & 0xFF;
  resp[MAC_HEADER + IP_HEADER + 3] = (checksum >> 8) & 0xFF;
  fill_mac_ip_header(resp, dip, src_mac);
  fill_ip_ip_header((IP_HEADER + len), &(resp[MAC_HEADER]),
                     dip, sip, IPPROTO_ICMP);
  packet_output_to_slirptux(tot_len, resp);
  free(resp);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void packet_icmp_input(t_machine *machine, char *src_mac, char *dst_mac,
                      char *sip, char *dip, int len, char *data)
{
  if (!in_cksum((u_short *) data, len))
    {
    if (data[0] == ICMP_ECHO)
      {
      if ((!strcmp(dip, get_gw_ip())) ||
          (!strcmp(dip, get_dns_ip())) ||
          (!strcmp(dip, get_unix2inet_ip())))
        {
        packet_icmp_echoreply(src_mac, dst_mac, sip, dip, len, data);
        }
      else
        KERR("DROP !!! ICMP %s %s %s\n", machine->name, sip, dip);
      }
    else if (data[0] == ICMP_ECHOREPLY)
      {
      }
    else
      KERR("DROP !!! ICMP %s %s %s\n", machine->name, sip, dip);
    }
  else
    KERR("DROP ICMP CHKSUM %s %s %s\n", machine->name, sip, dip);
}
/*---------------------------------------------------------------------------*/



