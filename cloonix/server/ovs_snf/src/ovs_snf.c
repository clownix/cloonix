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



#include "io_clownix.h"
#include "rpc_clownix.h"
#include "pcap_record.h"
#include "eventfull.h"
#include "tun_tap.h"
#include "inotify_trigger.h"



/*--------------------------------------------------------------------------*/
static int  g_llid;
static int  g_watchdog_ok;
static char g_net_name[MAX_NAME_LEN];
static char g_netns_namespace[MAX_PATH_LEN];
static char g_snf_name[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char g_snf_path[MAX_PATH_LEN];
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
  if (tid != type_hop_snf)
    KERR("ERROR %s %s %d %d", g_net_name, name, llid, g_llid);
  if (strcmp(g_snf_name, name))
    KERR("ERROR %s %s %s %d %d", g_net_name, name, g_snf_name, llid, g_llid);
  rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
  g_watchdog_ok = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_kil_req(int llid, int tid)
{
  unlink(g_ctrl_path);
  unlink(g_snf_path);
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
  char mac[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int num, status, cmd;
  memset(resp, 0, MAX_PATH_LEN);
  if (llid != g_llid)
    KERR("ERROR %s %d %d", g_net_name, llid, g_llid);
  if (tid != type_hop_snf)
    KERR("ERROR %s %d %d", g_net_name, tid, type_hop_snf);
  DOUT(FLAG_HOP_SIGDIAG, "SNF %s", line);
  if (!strncmp(line,
  "cloonsnf_suidroot", strlen("cloonsnf_suidroot")))
    {
    if (check_and_set_uid())
      snprintf(resp, MAX_PATH_LEN-1, "cloonsnf_suidroot_ko %s", g_snf_name);
    else
      snprintf(resp, MAX_PATH_LEN-1, "cloonsnf_suidroot_ok %s", g_snf_name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (sscanf(line,
  "cloonsnf_mac_spyed_tx_on=%s", mac) == 1) 
    {
    tun_tap_add_mac(1, mac);
    }
  else if (sscanf(line,
  "cloonsnf_mac_spyed_tx_off=%s", mac) == 1) 
    {
    tun_tap_del_mac(1, mac);
    }
  else if (sscanf(line,
  "cloonsnf_mac_spyed_rx_on=%s", mac) == 1)
    {
    tun_tap_add_mac(0, mac);
    }
  else if (sscanf(line,
  "cloonsnf_mac_spyed_rx_off=%s", mac) == 1)
    {
    tun_tap_del_mac(0, mac);
    }
  else if (!strcmp(line,
  "cloonsnf_mac_spyed_purge"))
    {
    tun_tap_purge_mac();
    }
  else if (sscanf(line,
  "cloonsnf_sync_wireshark_req %s %d %d", name, &num, &cmd) == 3) 
    {
    if (cmd)
      {
      status = 2;
      close_pcap_record();
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsnf_sync_wireshark_resp %s %d %d",
               name, num, status);
      rpct_send_sigdiag_msg(llid, tid, resp);
      } 
    else
      {
      if ((!access(g_ctrl_path, R_OK)) && (!inotify_get_state()))
        {
        status = 0;
        }
      else
        {
        KERR("WARNING WIRESHARK SPY NOT READY %d", inotify_get_state());
        status = 1;
        }
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonsnf_sync_wireshark_resp %s %d %d",
               name, num, status);
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
  KOUT("ERROR CONNECT %s %s", g_net_name, g_snf_name);
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
    KERR("ERROR XYX SELF DESTRUCT WATCHDOG %s %s", g_net_name, g_snf_name);
    KOUT("EXIT");
    }
  g_watchdog_ok = 0;
  clownix_timeout_add(1500, fct_timeout_self_destruct, NULL, NULL, NULL);
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
  char *snf = g_snf_name;
  int fd;
  if (argc != 5)
    KOUT("%d", argc);
  umask(0000);
  g_llid = 0;
  g_watchdog_ok = 0;
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_netns_namespace, 0, MAX_PATH_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_snf_path, 0, MAX_PATH_LEN);
  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_snf_name,  argv[3], MAX_NAME_LEN-1);
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s/c%s", root, SNF_DIR, snf);
  snprintf(g_snf_path, MAX_PATH_LEN-1,"%s/%s/%s", root, SNF_DIR, argv[4]);
  snprintf(g_netns_namespace, MAX_PATH_LEN-1, "%s%s_%s",
           PATH_NAMESPACE, BASE_NAMESPACE, g_net_name);

  msg_mngt_init("snf", IO_MAX_BUF_LEN);
  msg_mngt_heartbeat_init(heartbeat);
  fd = open(g_netns_namespace, O_RDONLY|O_CLOEXEC);
  if (fd < 0) 
    KOUT(" ");
  if (setns(fd, CLONE_NEWNET) != 0)
    KOUT(" ");
  close(fd);
  if (tun_tap_open(snf))
    KOUT("Problem tap %s\n", snf);
  if (!access(g_ctrl_path, R_OK))
    {
    KERR("ERROR %s exists ERASING", g_ctrl_path);
    unlink(g_ctrl_path);
    }
  if (!access(g_snf_path, R_OK))
    {
    KERR("ERROR %s exists ERASING", g_snf_path);
    unlink(g_snf_path);
    }
  string_server_unix(g_ctrl_path, connect_from_ctrl_client, "ctrl");
  daemon(0,0);
  eventfull_init(g_snf_name);
  pcap_record_init(g_snf_path);
  seteuid(getuid());
  cloonix_set_pid(getpid());
  clownix_timeout_add(2000, fct_timeout_self_destruct, NULL, NULL, NULL);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/

