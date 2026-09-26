/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */

int doorways_header_size(void);
typedef void (*t_doorways_end)(int llid);
typedef void (*t_doorways_llid)(int listen_llid, int llid);
typedef void (*t_doorways_rx)(int llid, int tid,
                              int len_bufraw, char *doors_bufraw,
                              int type, int val, int len, char *buf);
void doorways_sock_server(char *net_name, char *passwd,
                          t_doorways_llid cb_llid, t_doorways_end cb_end,
                          t_doorways_rx cb_rx_fct);
int doorways_tx_or_rx_still_in_queue(int llid);
int doorways_tx_queue_bytes(int llid);
int doorways_tx_bufraw(int llid,int tid,int type,int val,int len,char *buf);
int doorways_sig_bufraw(int llid, int tid, int type, int val, char *buf);
int doorways_sock_client_inet_start(uint32_t ip, int port, t_fd_event conn_rx);
int doorways_sock_client_unix_start(char *sock_path, t_fd_event conn_rx);
void doorways_sock_client_inet_delete(int llid);
int doorways_sock_client_inet_end(int type, int llid, int fd,
                                   char *passwd,
                                   t_doorways_end cb_end,
                                   t_doorways_rx cb_rx);
void doorways_sock_address_detect(char *doors_client_addr,
                                  uint32_t *ip, int *port);
void doorways_clean_llid(int llid);
void doorways_sock_init(int to_exit);

