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
#define ETHERNET_HEADER_LEN  14
#define IPV4_HEADER_LEN      20
#define TCP_HEADER_LEN       20
#define UDP_HEADER_LEN        8
#define ICMP_HEADER_LEN       8
uint32_t utils_get_cisco_ip(void);
uint32_t utils_get_dns_ip(void);
uint32_t utils_get_gw_ip(void);
void nat_utils_init(void);
/*---------------------------------------------------------------------------*/
