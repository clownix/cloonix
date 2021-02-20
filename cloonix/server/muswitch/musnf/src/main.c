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
#include "pcap_file.h"
#include "sock_fd.h"

/*****************************************************************************/
void send_config_modif(t_all_ctx *all_ctx)
{
  int is_blkd, cloonix_llid;
  int mutype;
  char resp[MAX_PATH_LEN];
  memset(resp, 0, MAX_PATH_LEN);
  cloonix_llid = blkd_get_cloonix_llid((void *) all_ctx);
  snprintf(resp, MAX_PATH_LEN-1, "GET_CONF_RESP %d",
                                 pcap_file_is_recording(all_ctx));
  if (msg_exist_channel(all_ctx, cloonix_llid, &is_blkd, __FUNCTION__))
    {
    mutype = blkd_get_our_mutype((void *) all_ctx);
    rpct_send_cli_resp(all_ctx, cloonix_llid, mutype, 0, 0, resp);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line)
{
  KOUT("%s", line);
}
/*---------------------------------------------------------------------------*/

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
void error_detected(t_all_ctx *all_ctx, char *err)
{
  KERR("%s %s", __FUNCTION__, err);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int rx_from_traffic_sock(t_all_ctx *all_ctx, int tidx, t_blkd *bd)
{
  long long usec;
  if (bd)
    {
    usec = cloonix_get_usec();
    pcap_file_rx_packet(all_ctx, usec, bd->payload_len, bd->payload_blkd);
    blkd_free((void *)all_ctx, bd);
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clean_before_exit(void *ptr)
{
  pcap_file_unlink();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int main (int argc, char *argv[])
{
  t_all_ctx *all_ctx;
  int snf_type;
  char *endptr;
  char snf_dir[MAX_PATH_LEN];
  if (argc != 5)
    KOUT(" ");
  snf_type = strtoul(argv[4], &endptr, 10);
  if ((endptr == NULL)||(endptr[0] != 0))
    KOUT(" ");
  if (snf_type != endp_type_snf)
    KOUT("%d", snf_type);
  cloonix_set_pid(getpid());
  all_ctx = msg_mngt_init((char *) argv[2], 0, IO_MAX_BUF_LEN); 
  blkd_set_our_mutype((void *) all_ctx, snf_type);
  strncpy(all_ctx->g_net_name, argv[1], MAX_NAME_LEN-1);
  strncpy(all_ctx->g_name, argv[2], MAX_NAME_LEN-1);
  strncpy(all_ctx->g_path, argv[3], MAX_PATH_LEN-1);
  strncpy(snf_dir, argv[3], MAX_PATH_LEN-1);
  endptr = strstr(snf_dir, "endp");
  if (!endptr)
    KOUT("%s", snf_dir);
  *endptr = 0;
  sock_fd_init(all_ctx);
  pcap_file_init(all_ctx, all_ctx->g_net_name, all_ctx->g_name, snf_dir);
  msg_mngt_loop(all_ctx);
  return 0;
}
/*--------------------------------------------------------------------------*/

