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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "llid_trace.h"
#include "machine_create.h"
#include "file_read_write.h"
#include "dpdk_ovs.h"
#include "dpdk_kvm.h"
#include "dpdk_msg.h"
#include "fmt_diag.h"
#include "dpdk_nat.h"
#include "dpdk_d2d.h"
#include "dpdk_xyx.h"
#include "dpdk_a2b.h"
#include "dpdk_phy.h"
#include "dpdk_tap.h"
#include "qmp.h"
#include "system_callers.h"
#include "stats_counters.h"

enum{
  msg_type_diag = 1,
  msg_type_pid,
};


/****************************************************************************/
typedef struct t_ovs
{
  char name[MAX_NAME_LEN];
  int clone_start_pid;
  int clone_start_pid_just_done;
  int pid;
  int ovsdb_pid;
  int ovs_pid;
  int ovs_pid_ready;
  int getsuidroot;
  int open_ovsdb;
  int open_ovsdb_wait;
  int open_ovsdb_ok;
  int open_ovs;
  int open_ovs_wait;
  int open_ovs_ok;
  int llid;
  int periodic_count;
  int unanswered_pid_req;
  int connect_try_count;
  int destroy_requested;
  int nb_resp_ovs_ko;
  int watchdog_protect;
  int global_pid_req_watch;
  int daemon_done_count;
  int daemon_done;
  int last_msg_req_destroy_sent;
} t_ovs;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_arg_ovsx
{
  char bin_path[MAX_PATH_LEN];
  char net[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char sock[MAX_PATH_LEN];
  char ovsx_bin[MAX_PATH_LEN];
  char dpdk_db_dir[MAX_PATH_LEN];
} t_arg_ovsx;
/*--------------------------------------------------------------------------*/

static uint32_t g_lcore_mask;
static uint32_t g_socket_mem;
static uint32_t g_cpu_mask;
static t_ovs *g_head_ovs;


/****************************************************************************/
static int connect_ovs_try(t_ovs *mu)
{
  char *sock = utils_get_dpdk_ovs_path(mu->name);
  int llid = string_client_unix(sock, uml_clownix_switch_error_cb,
                                uml_clownix_switch_rx_cb, "ovsdb");
  if (llid)
    {
    if (hop_event_alloc(llid, type_hop_ovsdb, mu->name, 0))
      KERR("%s", mu->name);
    llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_ovsdb);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int try_send_msg_ovs(t_ovs *cur, int msg_type, int tid, char *msg)
{
  int result = -1;
  if (cur->llid)
    {
    if (msg_exist_channel(cur->llid))
      {
      if (msg_type == msg_type_diag)
        {
        hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
        rpct_send_sigdiag_msg(cur->llid, tid, msg);
        result = 0;
        }
      else if (msg_type == msg_type_pid)
        {
        rpct_send_pid_req(cur->llid, tid, msg, 0);
        result = 0;
        }
      else
        KERR("ERROR DROP MSG OVS %s", msg);
      }
    else
      KERR("ERROR DROP MSG OVS %s", msg);
    }
  else
    KERR("ERROR DROP MSG OVS %s", msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void del_all_depends(void)
{
  if (dpdk_nat_get_qty())
    dpdk_nat_end_ovs();
  if (dpdk_a2b_get_qty())
    dpdk_a2b_end_ovs();
  if (dpdk_d2d_get_qty())
    dpdk_d2d_end_ovs();
  if (dpdk_xyx_get_qty())
    dpdk_xyx_end_ovs();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_destroy_requested(t_ovs *cur, int val)
{
  del_all_depends();
  cur->destroy_requested = val;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovs *ovs_alloc(char *name)
{
  t_ovs *cur;
  cur = (t_ovs *) clownix_malloc(sizeof(t_ovs), 9);
  memset(cur, 0, sizeof(t_ovs));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  g_head_ovs = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovs *ovs_find_with_llid(int llid)
{
  t_ovs *cur = g_head_ovs;
  if (cur && (cur->llid != llid))
    {
    KERR(" ");
    cur = NULL;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovs *ovs_find(void)
{
  t_ovs *cur = g_head_ovs;
  return (cur);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void ovs_free(char *name)
{
  t_ovs *cur = g_head_ovs;
  if (cur)
    {
    if (cur->ovsdb_pid > 0)
      {
      if (!kill(cur->ovsdb_pid, SIGKILL))
        KERR("ERROR: ovsdb was alive");
      }
    if (cur->ovs_pid > 0)
      {
      if (!kill(cur->ovs_pid, SIGKILL))
        KERR("ERROR: ovs was alive");
      }
    g_head_ovs = NULL;
    clownix_free(cur, __FUNCTION__);
    }
  else
    KERR("Free bad");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char **make_argv(t_arg_ovsx *arg_ovsx)
{
  static char *argv[8];
  memset(argv, 0, 8*sizeof(char *));
  argv[0] = arg_ovsx->bin_path;
  argv[1] = arg_ovsx->net;
  argv[2] = arg_ovsx->name;
  argv[3] = arg_ovsx->sock;
  argv[4] = arg_ovsx->ovsx_bin;
  argv[5] = arg_ovsx->dpdk_db_dir;
  return argv;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void ovs_watchdog(void *data)
{
  t_arg_ovsx *mu = (t_arg_ovsx *) data;
  t_ovs *cur = ovs_find();
  if (cur)
    cur->watchdog_protect = 0;
  if (cur && ((!cur->llid) || 
              (!cur->pid) || 
              (!cur->periodic_count)))
    {
    KERR("ERROR OVS %s TIMEOUT %s %d %d %d", cur->name, mu->name,
                                   cur->llid, cur->pid, cur->periodic_count);
    set_destroy_requested(cur, 1);
    }
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_birth(void *data)
{
  t_arg_ovsx *arg_ovsx = (t_arg_ovsx *) data;
  char **argv = make_argv(arg_ovsx);
  execv(arg_ovsx->bin_path, argv);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_last_msg_req_destroy(t_ovs *cur)
{
  try_send_msg_ovs(cur, msg_type_diag, 0, "cloonixovs_req_destroy");
  cur->last_msg_req_destroy_sent = 1;
  cur->destroy_requested = 1;
}

/****************************************************************************/
static void check_on_destroy_requested(t_ovs *cur)
{
  char *sock;
  if (cur->destroy_requested > 2)
    {
    cur->destroy_requested -= 1;
    }
  else if (cur->destroy_requested == 2)
    {
    send_last_msg_req_destroy(cur);
    cur->destroy_requested -= 1;
    }
  else if (cur->destroy_requested == 1)
    { 
    event_print("End OpenVSwitch %s", cur->name);
    sock = utils_get_dpdk_ovs_path(cur->name);
    if (cur->llid)
      llid_trace_free(cur->llid, 0, __FUNCTION__);
    if (cur->ovsdb_pid > 0)
      {
      if (!kill(cur->ovsdb_pid, SIGKILL))
        KERR("ERROR: ovsdb was alive");
      }
    if (cur->ovs_pid > 0)
      {
      if (!kill(cur->ovs_pid, SIGKILL))
        KERR("ERROR: ovs was alive");
      }
    unlink(sock);
    ovs_free(cur->name);
    }
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int create_ovs_drv_process(char *name)
{
  int pid = 0;
  char **argv;
  char bin_path[MAX_PATH_LEN];
  char *net = cfg_get_cloonix_name();
  char *sock = utils_get_dpdk_ovs_path(name);
  char *ovsx_bin = utils_get_dpdk_ovs_bin_dir();
  char *dpdk_db_dir = utils_get_dpdk_ovs_db_dir();
  t_arg_ovsx *arg_ovsx = (t_arg_ovsx *) clownix_malloc(sizeof(t_arg_ovsx), 10);
  memset(arg_ovsx, 0, sizeof(t_arg_ovsx));
  memset(bin_path, 0, MAX_PATH_LEN);
  sprintf(bin_path, "%s/server/ovs_drv/cloonix_ovs_drv", cfg_get_bin_dir());
  strncpy(arg_ovsx->bin_path, bin_path, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->net, net, MAX_NAME_LEN-1);
  strncpy(arg_ovsx->name, name, MAX_NAME_LEN-1);
  strncpy(arg_ovsx->sock, sock, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->ovsx_bin, ovsx_bin, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->dpdk_db_dir, dpdk_db_dir, MAX_PATH_LEN-1);
  if (file_exists(sock, F_OK))
    unlink(sock);
  if (!file_exists(bin_path, X_OK))
    KERR("ERROR %s Does not exist or not exec", bin_path);
  if (!file_exists(ovsx_bin, X_OK))
    KERR("ERROR %s Does not exist or not exec", bin_path);
  argv = make_argv(arg_ovsx);
  utils_send_creation_info("ovsx", argv);
  pid = pid_clone_launch(ovs_birth, process_demonized, NULL,
                         arg_ovsx, arg_ovsx, NULL, name, -1, 1);
  arg_ovsx = (t_arg_ovsx *) clownix_malloc(sizeof(t_arg_ovsx), 11);
  memset(arg_ovsx, 0, sizeof(t_arg_ovsx));
  strncpy(arg_ovsx->bin_path, bin_path, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->net, net, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->name, name, MAX_NAME_LEN-1);
  strncpy(arg_ovsx->sock, sock, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->ovsx_bin, ovsx_bin, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->dpdk_db_dir, dpdk_db_dir, MAX_PATH_LEN-1);
  clownix_timeout_add(2000, ovs_watchdog, (void *) arg_ovsx, NULL, NULL);
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dpdk_ovs_start_openvswitch_if_not_done(void)
{
  t_ovs *cur = g_head_ovs;
  if (cur == NULL)
    {
    event_print("Start OpenVSwitch");
    cur = ovs_alloc("ovsdb");
    cur->watchdog_protect = 1;
    cur->clone_start_pid = create_ovs_drv_process("ovsdb");
    cur->clone_start_pid_just_done = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_ovs_beat(void *data)
{
  t_ovs *cur = g_head_ovs;
  int type_hop_tid = type_hop_ovsdb;
  char msg[MAX_PATH_LEN];
  if ((cur) && (cur->last_msg_req_destroy_sent == 0))
    {
    if (!cur->clone_start_pid)
      KERR("ERROR");
    else if (cur->clone_start_pid_just_done == 1)
      cur->clone_start_pid_just_done = 0;
    else if (cur->connect_try_count == 0)
      cur->connect_try_count = 1;
    else if (cur->llid == 0)
      {
      cur->llid = connect_ovs_try(cur);
      cur->connect_try_count += 1;
      if (cur->connect_try_count == 16)
        {
        KERR("OVS %s NOT LISTENING destroy_request", cur->name);
        set_destroy_requested(cur, 1);
        }
      }
    else if (cur->pid == 0)
      {
      if (!strlen(cur->name))
        KOUT(" ");
      try_send_msg_ovs(cur, msg_type_pid, type_hop_tid, cur->name);
      }
    else if (cur->getsuidroot == 0)
      {
      try_send_msg_ovs(cur, msg_type_diag, 0, "cloonixovs_req_suidroot");
      }

    else if (cur->open_ovsdb == 0)
      {
      cur->open_ovsdb = 1;
      snprintf(msg, MAX_PATH_LEN-1,
      "cloonixovs_req_ovsdb lcore_mask=0x%x socket_mem=%d cpu_mask=0x%x",
      g_lcore_mask, g_socket_mem, g_cpu_mask);
      try_send_msg_ovs(cur, msg_type_diag, 0, msg);
      }
    else if (cur->open_ovsdb_ok == 0)
      {
      cur->open_ovsdb_wait += 1;
      if (cur->open_ovsdb_wait > 120)
        {
        KERR("ERROR OVSDB %s NOT RESPONDING", cur->name);
        set_destroy_requested(cur, 1);
        }
      }

    else if (cur->open_ovs == 0)
      {
      cur->open_ovs = 1;
      snprintf(msg, MAX_PATH_LEN-1,
      "cloonixovs_req_ovs lcore_mask=0x%x socket_mem=%d cpu_mask=0x%x",
      g_lcore_mask, g_socket_mem, g_cpu_mask);
      try_send_msg_ovs(cur, msg_type_diag, 0, msg);
      }
    else if (cur->open_ovs_ok == 0)
      {
      cur->open_ovs_wait += 1;
      if (cur->open_ovs_wait > 50)
        {
        KERR("ERROR OVS %s NOT RESPONDING", cur->name);
        set_destroy_requested(cur, 1);
        }
      }

    else
      {
      cur->periodic_count += 1;
      if (cur->periodic_count >= 10)
        {
        if (!strlen(cur->name))
          KOUT(" ");
        try_send_msg_ovs(cur, msg_type_pid, type_hop_tid, cur->name);
        cur->periodic_count = 5;
        cur->unanswered_pid_req += 1;
        if (cur->unanswered_pid_req > 50)
          {
          KERR("ERROR OVS %s NOT RESPONDING", cur->name);
          set_destroy_requested(cur, 1);
          }
        }
      }
    }
  if (cur)
    {
    check_on_destroy_requested(cur);
    cur->global_pid_req_watch += 1;
    if (cur->global_pid_req_watch > 150)
      {
      KERR("ERROR OVS %s NOT GIVING PID", cur->name);
      set_destroy_requested(cur, 1);
      }
    }
  clownix_timeout_add(50, timer_ovs_beat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_ovs_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_ovs *cur = g_head_ovs;
  int i, result = 0;
  if (cur)
    {
    if (cur->pid)
      result++;
    if (cur->ovs_pid > 0)
      result++;
    if (cur->ovsdb_pid > 0)
      result++;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid),25);
    memset(glob_lst, 0, result*sizeof(t_lst_pid));
    cur = g_head_ovs;
    i = 0;
    if (cur->pid)
      {
      strncpy(glob_lst[i].name, cur->name, MAX_NAME_LEN-1);
      glob_lst[i].pid = cur->pid;
      i++;
      }
    if (cur->ovsdb_pid > 0)
      {
      strcpy(glob_lst[i].name, "ovsdb-server");
      glob_lst[i].pid = cur->ovsdb_pid;
      i++;
      }
    if (cur->ovs_pid > 0)
      {
      strcpy(glob_lst[i].name, "ovs-vswitchd");
      glob_lst[i].pid = cur->ovs_pid;
      i++;
      }
    if (i != result)
      KOUT("%d %d", i, result);
    }
  *lst_pid = glob_lst;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_ovs_pid_resp(int llid, char *name, int toppid, int pid)
{
  t_ovs *cur = ovs_find_with_llid(llid);
  if (!cur)
    KERR("%s %d %d", name, toppid, pid);
  else
    {
    if (cur->daemon_done == 0)
      {
      cur->daemon_done_count += 1;;
      if (cur->daemon_done_count == 3)
        {
        printf("\n    OPENVSWITCH STARTED\n\n");
        cur->daemon_done = 1;
        }
      }
    cur->global_pid_req_watch = 0;
    cur->unanswered_pid_req = 0;
    if (strcmp(name, cur->name))
      KERR("%s %s", name, cur->name);
    if (cur->pid == 0)
      {
      cur->pid = pid;
      }
    else
      {
      if (cur->pid != pid)
        {
        if (cur->ovsdb_pid == 0)
          cur->ovsdb_pid = pid;
        if (cur->ovs_pid == 0)
          cur->ovs_pid = toppid;
        if ((cur->ovs_pid > 0) && (cur->ovsdb_pid > 0))
          cur->ovs_pid_ready = 1;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_ovs_rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  t_ovs *cur = ovs_find_with_llid(llid);
  if (!cur)
    KERR("%s", line);
  else
    {
    if (!strcmp(line, "cloonixovs_resp_suidroot_ok"))
      {
      cur->getsuidroot = 1;
      event_print("Start OpenVSwitch %s getsuidroot = 1", cur->name);
      }
    else if (!strcmp(line, "cloonixovs_resp_suidroot_ko"))
      {
      KERR("ERROR: cloonix_ovs is not suid root");
      KERR("%s", "\"sudo chmod u+s /usr/local/bin/cloonix"
                 "/server/dpdk/bin/ovs-vswitchd\"");
      set_destroy_requested(cur, 1);
      }
    else if (!strcmp(line, "cloonixovs_resp_ovsdb_ko"))
      {
      KERR("ERROR cloonixovs_resp_ovsdb_ko");
      set_destroy_requested(cur, 1);
      }
    else if (!strcmp(line, "cloonixovs_resp_ovsdb_ok"))
      {
      event_print("Start OpenVSwitch %s open_ovsdb = 1", cur->name);
      cur->open_ovsdb_ok = 1;
      }
    else if (!strcmp(line, "cloonixovs_resp_ovs_ko"))
      {
      KERR("ERROR ovs-vswitchd FAIL huge page memory?");
      set_destroy_requested(cur, 1);
      }
    else if (!strcmp(line, "cloonixovs_resp_ovs_ok"))
      {
      event_print("Start OpenVSwitch %s open_ovs = 1", cur->name);
      cur->open_ovs_ok = 1;
      }
    else
      fmt_rx_rpct_recv_sigdiag_msg(llid, tid, line);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_find_with_llid(int llid)
{
  int result = 0;
  t_ovs *mu = ovs_find_with_llid(llid);
  if (mu)
    {
    result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_try_send_sigdiag_msg(int tid, char *cmd)
{
  int result = -1;
  t_ovs *ovsdb = ovs_find();
  if ((ovsdb) && (ovsdb->ovs_pid_ready))
    result = try_send_msg_ovs(ovsdb, msg_type_diag, tid, cmd);
  else
    KERR("ERROR MSG DROPPED: %s", cmd);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *dpdk_ovs_format_ethd_eths(t_vm *vm, int eth)
{
  static char cmd[MAX_PATH_LEN*3];
  int i, first_dpdk = 0xFFFF, len = 0;
  memset(cmd, 0, MAX_PATH_LEN*3);
  for (i = 0; i < vm->kvm.nb_tot_eth; i++)
    {
    if ((vm->kvm.eth_table[i].endp_type == endp_type_ethd) ||
        (vm->kvm.eth_table[i].endp_type == endp_type_eths))
      {
      first_dpdk = i;
      break;
      }
    }
  if (first_dpdk == 0xFFFF)
    KOUT("%d", eth);
  if (eth == first_dpdk)
    {
    len += sprintf(cmd+len, " -object memory-backend-file,id=mem0,"
                            "size=%dM,share=on,mem-path=%s"
                            " -numa node,memdev=mem0 -mem-prealloc",
                            vm->kvm.mem, cfg_get_root_work());
    } 
  return cmd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *dpdk_ovs_format_ethd(t_vm *vm, int eth)
{
  static char cmd[MAX_PATH_LEN*3];
  int len = 0;
  char *mc = vm->kvm.eth_table[eth].mac_addr;
  char *endp_path = utils_get_dpdk_endp_path(vm->kvm.name, eth);
  memset(cmd, 0, MAX_PATH_LEN*3);
  len += sprintf(cmd+len, " -chardev socket,id=chard%d,path=%s,server=on",
                          eth, endp_path);
  len += sprintf(cmd+len, " -netdev type=vhost-user,id=net%d,"
                          "chardev=chard%d,queues=%d",eth,eth,MQ_QUEUES);
  len += sprintf(cmd+len, " -device virtio-net-pci,netdev=net%d,"
                          "mac=%02X:%02X:%02X:%02X:%02X:%02X,"
                          "bus=pci.0,addr=0x%x,csum=off,guest_csum=off,"
                          "guest_tso4=off,guest_tso6=off,guest_ecn=off,"
                          "mq=on,vectors=%d",
                          eth, mc[0]&0xFF, mc[1]&0xFF, mc[2]&0xFF,
                          mc[3]&0xFF, mc[4]&0xFF, mc[5]&0xFF, eth+5,
                          MQ_VECTORS);
  return cmd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *dpdk_ovs_format_eths(t_vm *vm, int eth)
{
  static char net_cmd[MAX_PATH_LEN*3];
  int len = 0;
  char *mc = vm->kvm.eth_table[eth].mac_addr;
  char *endp_path = utils_get_dpdk_endp_path(vm->kvm.name, eth);
  memset(net_cmd, 0, MAX_PATH_LEN*3);
  len += sprintf(net_cmd+len, " -chardev socket,id=chard%d,path=%s,server=on",
                              eth, endp_path);
  len += sprintf(net_cmd+len, " -netdev type=vhost-user,id=net%d,"
                              "chardev=chard%d", eth, eth);
  len += sprintf(net_cmd+len, " -device virtio-net-pci,netdev=net%d,"
                              "mac=%02X:%02X:%02X:%02X:%02X:%02X,"
                              "bus=pci.0,addr=0x%x,csum=off,guest_csum=off,"
                              "guest_tso4=off,guest_tso6=off,guest_ecn=off",
                              eth, mc[0]&0xFF, mc[1]&0xFF, mc[2]&0xFF,
                              mc[3]&0xFF, mc[4]&0xFF, mc[5]&0xFF, eth+5);
  return net_cmd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *dpdk_ovs_format_ethv(t_vm *vm, int eth, char *ifname)
{
  static char cmd[MAX_PATH_LEN*3];
  int len = 0;
  char *mc = vm->kvm.eth_table[eth].mac_addr;
  memset(cmd, 0, MAX_PATH_LEN*3);
  len += sprintf(cmd+len," -device virtio-net-pci,netdev=nvhost%d", eth);
  len += sprintf(cmd+len,",mac=%02X:%02X:%02X:%02X:%02X:%02X",
                         mc[0] & 0xFF, mc[1] & 0xFF, mc[2] & 0xFF,
                         mc[3] & 0xFF, mc[4] & 0xFF, mc[5] & 0xFF);
  len += sprintf(cmd+len,",bus=pci.0,addr=0x%x", eth+5);
  len += sprintf(cmd+len,",mq=on,vectors=%d", MQ_VECTORS);
  len += sprintf(cmd+len," -netdev type=tap,id=nvhost%d,vhost=on,", eth);
  len += sprintf(cmd+len,"vhostforce=on,queues=%d,", MQ_QUEUES);
  len += sprintf(cmd+len,"ifname=%s,script=no,downscript=no", ifname);
  return cmd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_still_present(void)
{
  int result = 0;
  if (g_head_ovs != NULL)
    {
    if (g_head_ovs->destroy_requested == 0)
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_ovs_client_destruct(void)
{
  t_ovs *cur = g_head_ovs;
  if (cur)
    del_all_depends();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_ovs_cnf(uint32_t lcore, uint32_t mem, uint32_t cpu)
{
  KERR("Config ovs dpdk: %X  %d  %X", lcore, mem, cpu);
  g_lcore_mask = lcore;
  g_socket_mem = mem;
  g_cpu_mask   = cpu;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_start_openvswitch(void *data)
{
  dpdk_ovs_start_openvswitch_if_not_done();
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int get_daemon_done(void)
{
  int result = 0;
  t_ovs *cur = g_head_ovs;
  if (cur)
    {
    result = cur->daemon_done;
    } 
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_ovs_destroy(void)
{
  t_ovs *cur = g_head_ovs;
  if (cur)
    send_last_msg_req_destroy(cur);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_ovs_init(uint32_t lcore, uint32_t mem, uint32_t cpu)
{
  g_head_ovs = NULL;
  clownix_timeout_add(5, timer_ovs_beat, NULL, NULL, NULL);
  clownix_timeout_add(10, timer_start_openvswitch, NULL, NULL, NULL);
  dpdk_tap_init();
  dpdk_phy_init();
  dpdk_kvm_init();
  dpdk_msg_init();
  dpdk_nat_init();
  dpdk_xyx_init();
  dpdk_a2b_init();
  dpdk_d2d_init();
  g_lcore_mask = lcore;
  g_socket_mem = mem;
  g_cpu_mask   = cpu;
}
/*--------------------------------------------------------------------------*/

