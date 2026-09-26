/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* ok when the requested state IS, ko when it cannot be, never on mere     */
/* acceptance: test() every period until 1 (ok), -1 (ko), or the limit.     */
/* 1: done, 0: not yet (err may say where it stands, shown on a timeout),
   -1: failed with err filled */
typedef int (*t_ack_test)(void *ctx, char *err);
/* Called once at the timeout, may be NULL: the place to dump a history. */
typedef void (*t_ack_stalled)(void *ctx);
void ack_pending(int llid, int tid, int period_ms, int limit_ms,
                 char *what, t_ack_test test, t_ack_stalled stalled, void *ctx);
