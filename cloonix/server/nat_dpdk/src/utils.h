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
#define EMPTY_HEAD 12
#define BOOTP_SERVER    67
#define BOOTP_CLIENT    68

#define MAX_RXTX_LEN 1500

struct rte_mbuf *utils_alloc_pktmbuf(int len);
struct rte_mempool *get_rte_mempool(void);
struct rte_mbuf *utils_alloc_and_copy_pktmbuf(int len, uint8_t *resp);
uint32_t utils_get_cisco_ip(void);
uint32_t utils_get_dns_ip(void);
uint32_t utils_get_gw_ip(void);
void utils_get_mac(char *str_mac, uint8_t mac[6]);

void utils_fill_udp_packet(uint8_t *buf,    uint32_t data_len,
                           uint8_t *smac,   uint8_t *dmac,
                           uint32_t sip,    uint32_t dip,
                           uint16_t sport,  uint16_t dport);

void utils_fill_tcp_packet(uint8_t *buf,  uint32_t data_len,
                           uint8_t *smac,     uint8_t *dmac,
                           uint32_t sip,      uint32_t dip,
                           uint16_t sport,    uint16_t dport,
                           uint32_t sent_seq, uint32_t recv_ack,
                           uint8_t tcp_flags, uint16_t rx_win);

void format_cisco_arp_req(uint8_t *data, uint8_t *smac, uint8_t *dmac,
                          uint32_t sip, uint32_t dip);


void utils_init(void);
/*---------------------------------------------------------------------------*/
