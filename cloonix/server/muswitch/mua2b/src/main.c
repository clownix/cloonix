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
#include "config.h"
#include "sched.h"
#include "wake_tx.h"
#include <pthread.h>

static pthread_t g_thread;

static int g_rx0, g_rx1;

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
static int resolv_input_cmd(char *cmd)
{
  int result = input_cmd_none;
  if (!strcmp(cmd, "delay"))
    result = input_cmd_delay;
  else if (!strcmp(cmd, "loss"))
    result = input_cmd_loss;
  else if (!strcmp(cmd, "qsize"))
    result = input_cmd_qsize;
  else if (!strcmp(cmd, "bsize"))
    result = input_cmd_bsize;
  else if (!strcmp(cmd, "brate"))
    result = input_cmd_rate;
  else if (!strcmp(cmd, "dump_config"))
    result = input_cmd_dump_config;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int resolv_input_dir(char *dir)
{
  int result = input_dir_none;
  if (!strcmp(dir, "A2B"))
    result = input_dir_a2b;
  else if (!strcmp(dir, "B2A"))
    result = input_dir_b2a;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  char resp[2*MAX_PATH_LEN];
  char cmd[MAX_PATH_LEN];
  char dir[MAX_PATH_LEN];
  int input_cmd, input_dir, val;
  if (strlen(line) >= MAX_PATH_LEN)
    {
    memset(resp, 0, 2*MAX_PATH_LEN);
    snprintf(resp, 2*MAX_PATH_LEN -1, "TOO LONG: %s", line);
    rpct_send_cli_resp(all_ctx, llid, tid, cli_llid, cli_tid, resp);
    }
  else if (sscanf(line, "%s %s %d", cmd, dir, &val) != 3)
    {
    memset(resp, 0, 2*MAX_PATH_LEN);
    if (sscanf(line, "%s", cmd) == 1)
      {
      input_cmd = resolv_input_cmd(cmd);
      if (input_cmd == input_cmd_dump_config)
        config_fill_resp(resp, 2*MAX_PATH_LEN-1);
      else
        snprintf(resp, 2*MAX_PATH_LEN -1, "BAD COMMAND: %s", line);
      }
    else
      {
      snprintf(resp, 2*MAX_PATH_LEN -1, "BAD FORMAT: %s", line);
      }
    rpct_send_cli_resp(all_ctx, llid, tid, cli_llid, cli_tid, resp);
    }
  else
    {
    input_cmd = resolv_input_cmd(cmd);
    input_dir = resolv_input_dir(dir);
    if (input_cmd == input_cmd_none)
      {
      memset(resp, 0, 2*MAX_PATH_LEN);
      snprintf(resp, 2*MAX_PATH_LEN -1, "BAD COMMAND: %s", line);
      rpct_send_cli_resp(all_ctx, llid, tid, cli_llid, cli_tid, resp);
      }
    else if (input_dir == input_dir_none)
      {
      memset(resp, 0, 2*MAX_PATH_LEN);
      snprintf(resp, 2*MAX_PATH_LEN -1, "BAD DIRECTION: %s", line);
      rpct_send_cli_resp(all_ctx, llid, tid, cli_llid, cli_tid, resp);
      }
    else if (config_recv_command(input_cmd, input_dir, val))
      {
      memset(resp, 0, 2*MAX_PATH_LEN);
      snprintf(resp, 2*MAX_PATH_LEN -1, "BAD VALUE: %s", line);
      rpct_send_cli_resp(all_ctx, llid, tid, cli_llid, cli_tid, resp);
      }
    else
      rpct_send_cli_resp(all_ctx, llid, tid, cli_llid, cli_tid, "OK");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int rx_from_traffic_sock(t_all_ctx *all_ctx, int tidx, t_blkd *bd)
{
  if (bd)
    {
    if (all_ctx->g_num == 0)
      {
      sched_tx_pkt(1, bd);
      wake_tx_send(all_ctx, 1);
      g_rx0 += 1;
      }
    else if (all_ctx->g_num == 1)
      {
      sched_tx_pkt(0, bd);
      wake_tx_send(all_ctx, 0);
      g_rx1 += 1;
      }
    else
      KOUT("%d", all_ctx->g_num);
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
  clownix_real_timer_init(1, all_ctx);
  strncpy(all_ctx->g_net_name, argv[0], MAX_NAME_LEN-1);
  strncpy(all_ctx->g_name, argv[1], MAX_NAME_LEN-1);
  strncpy(all_ctx->g_path, argv[2], MAX_PATH_LEN-1);
  sock_fd_init(all_ctx);
  config_init();
  sched_init(1, all_ctx);
  wake_tx_watch_fd(all_ctx, 1);
  msg_mngt_loop(all_ctx);
  return NULL;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void time_heartbeat(t_all_ctx *all_ctx, void *data)
{
  clownix_timeout_add(all_ctx, 100, time_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clean_before_exit(void *ptr)
{
  KERR("A2B");
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

  if (argc != 5)
    KOUT("%d", argc);
  
  a2b_type = strtoul(argv[4], &endptr, 10);
  if ((endptr == NULL)||(endptr[0] != 0))
    KOUT(" ");
  if (a2b_type != endp_type_a2b)
    KOUT("%d", a2b_type);

  wake_tx_init();

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
  config_init();
  sched_init(0, all_ctx);
  wake_tx_watch_fd(all_ctx, 0);
  clownix_timeout_add(all_ctx, 500, time_heartbeat, NULL, NULL, NULL);

  msg_mngt_loop(all_ctx);
  return 0;
}
/*--------------------------------------------------------------------------*/

