/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void init_packet_arp_mangle(void);
void packet_arp_mangle(int udp2tap, int len, uint8_t *data);
void set_arp_mangle_mac_hwaddr(uint8_t *mac);
