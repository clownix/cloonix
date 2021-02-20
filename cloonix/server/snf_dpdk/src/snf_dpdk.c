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




#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cirspy.h"
#include "eventfull.h"
#include "vhost_client.h"
#include "pcap_record.h"
#include "inotify_trigger.h"

/*--------------------------------------------------------------------------*/
static int  g_llid;
static int  g_watchdog_ok;
static char g_net_name[MAX_NAME_LEN];
static char g_memid[MAX_NAME_LEN];
static char g_snf_name[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_pcap_file[MAX_PATH_LEN];
static char g_snf_socket[MAX_PATH_LEN];
static char g_runtime[MAX_PATH_LEN];
static char g_prefix[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char *g_rte_argv[8];
static uint32_t g_cpu_flags;
/*--------------------------------------------------------------------------*/


#define RANDOM_APPEND_SIZE 8
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
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void send_eventfull_tx_rx(char *name, int num, int ms,
                          int ptx, int btx, int prx, int brx)
{
  char resp[MAX_PATH_LEN];
  memset(resp, 0, MAX_PATH_LEN);
  snprintf(resp, MAX_PATH_LEN-1, "endp_eventfull_tx_rx %s %d %d %d %d %d %d",
                                 name, num, ms, ptx, btx, prx, brx);
  rpct_send_diag_msg(NULL, g_llid, type_hop_snf_dpdk, resp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void send_config_modif(void)
{
  char resp[MAX_PATH_LEN];
  memset(resp, 0, MAX_PATH_LEN);
  snprintf(resp, MAX_PATH_LEN-1, 
  "cloonixsnf_GET_CONF_RESP %s %d", g_snf_name, pcap_record_is_on());
  rpct_send_diag_msg(NULL, g_llid, type_hop_snf_dpdk, resp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num)
{
  if (llid != g_llid)
    KERR("%s %s %d %d", g_net_name, name, llid, g_llid);
  if (tid != type_hop_snf_dpdk)
    KERR("%s %s %d %d", g_net_name, name, llid, g_llid);
  if (strcmp(g_snf_name, name))
    KERR("%s %s %s %d %d", g_net_name, name, g_snf_name, llid, g_llid);
  rpct_send_pid_resp(ptr, llid, tid, name, num, cloonix_get_pid(), getpid());
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
void rpct_recv_kil_req(void *ptr, int llid, int tid)
{
  inotify_trigger_end();
  pcap_record_unlink();
  vhost_client_end_and_exit();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line)
{
  int ret, num;
  char name[MAX_NAME_LEN];
  char strmac[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  memset(resp, 0, MAX_PATH_LEN);
  if (llid != g_llid)
    KERR("%s %d %d", g_net_name, llid, g_llid);
  if (tid != type_hop_snf_dpdk)
    KERR("%s %d %d", g_net_name, tid, type_hop_snf_dpdk);

  if (!strncmp(line,
  "cloonixsnf_suidroot", strlen("cloonixsnf_suidroot")))
    {
    if (check_and_set_uid())
      rpct_send_diag_msg(NULL, llid, tid, "cloonixsnf_suidroot_ko");
    else
      {
      pcap_record_init(g_pcap_file);
      eventfull_init_start();
      cirspy_init();
      rpct_send_diag_msg(NULL, llid, tid, "cloonixsnf_suidroot_ok");
      setenv("XDG_RUNTIME_DIR", g_runtime, 1);
      ret = rte_eal_init(7, g_rte_argv);
      if (ret < 0)
        KOUT("Cannot init EAL %d\n", rte_errno);
      vhost_client_start(g_snf_socket, g_memid);
      }
    }
  else if (!strncmp(line,
  "cloonixsnf_macliststart", strlen("cloonixsnf_macliststart")))
    {
    eventfull_obj_update_begin();
    }
  else if (sscanf(line, "cloonixsnf_mac name=%s num=%d mac=%s",
                         name, &num, strmac) == 3)
    {
    eventfull_obj_update_item(name, num, strmac);
    }
  else if (!strncmp(line,
  "cloonixsnf_maclistend", strlen("cloonixsnf_maclistend")))
    {
    eventfull_obj_update_end();
    }
  else
    KERR("%s %s", g_net_name, line);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void err_ctrl_cb (void *ptr, int llid, int err, int from)
{
  KERR("SNF DPDK SELF DESTRUCT CONNECTION");
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
    KERR("SNF DPDK SELF DESTRUCT WATCHDOG");
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
  char *sock = SNF_DPDK_SOCK_DIR;
  char *net = g_net_name;
  char *snf = g_snf_name;
  if (argc != 6)
    KOUT("%d", argc);
  g_llid = 0;
  g_watchdog_ok = 0;
  eventfull_init();
  memset(g_memid, 0, MAX_NAME_LEN);
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_snf_socket, 0, MAX_PATH_LEN);
  memset(g_runtime, 0, MAX_PATH_LEN);
  memset(g_prefix, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_pcap_file, 0, MAX_PATH_LEN);
  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_snf_name,  argv[3], MAX_NAME_LEN-1);
  snprintf(g_pcap_file, MAX_PATH_LEN-1,"%s/%s.pcap", argv[4], snf);
  snprintf(g_runtime, MAX_PATH_LEN-1, "%s/dpdk", root);
  snprintf(g_snf_socket, MAX_PATH_LEN-1, "%s/dpdk/mi_%s", root, snf);
  snprintf(g_prefix,MAX_PATH_LEN-1,"--file-prefix=cloonix%s", net);
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s/%s", root, sock, snf);
  snprintf(g_memid, MAX_NAME_LEN-1, "%s%s%s", net, snf, random_str());
  sscanf(argv[5], "%X", &g_cpu_flags);
  if (!access(g_ctrl_path, F_OK))                                       
    {
    KERR("%s exists ERASING", g_ctrl_path);
    unlink(g_ctrl_path);
    }
  vhost_client_init();
  msg_mngt_init("snf_dpdk", IO_MAX_BUF_LEN);
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

