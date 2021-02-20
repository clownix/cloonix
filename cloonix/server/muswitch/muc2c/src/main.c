/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <net/if.h>
#include <errno.h>
#include <linux/if_tun.h>
#include <netinet/in.h>

#include "ioc.h"
#include "sock_fd.h"
#include "tcp_fd.h"


/*****************************************************************************/
void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line)
{
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int  tap_fd_open(t_all_ctx *all_ctx, char *tap_name)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int  wif_fd_open(t_all_ctx *all_ctx, char *tap_name)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int  raw_fd_open(t_all_ctx *all_ctx, char *tap_name)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line)
{
  KERR("%s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int rx_from_traffic_sock(t_all_ctx *all_ctx, int tidx, t_blkd *bd)
{
  int is_blkd, llid = all_ctx->g_llid_tcp;
  if (bd)
    {
    if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
      blkd_put_tx((void *) all_ctx, 1, &llid, bd);
    else
      blkd_free((void *) all_ctx, bd);
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clean_before_exit(void *ptr)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int main (int argc, char *argv[])
{
  t_all_ctx *all_ctx;
  int c2c_type;
  char *endptr;
  if (argc != 7)
    KOUT("%d", argc);
  c2c_type = strtoul(argv[4], &endptr, 10);
  if ((endptr == NULL)||(endptr[0] != 0))
    KOUT(" ");
  if ((endptr == NULL)||(endptr[0] != 0))
    KOUT(" ");
  if (c2c_type != endp_type_c2c)
    KOUT("%d", c2c_type);
  cloonix_set_pid(getpid());
  all_ctx = msg_mngt_init((char *) argv[2], 0, IO_MAX_BUF_LEN); 
  strncpy(all_ctx->g_net_name, argv[1], MAX_NAME_LEN-1);
  strncpy(all_ctx->g_name, argv[2], MAX_NAME_LEN-1);
  strncpy(all_ctx->g_path, argv[3], MAX_PATH_LEN-1);
  blkd_set_our_mutype((void *) all_ctx, c2c_type);
  all_ctx->g_fd_tcp = strtoul(argv[5], &endptr, 10);
  strncpy(all_ctx->g_addr_port, argv[6], MAX_NAME_LEN-1);
  if (tcp_fd_init(all_ctx))
    KOUT("Bad connection fd to tcp c2c");
  sock_fd_init(all_ctx);
  msg_mngt_loop(all_ctx);
  return 0;
}
/*--------------------------------------------------------------------------*/

