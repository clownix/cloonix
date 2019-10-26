/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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


static t_all_ctx *g_all_ctx;


/*****************************************************************************/
static void fast_timeout_heartbeat(t_all_ctx *all_ctx, void *data)
{
  clownix_timeout_add(all_ctx, 1, fast_timeout_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_heartbeat(t_all_ctx *all_ctx, void *data)
{
  clownix_timeout_add(all_ctx, 100, timeout_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void tx_flow_ctrl(void *ptr, int llid, int stop)
{
  KERR("%d %d", llid, stop);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_flow_ctrl(void *ptr, int llid, int stop)
{
  KERR("%d %d", llid, stop);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void peer_flow_control(void *ptr, int llid,
                              char *name, int num, int tidx,
                              int rank, int stop)
{
  KERR("cloonix_evt_peer_flow_control name=%s num=%d tidx=%d rank=%d stop=%d",
  name, num, tidx, rank, stop);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int blkd_channel_create(void *ptr, int fd,
                        t_fd_event rx,
                        t_fd_event tx,
                        t_fd_error err,
                        char *from)
{
  if ((fd < 0) || (fd >= MAX_SIM_CHANNELS-1))
    KOUT("%d", fd);
  return channel_create((t_all_ctx *)ptr, 1, fd, rx, tx, err, from);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_all_ctx *get_all_ctx(void)
{
  return (g_all_ctx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{ 
  if (argc != 2)
    {
    printf("%s /home/perrier/cloonix_data/nemo/mu/spy_lan01\n", argv[0]);
    exit(1);
    }
  g_all_ctx = msg_mngt_init("cli_lan_spy");
  blkd_init((void *)g_all_ctx,tx_flow_ctrl,rx_flow_ctrl,peer_flow_control);
  blkd_set_our_mutype((void *) g_all_ctx, endp_type_lan_spy);
  if (blkd_addr_init((void *) g_all_ctx, argv[1]))
    KOUT("%s", argv[1]);
  clownix_timeout_add(g_all_ctx, 100, timeout_heartbeat, NULL, NULL, NULL);
  clownix_timeout_add(g_all_ctx, 100, fast_timeout_heartbeat, NULL, NULL, NULL);
  msg_mngt_loop(g_all_ctx);
  return 0;
}
/*--------------------------------------------------------------------------*/

