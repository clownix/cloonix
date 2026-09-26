/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */

/* ovs_snf --fifo--> cloonix-dumpcap --unix sock--> HERE --rpc--> client
                                     \--fifo--> webshark */
#ifndef PCAP_RELAY_H
#define PCAP_RELAY_H

void pcap_relay_open(int llid, int tid, char *name, int num,
                     char *filter);
void pcap_relay_close(int llid, int tid, char *name, int num);

void pcap_relay_snf_gone(char *name, int num);

void pcap_relay_beat(void);

void pcap_relay_init(void);

#define PCAP_TRACE 0

#if PCAP_TRACE
#define PLOG(fmt, ...) KERR("INFO pcap " fmt, ##__VA_ARGS__)
#else
#define PLOG(fmt, ...) do { } while (0)
#endif

#endif
