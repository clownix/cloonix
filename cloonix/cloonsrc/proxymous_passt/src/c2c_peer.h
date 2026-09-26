/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The switch says a c2c has gone: release the peer entry. */
int c2c_peer_delete(char *name);

int c2c_peer_dist_tcp_ip_port(char *name, uint32_t ip,
                              uint16_t tcp_port, char *passwd);
int c2c_peer_proxy_data_from_crun(char *name, int len, char *buf);
void c2c_peer_init(void);
