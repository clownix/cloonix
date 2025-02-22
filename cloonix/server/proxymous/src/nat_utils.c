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
#include "nat_utils.h"

static uint32_t g_mac_len;
static uint32_t g_ip_len;
static uint32_t g_udp_len;
static uint32_t g_tcp_len;

/****************************************************************************/
uint32_t utils_get_cisco_ip(void)
{
  uint32_t result;
  char *ip = NAT_IP_CISCO;
  if (ip_string_to_int (&result, ip))
    KOUT("%s", ip);
  return (result);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
uint32_t utils_get_dns_ip(void)
{
  uint32_t result;
  char *ip = NAT_IP_DNS;
  if (ip_string_to_int (&result, ip))
    KOUT("%s", ip);
  return (result);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
uint32_t utils_get_gw_ip(void)
{
  uint32_t result;
  char *ip = NAT_IP_GW;
  if (ip_string_to_int (&result, ip))
    KOUT("%s", ip);
  return (result);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void nat_utils_init(void)
{
  g_mac_len = ETHERNET_HEADER_LEN;
  g_ip_len  = IPV4_HEADER_LEN;
  g_udp_len = UDP_HEADER_LEN;
  g_tcp_len = TCP_HEADER_LEN;
}
/*---------------------------------------------------------------------------*/

