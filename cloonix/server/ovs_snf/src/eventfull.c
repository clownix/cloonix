/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "io_clownix.h"
#include "rpc_clownix.h"

int get_cloonix_llid(void);

/*--------------------------------------------------------------------------*/
typedef struct t_evt
{
  char name[MAX_NAME_LEN];
  int pkt_tx;
  int bytes_tx;
  int pkt_rx;
  int bytes_rx;
} t_evt;  
/*--------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
static t_evt g_evt;


/****************************************************************************/
static void send_eventfull_tx_rx(char *name, int ms,
                                 int ptx, int btx, int prx, int brx)
{
  char resp[MAX_PATH_LEN];
  memset(resp, 0, MAX_PATH_LEN);
  snprintf(resp, MAX_PATH_LEN-1,
  "endp_eventfull_tx_rx %s %d %d %d %d %d", name, ms, ptx, btx, prx, brx);
  rpct_send_poldiag_msg(get_cloonix_llid(), 0, resp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eventfull_can_be_sent(void *data)
{
  unsigned int ms = (unsigned int) cloonix_get_msec();
  send_eventfull_tx_rx(g_evt.name, ms,
                       g_evt.pkt_tx, g_evt.bytes_tx,
                       g_evt.pkt_rx, g_evt.bytes_rx);
  g_evt.pkt_tx   = 0;
  g_evt.bytes_tx = 0;
  g_evt.pkt_rx   = 0;
  g_evt.bytes_rx = 0;
  clownix_timeout_add(5, eventfull_can_be_sent, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_hook_spy(int is_tx, int len)
{
  if (is_tx) 
    {
    g_evt.pkt_tx += 1;
    g_evt.bytes_tx += len;
    }
  else
    {
    g_evt.pkt_rx += 1;
    g_evt.bytes_rx += len;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_init(char *name)
{
  memset(&g_evt, 0, sizeof(t_evt));
  strncpy(g_evt.name, name, MAX_NAME_LEN);
  clownix_timeout_add(200, eventfull_can_be_sent, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
