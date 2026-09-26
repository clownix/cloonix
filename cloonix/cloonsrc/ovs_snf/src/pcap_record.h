/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void pcap_record_start_phase2(void);
void pcap_record_close(void);
void pcap_record_close_writer(void);
int pcap_record_is_writing(void);
int pcap_record_probe_reader(void);
void pcap_record_rx_packet(uint64_t usec, int len, uint8_t *buf);
void pcap_record_init(char *pcap_file);

