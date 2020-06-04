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
void tcp_flush_all(void);
void tcp_end_of_flow(uint32_t sip, uint32_t dip,
                     uint16_t sport, uint16_t dport, int fast);
void tcp_rx_from_llid(int llid, int data_len, uint8_t *data);
void tcp_rx_err(int llid);
void tcp_connect_resp(uint32_t sip, uint32_t dip,
                      uint16_t sport, uint16_t dport,
                      int fd, int llid, int is_ko);

void tcp_input(uint8_t *smac, uint8_t *dmac, struct rte_ipv4_hdr *ipv4_h,
               struct rte_tcp_hdr *tcp_h, int data_len, uint8_t *data);

void tcp_init(void);
/*--------------------------------------------------------------------------*/

