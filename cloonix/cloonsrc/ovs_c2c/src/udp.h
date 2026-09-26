/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
int  udp_tx_sig_send(int len, uint8_t *tx);
int  udp_tx_traf_send(int len, uint8_t *tx);
int  udp_get_traffic_mngt(void);
void udp_enter_traffic_mngt(void);
void udp_close(void);
int udp_init(uint16_t udp_port);
