/*
 * Do not modify this file. Changes will be overwritten.
 *
 * Generated automatically using "make-regs.py", then trimmed by hand:
 * make-regs.py is not shipped with this tree, so a dissector removed from
 * the Makefile has to be removed here as well: its declaration and its
 * table entry.
 *
 * The two counts are an upper bound. register.c walks each table to its
 * NULL terminator and replaces the count with what it found, so a count
 * larger than the table is harmless and a count SMALLER than it silently
 * drops the dissectors past it. They are set to the real figures here.
 *
 * The file arrived saying 37 and 33 for tables of 32 and 28, which was
 * harmless, and register.c arrived with an off-by-one that dropped the last
 * entry of each table whatever the counts said. Both are fixed. If a
 * dissector is ever missing at run time with no error anywhere, those two
 * loops are the first place to look.
 */

#include "dissectors.h"

unsigned long dissector_reg_proto_count = 27;
unsigned long dissector_reg_handoff_count = 24;

void proto_register_arp(void);
void proto_register_data(void);
void proto_register_dhcp(void);
void proto_register_dhcpv6(void);
void proto_register_dns(void);
void proto_register_eth(void);
void proto_register_ethertype(void);
void proto_register_file(void);
void proto_register_file_pcap(void);
void proto_register_frame(void);
void proto_register_gre(void);
void proto_register_icmp(void);
void proto_register_igmp(void);
void proto_register_ip(void);
void proto_register_isl(void);
void proto_register_media(void);
void proto_register_media_type(void);
void proto_register_pcap_pktdata(void);
void proto_register_pcapng(void);
void proto_register_sll(void);
void proto_register_smtp(void);
void proto_register_tcp(void);
void proto_register_telnet(void);
void proto_register_text_lines(void);
void proto_register_udp(void);
void proto_register_vlan(void);
void proto_register_wlan(void);

dissector_reg_t const dissector_reg_proto[] = {
    { "proto_register_arp", proto_register_arp },
    { "proto_register_data", proto_register_data },
    { "proto_register_dhcp", proto_register_dhcp },
    { "proto_register_dhcpv6", proto_register_dhcpv6 },
    { "proto_register_dns", proto_register_dns },
    { "proto_register_eth", proto_register_eth },
    { "proto_register_ethertype", proto_register_ethertype },
    { "proto_register_file", proto_register_file },
    { "proto_register_file_pcap", proto_register_file_pcap },
    { "proto_register_frame", proto_register_frame },
    { "proto_register_gre", proto_register_gre },
    { "proto_register_icmp", proto_register_icmp },
    { "proto_register_igmp", proto_register_igmp },
    { "proto_register_ip", proto_register_ip },
    { "proto_register_isl", proto_register_isl },
    { "proto_register_media", proto_register_media },
    { "proto_register_media_type", proto_register_media_type },
    { "proto_register_pcap_pktdata", proto_register_pcap_pktdata },
    { "proto_register_pcapng", proto_register_pcapng },
    { "proto_register_sll", proto_register_sll },
    { "proto_register_smtp", proto_register_smtp },
    { "proto_register_tcp", proto_register_tcp },
    { "proto_register_telnet", proto_register_telnet },
    { "proto_register_text_lines", proto_register_text_lines },
    { "proto_register_udp", proto_register_udp },
    { "proto_register_vlan", proto_register_vlan },
    { "proto_register_wlan", proto_register_wlan },
    { NULL, NULL }
};

void proto_reg_handoff_arp(void);
void proto_reg_handoff_data(void);
void proto_reg_handoff_dhcp(void);
void proto_reg_handoff_dhcpv6(void);
void proto_reg_handoff_dns(void);
void proto_reg_handoff_eth(void);
void proto_reg_handoff_file_pcap(void);
void proto_reg_handoff_frame(void);
void proto_reg_handoff_gre(void);
void proto_reg_handoff_icmp(void);
void proto_reg_handoff_ieee802_3(void);
void proto_reg_handoff_igmp(void);
void proto_reg_handoff_ip(void);
void proto_reg_handoff_isl(void);
void proto_reg_handoff_pcap_pktdata(void);
void proto_reg_handoff_pcapng(void);
void proto_reg_handoff_sll(void);
void proto_reg_handoff_smtp(void);
void proto_reg_handoff_tcp(void);
void proto_reg_handoff_telnet(void);
void proto_reg_handoff_text_lines(void);
void proto_reg_handoff_udp(void);
void proto_reg_handoff_vlan(void);
void proto_reg_handoff_wlan(void);

dissector_reg_t const dissector_reg_handoff[] = {
    { "proto_reg_handoff_arp", proto_reg_handoff_arp },
    { "proto_reg_handoff_data", proto_reg_handoff_data },
    { "proto_reg_handoff_dhcp", proto_reg_handoff_dhcp },
    { "proto_reg_handoff_dhcpv6", proto_reg_handoff_dhcpv6 },
    { "proto_reg_handoff_dns", proto_reg_handoff_dns },
    { "proto_reg_handoff_eth", proto_reg_handoff_eth },
    { "proto_reg_handoff_file_pcap", proto_reg_handoff_file_pcap },
    { "proto_reg_handoff_frame", proto_reg_handoff_frame },
    { "proto_reg_handoff_gre", proto_reg_handoff_gre },
    { "proto_reg_handoff_icmp", proto_reg_handoff_icmp },
    { "proto_reg_handoff_ieee802_3", proto_reg_handoff_ieee802_3 },
    { "proto_reg_handoff_igmp", proto_reg_handoff_igmp },
    { "proto_reg_handoff_ip", proto_reg_handoff_ip },
    { "proto_reg_handoff_isl", proto_reg_handoff_isl },
    { "proto_reg_handoff_pcap_pktdata", proto_reg_handoff_pcap_pktdata },
    { "proto_reg_handoff_pcapng", proto_reg_handoff_pcapng },
    { "proto_reg_handoff_sll", proto_reg_handoff_sll },
    { "proto_reg_handoff_smtp", proto_reg_handoff_smtp },
    { "proto_reg_handoff_tcp", proto_reg_handoff_tcp },
    { "proto_reg_handoff_telnet", proto_reg_handoff_telnet },
    { "proto_reg_handoff_text_lines", proto_reg_handoff_text_lines },
    { "proto_reg_handoff_udp", proto_reg_handoff_udp },
    { "proto_reg_handoff_vlan", proto_reg_handoff_vlan },
    { "proto_reg_handoff_wlan", proto_reg_handoff_wlan },
    { NULL, NULL }
};
