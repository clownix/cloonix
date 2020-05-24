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
#include <pthread.h>

static pthread_t g_thread;

static int g_rx, g_tx, g_state=0;
static long long g_num_tx, g_num_rx;
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
  long long num;
  if (bd)
    {
    if (all_ctx->g_num != 1)
      KOUT("%d", all_ctx->g_num);
    if (g_state == 2)
      {
      g_rx += 1;
      if (sscanf(bd->payload_blkd, "MESSAGE: %lld ", &num) != 1)
        KOUT("%s", bd->payload_blkd);
      if (num != g_num_rx)
        KERR("%lld %lld", num, g_num_rx);
      g_num_rx += 1;
      }
    else if (g_state == 0)
      {
      if (!strcmp(bd->payload_blkd, "open"))
        g_state = 1;
      }
    blkd_free(NULL, bd);
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void *thread_b_access(void *iargv)
{
  char **argv = (char **) iargv;
  t_all_ctx *all_ctx;
  cloonix_set_pid(getpid());
  all_ctx = msg_mngt_init((char *) argv[1], 1, IO_MAX_BUF_LEN);
  blkd_set_our_mutype((void *) all_ctx, endp_type_a2b);
  strncpy(all_ctx->g_net_name, argv[0], MAX_NAME_LEN-1);
  strncpy(all_ctx->g_name, argv[1], MAX_NAME_LEN-1);
  strncpy(all_ctx->g_path, argv[2], MAX_PATH_LEN-1);
  sock_fd_init(all_ctx);
  msg_mngt_loop(all_ctx);
  return NULL;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_burst(t_all_ctx *all_ctx)
{
  int i;
  char msg[300];
  t_blkd *blkd;
  for (i=0; i<2000; i++)
    {
    g_tx += 1;
    sprintf(msg, "MESSAGE: %lld ", g_num_tx);
    g_num_tx += 1;
    blkd = blkd_create_tx_full_copy(strlen(msg) + 1, msg, 0, 0, 0);
    sock_fd_tx(all_ctx, blkd);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_msg(t_all_ctx *all_ctx, char *msg)
{
  t_blkd *blkd;
  blkd = blkd_create_tx_full_copy(strlen(msg) + 1, msg, 0, 0, 0);
  sock_fd_tx(all_ctx, blkd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_sched(long delta_ns, void *data)
{ 
  t_all_ctx *all_ctx = (t_all_ctx *) data;
  long long date_us;
  static int count = 0;
  count += 1;
  if (g_state == 1)
    {
    g_tx = 0;
    g_rx = 0;
    g_state =  2;
    g_num_tx = 0;
    g_num_rx = 0;
    }
  else if (g_state == 2)
    {
    send_burst(all_ctx);
    }
  if (count == 1000) 
    {
    KERR("%d %d %d", g_tx, g_rx, g_tx - g_rx);
    count = 0;
    if (g_state == 0)
      send_msg(all_ctx, "open");
    }
  if (clownix_real_timer_add(0, 10000, timer_sched, data, &date_us))
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clean_before_exit(void *ptr)
{
  KERR("TST");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int main (int argc, char *argv[])
{
  t_all_ctx *all_ctx;
  int a2b_type, len;
  char *endptr;
  char net[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char path[MAX_PATH_LEN];
  char *b_argv[3] = {net, name, path};
  long long date_us;

  if (argc != 5)
    KOUT("%d", argc);
  
  a2b_type = strtoul(argv[4], &endptr, 10);
  if ((endptr == NULL)||(endptr[0] != 0))
    KOUT(" ");
  if (a2b_type != endp_type_a2b)
    KOUT("%d", a2b_type);

  strncpy(net,  argv[1], MAX_NAME_LEN-1);
  strncpy(name, argv[2], MAX_NAME_LEN-1);
  strncpy(path, argv[3], MAX_PATH_LEN-1);
  len = strlen(path);
  if (len <= 1)
    KOUT("%s", path);
  if (path[len-1] != '0')
    KOUT("%s", path);
  path[len-1] = '1';
  if (pthread_create(&g_thread, NULL, thread_b_access, (void *)b_argv) != 0)
    KOUT(" ");

  cloonix_set_pid(getpid());
  all_ctx = msg_mngt_init((char *) argv[2], 0, IO_MAX_BUF_LEN);
  blkd_set_our_mutype((void *) all_ctx, a2b_type);
  clownix_real_timer_init(0, all_ctx);
  strncpy(all_ctx->g_net_name, argv[1], MAX_NAME_LEN-1);
  strncpy(all_ctx->g_name, argv[2], MAX_NAME_LEN-1);
  strncpy(all_ctx->g_path, argv[3], MAX_PATH_LEN-1);
  sock_fd_init(all_ctx);
  if (clownix_real_timer_add(0, 100000, timer_sched, (void *) all_ctx, &date_us))
    KOUT(" ");

  msg_mngt_loop(all_ctx);
  return 0;
}
/*--------------------------------------------------------------------------*/

