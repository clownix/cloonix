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
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mount.h>
#include <signal.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"
#include "rxtx.h"
#include "tun_tap.h"
#include "sched.h"



/*--------------------------------------------------------------------------*/
static int  g_netns_pid;
static int  g_llid;
static int  g_watchdog_ok;
static char g_net_name[MAX_NAME_LEN];
static char g_netns_namespace[MAX_PATH_LEN];
static char g_a2b_name[MAX_NAME_LEN];
static char g_vhost0[MAX_NAME_LEN];
static char g_vhost1[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char g_a2b_path[MAX_PATH_LEN];

/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_and_set_uid(void)
{
  int result = -1;
  uid_t uid;
  seteuid(0);
  setegid(0);
  uid = geteuid();
  if (uid == 0)
    {
    result = 0;
    umask (0000);
    if (setuid(0))
      KOUT(" ");
    if (setgid(0))
      KOUT(" ");
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  if (llid != g_llid)
    KERR("ERROR %s %s %d %d", g_net_name, name, llid, g_llid);
  if (tid != type_hop_a2b)
    KERR("ERROR %s %s %d %d", g_net_name, name, llid, g_llid);
  if (strcmp(g_a2b_name, name))
    KERR("ERROR %s %s %s %d %d", g_net_name, name, g_a2b_name, llid, g_llid);
  rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
  g_watchdog_ok = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_kil_req(int llid, int tid)
{
  unlink(g_ctrl_path);
  unlink(g_a2b_path);
  if (g_netns_pid > 0)
    kill(g_netns_pid, SIGKILL);
  exit(0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  KERR("ERROR %s", line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char resp[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  char cmd[MAX_NAME_LEN];
  char *msg;
  int cli_llid, cli_tid, dir, val;
  memset(resp, 0, MAX_PATH_LEN);
  if (llid != g_llid)
    KERR("ERROR %s %d %d", g_net_name, llid, g_llid);
  if (tid != type_hop_a2b)
    KERR("ERROR %s %d %d", g_net_name, tid, type_hop_a2b);
  DOUT(FLAG_HOP_SIGDIAG, "A2B %s", line);
  if (!strncmp(line,
  "a2b_suidroot", strlen("a2b_suidroot")))
    {
    if (check_and_set_uid())
      snprintf(resp, MAX_PATH_LEN-1, "a2b_suidroot_ko %s", g_a2b_name);
    else
      snprintf(resp, MAX_PATH_LEN-1, "a2b_suidroot_ok %s", g_a2b_name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (sscanf(line,
  "a2b_param_config %d %d %s", &cli_llid, &cli_tid, name) == 3)
    {
    msg = strstr(line, "dir_cmd_val=");
    if (!msg)
      KERR("ERROR %s %s", g_net_name, line);
    else
      {
      msg += strlen("dir_cmd_val=");
      if (sscanf(msg, "dir=%d cmd=%s val=%d", &dir, cmd, &val) == 3) 
        {
        snprintf(resp, MAX_PATH_LEN-1,
                 "a2b_param_config_ok %d %d %s",cli_llid,cli_tid,name);
        sched_cnf(dir, cmd, val);
        }
      else
        snprintf(resp, MAX_PATH_LEN-1,
                 "a2b_param_config_ko %d %d %s",cli_llid,cli_tid,name);
      rpct_send_sigdiag_msg(llid, tid, resp);
      }
    }
  else
    KERR("ERROR %s %s", g_net_name, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_ctrl_cb (int llid, int err, int from)
{
  KOUT("ERROR CONNECTION %s %s", g_net_name, g_a2b_name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_ctrl_cb (int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    KOUT("%s %s", g_net_name, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_ctrl_client(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, err_ctrl_cb, rx_ctrl_cb);
  g_llid = llid_new;
  g_watchdog_ok = 1;
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fct_timeout_self_destruct(void *data)
{
  if (g_watchdog_ok == 0)
    {
    KERR("SELF DESTRUCT WATCHDOG %s %s", g_net_name, g_a2b_name);
    KOUT("EXIT");
    }
  g_watchdog_ok = 0;
  clownix_timeout_add(5000, fct_timeout_self_destruct, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_cloonix_llid(void)
{
  return (g_llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main (int argc, char *argv[])
{
  char *root = g_root_path;
  char *a2b = g_a2b_name;
  int fd_rx_from_tap0, fd_tx_to_tap0, fd_rx_from_tap1, fd_tx_to_tap1;
  if (argc != 6)
    KOUT("ERROR %d", argc);
  g_llid = 0;
  g_watchdog_ok = 0;
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_netns_namespace, 0, MAX_PATH_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_a2b_name, 0, MAX_NAME_LEN);
  memset(g_vhost0, 0, MAX_NAME_LEN);
  memset(g_vhost1, 0, MAX_NAME_LEN);
  memset(g_a2b_path, 0, MAX_PATH_LEN);

  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_a2b_name,  argv[3], MAX_NAME_LEN-1);
  strncpy(g_vhost0,  argv[4], MAX_NAME_LEN-1);
  strncpy(g_vhost1,  argv[5], MAX_NAME_LEN-1);
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s/c%s", root, A2B_DIR, a2b);
  snprintf(g_a2b_path, MAX_PATH_LEN-1,"%s/%s/%s", root, A2B_DIR, a2b);
  snprintf(g_netns_namespace, MAX_PATH_LEN-1, "%s%s_%s",
           PATH_NAMESPACE, BASE_NAMESPACE, g_net_name);

  g_netns_pid = tun_tap_open(g_netns_namespace, g_vhost0,
                             &fd_rx_from_tap0, &fd_tx_to_tap0);
  g_netns_pid = tun_tap_open(g_netns_namespace, g_vhost1,
                             &fd_rx_from_tap1, &fd_tx_to_tap1);

  msg_mngt_init("a2b", IO_MAX_BUF_LEN);
  msg_mngt_heartbeat_ms_set(1);
  rxtx_init(fd_rx_from_tap0, fd_tx_to_tap0, fd_rx_from_tap1, fd_tx_to_tap1);
  if (!access(g_ctrl_path, F_OK))
    {
    KERR("ERROR %s exists ERASING", g_ctrl_path);
    unlink(g_ctrl_path);
    }
  if (!access(g_a2b_path, F_OK))
    {
    KERR("ERROR %s exists ERASING", g_a2b_path);
    unlink(g_a2b_path);
    }
  string_server_unix(g_ctrl_path, connect_from_ctrl_client, "ctrl");

  daemon(0,0);
  seteuid(getuid());
  cloonix_set_pid(getpid());
  clownix_timeout_add(15000, fct_timeout_self_destruct, NULL, NULL, NULL);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/

