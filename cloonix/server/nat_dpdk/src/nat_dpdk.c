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
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>


#define ALLOW_EXPERIMENTAL_API
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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "vhost_client.h"
#include "machine.h"
#include "txq_dpdk.h"
#include "utils.h"
#include "ssh_cisco_llid.h"
#include "ssh_cisco_dpdk.h"


/*--------------------------------------------------------------------------*/
static int  g_llid;
static int  g_watchdog_ok;
static char g_net_name[MAX_NAME_LEN];
static char g_memid[MAX_NAME_LEN];
static char g_nat_name[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_nat_socket[MAX_PATH_LEN];
static char g_runtime[MAX_PATH_LEN];
static char g_prefix[MAX_PATH_LEN];
static char g_ctrl_path[MAX_PATH_LEN];
static char g_cisco_path[MAX_PATH_LEN];
static char *g_rte_argv[6];
/*--------------------------------------------------------------------------*/

#define RANDOM_APPEND_SIZE 8
/*****************************************************************************/
static char *random_str(void)
{
  static char rd[RANDOM_APPEND_SIZE+1];
  int i;
  memset (rd, 0 , RANDOM_APPEND_SIZE+1);
  for (i=0; i<RANDOM_APPEND_SIZE; i++)
    rd[i] = 'A' + (rand() % 26);
  return rd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static uint8_t *get_mac(char *str_mac)
{
  static uint8_t mc[6];
  uint32_t i, mac[6];
  memset(mc, 0, 6);
  if (sscanf(str_mac, "%02X:%02X:%02X:%02X:%02X:%02X",
                      &(mac[0]), &(mac[1]), &(mac[2]),
                      &(mac[3]), &(mac[4]), &(mac[5])) != 6)
    KERR("%s", str_mac);
  else
    {
    for (i=0; i<6; i++)
      mc[i] = mac[i] & 0xFF;
    }
  return mc;
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
  if (tid != type_hop_nat_dpdk)
    KERR("%s %s %d %d", g_net_name, name, llid, g_llid);
  if (strcmp(g_nat_name, name))
    KERR("%s %s %s %d %d", g_net_name, name, g_nat_name, llid, g_llid);
  rpct_send_pid_resp(ptr, llid, tid, name, num, cloonix_get_pid(), getpid());
  g_watchdog_ok = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void end_clean_unlink(void)
{
  unlink(g_ctrl_path);
  unlink(g_cisco_path);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_kil_req(void *ptr, int llid, int tid)
{
  vhost_client_end_and_exit();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line)
{
  char resp[MAX_PATH_LEN];
  char name[MAX_NAME_LEN];
  char str_ip[MAX_NAME_LEN];
  uint32_t addr_ip;
  memset(resp, 0, MAX_PATH_LEN);
  if (sscanf(line, "whatip %s", name) == 1)
    {
    addr_ip = machine_ip_get(name);
    if (addr_ip)
      {
      int_to_ip_string (addr_ip, str_ip);
      snprintf(resp, MAX_PATH_LEN-1, "RESPOK %s %s", name, str_ip);
      }
    else
      snprintf(resp, MAX_PATH_LEN-1, "RESPKO UNKNOWN IP FOR VM: %s", name);
    }
  else
    snprintf(resp, MAX_PATH_LEN-1, "RESPKO NO CMD: %s", line);
  rpct_send_cli_resp(ptr, llid, tid, cli_llid, cli_tid, resp);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line)
{
  int num, ret;
  char resp[MAX_PATH_LEN];
  char mac[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  uint8_t *mc;
  memset(resp, 0, MAX_PATH_LEN);
  if (llid != g_llid)
    KERR("%s %d %d", g_net_name, llid, g_llid);
  if (tid != type_hop_nat_dpdk)
    KERR("%s %d %d", g_net_name, tid, type_hop_nat_dpdk);
  if (!strncmp(line,
  "cloonixnat_suidroot", strlen("cloonixnat_suidroot")))
    {
    if (check_and_set_uid())
      rpct_send_diag_msg(NULL, llid, tid, "cloonixnat_suidroot_ko");
    else
      {
      rpct_send_diag_msg(NULL, llid, tid, "cloonixnat_suidroot_ok");
      setenv("XDG_RUNTIME_DIR", g_runtime, 1);
      ret = rte_eal_init(4, g_rte_argv);
      if (ret < 0)
        KOUT("Cannot init EAL\n");
      machine_init();
      txq_dpdk_init();
      ssh_cisco_llid_init(g_cisco_path);
      ssh_cisco_dpdk_init();
      vhost_client_start(g_nat_socket, g_memid);
      }
    }
  else if (!strncmp(line,
  "cloonixnat_machine_begin", strlen("cloonixnat_machine_begin")))
    {
    machine_begin();
    }
  else if (sscanf(line,
  "cloonixnat_machine_add %s eth:%d mac:%s", name, &num, mac))
    {
    mc = get_mac(mac);
    machine_add(name, num, mc);
    }
  else if (!strncmp(line,
  "cloonixnat_machine_end", strlen("cloonixnat_machine_end")))
    {
    machine_end();
    }
  else
    KERR("%s %s", g_net_name, line);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void err_ctrl_cb (void *ptr, int llid, int err, int from)
{
  KERR("NAT DPDK SELF DESTRUCT CONNECTION");
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
    KERR("NAT DPDK SELF DESTRUCT WATCHDOG");
    rpct_recv_kil_req(NULL, 0, 0);
    }
  g_watchdog_ok = 0;
  clownix_timeout_add(500, fct_timeout_self_destruct, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main (int argc, char *argv[])
{
  char *root = g_root_path;
  char *sock = ENDP_SOCK_DIR;
  char *net = g_net_name;
  char *nat = g_nat_name;
  if (argc != 4)
    KOUT("%d", argc);
  g_llid = 0;
  g_watchdog_ok = 0;
  memset(g_memid, 0, MAX_NAME_LEN);
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_nat_socket, 0, MAX_PATH_LEN);
  memset(g_runtime, 0, MAX_PATH_LEN);
  memset(g_prefix, 0, MAX_PATH_LEN);
  memset(g_ctrl_path, 0, MAX_PATH_LEN);
  memset(g_cisco_path, 0, MAX_PATH_LEN);
  strncpy(g_net_name,  argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_nat_name,  argv[3], MAX_NAME_LEN-1);
  snprintf(g_runtime, MAX_PATH_LEN-1, "%s/dpdk", root);
  snprintf(g_nat_socket, MAX_PATH_LEN-1, "%s/dpdk/na_%s", root, nat);
  snprintf(g_prefix, MAX_PATH_LEN-1, "--file-prefix=cloonix%s", net);
  snprintf(g_ctrl_path, MAX_PATH_LEN-1,"%s/%s/%s", root, sock, nat);
  snprintf(g_cisco_path, MAX_PATH_LEN-1,"%s/%s/%s_0_u2i", root, sock, nat);
  snprintf(g_memid, MAX_NAME_LEN-1, "%s%s%s", net, nat, random_str());
  if (!access(g_ctrl_path, F_OK))
    {
    KERR("%s exists ERASING", g_ctrl_path);
    unlink(g_ctrl_path);
    }
  if (!access(g_cisco_path, F_OK))
    {
    KERR("%s exists ERASING", g_cisco_path);
    unlink(g_cisco_path);
    }
  msg_mngt_init("nat_dpdk", IO_MAX_BUF_LEN);
  msg_mngt_heartbeat_init(heartbeat);
  string_server_unix(g_ctrl_path, connect_from_ctrl_client, "ctrl");
  g_rte_argv[0] = argv[0];
  g_rte_argv[1] = g_prefix;
  g_rte_argv[2] = "--proc-type=secondary";
  g_rte_argv[3] = "--log-level=5";
  g_rte_argv[4] = NULL;
  daemon(0,0);
  utils_init();
  seteuid(getuid());
  cloonix_set_pid(getpid());
  clownix_timeout_add(1000, fct_timeout_self_destruct, NULL, NULL, NULL);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/

