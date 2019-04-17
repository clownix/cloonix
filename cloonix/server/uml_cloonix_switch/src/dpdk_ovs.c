/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include "dpdk_fmt.h"
#include "qmp.h"


enum{
  msg_type_diag = 1,
  msg_type_pid,
};

/****************************************************************************/
typedef struct t_dpdk_vm
{
  char name[MAX_NAME_LEN];
  int  num;
  int  must_unlock_coherency;
  int  dyn_vm_add_done;
  int  dyn_vm_add_done_acked;
  int  dyn_vm_add_done_notacked;
  int  destroy_done;
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
  int ovs_pid;
  int ovs_pid_ready;
  int getsuidroot;
  int open_ovs;
  int endp_type;
  int llid;
  int periodic_count;
  int unanswered_pid_req;
  int connect_try_count;
  int destroy_requested;
  struct t_ovs *prev;
  struct t_ovs *next;
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


static t_dpdk_vm *g_head_vm;
static t_ovs *g_head_ovs;
static int g_nb_ovs;


/****************************************************************************/
static void vm_check_unlock_coherency(void)
{
  int pid_responding_ovs_nb = 0;
  t_dpdk_vm *vm = g_head_vm;
  t_ovs *cur = g_head_ovs;
  while(cur)
    {
    if (cur->pid)
      pid_responding_ovs_nb += 1;
    cur = cur->next;
    }
  if (pid_responding_ovs_nb == 2)
    {
    while(vm)
      {
      if (vm->must_unlock_coherency)
        {
        recv_coherency_unlock();
        vm->must_unlock_coherency = 0;
        event_print("Unlock coherency vm %s", vm->name);
        }
      vm = vm->next;
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
static void vm_alloc(char *name, int num, int must_unlock_coherency)
{
  t_dpdk_vm *vm = vm_find(name);
  if (vm)
    KERR("%s", name);
  else
    {
    vm = (t_dpdk_vm *) clownix_malloc(sizeof(t_dpdk_vm), 8);
    memset(vm, 0, sizeof(t_dpdk_vm));
    strncpy(vm->name, name, MAX_NAME_LEN-1);
    vm->must_unlock_coherency = must_unlock_coherency;
    vm->num = num;
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
  t_dpdk_vm *vm = vm_find(name);
  if (!vm)
    KERR("%s", name);
  else
    {
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
                                uml_clownix_switch_rx_cb, "ovs");
  if (llid)
    {
    if (mu->endp_type == endp_type_ovs)
      {
      if (hop_event_alloc(llid, type_hop_ovs, mu->name, 0))
        KERR("%s", mu->name);
      llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_ovs);
      }
    else if (mu->endp_type == endp_type_ovsdb)
      {
      if (hop_event_alloc(llid, type_hop_ovsdb, mu->name, 0))
        KERR("%s", mu->name);
      llid_trace_alloc(llid, mu->name, 0, 0, type_llid_trace_endp_ovsdb);
      }
    else
      KOUT("%d", mu->endp_type);
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
        hop_event_hook(cur->llid, FLAG_HOP_DIAG, "pid_req");
        rpct_send_pid_req(NULL, cur->llid, tid, msg, 0);
        result = 0;
        }
      else
        KERR("%s", msg);
      }
    else
      KERR("%s", msg);
    }
  else
    KERR("%s", msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovs *ovs_alloc(char *name, int endp_type)
{
  t_ovs *mu;
  mu = (t_ovs *) clownix_malloc(sizeof(t_ovs), 9);
  memset(mu, 0, sizeof(t_ovs));
  strncpy(mu->name, name, MAX_NAME_LEN-1);
  mu->endp_type = endp_type;
  if (g_head_ovs)
    g_head_ovs->prev = mu;
  mu->next = g_head_ovs;
  g_head_ovs = mu;
  g_nb_ovs += 1;
  return mu;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovs *ovs_find(char *name)
{
  t_ovs *mu = g_head_ovs;
  while(name[0] && mu && strcmp(mu->name, name))
    mu = mu->next;
  return mu;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovs *ovs_find_with_llid(int llid)
{
  t_ovs *mu = g_head_ovs;
  while(mu && mu->llid != llid)
    mu = mu->next;
  return mu;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void ovs_free(char *name)
{
  t_ovs *mu = ovs_find(name);
  if (!mu)
    KOUT("%s", name);
  if (mu->prev)
    mu->prev->next = mu->next;
  if (mu->next)
    mu->next->prev = mu->prev;
  if (mu == g_head_ovs)
    g_head_ovs = mu->next;
  clownix_free(mu, __FUNCTION__);
  if (g_nb_ovs <= 0)
    KOUT("%d", g_nb_ovs);
  g_nb_ovs -= 1;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void ovs_end_part(char *name)
{
  t_ovs *mu = ovs_find(name);
  char *sock = utils_get_dpdk_ovs_path(name);
  if (!mu)
    KOUT("%s", name);
  if (mu->llid)
    llid_trace_free(mu->llid, 0, __FUNCTION__);
  if (mu->pid)
    kill(mu->pid, SIGKILL);
  else if (mu->clone_start_pid)
    kill(mu->clone_start_pid, SIGKILL);
  else
    KERR("BADPID");
  if (mu->ovs_pid)
    kill(mu->ovs_pid, SIGKILL);
  unlink(sock);
  ovs_free(name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void ovs_end(void)
{
  char cmd[2*MAX_PATH_LEN];
  t_dpdk_vm *nvm, *vm = g_head_vm;
  t_ovs *next, *cur = g_head_ovs;
  while(vm)
    {
    KERR("%s", vm->name);
    nvm = vm->next;
    machine_death(vm->name, error_death_noovs);
    vm = nvm;
    }
  while(cur)
    {
    next = cur->next;
    ovs_end_part(cur->name);
    cur = next;
    }
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1, "rm -rf %s", utils_get_dpdk_ovs_db_dir());
  clownix_system(cmd);
}
/*--------------------------------------------------------------------------*/

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
  t_ovs *cur = ovs_find(mu->name);
  if (strcmp(name, mu->name))
    KOUT("%s %s", name, mu->name);
  if (cur)
    {
    cur->destroy_requested = 1;
    KERR("OVS %s PREMATURE DEATH %s", cur->name, mu->name);
    }
  clownix_free(mu, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void ovs_watchdog(void *data)
{
  t_arg_ovsx *mu = (t_arg_ovsx *) data;
  t_ovs *cur = ovs_find(mu->name);
  if (cur && ((!cur->llid) || 
              (!cur->pid) || 
              (!cur->periodic_count)))
    {
    cur->destroy_requested = 1;
    KERR("OVS %s TIMEOUT %s %d %d %d", cur->name, mu->name,
                                   cur->llid, cur->pid, cur->periodic_count);
    }
  clownix_free(mu, __FUNCTION__);
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
static void erase_dpdk_qemu_path(char *name, int nb_dpdk)
{
  int i;
  char *endp_path;
  for (i=0; i<nb_dpdk; i++)
    {
    endp_path = utils_get_dpdk_endp_path(name, i);
    unlink(endp_path);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int test_if_dpdk_qemu_path_is_ok(char *name, int nb_dpdk)
{
  int result = 1;
  char *endp_path;
  endp_path = utils_get_dpdk_endp_path(name, 0);
  if (access(endp_path, F_OK))
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void check_on_vm_dyn_declaration(void)
{
  t_dpdk_vm *nvm, *vm = g_head_vm;
  t_ovs *ovsdb = ovs_find("ovsdb");
  t_ovs *ovs = ovs_find("ovs");
  int ovs_start_complete = 0;

  if ((ovs != NULL) && (ovsdb != NULL) &&
      (ovs->ovs_pid_ready == 1) && (ovs->periodic_count > 0))
    ovs_start_complete = 1;

  if (ovs_start_complete == 1)
    {
    while(vm)
      {
      nvm = vm->next;
      if (vm->dyn_vm_add_done == 0)
        {
        if (test_if_dpdk_qemu_path_is_ok(vm->name, vm->num))
          {
          dpdk_dyn_add_eth(vm->name, vm->num);
          vm->dyn_vm_add_done = 1;
          }
        }
      else if (vm->dyn_vm_add_done_acked == 0)
        {
        vm->dyn_vm_add_done_notacked += 1;
        if (vm->dyn_vm_add_done_notacked == 10)
          {
          KERR("%s", vm->name);
          machine_death(vm->name, error_death_noovstime);
          }
        }
      vm = nvm;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void check_on_destroy_requested(void)
{
  t_ovs *cur = g_head_ovs;
  while(cur)
    {
    if (cur->destroy_requested == 1)
      { 
      event_print("End OpenVSwitch %s", cur->name);
      ovs_end();
      break;
      }
    else if (cur->destroy_requested > 1)
      cur->destroy_requested -= 1;
    cur = cur->next;
    } 
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_vm_add_beat(void *data)
{
  check_on_vm_dyn_declaration();
  clownix_timeout_add(20, timer_vm_add_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_ovs_beat(void *data)
{
  t_ovs *cur = g_head_ovs;
  int type_hop_tid=0;
  int ovs_start_complete = 0;
  while(cur)
    {
    if (cur->endp_type == endp_type_ovs)
      {
      type_hop_tid = type_hop_ovs;
      ovs_start_complete = cur->periodic_count;
      }
    else if (cur->endp_type == endp_type_ovsdb)
      type_hop_tid = type_hop_ovsdb;
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
      try_send_msg_ovs(cur, msg_type_pid, type_hop_tid, cur->name);
    else if ((cur->getsuidroot == 0) && (cur->endp_type == endp_type_ovsdb))
      try_send_msg_ovs(cur, msg_type_diag, 0, "cloonixovs_req_suidroot");
    else if ((cur->open_ovs == 0) && (cur->endp_type == endp_type_ovs))
      try_send_msg_ovs(cur, msg_type_diag, 0, "cloonixovs_req_ovs");
    else if ((cur->open_ovs == 0) && (cur->endp_type == endp_type_ovsdb))
      try_send_msg_ovs(cur, msg_type_diag, 0, "cloonixovs_req_ovsdb");
    else
      {
      cur->periodic_count += 1;
      if (cur->periodic_count >= 5)
        {
        try_send_msg_ovs(cur, msg_type_pid, type_hop_tid, cur->name);
        cur->periodic_count = 1;
        cur->unanswered_pid_req += 1;
        if (cur->unanswered_pid_req > 3)
          {
          cur->destroy_requested = 1;
          KERR("OVS %s NOT RESPONDING destroy_requested", cur->name);
          }
        }
      }
    cur = cur->next;
    }
  check_on_destroy_requested();
  if (ovs_start_complete)
    clownix_timeout_add(100, timer_ovs_beat, NULL, NULL, NULL);
  else
    clownix_timeout_add(10, timer_ovs_beat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int create_muovs_process(char *name, int endp_type)
{
  int pid = 0;
  char **argv;
  char *bin_path = utils_get_endp_bin_path(endp_type);
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
  clownix_timeout_add(500, ovs_watchdog, (void *) arg_ovsx, NULL, NULL);
  return pid;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void wrapper_vm_free(char *name, int num)
{
  t_ovs *cur = g_head_ovs;
  vm_free(name);
  erase_dpdk_qemu_path(name, num);
  if (g_head_vm == NULL)
    {
    if (g_nb_ovs != 0)
      {
      while(cur)
        {
        cur->destroy_requested = 3;
        cur = cur->next;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_ovs_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_ovs *cur = g_head_ovs;
  int i, result = 0;
  while(cur)
    {
    if (cur->pid)
      result++;
    if (cur->ovs_pid)
      result++;
    cur = cur->next;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid),25);
    memset(glob_lst, 0, result*sizeof(t_lst_pid));
    cur = g_head_ovs;
    i = 0;
    while(cur)
      {
      if (cur->pid)
        {
        strncpy(glob_lst[i].name, cur->name, MAX_NAME_LEN-1);
        glob_lst[i].pid = cur->pid;
        i++;
        }
      if (cur->ovs_pid)
        {
        strncpy(glob_lst[i].name, cur->name, MAX_NAME_LEN-1);
        if (!strcmp(cur->name, "ovs"))
          strcat(glob_lst[i].name, "-vswitchd");
        else if (!strcmp(cur->name, "ovsdb"))
          strcat(glob_lst[i].name, "-server");
        else
          KERR("%s", cur->name);
        glob_lst[i].pid = cur->ovs_pid;
        i++;
        }
      cur = cur->next;
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
  t_ovs *mu, *cur = ovs_find_with_llid(llid);
  char txt[MAX_PATH_LEN];
  memset(txt, 0, MAX_PATH_LEN);
  snprintf(txt, MAX_PATH_LEN-1, "pid_resp %s %d", name, pid);
  hop_event_hook(llid, FLAG_HOP_DIAG, txt);
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
      vm_check_unlock_coherency();
      if (cur->endp_type == endp_type_ovsdb)
        {
        if (g_nb_ovs != 1)
          KERR("%s %d %d %d", name, toppid, pid, cur->clone_start_pid);
        else
          {
          mu = ovs_alloc("ovs", endp_type_ovs);
          mu->clone_start_pid = create_muovs_process("ovs", endp_type_ovs);
          mu->clone_start_pid_just_done = 1;
          }
        }
      }
    else
      {
      if (cur->pid != pid)
        KERR("%s %d %d %d", name, toppid, pid, cur->pid);
      if (cur->ovs_pid == 0)
        cur->ovs_pid = toppid;
      else
        {
        if (cur->ovs_pid != toppid)
          KERR("%s %d %d %d", name, toppid, cur->ovs_pid, cur->pid);
        else
           {
           cur->ovs_pid_ready = 1;
           }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_ovs_rpct_recv_diag_msg(int llid, int tid, char *line)
{
  t_ovs *cur = ovs_find_with_llid(llid);
  char txt[MAX_PATH_LEN];
  memset(txt, 0, MAX_PATH_LEN);
  snprintf(txt, MAX_PATH_LEN-1, "diag_msg %s", line);
  hop_event_hook(llid, FLAG_HOP_DIAG, txt);
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
                 "/server/dpdk/sbin/ovs-vswitchd\"");
      KERR(" destroy_requested for %s", cur->name);
      cur->destroy_requested = 1;
      }
    else if (!strcmp(line, "cloonixovs_resp_ovs_ok"))
      {
      cur->open_ovs = 1;
      event_print("Start OpenVSwitch %s open_ovs = 1", cur->name);
      }
    else
      dpdk_fmt_rx_rpct_recv_diag_msg(llid, tid, line);
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
void dpdk_ovs_start_vm(char *name, int num)
{
  t_ovs *cur;
  erase_dpdk_qemu_path(name, num);
  if (g_nb_ovs != 0)
    {
    recv_coherency_unlock();
    vm_alloc(name, num, 0);
    }
  else
    {
    vm_alloc(name, num, 1);
    event_print("Start OpenVSwitch");
    cur = ovs_alloc("ovsdb", endp_type_ovsdb);
    cur->clone_start_pid = create_muovs_process("ovsdb", endp_type_ovsdb);
    cur->clone_start_pid_just_done = 1;
    }
  event_print("Alloc ovs vm %s %d", name, num);
  if (g_nb_ovs != 0)
    event_print("Unlock coherency vm %s", name);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_ovs_end_vm_qmp_shutdown(char *name)
{
  t_dpdk_vm *vm = vm_find(name);
  if (vm)
    {
    dpdk_dyn_end_vm_qmp_shutdown(name, vm->num);
    event_print("Free ovs vm %s %d", name, vm->num);
    wrapper_vm_free(name, vm->num);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_ovs_end_vm_kill_shutdown(char *name)
{
  dpdk_ovs_end_vm_qmp_shutdown(name);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void dpdk_ovs_ack_add_eth_vm(char *name, int is_ko)
{
  t_dpdk_vm *vm = vm_find(name);
  if (!vm)
    KERR("%s", name);
  else
    vm->dyn_vm_add_done_acked =  1;
  if (is_ko)
    {
    KERR("FATAL NO ETH FOR DPDK: %s", name);
    machine_death(name, error_death_noovseth);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_ovs_end_vm(char *name, int num)
{
  t_dpdk_vm *vm = vm_find(name);
  if (!vm)
    KERR("%s", name);
  else
    {
    if (vm->destroy_done == 0)
      {
      vm->destroy_done = 1;
      if (dpdk_dyn_del_all_lan(name))
        {
        KERR("%s", name);
        wrapper_vm_free(name, num);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_eth_exists(char *name, int num)
{
  int result = 0;
  t_dpdk_vm *vm = vm_find(name);
  if (vm && (num < vm->num))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_try_send_diag_msg(int tid, char *cmd)
{
  int result = -1;
  t_ovs *ovsdb = ovs_find("ovsdb");
  if ((ovsdb) && (ovsdb->ovs_pid_ready))
    {
    result = try_send_msg_ovs(ovsdb, msg_type_diag, tid, cmd);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *dpdk_ovs_format_net(t_vm *vm, int eth, int tot_eth)
{
  static char net_cmd[MAX_PATH_LEN*3];
  int len = 0;
  char *mc = vm->kvm.eth_params[eth].mac_addr;
  char *endp_path = utils_get_dpdk_endp_path(vm->kvm.name, eth);

  len += sprintf(net_cmd+len, 
  " -chardev socket,id=chard%d,path=%s,server", eth, endp_path);

  len += sprintf(net_cmd+len,
  " -netdev type=vhost-user,id=net%d,chardev=chard%d,vhostforce", eth, eth);
  
  len += sprintf(net_cmd+len,
  " -device virtio-net-pci,netdev=net%d,mac=%02X:%02X:%02X:%02X:%02X:%02X",
  eth, mc[0]&0xFF, mc[1]&0xFF, mc[2]&0xFF, mc[3]&0xFF, mc[4]&0xFF, mc[5]&0xFF);

  len += sprintf(net_cmd+len, " -object" 
  " memory-backend-file,id=mem%d,size=%dM,"
  "mem-path=/var/lib/hugetlbfs/user/perrier/pagesize-2MB,share=on"
  " -numa node,memdev=mem%d -mem-prealloc", eth, vm->kvm.mem/tot_eth, eth);
  return net_cmd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_ovs_still_present(void)
{
  int result = 0;
  if ((g_head_vm != NULL) || (g_head_ovs != NULL))
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
    nb_endp += vm->num;
    vm = vm->next;
    }
  result = (t_topo_endp *) clownix_malloc(nb_endp*sizeof(t_topo_endp),13);
  memset(result, 0, nb_endp*sizeof(t_topo_endp));
  vm = g_head_vm;
  while(vm)
    {
    for (i=0; i<vm->num; i++)
      {
      if (dpdk_dyn_topo_endp(vm->name, i, &(result[nb_endp_vlan])))
        nb_endp_vlan += 1;
      }
    vm = vm->next;
    }
  *nb = nb_endp_vlan;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_ovs_init(void)
{
  g_head_vm = NULL;
  g_head_ovs = NULL;
  g_nb_ovs = 0;
  clownix_timeout_add(50, timer_ovs_beat, NULL, NULL, NULL);
  clownix_timeout_add(50, timer_vm_add_beat, NULL, NULL, NULL);
  dpdk_dyn_init();
  dpdk_msg_init();
}
/*--------------------------------------------------------------------------*/



