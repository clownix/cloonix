/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
void sig_process_heartbeat(void);
void sig_process_proxy_peer_data_to_crun(char *name, int len, char *buf);
void sig_process_tx_proxy_callback_connect(char *name, int pair_llid);
int get_llid_sig(void);
char *get_proxyshare(void);

void udp_proxy_dist_udp_ip_port(char *name, uint32_t ip, uint16_t udp_port);
void udp_proxy_end(void);
/* The proxy of this c2c, and only that one. */
void udp_proxy_end_of(char *name);
/* One udp proxy per c2c, up to five at once. */
int udp_proxy_init_for(char *name, char *proxyshare, uint16_t *udp_port,
                       uint16_t c2c_udp_port_low);
void udp_proxy_init_all(void);
void sig_process_rx(char *proxyshare, char *sigbuf);
int set_config(char *net, uint16_t pmain, uint16_t pjsweb, char *passwd);
int jsweb_relay_init(uint16_t pjsweb);
int jsweb_relay_nb_peers(void);
