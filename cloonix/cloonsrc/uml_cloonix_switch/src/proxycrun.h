/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void proxycrun_transmit_write_start_status_file_ready(void);
void proxycrun_transmit_req_nat(char *name, int on, char *ports);
void proxycrun_transmit_req_nat_publish(char *name, char *ports);
void proxycrun_kill(void);
int proxycrun_connect_proxy_ok(void);
long long proxycrun_silence_ms(void);
void proxycrun_transmit_proxy_data(char *name, int len, char *buf);
void proxycrun_transmit_c2c_del(char *name);

void proxycrun_transmit_dist_tcp_ip_port(char *name, uint32_t ip,
                                         uint16_t port, char *passwd);
void proxycrun_transmit_dist_udp_ip_port(char *name,uint32_t ip,uint16_t port);
void proxycrun_transmit_req_udp(char *name, int c2c_udp_port_low);
void proxycrun_init(char *passwd);
