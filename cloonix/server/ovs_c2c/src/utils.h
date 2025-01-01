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

#include <arpa/inet.h>


#define MAIN_INACTIVITY_COUNT 1000000
#define MAIN_DESTRUCTION_DECREMENTER 3000
#define RESET_DESTRUCTION_DECREMENTER 2000

#define RTE_TCP_CWR_FLAG 0x80 /**< Congestion Window Reduced */
#define RTE_TCP_ECE_FLAG 0x40 /**< ECN-Echo */
#define RTE_TCP_URG_FLAG 0x20 /**< Urgent Pointer field significant */
#define RTE_TCP_ACK_FLAG 0x10 /**< Acknowledgment field significant */
#define RTE_TCP_PSH_FLAG 0x08 /**< Push Function */
#define RTE_TCP_RST_FLAG 0x04 /**< Reset the connection */
#define RTE_TCP_SYN_FLAG 0x02 /**< Synchronize sequence numbers */
#define RTE_TCP_FIN_FLAG 0x01 /**< No more data from sender */


#define ARP_OP_REQUEST    1 /* request to resolve address */
#define ARP_OP_REPLY      2 /* response to previous request */
#define ARP_OP_REVREQUEST 3 /* request proto addr given hardware */
#define ARP_OP_REVREPLY   4 /* response giving protocol address */
#define ARP_OP_INVREQUEST 8 /* request to identify peer */
#define ARP_OP_INVREPLY   9 /* response identifying peer */


#define BOOTP_SERVER    67
#define BOOTP_CLIENT    68

#define MAX_RXTX_LEN 1500

#define ETHERNET_HEADER_LEN             14
#define IPV4_HEADER_LEN                 20
#define TCP_HEADER_LEN                  20
#define UDP_HEADER_LEN  8
#define ICMP_HEADER_LEN  8

#define ARP_IPV4_LEN      20
#define ARP_HEADER_LEN    28

#define MAX_TAP_BUF_LEN 1600
#define HEADER_TAP_MSG 4
#define END_FRAME_ADDED_CHECK_LEN 4

#define TOT_MSG_BUF_LEN (MAX_TAP_BUF_LEN+HEADER_TAP_MSG+END_FRAME_ADDED_CHECK_LEN)


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


char *utils_spy_on_tcp(int len, uint8_t *buf);
void* utils_malloc(int size);
void utils_free(void *ptr);
void utils_init(void);
/*---------------------------------------------------------------------------*/
