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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "suid_power.h"
#include "event_subscriber.h"
#include "uml_clownix_switch.h"
#include "automates.h"
#include "hop_event.h"
#include "ovs.h"
#include "llid_trace.h"
#include "cnt.h"
#include "crun.h"

static int g_nb_pid_resp;
static int g_nb_pid_resp_warning;
static int g_llid;
static int g_first_ever_start;
static int g_suid_power_root_resp_ok;
static int g_suid_power_pid;
static char g_sock_path[MAX_PATH_LEN];
static char g_bin_suid[MAX_PATH_LEN];
static char g_bin_dir[MAX_PATH_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_cloonix_net[MAX_NAME_LEN];
static char g_conf_rank[MAX_NAME_LEN];
static int g_tab_pid[MAX_VM];
static t_qemu_vm_end g_qemu_vm_end;
static void suid_power_start(void);

static t_topo_info_phy g_topo_info_phy[MAX_PHY];
static int g_nb_phy;

static int g_nb_brgs;
static t_topo_bridges g_brgs[MAX_OVS_BRIDGES];

int get_glob_req_self_destruction(void);

int get_conf_rank(void);
int get_killing_cloonix(void);

static int g_destroy_req;

/****************************************************************************/
static void unformat_bridges(char *msg)
{
  int i, j, nb, nb_port;
  char *ptr = strstr(msg, "<ovs_bridges>");
  if (!ptr)
    KERR("ERROR %s", msg);
  else
    {
    if (sscanf(ptr, "<ovs_bridges> %d ", &nb) != 1)
      KERR("ERROR %s", msg);
    else
      {
      if ((nb < 0) || (nb >= MAX_OVS_BRIDGES))
        KOUT("%s", msg);
      g_nb_brgs = nb;
      for (i=0; i<nb; i++)
        {
        ptr = strstr(ptr, "<br>");
        if (!ptr)
          KOUT("%s", msg);
        if (sscanf(ptr, "<br> %s %d </br>", g_brgs[i].br, &nb_port) != 2)
          KOUT("%s", msg);
        g_brgs[i].nb_ports = nb_port;
        for (j=0; j<nb_port; j++)
          {
          ptr = strstr(ptr, "<port>");
          if (!ptr)
            KOUT("%s", msg);
          if (sscanf(ptr, "<port> %s </port>", g_brgs[i].ports[j]) != 1)
          KOUT("%s", msg);
          ptr = strstr(ptr, "</port>");
          if (!ptr)
            KOUT("%s", msg);
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void ovs_topo_arrival(char *msg)
{
  int nb_brgs;
  t_topo_bridges *brgs;
  char *ptr;
  if (get_glob_req_self_destruction())
    return;
  ptr = strstr(msg, "</ovs_topo_config>");
  if (!ptr)
    KERR("ERROR %s", msg);
  else
    {  
    nb_brgs = g_nb_brgs;
    brgs = (t_topo_bridges *) malloc(MAX_OVS_BRIDGES*sizeof(t_topo_bridges));
    memcpy(brgs, g_brgs, MAX_OVS_BRIDGES * sizeof(t_topo_bridges));
    g_nb_brgs = 0;
    memset(g_brgs, 0, MAX_OVS_BRIDGES * sizeof(t_topo_bridges));
    unformat_bridges(msg);
    if ((nb_brgs != g_nb_brgs) ||
        (memcmp(g_brgs, brgs, MAX_OVS_BRIDGES * sizeof(t_topo_bridges))))
      {
      cfg_hysteresis_send_topo_info();
      }
    free(brgs);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *linearize(t_vm *vm)
{
  static char line[MAX_PRINT_LEN];
  char **argv = vm->launcher_argv;
  char *dtach, *name = vm->kvm.name;
  int i, ln, argc = 0, len = 0, vm_id = vm->kvm.vm_id;
  int nb_eth;
  utils_get_eth_numbers(vm->kvm.nb_tot_eth, vm->kvm.eth_table, &nb_eth);
  while (argv[argc])
    {
    if (strchr(argv[argc], '"'))
      KOUT("%s", argv[argc]);
    len += strlen(argv[argc]) + 4;
    if (len + 100 > MAX_PRINT_LEN)
      KOUT("%d %d %d", argc, len, MAX_PRINT_LEN);
    argc++;
    }
  dtach = utils_get_dtach_sock_path(name);
  ln = sprintf(line, 
       "cloonsuid_req_launch name=%s vm_id=%d dtach=%s argc=%d:",
       name, vm_id, dtach, argc);
  for (i=0; i<argc; i++)
    {
    ln += sprintf(line+ln, "\"%s", argv[i]);
    }
  ln += sprintf(line+ln, "\"");
  return line;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_monitoring(void *data)
{
  static int old_nb_pid_resp;
  static int count = 0;

  if (get_glob_req_self_destruction())
    {
    if ((g_llid) && (msg_exist_channel(g_llid)))
      rpct_send_pid_req(g_llid, type_hop_suid_power, "suid_power", 0);
    return;
    }

  cnt_timer_beat(g_llid);
  count += 1;
  if (count == 10)
    {
    rpct_send_poldiag_msg(g_llid, type_hop_suid_power, "cloonsuid_req_phy");
    count = 0;
    }
  if (old_nb_pid_resp == g_nb_pid_resp)
    g_nb_pid_resp_warning++;
  else
    g_nb_pid_resp_warning = 0;
  if (g_nb_pid_resp_warning > 100)
    {
    KERR("ERROR RESTARTING SUID_POWER %d", g_llid);
    llid_trace_free(g_llid, 0, __FUNCTION__);
    g_llid = 0; 
    g_nb_pid_resp_warning = 0;
    event_print("SUID_POWER CONTACT LOSS");
    suid_power_start();
    g_first_ever_start = 0;
    }
  else
    {
    if (g_nb_pid_resp_warning == 100)
      KERR("WARNING MONITOR %d", g_nb_pid_resp_warning);
    if ((g_llid) && (msg_exist_channel(g_llid)))
      rpct_send_pid_req(g_llid, type_hop_suid_power, "suid_power", 0);
    old_nb_pid_resp = g_nb_pid_resp;
    }
  clownix_timeout_add(100, timer_monitoring, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_connect(void *data)
{
  static int count = 0;
  int llid;
  char *ctrl = (char *) data;
  if (get_glob_req_self_destruction())
    return;
  llid = string_client_unix(ctrl, uml_clownix_switch_error_cb,
                                  uml_clownix_switch_rx_cb, "suid_power");
  if (llid)
    {
    count = 0;
    g_suid_power_pid = 0;
    g_llid = llid;
    llid_trace_alloc(llid, "suid_power", 0, 0, type_llid_trace_endp_suid);
    if (hop_event_alloc(llid, type_hop_suid_power, "suid_power", 0))
      KERR("ERROR  ");
    rpct_send_pid_req(llid, type_hop_suid_power, "suid_power", 0);
    clownix_timeout_add(100, timer_monitoring, NULL, NULL, NULL);
    }
  else
    {
    count++;
    if (count < 100)
      clownix_timeout_add(50, timer_connect, data, NULL, NULL);
    else
      KOUT("suid_power wait giving up");
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void end_process(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void suid_power_start(void)
{
  char *argv[6];

  if (get_glob_req_self_destruction())
    return;
  if (get_killing_cloonix())
    return;

  argv[0] = g_bin_suid;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = g_bin_dir;
  argv[4] = g_conf_rank;
  argv[5] = NULL;

  pid_clone_launch(utils_execv, end_process, NULL,
                 (void *) argv, NULL, NULL, "suid_power", -1, 1);
  clownix_timeout_add(100, timer_connect, (void *)g_sock_path,NULL,NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_pid_resp(int llid, int tid, char *name, int pid)
{
  char *msg1 = "cloonsuid_req_suidroot";
  char *msg2 = "cloonsuid_req_phy";
  if (get_glob_req_self_destruction())
    return;
  if (llid != g_llid)
    KERR("ERROR %d %d", llid, g_llid);
  if ((tid != type_hop_suid_power) || (strcmp(name, "suid_power")))
    KERR("ERROR %d %d %s", tid, type_hop_suid_power, name);
  if ((g_suid_power_pid) && (g_suid_power_pid != pid))
    {
    event_print("SUID_POWER PID CHANGE: %d to %d", g_suid_power_pid, pid);
    KERR("ERROR SUID_POWER PID CHANGE: %d to %d", g_suid_power_pid, pid);
    }
  g_nb_pid_resp++;
  g_suid_power_pid = pid;
  if (g_first_ever_start == 0)
    {
    g_first_ever_start = 1;
    if ((g_llid) && (msg_exist_channel(g_llid)))
      {
      hop_event_hook(llid, FLAG_HOP_SIGDIAG, msg1);
      rpct_send_sigdiag_msg(llid, type_hop_suid_power, msg1);
      hop_event_hook(llid, FLAG_HOP_SIGDIAG, msg2);
      rpct_send_poldiag_msg(g_llid, type_hop_suid_power, msg2);
      }
    else
      KERR("ERROR %d", g_llid);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int suid_power_diag_llid(int llid)
{
  int result = 0;
  if (g_llid == llid)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_poldiag_resp(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  char phy_mac[MAX_NAME_LEN];
  int i, j, prev_nb_phy, nb_phy;
  char *ptr;
  t_topo_info_phy *phy;

  if (llid != g_llid)
    KERR("ERROR %d %d", llid, g_llid);
  if (tid != type_hop_suid_power)
    KERR("ERROR %d %d", tid, type_hop_suid_power);

  if (!strncmp(line,
  "<ovs_topo_config>", strlen("<ovs_topo_config>")))
    {
    ovs_topo_arrival(line);
    }
  else if (sscanf(line,
  "cloonsuid_resp_phy nb_phys=%d", &nb_phy) == 1)
    {
    if (nb_phy > MAX_PHY)
      KOUT("%d %d", nb_phy, MAX_PHY);
    prev_nb_phy = g_nb_phy;
    phy = (t_topo_info_phy *) malloc(prev_nb_phy * sizeof(t_topo_info_phy));
    memcpy(phy, g_topo_info_phy, prev_nb_phy * sizeof(t_topo_info_phy));
    memset(g_topo_info_phy, 0, prev_nb_phy * sizeof(t_topo_info_phy));
    ptr = line;
    j = 0;
    for (i=0; i<nb_phy; i++)
      {
      ptr = strstr(ptr, "phy_name:");
      if (!ptr)
        {
        KERR("%s", line);
        break;
        }
      if (sscanf(ptr, "phy_name:%s phy_mac:%s", name, phy_mac) != 2)
        KERR("%s", line);
      else
        {
        strncpy(g_topo_info_phy[j].name,   name, MAX_NAME_LEN-1);
        strncpy(g_topo_info_phy[j].phy_mac, phy_mac, MAX_NAME_LEN-1);
        j += 1;
        }
      ptr += strlen("phy_name:");
      }
    g_nb_phy = j;
    if ((prev_nb_phy != g_nb_phy) ||
        (memcmp(phy, g_topo_info_phy, prev_nb_phy * sizeof(t_topo_info_phy))))
      {
      cfg_hysteresis_send_topo_info();
      }
    free(phy);
    }
  else
    KERR("ERROR %s", line);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_sigdiag_resp(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  int  vm_id, pid;

  if (llid != g_llid)
    KERR("ERROR %d %d", llid, g_llid);
  if (tid != type_hop_suid_power)
    KERR("ERROR %d %d", tid, type_hop_suid_power);

  if (!strcmp(line,
  "cloonsuid_resp_suidroot_ko"))
    {
    KERR("Started suid_power: %s %s", g_cloonix_net, line);
    g_suid_power_root_resp_ok = 0;
    }
  else if (!strcmp(line,
  "cloonsuid_resp_suidroot_ok")) 
    {
    g_suid_power_root_resp_ok = 1;
    }
  else if (sscanf(line,
  "cloonsuid_resp_launch_vm_ok name=%s", name) == 1)
    {
    g_qemu_vm_end(name);
    }
  else if (sscanf(line,
  "cloonsuid_resp_launch_vm_ko name=%s", name) == 1)
    {
    KERR("suid_power reports bad vm start for: %s", name);
    g_qemu_vm_end(name);
    }
  else if (sscanf(line,
  "cloonsuid_resp_pid_ok vm_id=%d pid=%d",
           &vm_id, &pid) == 2)
    {
    if ((vm_id < 0) || (vm_id >= MAX_VM))
      KERR("ERROR %s", line);
    else
      g_tab_pid[vm_id] = pid;
    }
  else if (sscanf(line,
  "cloonsuid_resp_pid_ko vm_id=%d", &vm_id) == 1)
    {
    if ((vm_id < 0) || (vm_id >= MAX_VM))
      KERR("ERROR %s", line);
    else
      g_tab_pid[vm_id] = 0;
    }
  else if (!strncmp(line,
  "cloonsuid_crun", strlen("cloonsuid_crun")))
    crun_sigdiag_resp(g_llid, line);
  else 
    KERR("ERROR suid_power: %s %s", g_cloonix_net, line);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int suid_power_get_topo_info_phy(t_topo_info_phy **phy)
{
  *phy = g_topo_info_phy;
  return g_nb_phy;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int suid_power_info_phy_exists(char *name)
{
  int i, result = 0;
  for (i=0; i<g_nb_phy; i++)
    {
    if (!strcmp(g_topo_info_phy[i].name, name))
      result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int suid_power_get_topo_bridges(t_topo_bridges **bridges)
{
  *bridges = g_brgs;
  return g_nb_brgs;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_first_start(void)
{
  suid_power_start();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_launch_vm(t_vm *vm, t_qemu_vm_end end)
{
  char *msg;
  g_qemu_vm_end = end;
  msg = linearize(vm);
  hop_event_hook(g_llid, FLAG_HOP_SIGDIAG, msg);
  rpct_send_sigdiag_msg(g_llid, type_hop_suid_power, msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int suid_power_get_pid(int vm_id)
{
  if ((vm_id < 0) || (vm_id >= MAX_VM))
    KOUT(" ");
  return g_tab_pid[vm_id];
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_kill_vm(int vm_id)
{
  char req[MAX_PATH_LEN];
  if ((g_llid) && (msg_exist_channel(g_llid)))
    {
    memset(req, 0, MAX_PATH_LEN);
    snprintf(req, MAX_PATH_LEN-1, "cloonsuid_req_vm_kill vm_id: %d", vm_id);
    hop_event_hook(g_llid, FLAG_HOP_SIGDIAG, req);
    rpct_send_sigdiag_msg(g_llid, type_hop_suid_power, req);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_kill_pid(int pid)
{
  char req[MAX_PATH_LEN];
  if ((g_llid) && (msg_exist_channel(g_llid)))
    {
    memset(req, 0, MAX_PATH_LEN);
    snprintf(req, MAX_PATH_LEN-1, "cloonsuid_req_pid_kill pid: %d", pid);
    hop_event_hook(g_llid, FLAG_HOP_SIGDIAG, req);
    rpct_send_sigdiag_msg(g_llid, type_hop_suid_power, req);
    }
  else
    KERR("ERROR suid_power_kill_pid");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_ifup_phy(char *phy)
{
  char req[MAX_PATH_LEN];
  if ((g_llid) && (msg_exist_channel(g_llid)))
    {
    memset(req, 0, MAX_PATH_LEN);
    snprintf(req, MAX_PATH_LEN-1, "cloonsuid_req_ifup phy: %s", phy);
    hop_event_hook(g_llid, FLAG_HOP_SIGDIAG, req);
    rpct_send_sigdiag_msg(g_llid, type_hop_suid_power, req);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_ifdown_phy(char *phy)
{
  char req[MAX_PATH_LEN];
  if ((g_llid) && (msg_exist_channel(g_llid)))
    {
    memset(req, 0, MAX_PATH_LEN);
    snprintf(req, MAX_PATH_LEN-1, "cloonsuid_req_ifdown phy: %s", phy);
    hop_event_hook(g_llid, FLAG_HOP_SIGDIAG, req);
    rpct_send_sigdiag_msg(g_llid, type_hop_suid_power, req);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int suid_power_pid(void)
{
  return (g_suid_power_pid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_llid_closed(int llid, int from_clone)
{
  if ((!from_clone) && (llid == g_llid))
    {
    if (g_destroy_req == 0)
      KERR("ERROR UNEXPECTED SUID_POWER DISCONNECTION %d", llid);
    g_llid = 0;
    if (g_suid_power_pid)
      kill(g_suid_power_pid, SIGKILL);
    g_suid_power_pid = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_fork_closed(void)
{
  if ((g_llid) && (msg_exist_channel(g_llid)))
    {
    msg_delete_channel(g_llid);
    g_llid = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int suid_power_create_cnt(int cli_llid, int cli_tid, int vm_id,
                                t_topo_cnt *cnt, char *err)
{
  int result = -1;
  memset(err, 0, MAX_PATH_LEN);
  if ((g_llid) && (msg_exist_channel(g_llid)))
    {
    result = cnt_create(g_llid, cli_llid, cli_tid, vm_id, cnt, err);
    }
  else
    {
    snprintf(err, MAX_PATH_LEN-1, 
    "ERROR Bad Connection suid_power, %s create fail", cnt->name);  
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_delete_cnt(int cli_llid, int cli_tid, char *name)
{
  cnt_delete(g_llid, cli_llid, cli_tid, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int suid_power_delete_cnt_all(void)
{
  return (cnt_delete_all(g_llid));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_kill(void *data)
{
  g_llid = 0;
  if (g_suid_power_pid)
    kill(g_suid_power_pid, SIGKILL);
  g_suid_power_pid = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_self_kill(void)
{
  g_destroy_req = 1;
  rpct_send_kil_req(g_llid, 0);
  clownix_timeout_add(50, timer_kill, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void suid_power_init(void)
{
  char *bin_path;
  char *root_path;
  char *cloonix_net;
  memset(g_tab_pid, 0, MAX_VM * sizeof(int));
  g_destroy_req = 0;
  g_suid_power_pid = 0;
  g_nb_pid_resp = 0;
  g_nb_pid_resp_warning = 0;
  g_llid = 0;
  g_first_ever_start = 0;
  g_suid_power_root_resp_ok = 0;
  g_nb_phy = 0;
  sprintf(g_conf_rank, "%d", get_conf_rank());
  memset(g_topo_info_phy, 0, MAX_PHY * sizeof(t_topo_info_phy));
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_bin_suid, 0, MAX_PATH_LEN);
  memset(g_cloonix_net, 0, MAX_NAME_LEN);
  memset(g_sock_path, 0, MAX_NAME_LEN);
  memset(g_bin_dir, 0, MAX_NAME_LEN);
  root_path = cfg_get_root_work();
  bin_path = utils_get_suid_power_bin_path();
  cloonix_net = cfg_get_cloonix_name();
  strncpy(g_bin_dir, cfg_get_bin_dir(), MAX_NAME_LEN-1);
  strncpy(g_cloonix_net, cloonix_net, MAX_NAME_LEN-1);
  strncpy(g_bin_suid, bin_path, MAX_PATH_LEN-1);
  strncpy(g_root_path, root_path, MAX_PATH_LEN-1);
  snprintf(g_sock_path,MAX_PATH_LEN-1,"%s/%s",root_path,SUID_POWER_SOCK_DIR);
}
/*--------------------------------------------------------------------------*/

