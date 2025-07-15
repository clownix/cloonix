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
#include "utils.h"
#include "ltcp.h"
#include "ludp.h"
#include "licmp.h"
#include "rxtx.h"
#include "tun_tap.h"
#include "machine.h"
#include "ssh_cisco_llid.h"



/*--------------------------------------------------------------------------*/
static int  g_netns_pid;
static int  g_llid;
static int  g_llid_prxy;
static int  g_watchdog_ok;
static int  g_watchdog_proxy_ok;
static char g_net_name[MAX_NAME_LEN];
static char g_netns_namespace[MAX_PATH_LEN];
static char g_nat_name[MAX_NAME_LEN];
static char g_vhost[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char g_prxy_path[MAX_PATH_LEN];
static char g_nat_path[MAX_PATH_LEN];
static char g_cisco_path[MAX_PATH_LEN];

/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
int get_cloonix_llid(void)
{   
  return (g_llid);
}   
/*--------------------------------------------------------------------------*/
    
/****************************************************************************/
char *get_nat_name(void)
{   
  return (g_nat_name);
}   
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_net_name(void)
{   
  return (g_net_name);
}   
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
static void err_prxy_cb (int llid, int err, int from)
{
  KOUT("ERROR CONNECTION %s %s %d %d",g_net_name,get_nat_name(),err,from);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_prxy_cb (int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    KOUT("ERROR %s %s", g_net_name, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void heartbeat (int delta)
{
  static int count_ticks_blkd = 0;
  int llid;
  count_ticks_blkd += 1;
  if (count_ticks_blkd == 10)
    {
    count_ticks_blkd = 0;
    if (g_llid_prxy == 0)
      {
      llid = string_client_unix(g_prxy_path, err_prxy_cb, rx_prxy_cb, "prxy");
      if (llid != 0)
        g_llid_prxy = llid;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_llid_prxy(void)
{
  return g_llid_prxy;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  char msg[MAX_PATH_LEN];
  if (llid != g_llid)
    KERR("ERROR %s %s %d %d", g_net_name, name, llid, g_llid);
  if (tid != type_hop_nat_main)
    KERR("ERROR %s %s %d %d", g_net_name, name, llid, g_llid);
  if (strcmp(get_nat_name(), name))
    KERR("ERROR %s %s %s %d %d",g_net_name,name,get_nat_name(),llid,g_llid);
  if (g_llid_prxy)
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,
    "nat_proxy_beat_req %s", get_nat_name());
    rpct_send_poldiag_msg(g_llid_prxy, 0, msg);
    rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
    }
  g_watchdog_ok = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_kil_req(int llid, int tid)
{
  unlink(g_ctrl_path);
  unlink(g_prxy_path);
  unlink(g_nat_path);
  unlink(g_cisco_path);
  if (g_netns_pid > 0)
    kill(g_netns_pid, SIGKILL);
  exit(0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  memset(name, 0, MAX_NAME_LEN);
  if (sscanf(line, "nat_proxy_beat_resp %s", name) == 1)
    {
    if (!strcmp(get_nat_name(), name))
      g_watchdog_proxy_ok = 1;
    else
      KERR("ERROR %s %s", line, get_nat_name());
    }
  else
    KERR("ERROR %s", line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char dgram_rx[MAX_PATH_LEN];
  char dgram_tx[MAX_PATH_LEN];
  char stream[MAX_PATH_LEN];
  uint32_t sip, dip, ipdst;
  uint16_t sport, dport, ident, seq;
  char resp[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  char str_ip[MAX_NAME_LEN];
  int num, cli_llid, cli_tid;
  uint32_t addr_ip;
  uint8_t m[6];
  memset(resp, 0, MAX_PATH_LEN);
  if (llid == g_llid_prxy)
    {
    if (sscanf(line,
    "nat_proxy_hello_resp %s", name) == 1)
      {
      if (strcmp(name, get_nat_name()))
        KERR("ERROR %s %s", name, get_nat_name());
      snprintf(resp,MAX_PATH_LEN-1,"nat_suidroot_ok %s",get_nat_name());
      rpct_send_sigdiag_msg(g_llid, tid, resp);
      }
    else if (sscanf(line,
    "nat_proxy_tcp_req_ko %s strm:%s sip:%X dip:%X sport:%hu dport:%hu",
    name, stream, &sip, &dip, &sport, &dport) == 6)
      {
      if (strcmp(name, get_nat_name()))
        KERR("ERROR %s %s", name, get_nat_name());
      tcp_stream_proxy_resp(-1, stream, sip, dip, sport, dport);
      }
    else if (sscanf(line,
    "nat_proxy_tcp_req_ok %s strm:%s sip:%X dip:%X sport:%hu dport:%hu",
    name, stream, &sip, &dip, &sport, &dport) == 6)
      {
      if (strcmp(name, get_nat_name()))
        KERR("ERROR %s %s", name, get_nat_name());
      tcp_stream_proxy_resp(0, stream, sip, dip, sport, dport);
      }
    else if (sscanf(line,
    "nat_proxy_tcp_resp_ko %s sip:%X dip:%X sport:%hu dport:%hu",
    name, &sip, &dip, &sport, &dport) == 5)
      {
      if (strcmp(name, get_nat_name()))
        KERR("ERROR %s %s", name, get_nat_name());
      tcp_connect_resp(-1, sip, dip, sport, dport);
      }
    else if (sscanf(line,
    "nat_proxy_tcp_resp_ok %s sip:%X dip:%X sport:%hu dport:%hu",
    name, &sip, &dip, &sport, &dport) == 5)
      {
      if (strcmp(name, get_nat_name()))
        KERR("ERROR %s %s", name, get_nat_name());
      tcp_connect_resp(0, sip, dip, sport, dport);
      }
    else if (sscanf(line,
    "nat_proxy_tcp_fatal_error %s sip:%X dip:%X sport:%hu dport:%hu",
    name, &sip, &dip, &sport, &dport) == 5)
      {
      if (strcmp(name, get_nat_name()))
        KERR("ERROR %s %s", name, get_nat_name());
      tcp_fatal_error(sip, dip, sport, dport);
      }
    else if (sscanf(line,
    "nat_proxy_udp_resp_ko %s dgrm_rx:%s dgrm_tx:%s "
    "sip:%X dip:%X dest:%X sport:%hu dport:%hu",
    name, dgram_rx, dgram_tx, &sip, &dip, &ipdst, &sport, &dport) == 8)
      {
      if (strcmp(name, get_nat_name()))
        KERR("ERROR %s %s", name, get_nat_name());
      udp_dgram_proxy_resp(-1,dgram_rx,dgram_tx,sip,dip,ipdst,sport,dport);
      }
    else if (sscanf(line,
    "nat_proxy_udp_resp_ok %s dgrm_rx:%s dgrm_tx:%s "
    "sip:%X dip:%X dest:%X sport:%hu dport:%hu",
    name, dgram_rx, dgram_tx, &sip, &dip, &ipdst, &sport, &dport) == 8)
      {
      if (strcmp(name, get_nat_name()))
        KERR("ERROR %s %s", name, get_nat_name());
      udp_dgram_proxy_resp(0,dgram_rx,dgram_tx,sip,dip,ipdst,sport,dport);
      }
    else if (sscanf(line,
    "nat_proxy_icmp_resp_ko %s ipdst:%X ident:%hu seq:%hu",
    name, &ipdst, &ident, &seq) == 4)
      {
      if (strcmp(name, get_nat_name()))
        KERR("ERROR %s %s", name, get_nat_name());
      icmp_resp(ipdst, ident, seq, 1);
      }
    else if (sscanf(line,
    "nat_proxy_icmp_resp_ok %s ipdst:%X ident:%hu seq:%hu",
    name, &ipdst, &ident, &seq) == 4)
      {
      if (strcmp(name, get_nat_name()))
        KERR("ERROR %s %s", name, get_nat_name());
      icmp_resp(ipdst, ident, seq, 0);
      }
    else
      KERR("ERROR %s", line);
    }
  else if (llid == g_llid)
    {
    if (tid != type_hop_nat_main)
      KERR("ERROR %s %d %d", g_net_name, tid, type_hop_nat_main);
    DOUT(FLAG_HOP_SIGDIAG, "NAT %s", line);
    if (!strncmp(line,
    "nat_suidroot", strlen("nat_suidroot")))
      {
      if (check_and_set_uid())
        {
        snprintf(resp, MAX_PATH_LEN-1,
        "nat_suidroot_ko %s", get_nat_name());
        rpct_send_sigdiag_msg(llid, tid, resp);
        }
      else if (g_llid_prxy == 0) 
        {
        snprintf(resp, MAX_PATH_LEN-1,
        "nat_suidroot_ko %s", get_nat_name());
        rpct_send_sigdiag_msg(llid, tid, resp);
        }
      else
        {
        memset(resp, 0, MAX_PATH_LEN);
        snprintf(resp, MAX_PATH_LEN-1,
        "nat_proxy_hello_req %s", get_nat_name());
        rpct_send_sigdiag_msg(g_llid_prxy, 0, resp);
        }
      }
    else if (!strncmp(line,
    "nat_machine_begin", strlen("nat_machine_begin")))
      {
      machine_begin();
      }
    else if (sscanf(line,
    "nat_machine_add %s eth:%d "
    "mac:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
      name, &num, &(m[0]), &(m[1]), &(m[2]), &(m[3]), &(m[4]), &(m[5])) == 8)
      {
      machine_add(name, num, m);
      }
    else if (!strncmp(line,
    "nat_machine_end", strlen("nat_machine_end")))
      {
      machine_end();
      }
  
    else if (sscanf(line,
    "nat_whatip cli_llid=%d cli_tid=%d name=%s",&cli_llid,&cli_tid,name) == 3)
      {
      memset(resp, 0, MAX_PATH_LEN);
      addr_ip = machine_ip_get(name);
      if (addr_ip)
        {
        int_to_ip_string(addr_ip, str_ip);
        snprintf(resp,MAX_PATH_LEN-1,
        "nat_whatip_ok cli_llid=%d cli_tid=%d name=%s ip=%s",
        cli_llid, cli_tid, name, str_ip);
        }
      else
        {
        snprintf(resp,MAX_PATH_LEN-1,
        "nat_whatip_ko cli_llid=%d cli_tid=%d name=%s",cli_llid,cli_tid,name);
        }
      rpct_send_sigdiag_msg(llid, tid, resp);
      }
    else
      KERR("ERROR %s %s", g_net_name, line);
    }
  else
    KERR("ERROR %s %d %d %d", g_net_name, llid, g_llid, g_llid_prxy);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_ctrl_cb (int llid, int err, int from)
{
  KOUT("ERROR CONNECTION %s %s", g_net_name, get_nat_name());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_ctrl_cb (int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    KOUT("ERROR %s %s", get_nat_name(), buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_ctrl_client(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, err_ctrl_cb, rx_ctrl_cb);
  g_llid = llid_new;
  g_watchdog_ok = 1;
  g_watchdog_proxy_ok = 1;
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fct_timeout_self_destruct(void *data)
{
  if (g_watchdog_ok == 0)
    {
    KERR("SELF DESTRUCT WATCHDOG MASTER %s %s", g_net_name, get_nat_name());
    KOUT("EXIT");
    }
  g_watchdog_ok = 0;
  if (g_watchdog_proxy_ok == 0)
    {
    KERR("SELF DESTRUCT WATCHDOG PROXY %s %s", g_net_name, get_nat_name());
    KOUT("EXIT");
    }
  g_watchdog_proxy_ok = 0;
  clownix_timeout_add(500, fct_timeout_self_destruct, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main (int argc, char *argv[])
{
  char *root = g_root_path;
  char *nat = g_nat_name;
  int fd_rx_from_tap, fd_tx_to_tap;
  if (argc != 5)
    KOUT("ERROR %d", argc);
  umask(0000);
  g_llid = 0;
  g_llid_prxy = 0;
  g_watchdog_ok = 0;
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_netns_namespace, 0, MAX_PATH_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_prxy_path, 0, MAX_PATH_LEN);
  memset(g_nat_name, 0, MAX_NAME_LEN);
  memset(g_vhost, 0, MAX_NAME_LEN);
  memset(g_nat_path, 0, MAX_PATH_LEN);
  memset(g_cisco_path, 0, MAX_PATH_LEN);
  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_nat_name,  argv[3], MAX_NAME_LEN-1);
  strncpy(g_vhost,  argv[4], MAX_NAME_LEN-1);
  snprintf(g_ctrl_path,MAX_PATH_LEN-1,"%s/%s/c%s",root,NAT_MAIN_DIR, nat);
  snprintf(g_nat_path, MAX_PATH_LEN-1,"%s/%s/%s", root, NAT_MAIN_DIR, nat);
  snprintf(g_prxy_path, MAX_PATH_LEN-1,"%s_%s/%s/%s",
          PROXYSHARE_IN, g_net_name, NAT_PROXY_DIR, nat);
  snprintf(g_cisco_path, MAX_PATH_LEN-1,"%s/%s/%s_0_u2i", root, NAT_MAIN_DIR, nat);
  snprintf(g_netns_namespace, MAX_PATH_LEN-1, "%s%s_%s",
           PATH_NAMESPACE, BASE_NAMESPACE, g_net_name);
  g_netns_pid = tun_tap_open(g_netns_namespace, g_vhost,
                             &fd_rx_from_tap, &fd_tx_to_tap);
  msg_mngt_init("nat", TRAF_TAP_BUF_LEN);
  msg_mngt_heartbeat_init(heartbeat);
  rxtx_init(fd_rx_from_tap, fd_tx_to_tap);
  if (!access(g_ctrl_path, R_OK))
    {
    KERR("ERROR %s exists ERASING", g_ctrl_path);
    unlink(g_ctrl_path);
    }
  if (!access(g_nat_path, R_OK))
    {
    KERR("ERROR %s exists ERASING", g_nat_path);
    unlink(g_nat_path);
    }
  if (!access(g_cisco_path, R_OK))
    {
    KERR("ERROR %s exists ERASING", g_cisco_path);
    unlink(g_cisco_path);
    }
  string_server_unix(g_ctrl_path, connect_from_ctrl_client, "ctrl");
  ssh_cisco_llid_init(g_cisco_path);
  daemon(0,0);
  seteuid(getuid());
  cloonix_set_pid(getpid());
  clownix_timeout_add(1500, fct_timeout_self_destruct, NULL, NULL, NULL);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/

