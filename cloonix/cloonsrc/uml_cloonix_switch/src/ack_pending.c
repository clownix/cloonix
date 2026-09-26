/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "ack_pending.h"

typedef struct t_ack
{
  int llid;
  int tid;
  int period_ms;
  int left_ms;
  int limit_ms;
  char what[MAX_NAME_LEN];
  t_ack_test test;
  t_ack_stalled stalled;
  void *ctx;
} t_ack;

static void timer_ack(void *data)
{
  t_ack *ack = (t_ack *) data;
  char err[MAX_PATH_LEN];
  int r;
  memset(err, 0, MAX_PATH_LEN);
  if (!msg_exist_channel(ack->llid))
    r = 2;
  else
    r = ack->test(ack->ctx, err);
  if (r == 1)
    send_status_ok(ack->llid, ack->tid, "ok");
  else if (r == -1)
    {
    KERR("ERROR %s: %s", ack->what, err);
    send_status_ko(ack->llid, ack->tid, err);
    }
  else if (r == 0)
    {
    ack->left_ms -= ack->period_ms;
    if (ack->left_ms > 0)
      {
      clownix_timeout_add(ack->period_ms/10, timer_ack, data, NULL, NULL);
      return;
      }
    KERR("ERROR %s: still not done after %d ms, the client is told TIMEOUT%s%s",
         ack->what, ack->limit_ms, err[0] ? ": " : "", err);
    if (ack->stalled)
      ack->stalled(ack->ctx);
    snprintf(err, MAX_PATH_LEN-1, "TIMEOUT %s", ack->what);
    send_status_ko(ack->llid, ack->tid, err);
    }
  if (ack->ctx)
    free(ack->ctx);
  free(ack);
}

void ack_pending(int llid, int tid, int period_ms, int limit_ms,
                 char *what, t_ack_test test, t_ack_stalled stalled, void *ctx)
{
  t_ack *ack = (t_ack *) malloc(sizeof(t_ack));
  memset(ack, 0, sizeof(t_ack));
  ack->llid = llid;
  ack->tid = tid;
  ack->period_ms = period_ms;
  ack->left_ms = limit_ms;
  ack->limit_ms = limit_ms;
  strncpy(ack->what, what, MAX_NAME_LEN-1);
  ack->test = test;
  ack->stalled = stalled;
  ack->ctx = ctx;
  timer_ack((void *) ack);
}
