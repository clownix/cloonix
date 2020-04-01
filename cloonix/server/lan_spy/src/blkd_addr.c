/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ioc.h"
#include "blkd_addr.h"
#include "main.h"

static int g_llid_spy_lan;

/****************************************************************************/
void blkd_addr_print(char *str, char *addr)
{
  int i, iaddr[ETH_ALEN];
  str[MAX_LEN_PRINT-1] = 0;
  for (i=0; i<ETH_ALEN; i++)
    iaddr[i] = (int) (addr[i]) & 0xFF;
  snprintf(str, MAX_LEN_PRINT-2, "%02X:%02X:%02X:%02X:%02X:%02X", 
         iaddr[0], iaddr[1],iaddr[2],iaddr[3],iaddr[4],iaddr[5]);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tx_blkd(t_all_ctx *all_ctx,  int len, char *payload)
{
  t_blkd *bd;
  int llid_tab[1];
  if (msg_exist_channel(all_ctx, g_llid_spy_lan, __FUNCTION__))
    {
    llid_tab[0] = g_llid_spy_lan;
    bd = blkd_create_tx_empty(0,0,0);
    memcpy(bd->payload_blkd, payload, len);
    bd->payload_len = len;
    blkd_put_tx((void *) all_ctx, 1, llid_tab, bd);
    }
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_blkd(void *ptr, int llid)
{
  t_blkd *blkd;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  char str[MAX_LEN_PRINT];
  int len;
  if (all_ctx && llid)
    {
    blkd = blkd_get_rx(ptr, llid);
    while(blkd)
      {
      len = blkd->payload_len;
      blkd_addr_print(str, &(blkd->payload_blkd[6]));
      KERR("len=%d src_mac=%s", len, str);
      blkd_free(ptr, blkd);
      blkd = blkd_get_rx(ptr, llid);
      }
    }
  else
    KOUT(" ");
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_blkd(void *ptr, int llid, int err, int from)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int blkd_addr_init(void *ptr, char *path)
{
  int result = -1;
  g_llid_spy_lan = blkd_client_connect(ptr, path, rx_blkd, err_blkd);
  if (g_llid_spy_lan > 0)
    result = 0;
  return result;
}
/*-------------------------------------------------------------------------*/
