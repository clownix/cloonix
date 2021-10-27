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
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>



#include <rte_compat.h>
#include <rte_bus_pci.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_meter.h>
#include <rte_pci.h>
#include <rte_version.h>
#include <rte_vhost.h>
#include <rte_errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "vhost_client.h"
#include "udp.h"


/*--------------------------------------------------------------------------*/
static int  g_llid;
static int  g_watchdog_ok;
static char g_net_name[MAX_NAME_LEN];
static char g_memid[MAX_NAME_LEN];
static char g_d2d_name[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_d2d_socket[MAX_PATH_LEN];
static char g_runtime[MAX_PATH_LEN];
static char g_prefix[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char *g_rte_argv[8];
static uint16_t g_udp_port;
static uint32_t g_cpu_flags;
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
    KERR("%s %s %d %d", g_net_name, name, llid, g_llid);
  if (tid != type_hop_d2d_dpdk)
    KERR("%s %s %d %d", g_net_name, name, llid, g_llid);
  if (strcmp(g_d2d_name, name))
    KERR("%s %s %s %d %d", g_net_name, name, g_d2d_name, llid, g_llid);
  rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
  g_watchdog_ok = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void end_clean_unlink(void)
{
  unlink(g_ctrl_path);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_kil_req(int llid, int tid)
{
  vhost_client_stop();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void reply_probe_udp(void)
{
  char resp[MAX_PATH_LEN];
  snprintf(resp, MAX_PATH_LEN-1,"cloonixd2d_receive_probe_udp %s",g_d2d_name);
  rpct_send_sigdiag_msg(g_llid, type_hop_d2d_dpdk, resp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  KERR("%s", line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  uint32_t dist_ip;
  uint16_t dist_port;
  int ret;
  DOUT(FLAG_HOP_SIGDIAG, "D2D %s", line);
  memset(resp, 0, MAX_PATH_LEN);
  if (llid != g_llid)
    KERR("%s %d %d", g_net_name, llid, g_llid);
  if (tid != type_hop_d2d_dpdk)
    KERR("%s %d %d", g_net_name, tid, type_hop_d2d_dpdk);
  if (!strncmp(line,
  "cloonixd2d_suidroot", strlen("cloonixd2d_suidroot")))
    {
    if (check_and_set_uid())
      snprintf(resp, MAX_PATH_LEN-1, "cloonixd2d_suidroot_ko %s", g_d2d_name);
    else
      snprintf(resp, MAX_PATH_LEN-1, "cloonixd2d_suidroot_ok %s", g_d2d_name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (!strncmp(line,
  "cloonixd2d_eal_init", strlen("cloonixd2d_eal_init")))
    {
    setenv("XDG_RUNTIME_DIR", g_runtime, 1);
    ret = rte_eal_init(7, g_rte_argv);
    if (ret < 0)
      KOUT("Cannot init EAL %s %d\n", g_net_name, rte_errno);
    snprintf(resp, MAX_PATH_LEN-1, "cloonixd2d_eal_init_ok %s", g_d2d_name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (sscanf(line,
  "cloonixd2d_vhost_start %s", name) == 1)
    {
    vhost_client_start(g_memid, g_d2d_socket);
    snprintf(resp, MAX_PATH_LEN-1, 
    "cloonixd2d_vhost_start_ok %s", name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (sscanf(line,
  "cloonixd2d_vhost_stop %s", name) == 1)
    {
    vhost_client_stop();
    snprintf(resp, MAX_PATH_LEN-1,
    "cloonixd2d_vhost_stop_ok %s", name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (sscanf(line,
  "cloonixd2d_get_udp_port %s", name) == 1)
    {
    g_udp_port = udp_init();
    if (g_udp_port == -1)
      {
      snprintf(resp, MAX_PATH_LEN-1, 
      "cloonixd2d_get_udp_port_ko %s", name);
      }
    else
      {
      snprintf(resp, MAX_PATH_LEN-1, 
      "cloonixd2d_get_udp_port_ok %s udp_port=%hu", name, g_udp_port);
      }
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (sscanf(line,
  "cloonixd2d_set_dist_udp_ip_port %s %x %hu",name,&dist_ip,&dist_port)==3)
    {
    udp_fill_dist_addr(dist_ip, dist_port);
    snprintf(resp, MAX_PATH_LEN-1,
    "cloonixd2d_set_dist_udp_ip_port_ok %s", name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else if (sscanf(line,
  "cloonixd2d_send_probe_udp %s", name) == 1)
    {
    if (strcmp(g_d2d_name, name))
      KERR("%s %s %s", g_net_name, name, g_d2d_name);
    udp_tx_sig(strlen("probe")+1, (uint8_t *) "probe");
    }
  else
    KERR("%s %s", g_net_name, line);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void err_ctrl_cb (int llid, int err, int from)
{
  KERR("D2D DPDK SELF DESTRUCT CONNECTION");
  rpct_recv_kil_req(0, 0);
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
    KERR("D2D DPDK SELF DESTRUCT WATCHDOG");
    rpct_recv_kil_req(0, 0);
    }
  g_watchdog_ok = 0;
  clownix_timeout_add(500, fct_timeout_self_destruct, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static char *get_cpu_from_flags(uint32_t flags)
{
  static char g_lcore_cpu[MAX_NAME_LEN];
  int i, cpu;
  for (i=0; i<64; i++)
    {
    if ((flags >> i) & 0x01)
      break;
    }
  cpu = i;
  memset(g_lcore_cpu, 0, MAX_NAME_LEN);
  snprintf(g_lcore_cpu, MAX_NAME_LEN-1, "--lcores=0@0,%d@%d", cpu, cpu);
  return g_lcore_cpu;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_net_name(void)
{
  return g_net_name;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_d2d_name(void)
{
  return g_d2d_name;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main (int argc, char *argv[])
{
  char *root = g_root_path;
  char *sock = D2D_DPDK_SOCK_DIR;
  char *net = g_net_name;
  char *d2d = g_d2d_name;
  if (argc != 6)
    KOUT("%d", argc);
  g_llid = 0;
  g_watchdog_ok = 0;
  memset(g_memid, 0, MAX_NAME_LEN);
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_d2d_socket, 0, MAX_PATH_LEN);
  memset(g_runtime, 0, MAX_PATH_LEN);
  memset(g_prefix, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_d2d_name,  argv[3], MAX_NAME_LEN-1);
  snprintf(g_runtime, MAX_PATH_LEN-1, "%s/dpdk", root);
  snprintf(g_d2d_socket, MAX_PATH_LEN-1, "%s/dpdk/d2d_%s", root, d2d);
  snprintf(g_prefix, MAX_PATH_LEN-1, "--file-prefix=cloonix%s", net);
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s/%s", root, sock, d2d);
  sscanf(argv[4], "%X", &g_cpu_flags);
  strncpy(g_memid, argv[5], MAX_NAME_LEN-1);
  if (!access(g_ctrl_path, F_OK))
    {
    KERR("%s exists ERASING", g_ctrl_path);
    unlink(g_ctrl_path);
    }
  if (!access(g_d2d_socket, F_OK))
    {
    KERR("%s exists ERASING", g_d2d_socket);
    unlink(g_d2d_socket);
    }
  vhost_client_init();
  msg_mngt_init("d2d_dpdk", IO_MAX_BUF_LEN);
  msg_mngt_heartbeat_init(heartbeat);
  string_server_unix(g_ctrl_path, connect_from_ctrl_client, "ctrl");
  g_rte_argv[0] = argv[0];
  g_rte_argv[1] = "--iova-mode=va";
  g_rte_argv[2] = g_prefix;
  g_rte_argv[3] = "--proc-type=secondary";
  g_rte_argv[4] = "--log-level=5";
  g_rte_argv[5] = get_cpu_from_flags(g_cpu_flags);
  g_rte_argv[6] = "--";
  g_rte_argv[7] = NULL;
  daemon(0,0);
  seteuid(getuid());
  cloonix_set_pid(getpid());
  clownix_timeout_add(1500, fct_timeout_self_destruct, NULL, NULL, NULL);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/

