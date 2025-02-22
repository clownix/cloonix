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
void proxycrun_transmit_req_nat(char *name, int on);
void proxycrun_kill(void);
int proxycrun_connect_proxy_ok(void);
void proxycrun_transmit_proxy_data(char *name, int len, char *buf);
void proxycrun_transmit_dist_tcp_ip_port(char *name, uint32_t ip,
                                         uint16_t port, char *passwd);
void proxycrun_transmit_dist_udp_ip_port(char *name,uint32_t ip,uint16_t port);
void proxycrun_transmit_req_udp(char *name);
int proxycrun_transmit_config(int pweb, char *passwd);
void proxycrun_init(int pweb, char *passwd);
