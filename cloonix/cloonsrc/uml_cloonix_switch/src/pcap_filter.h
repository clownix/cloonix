/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */

/* tcpdump-syntax capture filter of the pcap relay. Understood: ip ip6 arp tcp
   udp icmp icmp6, [src|dst] host, net <ip>/<len>, [src|dst] port, portrange,
   less, greater, and/or/not (&& || !), parentheses. No offsets/vlan/state. */
#ifndef PCAP_FILTER_H
#define PCAP_FILTER_H

#include <stdint.h>

typedef struct t_pcap_filter t_pcap_filter;

t_pcap_filter *pcap_filter_compile(const char *text, char *err, int errlen);

int pcap_filter_match(t_pcap_filter *flt, const uint8_t *pkt, int len);

void pcap_filter_free(t_pcap_filter *flt);

const char *pcap_filter_text(t_pcap_filter *flt);

#endif
