/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include "rxtx.h"
#include "tun_tap.h"
#include "udp.h"
#include "packet_arp_mangle.h"



/*--------------------------------------------------------------------------*/
static int  g_netns_pid;
static int  g_llid;
static int  g_watchdog_ok;
static char g_net_name[MAX_NAME_LEN];
static char g_netns_namespace[MAX_PATH_LEN];
static char g_c2c_name[MAX_NAME_LEN];
static char g_vhost[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char g_c2c_path[MAX_PATH_LEN];
static uint16_t g_udp_port;
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
    if (setuid(0))
      KOUT(" ");
    if (setgid(0))
      KOUT(" ");
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void heartbeat (int delta)
{
  static int count_ticks_blkd = 0;
  count_ticks_blkd += 1;
  if (count_ticks_blkd == 10)
    {
    count_ticks_blkd = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  if (llid != g_llid)
    KERR("ERROR %s %s %d %d", g_net_name, name, llid, g_llid);
  if (tid != type_hop_c2c)
    KERR("ERROR %s %s %d %d", g_net_name, name, llid, g_llid);
  if (strcmp(g_c2c_name, name))
    KERR("ERROR %s %s %s %d %d", g_net_name, name, g_c2c_name, llid, g_llid);
  rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
  g_watchdog_ok = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_kil_req(int llid, int tid)
{
  if (g_netns_pid > 0)
    kill(g_netns_pid, SIGKILL);
  unlink(g_ctrl_path);
  unlink(g_c2c_path);
  exit(0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void reply_probe_udp(void)
{
  char resp[MAX_PATH_LEN];
  snprintf(resp, MAX_PATH_LEN-1,"c2c_receive_probe_udp %s",g_c2c_name);
  rpct_send_sigdiag_msg(g_llid, type_hop_c2c, resp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void reply_enter_traffic_udp(void)
{
  char resp[MAX_PATH_LEN];
  snprintf(resp, MAX_PATH_LEN-1,"c2c_enter_traffic_udp_ack %s",g_c2c_name);
  rpct_send_sigdiag_msg(g_llid, type_hop_c2c, resp);
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
  uint8_t mac[6];

  memset(resp, 0, MAX_PATH_LEN);
  if (llid != g_llid)
    KERR("ERROR %s %d %d", g_net_name, llid, g_llid);
  if (tid != type_hop_c2c)
    KERR("ERROR %s %d %d", g_net_name, tid, type_hop_c2c);
  DOUT(FLAG_HOP_SIGDIAG, "C2C %s", line);

  if (!strncmp(line,
  "c2c_suidroot", strlen("c2c_suidroot")))
    {
    if (check_and_set_uid())
      snprintf(resp, MAX_PATH_LEN-1, "c2c_suidroot_ko %s", g_c2c_name);
    else
      snprintf(resp, MAX_PATH_LEN-1, "c2c_suidroot_ok %s", g_c2c_name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }

  else if (sscanf(line,
  "c2c_get_free_udp_port %s proxycrun %hu", g_c2c_name, &g_udp_port) == 2)
    {
    if (udp_init(g_udp_port))
      {
      snprintf(resp, MAX_PATH_LEN-1,
      "c2c_get_free_udp_port_ko %s", g_c2c_name);
      KERR("ERROR %s", line);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1,
      "c2c_get_free_udp_port_ok %s udp_port=%hu", g_c2c_name, g_udp_port);
      }
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (sscanf(line,
  "c2c_send_probe_udp %s", g_c2c_name) == 1)
    {
    udp_tx_sig_send(strlen("probe")+1, (uint8_t *) "probe");
    }
  else if (sscanf(line,
  "c2c_enter_traffic_udp %s", g_c2c_name) == 1)
    {
    udp_enter_traffic_mngt();
    reply_enter_traffic_udp();
    }
  else if (sscanf(line,
  "c2c_mac_mangle %s %hhX:%hhX:%hhX:%hhX:%hhX:%hhX", g_c2c_name,
  &(mac[0]), &(mac[1]), &(mac[2]), &(mac[3]), &(mac[4]), &(mac[5])) == 7)
    {
    KERR("INFO: %s %s", g_net_name, line);
    rxtx_mac_mangle(mac);
    }
  else
    KERR("ERROR %s %s", g_net_name, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_ctrl_cb (int llid, int err, int from)
{
  KERR("SELF DESTRUCT CONNECTION %s %s", g_net_name, g_c2c_name);
  KOUT("EXIT");
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
    KERR("SELF DESTRUCT WATCHDOG %s %s", g_net_name, g_c2c_name);
    KOUT("EXIT");
    }
  g_watchdog_ok = 0;
  clownix_timeout_add(600, fct_timeout_self_destruct, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_cloonix_llid(void)
{
  return (g_llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_net_name(void)
{
  return (g_net_name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main (int argc, char *argv[])
{
  char *root = g_root_path;
  char *c2c = g_c2c_name;
  int fd_rx_from_tap, fd_tx_to_tap;
  if (argc != 5)
    KOUT("ERROR %d", argc);
  umask(0000);
  g_llid = 0;
  g_watchdog_ok = 0;
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_netns_namespace, 0, MAX_PATH_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_c2c_name, 0, MAX_NAME_LEN);
  memset(g_vhost, 0, MAX_NAME_LEN);
  memset(g_c2c_path, 0, MAX_PATH_LEN);
  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_c2c_name,  argv[3], MAX_NAME_LEN-1);
  strncpy(g_vhost,  argv[4], MAX_NAME_LEN-1);
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s/c%s", root, C2C_DIR, c2c);
  snprintf(g_c2c_path, MAX_PATH_LEN-1,"%s/%s/%s", root, C2C_DIR, c2c);
  snprintf(g_netns_namespace, MAX_PATH_LEN-1, "%s%s_%s",
           PATH_NAMESPACE, BASE_NAMESPACE, g_net_name);

  g_netns_pid = tun_tap_open(g_netns_namespace, g_vhost,
                             &fd_rx_from_tap, &fd_tx_to_tap);

  init_packet_arp_mangle();
  msg_mngt_init("c2c", TRAF_TAP_BUF_LEN);
  msg_mngt_heartbeat_init(heartbeat);
  rxtx_init(fd_rx_from_tap, fd_tx_to_tap);
  if (!access(g_ctrl_path, F_OK))
    {
    KERR("ERROR %s exists ERASING", g_ctrl_path);
    unlink(g_ctrl_path);
    }
  if (!access(g_c2c_path, F_OK))
    {
    KERR("ERROR %s exists ERASING", g_c2c_path);
    unlink(g_c2c_path);
    }
  string_server_unix(g_ctrl_path, connect_from_ctrl_client, "ctrl");
  daemon(0,0);
  seteuid(getuid());
  cloonix_set_pid(getpid());
  clownix_timeout_add(1500, fct_timeout_self_destruct, NULL, NULL, NULL);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/

