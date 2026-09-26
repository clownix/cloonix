/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The 16 byte header, also compiled by the windows agent.                  */
#include "agent_wire.h"

int sock_header_get_size(void)
{
  return 16;
}

void sock_header_set_info(char *tx, int llid_dido,
                          int vwifi_base, int vwifi_cid,
                          int type, int val, int len, char **ntx)
{
  tx[0] = ((llid_dido & 0xFF00) >> 8) & 0xFF;
  tx[1] = llid_dido & 0xFF;
  tx[2] = ((vwifi_base & 0xFF00) >> 8) & 0xFF;
  tx[3] = vwifi_base & 0xFF;
  tx[4] = ((vwifi_cid & 0xFF00) >> 8) & 0xFF;
  tx[5] = vwifi_cid & 0xFF;
  tx[6] = ((len & 0xFF00) >> 8) & 0xFF;
  tx[7] = len & 0xFF;
  tx[8] = ((type & 0xFF00) >> 8) & 0xFF;
  tx[9] = type & 0xFF;
  tx[10] = ((val & 0xFF00) >> 8) & 0xFF;
  tx[11] = val & 0xFF;
  tx[12] = 0xCA;
  tx[13] = 0xFE;
  tx[14] = 0xDE;
  tx[15] = 0xCA;
  *ntx = &(tx[16]);
}

int sock_header_get_info(char *rx, int *llid_dido,
                         int *vwifi_base, int *vwifi_cid,
                         int *type, int *val, int *len, char **nrx)
{
  int result  = -1;
  *llid_dido  = ((rx[0] & 0xFF) << 8) + (rx[1] & 0xFF);
  *vwifi_base = ((rx[2] & 0xFF) << 8) + (rx[3] & 0xFF);
  *vwifi_cid  = ((rx[4] & 0xFF) << 8) + (rx[5] & 0xFF);
  *len        = ((rx[6] & 0xFF) << 8) + (rx[7] & 0xFF);
  *type       = ((rx[8] & 0xFF) << 8) + (rx[9] & 0xFF);
  *val        = ((rx[10] & 0xFF) << 8) + (rx[11] & 0xFF);
  if (((rx[12] & 0xFF) == 0xCA) &&
      ((rx[13] & 0xFF) == 0xFE) &&
      ((rx[14] & 0xFF) == 0xDE) &&
      ((rx[15] & 0xFF) == 0xCA))
    {
    result = 0;
    }
  else
    {
    *llid_dido = 0;
    *vwifi_base = 0;
    *vwifi_cid = 0;
    *len  = 0;
    *type = 0;
    *val  = 0;
    }
  *nrx  = &(rx[16]);
  return result;
}
