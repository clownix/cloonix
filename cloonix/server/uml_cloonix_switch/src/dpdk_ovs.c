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
#include "endp_mngt.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "llid_trace.h"
#include "machine_create.h"
#include "file_read_write.h"
#include "dpdk_ovs.h"
#include "dpdk_dyn.h"
#include "dpdk_msg.h"
#include "fmt_diag.h"
#include "dpdk_tap.h"
#include "dpdk_snf.h"
#include "dpdk_nat.h"
#include "dpdk_d2d.h"
#include "dpdk_a2b.h"
#include "qmp.h"
#include "system_callers.h"
#include "stats_counters.h"

enum{
  msg_type_diag = 1,
  msg_type_pid,
};

char *get_user(void);

typedef struct t_dpdk_eth
{
  int pkt_tx;
  int pkt_rx;
  int byte_tx;
  int byte_rx;
  int ms;
} t_dpdk_eth;

/****************************************************************************/
typedef struct t_dpdk_vm
{
  char name[MAX_NAME_LEN];
  int  nb_tot_eth;
  t_eth_table eth_table[MAX_SOCK_VM+MAX_DPDK_VM+MAX_WLAN_VM];
  int  dyn_vm_add_delay;
  int  dyn_vm_add_eth_todo;
  int  dyn_vm_add_done;
  int  dyn_vm_add_done_acked;
  int  dyn_vm_add_done_notacked;
  int  destroy_done;
  t_dpdk_eth eth[MAX_DPDK_VM];
  struct t_dpdk_vm *prev;
  struct t_dpdk_vm *next;
} t_dpdk_vm;
/*--------------------------------------------------------------------------*/

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
  int destroy_msg_sent;
  int nb_resp_ovs_ko;
  int watchdog_protect;
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
static t_dpdk_vm *g_head_vm;
static t_ovs *g_head_ovs;
static uint32_t g_restart_hysteresis;

/****************************************************************************/
static void erase_dpdk_qemu_path(char *name, int num_eth)
{
  char *endp_path;
  endp_path = utils_get_dpdk_endp_path(name, num_eth);
  unlink(endp_path);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void test_and_end_ovs(void)
{
  t_ovs *cur = g_head_ovs;
  int nb_tap = dpdk_tap_get_qty();
  int nb_snf = dpdk_snf_get_qty();
  int nb_nat = dpdk_nat_get_qty();
  int nb_a2b = dpdk_a2b_get_qty();
  int nb_d2d = dpdk_d2d_get_qty();
  if ((g_head_vm == NULL) &&
      (nb_tap == 0) &&
      (nb_nat == 0) &&
      (nb_a2b == 0) &&
      (nb_d2d == 0) &&
      (nb_snf == 0))
    {
    if (!dpdk_dyn_is_all_empty())
      KERR(" ");
    if (cur)
      {
      if (cur->watchdog_protect == 0)
        {
        if (cur->destroy_requested == 0)
          {
          cur->destroy_requested = 4;
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dpdk_vm *vm_find(char *name)
{
  t_dpdk_vm *vm = g_head_vm;
  while(name[0] && vm && strcmp(vm->name, name))
    vm = vm->next;
  return vm;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vm_alloc(char *name, int nb_tot_eth, t_eth_table *eth_tab)
{
  t_dpdk_vm *vm = vm_find(name);
  if (vm)
    KERR("%s", name);
  else
    {
    vm = (t_dpdk_vm *) clownix_malloc(sizeof(t_dpdk_vm), 8);
    memset(vm, 0, sizeof(t_dpdk_vm));
    strncpy(vm->name, name, MAX_NAME_LEN-1);
    vm->nb_tot_eth = nb_tot_eth;
    memcpy(vm->eth_table, eth_tab, nb_tot_eth * sizeof(t_eth_table));
    if (g_head_vm)
      g_head_vm->prev = vm;
    vm->next = g_head_vm;
    g_head_vm = vm;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vm_free(char *name)
{
  int i;
  t_dpdk_vm *vm = vm_find(name);
  if (!vm)
    KERR("%s", name);
  else
    {
    for (i=0; i<vm->nb_tot_eth; i++)
      {
      if (vm->eth_table[i].eth_type == eth_type_dpdk)
        erase_dpdk_qemu_path(name, i);
      }
    if (vm->prev)
      vm->prev->next = vm->next;
    if (vm->next)
      vm->next->prev = vm->prev;
    if (vm == g_head_vm)
      g_head_vm = vm->next;
    clownix_free(vm, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

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
        hop_event_hook(cur->llid, FLAG_HOP_DIAG, msg);
        rpct_send_diag_msg(NULL, cur->llid, tid, msg);
        result = 0;
        }
      else if (msg_type == msg_type_pid)
        {
        rpct_send_pid_req(NULL, cur->llid, tid, msg, 0);
        result = 0;
        }
      else
        KERR("%s", msg);
      }
    }
  return result;
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
static void timer_restart_hysteresis(void *data)
{  
  g_restart_hysteresis = 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void ovs_free(char *name)
{
  t_ovs *cur = g_head_ovs;
  if (g_restart_hysteresis != 1)
    KERR("Bad g_restart_hysteresis");
  if (cur)
    {
    if (cur->ovsdb_pid > 0)
      {
      if (!kill(cur->ovsdb_pid, SIGKILL))
        KERR("ERR: ovsdb was alive");
      }
    if (cur->ovs_pid > 0)
      {
      if (!kill(cur->ovs_pid, SIGKILL))
        KERR("ERR: ovs was alive");
      }
    g_head_ovs = NULL;
    clownix_free(cur, __FUNCTION__);
    clownix_timeout_add(50, timer_restart_hysteresis, NULL, NULL, NULL);
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
static void ovs_death(void *data, int status, char *name)
{
  t_arg_ovsx *mu = (t_arg_ovsx *) data;
  t_ovs *cur = ovs_find();
  if (strcmp(name, mu->name))
    KOUT("%s %s", name, mu->name);
  if (cur)
    {
    if (cur->destroy_msg_sent == 0)
      KERR("OVS PREMATURE DEATH %s", mu->name);
    cur->destroy_requested = 1;
    }
  else
    KERR("OVS DEATH %s", mu->name);
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
    cur->destroy_requested = 1;
    KERR("OVS %s TIMEOUT %s %d %d %d", cur->name, mu->name,
                                   cur->llid, cur->pid, cur->periodic_count);
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
static void check_on_destroy_requested(t_ovs *cur)
{
  char *sock;
  t_dpdk_vm *nvm, *vm;
  test_and_end_ovs();
  vm = g_head_vm;
  if (cur->destroy_requested == 4)
    {
    cur->destroy_msg_sent = 1;
    try_send_msg_ovs(cur, msg_type_diag, 0, "cloonixovs_req_destroy");
    }
  if (cur->destroy_requested == 1)
    { 
    g_restart_hysteresis = 1;
    while(vm)
      {
      KERR("FAST DESTRUCT %s", vm->name);
      nvm = vm->next;
      machine_death(vm->name, error_death_noovs);
      vm = nvm;
      }
    event_print("End OpenVSwitch %s", cur->name);
    sock = utils_get_dpdk_ovs_path(cur->name);
    dpdk_ovs_urgent_client_destruct();
    if (cur->llid)
      {
      llid_trace_free(cur->llid, 0, __FUNCTION__);
      cur->llid = 0;
      }
    if (cur->ovsdb_pid > 0)
      {
      if (!kill(cur->ovsdb_pid, SIGKILL))
        KERR("ERR: ovsdb was alive");
      cur->ovsdb_pid = 0;
      }
    if (cur->ovs_pid > 0)
      {
      if (!kill(cur->ovs_pid, SIGKILL))
        KERR("ERR: ovs was alive");
      cur->ovs_pid = 0;
      }
    unlink(sock);
    mk_dpdk_ovs_db_dir();
    ovs_free(cur->name);
    }
  else if (cur->destroy_requested > 1)
    {
    cur->destroy_requested -= 1;
    }
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_dpdk_dyn_del_eth(t_dpdk_vm *vm)
{
  int i;
  for (i=0; i<vm->nb_tot_eth; i++)
    {
    if (vm->eth_table[i].eth_type == eth_type_dpdk)
      {
      dpdk_dyn_del_eth(vm->name, i);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int try_connect_eth(t_dpdk_vm *vm)
{
  char *endp_path;
  int i, success, some_more, result = 0;
  char strmac[MAX_NAME_LEN];
  char *mc; 
  success = 0;
  for (i = vm->dyn_vm_add_eth_todo; i < vm->nb_tot_eth; i++)
    {
    if (vm->eth_table[i].eth_type == eth_type_dpdk)
      {
      endp_path = utils_get_dpdk_endp_path(vm->name, i);
      if (!access(endp_path, F_OK))
        {
        success = 1;
        vm->dyn_vm_add_eth_todo = i+1;
        event_print("Add eth %s %d", vm->name, i);
        mc = vm->eth_table[i].mac_addr;
        sprintf(strmac, " mac=%02X:%02X:%02X:%02X:%02X:%02X",
                          mc[0]&0xFF, mc[1]&0xFF, mc[2]&0xFF,
                          mc[3]&0xFF, mc[4]&0xFF, mc[5]&0xFF);
        dpdk_dyn_add_eth(vm->name, i, strmac);
        break;
        }
      }
    }
  if (success)
    {
    some_more = 0;
    for (i = vm->dyn_vm_add_eth_todo; i < vm->nb_tot_eth; i++)
      {
      if (vm->eth_table[i].eth_type == eth_type_dpdk)
        some_more = 1;
      }
    if (some_more == 0)
      {
      event_print("Add eth %s all done", vm->name);
      result  = 1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_vm_add_beat(void *data)
{
  int nb_sock, nb_dpdk, nb_wlan;
  t_vm *kvm;
  t_dpdk_vm *nvm, *vm;
  t_ovs *ovsdb = ovs_find();
  vm = g_head_vm;
  while(vm)
    {
    nvm = vm->next;
    kvm = cfg_get_vm(vm->name);
    if (!kvm)
      {
      event_print("timer !kvm call_dpdk_dyn_del_eth %s", vm->name);
      call_dpdk_dyn_del_eth(vm);
      }
    else if (kvm->vm_to_be_killed == 1)
      {
      event_print("timer to_be_killed call_dpdk_dyn_del_eth %s", vm->name);
      call_dpdk_dyn_del_eth(vm);
      KERR("%s", vm->name);
      }
    else
      {
      utils_get_eth_numbers(kvm->kvm.nb_tot_eth, kvm->kvm.eth_table,
                            &nb_sock, &nb_dpdk, &nb_wlan);
      if (nb_dpdk != 0)
        {
        if (!kvm)
          {
          event_print("timer !kvm call_dpdk_dyn_del_eth %s", vm->name);
          call_dpdk_dyn_del_eth(vm);
          KERR("%s", vm->name);
          }
        else if (kvm->vm_to_be_killed == 1)
          {
          event_print("timer to_be_killed call_dpdk_dyn_del_eth %s", vm->name);
          call_dpdk_dyn_del_eth(vm);
          KERR("%s", vm->name);
          }
        else
          {
          if (kvm && (vm->dyn_vm_add_done == 0))
            {
            if ((ovsdb != NULL) &&
                (ovsdb->ovs_pid_ready == 1) &&
                (ovsdb->periodic_count > 1))
              {
              if (ovsdb->destroy_requested)
                {
                KERR("OVS BEING DESTROYED %s", vm->name);
                machine_death(vm->name, error_death_no_dpdk);
                }
              if (!dpdk_ovs_get_dpdk_usable())
                {
                KERR("0 Hugepages no dpdk for %s", vm->name);
                machine_death(vm->name, error_death_no_dpdk);
                }
              vm->dyn_vm_add_delay += 1;
              if (vm->dyn_vm_add_delay >= 1)
                vm->dyn_vm_add_done = try_connect_eth(vm);
              }
            }
          if (kvm && (vm->dyn_vm_add_done_acked == 0))
            {
            vm->dyn_vm_add_done_notacked += 1;
            if (vm->dyn_vm_add_done_notacked == 400)
              {
              KERR("%s", vm->name);
              machine_death(vm->name, error_death_noovstime);
              }
            }
          }
        }
      }

    vm = nvm;
    }
  clownix_timeout_add(30, timer_vm_add_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_ovs_beat(void *data)
{
  t_ovs *cur = g_head_ovs;
  int type_hop_tid = type_hop_ovsdb;
  char msg[MAX_PATH_LEN];
  if (cur)
    {
    if (!cur->clone_start_pid)
      KERR(" ");
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
        cur->destroy_requested = 1;
        KERR("OVS %s NOT LISTENING destroy_requeste", cur->name);
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
        cur->destroy_requested = 1;
        KERR("OVSDB %s NOT RESPONDING destroy_requested", cur->name);
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
      if (cur->open_ovs_wait > 40)
        {
        cur->destroy_requested = 1;
        KERR("OVS %s NOT RESPONDING destroy_requested", cur->name);
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
        if (cur->unanswered_pid_req > 6)
          {
          cur->destroy_requested = 1;
          KERR("OVS %s NOT RESPONDING destroy_requested", cur->name);
          }
        }
      }
    check_on_destroy_requested(cur);
    }
  clownix_timeout_add(50, timer_ovs_beat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int create_muovs_process(char *name)
{
  int pid = 0;
  char **argv;
  char *bin_path = utils_get_endp_bin_path(endp_type_ovsdb);
  char *net = cfg_get_cloonix_name();
  char *sock = utils_get_dpdk_ovs_path(name);
  char *ovsx_bin = utils_get_dpdk_ovs_bin_dir();
  char *dpdk_db_dir = utils_get_dpdk_ovs_db_dir();
  t_arg_ovsx *arg_ovsx = (t_arg_ovsx *) clownix_malloc(sizeof(t_arg_ovsx), 10);
  memset(arg_ovsx, 0, sizeof(t_arg_ovsx));
  strncpy(arg_ovsx->bin_path, bin_path, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->net, net, MAX_NAME_LEN-1);
  strncpy(arg_ovsx->name, name, MAX_NAME_LEN-1);
  strncpy(arg_ovsx->sock, sock, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->ovsx_bin, ovsx_bin, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->dpdk_db_dir, dpdk_db_dir, MAX_PATH_LEN-1);
  if (file_exists(sock, F_OK))
    unlink(sock);
  if (!file_exists(bin_path, X_OK))
    KERR("%s Does not exist or not exec", bin_path);
  if (!file_exists(ovsx_bin, X_OK))
    KERR("%s Does not exist or not exec", bin_path);
  argv = make_argv(arg_ovsx);
  utils_send_creation_info("ovsx", argv);
  pid = pid_clone_launch(ovs_birth, ovs_death, NULL,
                         arg_ovsx, arg_ovsx, NULL, name, -1, 1);
  arg_ovsx = (t_arg_ovsx *) clownix_malloc(sizeof(t_arg_ovsx), 11);
  memset(arg_ovsx, 0, sizeof(t_arg_ovsx));
  strncpy(arg_ovsx->bin_path, bin_path, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->net, net, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->name, name, MAX_NAME_LEN-1);
  strncpy(arg_ovsx->sock, sock, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->ovsx_bin, ovsx_bin, MAX_PATH_LEN-1);
  strncpy(arg_ovsx->dpdk_db_dir, dpdk_db_dir, MAX_PATH_LEN-1);
  clownix_timeout_add(1000, ovs_watchdog, (void *) arg_ovsx, NULL, NULL);
  return pid;
}
/*--------------------------------------------------------------------------*/

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
    cur->unanswered_pid_req = 0;
    if (strcmp(name, cur->name))
      KERR("%s %s", name, cur->name);
    if (cur->pid == 0)
      {
      if (cur->clone_start_pid != pid)
        KERR("%s %d %d %d", name, toppid, pid, cur->clone_start_pid);
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
void dpdk_ovs_rpct_recv_diag_msg(int llid, int tid, char *line)
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
      KERR(" destroy_requested for %s", cur->name);
      cur->destroy_requested = 1;
      }
    else if (!strcmp(line, "cloonixovs_resp_ovsdb_ko"))
      {
      KERR("cloonixovs_resp_ovsdb_ko");
      cur->destroy_requested = 1;
      }
    else if (!strcmp(line, "cloonixovs_resp_ovsdb_ok"))
      {
      event_print("Start OpenVSwitch %s open_ovsdb = 1", cur->name);
      cur->open_ovsdb_ok = 1;
      }
    else if (!strcmp(line, "cloonixovs_resp_ovs_ko"))
      {
      KERR("cloonixovs_resp_ovs_ko ovs-vswitchd FAIL huge page memory?");
      cur->destroy_requested = 1;
      }
    else if (!strcmp(line, "cloonixovs_resp_ovs_ok"))
      {
      event_print("Start OpenVSwitch %s open_ovs = 1", cur->name);
      cur->open_ovs_ok = 1;
      }
    else
      fmt_rx_rpct_recv_diag_msg(llid, tid, line);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_muovs_ready(void)
{
  int result = 0;
  t_ovs *cur = g_head_ovs;
  if (cur)
    {
    if ((g_restart_hysteresis == 0) &&
        (cur->destroy_requested == 0) &&
        (cur->ovs_pid_ready == 1) &&
        (cur->periodic_count > 3))
      result = 1;
    }
  return result;
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

/*****************************************************************************/
void dpdk_ovs_ack_add_eth_vm(char *name, int is_ko)
{
  t_dpdk_vm *vm = vm_find(name);
  if (!vm)
    KERR("%s", name);
  else
    {
    vm->dyn_vm_add_done_acked =  1;
    }
  if (is_ko)
    {
    KERR("FATAL NO ETH FOR DPDK: %s", name);
    machine_death(name, error_death_noovseth);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_eth_exists(char *name, int num)
{
  int result = 0;
  t_dpdk_vm *vm = vm_find(name);
  if ((num < 0) || 
      (num > MAX_SOCK_VM + MAX_DPDK_VM + MAX_WLAN_VM))
    KOUT("%d", num);
  if (vm)
    {
    if (vm->eth_table[num].eth_type == eth_type_dpdk)
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_try_send_diag_msg(int tid, char *cmd)
{
  int result = -1;
  t_ovs *ovsdb = ovs_find();
  if ((ovsdb) && (ovsdb->ovs_pid_ready))
    {
    result = try_send_msg_ovs(ovsdb, msg_type_diag, tid, cmd);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *dpdk_ovs_format_net(t_vm *vm, int eth)
{
  static char net_cmd[MAX_PATH_LEN*3];
  int i, first_dpdk = 0xFFFF, len = 0;
  char *mc = vm->kvm.eth_table[eth].mac_addr;
  char *endp_path = utils_get_dpdk_endp_path(vm->kvm.name, eth);

  for (i = 0; i < vm->kvm.nb_tot_eth; i++)
    {
    if (vm->kvm.eth_table[i].eth_type == eth_type_dpdk)
      {
      first_dpdk = i;
      break;
      }
    }
  if (first_dpdk == 0xFFFF)
    KOUT("%d", eth);
  
  if (eth == first_dpdk)
    {
    len += sprintf(net_cmd+len, " -object memory-backend-file,id=mem0,"
                                "size=%dM,share=on,mem-path=%s"
                                " -numa node,memdev=mem0 -mem-prealloc",
                                vm->kvm.mem, cfg_get_root_work());
    }

  len += sprintf(net_cmd+len, " -chardev socket,id=chard%d,path=%s,server",
                              eth, endp_path);

  len += sprintf(net_cmd+len, " -netdev type=vhost-user,id=net%d,"
                              "chardev=chard%d,vhostforce,queues=%d",
                              eth, eth, MQ_QUEUES);
  
  len += sprintf(net_cmd+len, " -device virtio-net-pci,netdev=net%d,"
                              "mac=%02X:%02X:%02X:%02X:%02X:%02X,"
                              "mq=on,vectors=%d,bus=pci.0,addr=0x%x,"
                              "csum=off,guest_csum=off,guest_tso4=off,"
                              "guest_tso6=off,guest_ecn=on,mrg_rxbuf=on",
                              eth, mc[0]&0xFF, mc[1]&0xFF, mc[2]&0xFF,
                              mc[3]&0xFF, mc[4]&0xFF, mc[5]&0xFF,
                              MQ_VECTORS, eth+5);

  return net_cmd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_still_present(void)
{
  int result = 0;
  if ((g_head_vm != NULL) || 
      (dpdk_tap_get_qty() != 0) ||
      (dpdk_snf_get_qty() != 0) ||
      (dpdk_nat_get_qty() != 0) ||
      (dpdk_a2b_get_qty() != 0) ||
      (dpdk_d2d_get_qty() != 0) ||
      (g_head_ovs != NULL))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *dpdk_ovs_translate_topo_endp(int *nb)
{
  int i, nb_endp = 0, nb_endp_vlan = 0;
  t_dpdk_vm *vm;
  t_topo_endp *result;
  vm = g_head_vm;
  while(vm)
    {
    for (i=0; i<vm->nb_tot_eth; i++)
      {
      if (vm->eth_table[i].eth_type == eth_type_dpdk)
        nb_endp += 1;
      }
    vm = vm->next;
    }
  result = (t_topo_endp *) clownix_malloc(nb_endp*sizeof(t_topo_endp),13);
  memset(result, 0, nb_endp*sizeof(t_topo_endp));
  vm = g_head_vm;
  while(vm)
    {
    for (i=0; i<vm->nb_tot_eth; i++)
      {
      if (vm->eth_table[i].eth_type == eth_type_dpdk)
        {
        if (dpdk_dyn_topo_endp(vm->name, i, &(result[nb_endp_vlan])))
          nb_endp_vlan += 1;
        }
      }
    vm = vm->next;
    }
  *nb = nb_endp_vlan;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_get_nb(void)
{
  int nb_vm, i, j, result = 0;
  t_vm *vm = cfg_get_first_vm(&nb_vm);
  for (i=0; i<nb_vm; i++)
    {
    if (!vm)
      KOUT(" ");
    for (j=0; j<vm->kvm.nb_tot_eth; j++)
      {
      if (vm->kvm.eth_table[j].eth_type == eth_type_dpdk)
        result += 1;
      }
    vm = vm->next;
    }
  if (vm)
    KOUT(" ");
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_ovs_urgent_client_destruct(void)
{
  if (dpdk_tap_get_qty())
    dpdk_tap_end_ovs();
  if (dpdk_snf_get_qty())
    dpdk_snf_end_ovs();
  if (dpdk_nat_get_qty())
    dpdk_nat_end_ovs();
  if (dpdk_a2b_get_qty())
    dpdk_a2b_end_ovs();
  if (dpdk_d2d_get_qty())
    dpdk_d2d_end_ovs();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_ovs_start_openvswitch_if_not_done(void)
{
  t_ovs *cur = g_head_ovs;
  if (cur == NULL)
    {
    event_print("Start OpenVSwitch");
    cur = ovs_alloc("ovsdb");
    cur->watchdog_protect = 1;
    cur->clone_start_pid = create_muovs_process("ovsdb");
    cur->clone_start_pid_just_done = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_ovs_add_vm(char *name,int nb_tot_eth,t_eth_table *eth_tab)
{
  int i;
  for (i=0; i<nb_tot_eth; i++)
    {
    if (eth_tab[i].eth_type == eth_type_dpdk)
      erase_dpdk_qemu_path(name, i);
    }
  vm_alloc(name, nb_tot_eth, eth_tab);
  dpdk_ovs_start_openvswitch_if_not_done();
  event_print("Alloc ovs vm %s", name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_ovs_del_vm(char *name)
{
  t_dpdk_vm *vm = vm_find(name);
  if (vm)
    {
    if (vm->destroy_done == 0)
      {
      event_print("Free qmp_shutdown vm %s", name);
      call_dpdk_dyn_del_eth(vm);
      vm->destroy_done = 1;
      vm_free(name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_get_dpdk_usable(void)
{
  int result = 0;
  t_ovs *cur = g_head_ovs;
  if (cur)
    result = cur->open_ovs_ok;
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_ovs_fill_eventfull(char *name, int num, int ms,
                            int ptx, int prx, int btx, int brx)
{
  int result = -1;
  t_dpdk_vm *vm = vm_find(name);
  if (vm)
    {
    if ((num < 0) || (num > MAX_DPDK_VM))
      KOUT("%d", num);
    vm->eth[num].ms      = ms;
    vm->eth[num].pkt_tx  += ptx;
    vm->eth[num].pkt_rx  += prx;
    vm->eth[num].byte_tx += btx;
    vm->eth[num].byte_rx += brx;
    stats_counters_update_endp_tx_rx(name, num, ms, ptx, btx, prx, brx);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_ovs_collect_dpdk(t_eventfull_endp *eventfull)
{
  int nb_vm, i, j, result = 0;;
  t_vm *vm = cfg_get_first_vm(&nb_vm);
  t_dpdk_vm *cur;
  for (i=0; i<nb_vm; i++)
    {
    if (!vm)
      KOUT(" ");
    cur = vm_find(vm->kvm.name);
    for (j=0; j<vm->kvm.nb_tot_eth; j++)
      {
      if (vm->kvm.eth_table[j].eth_type == eth_type_dpdk)
        {
        strncpy(eventfull[result].name, vm->kvm.name, MAX_NAME_LEN-1);
        eventfull[result].num  = j;
        eventfull[result].type = endp_type_kvm_dpdk;
        if (j == 0)
          {
          eventfull[result].ram  = vm->ram;
          eventfull[result].cpu  = vm->cpu;
          }
        if (cur)
          {
          eventfull[result].ptx  = cur->eth[j].pkt_tx;
          eventfull[result].prx  = cur->eth[j].pkt_rx;
          eventfull[result].btx  = cur->eth[j].byte_tx;
          eventfull[result].brx  = cur->eth[j].byte_rx;
          eventfull[result].ms   = cur->eth[j].ms;
          memset(&(cur->eth[j]), 0, sizeof(t_dpdk_eth));
          }
        result += 1;
        }
      }
    vm = vm->next;
    }
  if (vm)
    KOUT(" ");
  return result;
}
/*---------------------------------------------------------------------------*/

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
void dpdk_ovs_init(uint32_t lcore, uint32_t mem, uint32_t cpu)
{
  g_head_vm = NULL;
  g_head_ovs = NULL;
  clownix_timeout_add(50, timer_ovs_beat, NULL, NULL, NULL);
  clownix_timeout_add(50, timer_vm_add_beat, NULL, NULL, NULL);
  dpdk_dyn_init();
  dpdk_msg_init();
  dpdk_tap_init();
  dpdk_snf_init();
  dpdk_nat_init();
  dpdk_a2b_init();
  dpdk_d2d_init();
  g_lcore_mask = lcore;
  g_socket_mem = mem;
  g_cpu_mask   = cpu;
  g_restart_hysteresis = 0;
}
/*--------------------------------------------------------------------------*/

