/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef void (*t_rx_pktbuf_frame)(void *ctx, t_rx_pktbuf *rp);
typedef void (*t_rx_pktbuf_desync)(void *ctx);
void rx_pktbuf_reset(t_rx_pktbuf *rp);
int  rx_pktbuf_feed(t_rx_pktbuf *rp, int len, char *buf, void *ctx,
                    t_rx_pktbuf_frame on_frame, t_rx_pktbuf_desync on_desync);
