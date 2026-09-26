/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
int util_read (char *ptr, int max, int fd);
int util_proxy_client_socket_unix(char *pname, int *fd);
int util_client_socket_unix(char *pname, int *fd);
int util_client_socket_inet(uint32_t ip, uint16_t port, int *fd);
int util_socket_unix_dgram(char *pname);
int util_socket_listen_unix(char *pname);
int util_socket_listen_inet( uint16_t port );
void util_fd_accept(int fd_listen, int *fd, const char *fct);
void util_free_fd(int fd);
int util_nonblock_client_socket_unix(char *pname, int *fd);
int util_nonblock_client_socket_inet(uint32_t ip, uint16_t port);
int util_write_all(int fd, const char *buf, int len);
