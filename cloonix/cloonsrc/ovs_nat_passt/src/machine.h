/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
uint32_t machine_ip_get(char *name);
void machine_learn_ip(uint8_t *mac, uint32_t ip);
int machine_known(uint8_t *macs, uint32_t *ips, int max);
int machine_mac_of_ip(uint32_t ip, uint8_t *mac);
void machine_begin(void);
void machine_add(char *name, int num, uint8_t *mc);
void machine_end(void);

