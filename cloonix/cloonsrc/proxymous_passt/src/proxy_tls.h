/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* TLS termination for the https port and the doors port. See proxy_tls.c. */

/* Certificate, key and the shared SSL_CTX, once. 0 when tls can be served. */
int proxy_tls_init(void);


void *proxy_tls_alloc(void);
void  proxy_tls_free(void *handle);

typedef void (*t_proxy_tls_out)(void *user, int len, char *buf);
int proxy_tls_rx_cipher(void *handle, int len, char *buf,
                        t_proxy_tls_out plain_cb,
                        t_proxy_tls_out cipher_cb, void *user);

int proxy_tls_tx_plain(void *handle, int len, char *buf,
                       t_proxy_tls_out cipher_cb, void *user);
