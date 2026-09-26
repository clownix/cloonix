/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void rxtx_tx_enqueue(int len, uint8_t *buf);
void rxtx_mac_mangle(uint8_t *mac);
void rxtx_init(int fd_rx_from_tap, int fd_tx_to_tap);
