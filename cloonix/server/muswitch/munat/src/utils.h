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
#define ICMP_LEN 84

#define MAC_HEADER 14
#define ARP_HEADER 28 
#define IP_HEADER 20 
#define UDP_HEADER 8 
#define TCP_HEADER 20 


#define TH_FIN  0x01
#define TH_SYN  0x02
#define TH_RST  0x04
#define TH_PUSH 0x08
#define TH_ACK  0x10
#define TH_URG  0x20


#define BOOTP_SERVER    67
#define BOOTP_CLIENT    68
#define BOOTP_REQUEST   1
#define BOOTP_REPLY     2
#define DHCP_OPT_LEN		312


#define ICMP_PACKET_LEN (ICMP_LEN+MAC_HEADER)
/*---------------------------------------------------------------------------*/
enum
{
  proto_none,
  proto_arp_req,
  proto_arp_resp,
  proto_ip,
};
/*---------------------------------------------------------------------------*/
void init_utils(char *our_ip_dns, char *our_ip_gw,  char *our_ip_unix2inet);
char *debug_print_packet(int is_tx, int ilen, char *buf);
unsigned short in_cksum(unsigned short *addr, int len);
/*---------------------------------------------------------------------------*/
int get_proto(int len, char *data);
char *get_src_mac(char *data);
char *get_src_mac_zero_end_byte(char *data);
char *get_dst_mac(char *data);
char *get_arp_tip(char *data);
char *get_arp_sip(char *data);
/*---------------------------------------------------------------------------*/
void packet_ip_input(t_machine *machine, char *src_mac, char *dst_mac,
                     int len, char *data);
/*---------------------------------------------------------------------------*/
int format_arp_req(char *sip, char *tip, char **resp_data);
int format_arp_resp(char *mac, char *sip, char *tip, char **resp_data);
/*---------------------------------------------------------------------------*/
void packet_icmp_input(t_machine *machine, char *src_mac, char *dst_mac,
                       char *sip, char *dip, int len, char *data);
void packet_udp_input(t_machine *machine, char *src_mac, char *dst_mac,
                      char *sip, char *dip, int len, char *data);
/*---------------------------------------------------------------------------*/
void fill_mac_ip_header(char *data, char *sip, char *dmac);
void fill_ip_ip_header(int len, char *data, char *sip, char *dip, int proto);
void fill_udp_ip_header(int len, char *data, int sport, int dport);
/*---------------------------------------------------------------------------*/
int format_icmp_tx_buf(char *mac_dst_ascii, char *ip_dst_ascii,
                       int seq_num, char **result);
/*---------------------------------------------------------------------------*/
char *flags_to_ascii(char flags);
/*---------------------------------------------------------------------------*/
char *get_dns_ip(void);
char *get_gw_ip(void);
char *get_unix2inet_ip(void);
/*---------------------------------------------------------------------------*/
int ip_string_to_int (uint32_t *inet_addr, char *ip_string);
/*---------------------------------------------------------------------------*/
void int_to_ip_string (uint32_t addr, char *ip_string);
/*---------------------------------------------------------------------------*/



