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
#include "utils.h"
#include "sched.h"
#include "flow_tab.h"


/*--------------------------------------------------------------------------*/
static int  g_llid;
static int  g_watchdog_ok;
static char g_net_name[MAX_NAME_LEN];
static char g_memid0[MAX_NAME_LEN];
static char g_memid1[MAX_NAME_LEN];
static char g_a2b_name[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_a2b0_socket[MAX_PATH_LEN];
static char g_a2b1_socket[MAX_PATH_LEN];
static char g_runtime[MAX_PATH_LEN];
static char g_prefix[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char *g_rte_argv[8];
static int g_started_vhost;
static uint32_t g_cpu_flags;
/*--------------------------------------------------------------------------*/

#define RANDOM_APPEND_SIZE 8
#define FLOW_TAB_SIZE 2048
#define FLOW_TAB_PORT 7681
/*****************************************************************************/
static char *random_str(void)
{
  static char rd[RANDOM_APPEND_SIZE+1];
  int i;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  srand(ts.tv_nsec);
  memset (rd, 0 , RANDOM_APPEND_SIZE+1);
  for (i=0; i<RANDOM_APPEND_SIZE; i++)
    rd[i] = 'A' + (rand() % 26);
  return rd;
}
/*---------------------------------------------------------------------------*/

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
  static int count_ticks_rpct = 0;
  count_ticks_blkd += 1;
  count_ticks_rpct += 1;
  if (count_ticks_blkd == 5)
    {
    blkd_heartbeat(NULL);
    count_ticks_blkd = 0;
    }
  if (count_ticks_rpct == 100)
    {
    rpct_heartbeat(NULL);
    count_ticks_rpct = 0;
    if (g_started_vhost == 1)
      flow_tab_periodic_dump(FLOW_TAB_SIZE);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void req_unix2inet_conpath_evt(int llid, char *name)
{
  char msg[MAX_PATH_LEN];
  sprintf(msg, "unix2inet_conpath_evt_monitor llid=%d name=%s", llid, name);
  rpct_send_app_msg(NULL, g_llid, 0, msg);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num)
{
  if (llid != g_llid)
    KERR("%s %s %d %d", g_net_name, name, llid, g_llid);
  if (tid != type_hop_a2b_dpdk)
    KERR("%s %s %d %d", g_net_name, name, llid, g_llid);
  if (strcmp(g_a2b_name, name))
    KERR("%s %s %s %d %d", g_net_name, name, g_a2b_name, llid, g_llid);
  rpct_send_pid_resp(ptr, llid, tid, name, num, cloonix_get_pid(), getpid());
  g_watchdog_ok = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void end_clean_unlink(void)
{
  unlink(g_ctrl_path);
  rte_exit(EXIT_SUCCESS, "Exit a2b");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_kil_req(void *ptr, int llid, int tid)
{
  if (g_started_vhost)
    {
    vhost_client_end_and_exit();
    }
  else
    {
    end_clean_unlink();
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line)
{
  int ret, dir, type, val;
  char name[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  memset(resp, 0, MAX_PATH_LEN);
  if (llid != g_llid)
    KERR("%s %d %d", g_net_name, llid, g_llid);
  if (tid != type_hop_a2b_dpdk)
    KERR("%s %d %d", g_net_name, tid, type_hop_a2b_dpdk);
  if (!strncmp(line,
  "cloonixa2b_suidroot", strlen("cloonixa2b_suidroot")))
    {
    if (check_and_set_uid())
      snprintf(resp, MAX_PATH_LEN-1, "cloonixa2b_suidroot_ko %s", g_a2b_name);
    else
      snprintf(resp, MAX_PATH_LEN-1, "cloonixa2b_suidroot_ok %s", g_a2b_name);
    rpct_send_diag_msg(NULL, llid, tid, resp);
    }
  else if (sscanf(line,
  "cloonixa2b_vhost_start %s", name) == 1)
    {
    setenv("XDG_RUNTIME_DIR", g_runtime, 1);
    ret = rte_eal_init(7, g_rte_argv);
    if (ret < 0)
      KOUT("Cannot init EAL %d\n", rte_errno);
    flow_tab_init(name, FLOW_TAB_PORT, FLOW_TAB_SIZE);
    vhost_client_start(g_a2b0_socket, g_memid0, g_a2b1_socket, g_memid1);
    g_started_vhost = 1;
    snprintf(resp, MAX_PATH_LEN-1, 
    "cloonixa2b_vhost_start_ok %s", name);
    rpct_send_diag_msg(NULL, llid, tid, resp);
    }
  else if (sscanf(line,
  "cloonixa2b_vhost_cnf %s dir=%d type=%d val=%d",name,&dir,&type,&val)==4)
    {
    sched_cnf(dir, type, val);
    }
  else
    KERR("%s %s", g_net_name, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_ctrl_cb (void *ptr, int llid, int err, int from)
{
  KERR("A2B DPDK SELF DESTRUCT CONNECTION");
  rpct_recv_kil_req(NULL, 0, 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_ctrl_cb (int llid, int len, char *buf)
{
  if (rpct_decoder(NULL, llid, len, buf))
    KOUT("%s %s", g_net_name, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_ctrl_client(void *ptr, int llid, int llid_new)
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
    KERR("A2B DPDK SELF DESTRUCT WATCHDOG");
    rpct_recv_kil_req(NULL, 0, 0);
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
int main (int argc, char *argv[])
{
  char *root = g_root_path;
  char *sock = A2B_DPDK_SOCK_DIR;
  char *net = g_net_name;
  char *a2b = g_a2b_name;
  if (argc != 5)
    KOUT("%d", argc);
  g_llid = 0;
  g_watchdog_ok = 0;
  g_started_vhost = 0;
  memset(g_memid0, 0, MAX_NAME_LEN);
  memset(g_memid1, 0, MAX_NAME_LEN);
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_a2b0_socket, 0, MAX_PATH_LEN);
  memset(g_a2b1_socket, 0, MAX_PATH_LEN);
  memset(g_runtime, 0, MAX_PATH_LEN);
  memset(g_prefix, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_a2b_name,  argv[3], MAX_NAME_LEN-1);
  snprintf(g_runtime, MAX_PATH_LEN-1, "%s/dpdk", root);
  snprintf(g_a2b0_socket, MAX_PATH_LEN-1, "%s/dpdk/a2_%s", root, a2b);
  snprintf(g_a2b1_socket, MAX_PATH_LEN-1, "%s/dpdk/b2_%s", root, a2b);
  snprintf(g_prefix, MAX_PATH_LEN-1, "--file-prefix=cloonix%s", net);
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s/%s", root, sock, a2b);
  snprintf(g_memid0, MAX_NAME_LEN-1, "%s%s0%s", net, a2b, random_str());
  snprintf(g_memid1, MAX_NAME_LEN-1, "%s%s1%s", net, a2b, random_str());
  sscanf(argv[4], "%X", &g_cpu_flags);
  if (!access(g_ctrl_path, F_OK))
    {
    KERR("%s exists ERASING", g_ctrl_path);
    unlink(g_ctrl_path);
    }
  vhost_client_init();
  utils_init();
  msg_mngt_init("a2b_dpdk", IO_MAX_BUF_LEN);
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
  clownix_timeout_add(1000, fct_timeout_self_destruct, NULL, NULL, NULL);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/

