/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Frames of the agent pipe reassembled from a byte stream, both ends. */
#include <stdio.h>
#include <string.h>
#include "glob_common.h"
#include "rx_pktbuf.h"

void rx_pktbuf_reset(t_rx_pktbuf *rp)
{
  rp->offset = 0;
  rp->paylen = 0;
  rp->payload = NULL;
  rp->dido_llid = 0;
  rp->vwifi_base = 0;
  rp->vwifi_cid = 0;
}

static int rx_pktbuf_fill(int *len, char *buf, t_rx_pktbuf *rp)
{
  int result, headsize = sock_header_get_size();
  int len_chosen, len_desired, len_avail = *len;
  if (rp->offset < headsize)
    {
    len_desired = headsize - rp->offset;
    if (len_avail >= len_desired)
      {
      len_chosen = len_desired;
      result = 1;
      }
    else
      {
      len_chosen = len_avail;
      result = 2;
      }
    }
  else
    {
    if (rp->paylen <= 0)
      KOUT("ERROR");
    len_desired = headsize + rp->paylen - rp->offset;
    if (len_avail >= len_desired)
      {
      len_chosen = len_desired;
      result = 3;
      }
    else
      {
      len_chosen = len_avail;
      result = 2;
      }
    }
  if (len_chosen + rp->offset > MAX_A2D_LEN)
    KOUT("ERROR %d %d", len_chosen, rp->offset);
  memcpy(rp->rawbuf + rp->offset, buf, len_chosen);
  rp->offset += len_chosen;
  *len -= len_chosen;
  return result;
}

int rx_pktbuf_feed(t_rx_pktbuf *rp, int len, char *buf, void *ctx,
                   t_rx_pktbuf_frame on_frame, t_rx_pktbuf_desync on_desync)
{
  int res, len_done, len_left_to_do = len;
  while (len_left_to_do)
    {
    len_done = len - len_left_to_do;
    res = rx_pktbuf_fill(&len_left_to_do, buf + len_done, rp);
    if (res == 1)
      {
      if (sock_header_get_info(rp->rawbuf, &(rp->dido_llid),
                               &(rp->vwifi_base), &(rp->vwifi_cid),
                               &(rp->type), &(rp->val),
                               &(rp->paylen), &(rp->payload)))
        {
        rx_pktbuf_reset(rp);
        on_desync(ctx);
        return -1;
        }
      }
    else if (res == 3)
      {
      if (!rp->payload)
        KOUT("ERROR");
      on_frame(ctx, rp);
      rx_pktbuf_reset(rp);
      }
    else if (res != 2)
      KOUT("ERROR %d", res);
    }
  return 0;
}
