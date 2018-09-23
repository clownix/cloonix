/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <signal.h>

#include "ioc.h"
#include "sock_fd.h"


/*****************************************************************************/
static void rx_from_tcp(void *ptr, int llid)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  t_blkd *bd = blkd_get_rx(ptr, llid);
  if (!bd)
    KERR(" ");
  else
    {
    while(bd)
      {
      sock_fd_tx(all_ctx, bd);
      bd = blkd_get_rx(ptr, llid);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void err_tcp (void *ptr, int llid, int err, int from)
{
  exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int tcp_fd_init(t_all_ctx *all_ctx)
{
  int result = 0;
  all_ctx->g_llid_tcp = blkd_watch_fd((void *) all_ctx, 
                                      all_ctx->g_addr_port,
                                      all_ctx->g_fd_tcp, 
                                      rx_from_tcp, 
                                      err_tcp);
  if (all_ctx->g_llid_tcp <= 0)
    {
    KERR(" ");
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/


