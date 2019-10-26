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
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "heartbeat.h"
#include "machine_create.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "system_callers.h"
#include "automates.h"
#include "lan_to_name.h"
#include "utils_cmd_line_maker.h"
#include "file_read_write.h"
#include "qmonitor.h"
#include "qmp.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "timeout_service.h"
#include "c2c.h"
#include "xwy.h"
#include "mulan_mngt.h"
#include "endp_mngt.h"
#include "endp_evt.h"
#include "hop_event.h"
#include "c2c_utils.h"
#include "qmp.h"
#include "dpdk_ovs.h"
#include "dpdk_dyn.h"
#include "dpdk_tap.h"



int produce_list_commands(t_list_commands *list);

static void recv_del_vm(int llid, int tid, char *name);
static void recv_halt_vm(int llid, int tid, char *name);
static void recv_reboot_vm(int llid,int tid,char *name);
static void recv_promiscious(int llid, int tid, char *name, int eth, int on);



int inside_cloonix(char **name);

extern int clownix_server_fork_llid;
static void add_lan_endp(int llid, int tid, char *name, int num, char *lan);
static int g_in_cloonix;
static char *g_cloonix_vm_name;

int file_exists(char *path, int mode);


/*****************************************************************************/
typedef struct t_coherency_delay
{
  int llid;
  int tid;
  char name[MAX_NAME_LEN];
  int  num;
  char lan[MAX_NAME_LEN];
  struct t_coherency_delay *prev;
  struct t_coherency_delay *next;
} t_coherency_delay;
/*---------------------------------------------------------------------------*/
typedef struct t_add_vm_cow_look
{
  char msg[MAX_PRINT_LEN];
  int llid;
  int tid;
  int vm_id;
  t_topo_kvm kvm;
} t_add_vm_cow_look;
/*---------------------------------------------------------------------------*/
typedef struct t_timer_endp
{
  int  timer_lan_ko_resp;
  int  llid;
  int  tid;
  char name[MAX_NAME_LEN];
  int  num;
  char lan[MAX_NAME_LEN];
  int  count;
  char label[MAX_PATH_LEN];
  struct t_timer_endp *prev;
  struct t_timer_endp *next;
} t_timer_endp;
/*--------------------------------------------------------------------------*/

static t_timer_endp *g_head_timer;
static int glob_coherency;
static t_coherency_delay *g_head_coherency;
static long long g_coherency_abs_beat_timer;
static int g_coherency_ref_timer;
static int g_inhib_new_clients;


/*****************************************************************************/
static int get_inhib_new_clients(void)
{
  if (g_inhib_new_clients)
    {
    KERR("Server being killed, no new client");
    event_print("Server being killed, no new client");
    }
  return g_inhib_new_clients;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_coherency_unlock(void)
{
  glob_coherency -= 1;
  if (glob_coherency < 0)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_coherency_lock(void)
{
  glob_coherency += 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int recv_coherency_locked(void)
{
  return glob_coherency;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_coherency_delay *coherency_add_chain(int llid, int tid, 
                                              char *name, int num, 
                                              char *lan)
{
  t_coherency_delay *cur, *elem;
  elem = (t_coherency_delay *) clownix_malloc(sizeof(t_coherency_delay), 16);
  memset(elem, 0, sizeof(t_coherency_delay));
  elem->llid = llid;
  elem->tid = tid;
  elem->num = num;
  strcpy(elem->name, name);
  strcpy(elem->lan, lan);
  if (g_head_coherency)
    {
    cur = g_head_coherency;
    while (cur && cur->next)
      cur = cur->next;
    cur->next = elem;
    elem->prev = cur;
    }
  else
    g_head_coherency = elem;
  return g_head_coherency;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void coherency_del_chain(t_coherency_delay *cd)
{
  if (cd->prev)
    cd->prev->next = cd->next;
  if (cd->next)
    cd->next->prev = cd->prev;
  if (cd == g_head_coherency)
    g_head_coherency = cd->next;
  clownix_free(cd, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void delayed_coherency_cmd_timeout(void *data)
{
  t_coherency_delay *next, *cur = (t_coherency_delay *) data;
  g_coherency_abs_beat_timer = 0;
  g_coherency_ref_timer = 0;
  if (g_head_coherency != cur)
    KOUT(" ");
  if (recv_coherency_locked())
    clownix_timeout_add(20, delayed_coherency_cmd_timeout, data, 
                        &g_coherency_abs_beat_timer, &g_coherency_ref_timer);
  else
    {
    while (cur)
      {
      next = cur->next;
      add_lan_endp(cur->llid, cur->tid, cur->name, cur->num, cur->lan);
      coherency_del_chain(cur);
      cur = next;
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_work_dir_req(int llid, int tid)
{
  t_topo_clc *conf = cfg_get_topo_clc();
  send_work_dir_resp(llid, tid, conf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void recv_promiscious(int llid, int tid, char *name, int eth, int on)
{
  char info[MAX_PRINT_LEN];
  t_vm *vm;
  vm = cfg_get_vm(name);
  event_print("Rx Req promisc %d for  %s eth%d", on, name, eth);
  if (!vm)
    {
    sprintf( info, "Machine %s does not exist", name);
    send_status_ko(llid, tid, info);
    }
  else if ((eth < 0) || (eth >= vm->kvm.nb_dpdk + vm->kvm.nb_eth))
    {
    sprintf( info, "eth%d for machine %s does not exist", eth, name);
    send_status_ko(llid, tid, info);
    }
  else
    {
    sprintf( info, "OBSOLETE PROMISC");
    send_status_ko(llid, tid, info);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_vmcmd(int llid, int tid, char *name, int cmd, int param)
{
  switch(cmd)
    {
    case vmcmd_del:
      recv_del_vm(llid, tid, name);
      break;
    case vmcmd_halt_with_qemu:
      recv_halt_vm(llid, tid, name);
      break;
    case vmcmd_reboot_with_qemu:
      recv_reboot_vm(llid, tid, name);
      break;
    case vmcmd_promiscious_flag_set:
      recv_promiscious(llid, tid, name, param, 1);
      break;
    case vmcmd_promiscious_flag_unset:
      recv_promiscious(llid, tid, name, param, 0);
      break;
    default:
      KERR("%d", cmd);
      break;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int machine_not_ready(char *name)
{
  int result = -1;
  t_vm *vm = cfg_get_vm(name);
  if (vm)
    {
    if (!cfg_is_a_zombie(name))
      result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_add_lan(int llid, int tid, char *name, int num, char *lan)
{
  char info[MAX_PRINT_LEN];
  int tidx, type;
  if (get_inhib_new_clients())
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
  else if (cfg_name_is_in_use(1, lan, info))
    send_status_ko(llid, tid, info);
  else if (dpdk_dyn_eth_exists(name, num))
    {
    if (dpdk_dyn_add_lan_to_eth(llid, tid, lan, name, num, info))
      send_status_ko(llid, tid, info);
    }
  else if (mulan_is_zombie(lan))
    {
    sprintf( info, "lan %s is zombie",  lan);
    send_status_ko(llid, tid, info);
    }
  else if (!endp_mngt_exists(name, num, &type))
    {
    sprintf(info, "endp %s %d not found", name, num);
    send_status_ko(llid, tid, info);
    }
  else if (endp_evt_lan_find(name, num, lan, &tidx))
    {
    sprintf(info, "%s %d already has %s ulan", name, num, lan);
    send_status_ko(llid, tid, info);
    }
  else if (endp_evt_lan_full(name, num, &tidx))
    {
    sprintf(info, "%s in %d ulan max increase MAX_TRAF_ENDPOINT and recompile",
            name, MAX_TRAF_ENDPOINT);
    send_status_ko(llid, tid, info);
    }
  else if (((type == endp_type_kvm_eth)||(type == endp_type_kvm_wlan)) &&
           (machine_not_ready(name)))
    {
    snprintf(info, MAX_PATH_LEN-1, "machine %s not ready", name);
    send_status_ko(llid, tid, info);
    }
  else
    {
    event_print("Adding lan %s in %s %d", lan, name, num);
    endp_evt_add_lan(llid, tid, name, num, lan, tidx);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_timer_endp *timer_find(char *name, int num)
{
  t_timer_endp *cur = NULL;
  if (name[0])
    {
    cur = g_head_timer;
    while(cur)
      {
      if ((!strcmp(cur->name, name)) && (cur->num == num))
        break;
      cur = cur->next;
      }
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_timer_endp *timer_alloc(char *name, int num)
{
  t_timer_endp *timer = timer_find(name, num);
  if (timer)
    KOUT("%s %d", name, num);
  timer = (t_timer_endp *)clownix_malloc(sizeof(t_timer_endp), 4);
  memset(timer, 0, sizeof(t_timer_endp));
  strncpy(timer->name, name, MAX_NAME_LEN-1);
  timer->num = num;
  if (g_head_timer)
    g_head_timer->prev = timer;
  timer->next = g_head_timer;
  g_head_timer = timer;
  return timer;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_free(t_timer_endp *timer)
{
  if (timer->prev)
    timer->prev->next = timer->next;
  if (timer->next)
    timer->next->prev = timer->prev;
  if (timer == g_head_timer)
    g_head_timer = timer->next;
  clownix_free(timer, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_endp(void *data)
{
  t_timer_endp *te = (t_timer_endp *) data;
  char err[MAX_PATH_LEN];
  if ((endp_evt_exists(te->name, te->num)) ||
      (dpdk_dyn_eth_exists(te->name, te->num)))
    {
    local_add_lan(te->llid, te->tid, te->name, te->num, te->lan);
    timer_free(te);
    }
  else
    {
    te->count++;
    if (te->count >= 100)
      {
      sprintf(err, "bad endpoint start: %s %d %s", te->name, te->num, te->lan);
      send_status_ko(te->llid, te->tid, err);
      timer_free(te);
      }
    else
      {
      clownix_timeout_add(10, timer_endp, (void *) te, NULL, NULL);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_endp_init(int llid, int tid, char *name,
                                int num, char *lan)
{
  t_timer_endp *te = timer_find(name, num);
  if (te)
    send_status_ko(llid, tid, "endpoint already waiting");
  else
    {
    te = timer_alloc(name, num);
    te->llid = llid;
    te->tid = tid;
    strncpy(te->lan, lan, MAX_NAME_LEN-1);
    clownix_timeout_add(10, timer_endp, (void *) te, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_lan_endp(int llid, int tid, char *name, int num, char *lan)
{
  if (recv_coherency_locked())
    {
    g_head_coherency = coherency_add_chain(llid, tid, name, num, lan);
    if (!g_coherency_abs_beat_timer)
      clownix_timeout_add(20, delayed_coherency_cmd_timeout,
                          (void *) g_head_coherency,
                          &g_coherency_abs_beat_timer,
                          &g_coherency_ref_timer);
    }
  else
    {
    if ((!endp_evt_exists(name, num)) &&
        (!dpdk_dyn_eth_exists(name, num)))
      timer_endp_init(llid, tid, name, num, lan);
    else
      local_add_lan(llid, tid, name, num, lan);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_add_lan_endp(int llid, int tid, char *name, int num, char *lan)
{
  char info[MAX_PATH_LEN];
  int type, is_dpdk = 0;
  t_vm *vm = cfg_get_vm(name);
  event_print("Rx Req add lan %s in %s %d", lan, name, num);
  if (get_inhib_new_clients())
    {
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    }
  else if (cfg_name_is_in_use(1, lan, info))
    {
    send_status_ko(llid, tid, info);
    }
  else 
    {
    if (dpdk_ovs_exist_tap(name))
      {
      if (!dpdk_ovs_muovs_ready())
        {
        snprintf(info, MAX_PATH_LEN, "Problem dpdk muovs not ready");
        send_status_ko(llid, tid, info);
        }
      else if (num != 0)
        {
        snprintf(info, MAX_PATH_LEN, "Problem dpdk tap %s %d", name, num);
        send_status_ko(llid, tid, info);
        }
      else if (mulan_can_be_found_with_name(lan))
        {
        snprintf(info, MAX_PATH_LEN, "tap %s is DPDK", name);
        send_status_ko(llid, tid, info);
        }
      else
        {
        if (dpdk_ovs_add_lan_tap(llid, tid, lan, name))
          {
          snprintf(info, MAX_PATH_LEN, "add lan tap %s %s", lan, name);
          send_status_ko(llid, tid, info);
          }
        }
      }
    else
      {
      if ((vm) && (num < vm->kvm.nb_dpdk))
        is_dpdk = 1; 
      if ((!endp_mngt_exists(name, num, &type)) &&
          (!dpdk_ovs_eth_exists(name, num)))
        {
        snprintf(info, MAX_PATH_LEN, "endp %s %d not found", name, num);
        send_status_ko(llid, tid, info);
        }
      else
        {
        if ((is_dpdk == 1) && 
            (mulan_can_be_found_with_name(lan)))
          {
          snprintf(info, MAX_PATH_LEN, "eth %s %d is DPDK", name, num);
          send_status_ko(llid, tid, info);
          }
        else if ((is_dpdk == 0) && (dpdk_dyn_lan_exists(lan)))
          {
          snprintf(info, MAX_PATH_LEN, "eth %s %d is NOT DPDK", name, num);
          send_status_ko(llid, tid, info);
          }
        else
          add_lan_endp(llid, tid, name, num, lan);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_evt_print_sub(int llid, int tid)
{
  event_print("Rx Req subscribing to print for client: %d", llid);
  if (msg_exist_channel(llid))
    {
    event_subscribe(sub_evt_print, llid, tid);
    send_status_ok(llid, tid, "printsub");
    }
  else
    send_status_ko(llid, tid, "Abnormal!");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_topo_sub(int llid, int tid)
{
  event_print("Rx Req subscribing to topology for client: %d", llid);
  if (msg_exist_channel(llid))
    {
    event_subscribe(sub_evt_topo, llid, tid);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
  else
    send_status_ko(llid, tid, "Abnormal!");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_del_lan_endp(int llid, int tid, char *name, int num, char *lan)
{
  char info[MAX_PRINT_LEN];
  int tidx;
  event_print("Rx Req del lan %s of %s %d", lan, name, num);
  if (dpdk_ovs_exist_tap(name))
    {
    if (num != 0)
      {
      snprintf(info, MAX_PATH_LEN, "Problem dpdk tap %s %d", name, num);
      send_status_ko(llid, tid, info);
      }
    else if (mulan_can_be_found_with_name(lan))
      {
      snprintf(info, MAX_PATH_LEN, "tap %s is DPDK", name);
      send_status_ko(llid, tid, info);
      }
    else if (!dpdk_dyn_lan_exists(lan))
      {
      snprintf(info, MAX_PATH_LEN, "dpdk lan %s does not exist", lan);
      send_status_ko(llid, tid, info);
      }
    else
      {
      if (dpdk_ovs_del_lan_tap(llid, tid, lan, name))
        {
        snprintf(info, MAX_PATH_LEN, "del lan tap %s %s", lan, name);
        send_status_ko(llid, tid, info);
        }
      }
    }
  else if (dpdk_ovs_eth_exists(name, num))
    {
    if (dpdk_dyn_del_lan_from_eth(llid, tid, lan, name, num, info))
      send_status_ko(llid, tid, info);
    }
  else if (!lan_get_with_name(lan))
    {
    sprintf(info, "lan %s does not exist", lan);
    send_status_ko(llid, tid, info);
    }
  else if (!endp_evt_lan_find(name, num, lan, &tidx))
    {
    sprintf(info, "lan %s not found in %s", lan, name);
    send_status_ko(llid, tid, info);
    }
  else if (endp_evt_del_lan(name, num, tidx, lan))
    {
    sprintf(info, "Abnormal: lan %s not found in %s %d", lan, name, num);
    send_status_ko(llid, tid, info);
    }
  else
    {
    send_status_ok(llid, tid, "endpointdellan");
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_kill_uml_clownix(int llid, int tid)
{
  g_inhib_new_clients = 1;
  event_print("Rx Req Self-Destruction");
  mulan_del_all();
  c2c_free_all();
  machine_recv_kill_clownix();
  endp_mngt_stop_all_sat();
  dpdk_ovs_urgent_client_destruct();
  auto_self_destruction(llid, tid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int test_machine_is_kvm_able(void)
{
  int found = 0;
  FILE *fhd;
  char *result = NULL;
  fhd = fopen("/proc/cpuinfo", "r");
  if (fhd)
    {
    result = (char *) malloc(500);
    while(!found)
      {
      if (fgets(result, 500, fhd) != NULL)
        {
        if (!strncmp(result, "flags", strlen("flags")))
          found = 1;
        }
      else
        KOUT(" ");
      }
    fclose(fhd);
    }
  if (!found)
    KOUT(" ");
  found = 0;
  if ((strstr(result, "vmx")) || (strstr(result, "svm")))
    found = 1;
  free(result);
  return found;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int i_have_read_write_access(char *path)
{
  return ( ! access(path, R_OK|W_OK) );
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *read_sys_file(char *file_name, char *err)
{
  char *buf = NULL;
  int fd;
  fd = open(file_name, O_RDONLY);
  if (fd > 0)
    {
    buf = (char *) clownix_malloc(100,13);
    read(fd, buf, 99);
    buf[99] = 0;
    close (fd);
    }
  else
    sprintf(err, "Cannot open file %s\n", file_name);
  return buf;
}
/*--------------------------------------------------------------------------*/

#define SYS_KVM_DEV "/sys/devices/virtual/misc/kvm/dev"
/*****************************************************************************/
static int get_dev_kvm_major_minor(int *major, int *minor, char *info)
{
  int result = -1;
  char err[MAX_PATH_LEN];
  char *buf;
  if (file_exists(SYS_KVM_DEV, F_OK))
    {
    buf = read_sys_file(SYS_KVM_DEV, err);
    if (buf)
      {
      if (sscanf(buf, "%d:%d", major, minor) == 2)
        result = 0;
      else
        sprintf(info, "UNEXPECTED %s\n", SYS_KVM_DEV);
      }
    else
      sprintf(info, "ERR %s %s\n", SYS_KVM_DEV, err);
    clownix_free(buf, __FUNCTION__);
    }
  else
    sprintf(info,
            "/dev/kvm not found, \"modprobe kvm_intel/kvm_amd nested=1\""
            " and \"chmod 666 /dev/kvm\", (on the real host!)\n");
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int inside_cloonix_test_dev_kvm(char *err)
{
  int major, minor;
  int result = -1;
  char cmd[MAX_PATH_LEN];
  if (!get_dev_kvm_major_minor(&major, &minor, err))
    {
    if (major == 10)
      {
      sprintf(cmd, "/bin/mknod /dev/kvm c %d %d", major, minor);
      if (!clownix_system(cmd))
        {
        result = 0;
        sprintf(cmd, "/bin/chmod 666 /dev/kvm");
        clownix_system(cmd);
        }
      else
        sprintf(err, "/bin/mknod /dev/kvm c %d %d", major, minor);
      }
    else
      sprintf(err, "/dev/kvm: %d:%d major is not 10, something wrong\n",
              major, minor);
    }
  return result;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
static int test_dev_kvm(char *info)
{
  int result = -1;
  int fd;
  if (test_machine_is_kvm_able())
    {
    result = 0;
    if (access("/dev/kvm", F_OK))
      {
      if (g_in_cloonix)
        result = inside_cloonix_test_dev_kvm(info);
      else
        {
        sprintf(info, "/dev/kvm not found see \"KVM module\" "
                      "in \"Depends\" chapter of doc");
        result = -1;
        }
      }
    else if (!i_have_read_write_access("/dev/kvm"))
      {
      sprintf(info, "/dev/kvm not writable see \"KVM module\" "
                    "in \"Depends\" chapter of doc");
      result = -1;
      }
    else
      {
      fd = open("/dev/kvm", O_RDWR);
      if (fd < 0)
        {
        sprintf(info, "/dev/kvm not openable  \n");
        result = -1;
        }
      close(fd);
      }

    }
  else
    {
    sprintf(info, "No hardware virtualisation! kvm needs it!\n");
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int test_qemu_kvm_wanted_files(t_topo_kvm *kvm, char *rootfs, 
                                      char *bzimage, char *info) 
{
  int result = 0;
  char bz_image[MAX_PATH_LEN];
  char qemu_kvm_exe[MAX_PATH_LEN];
  memset(bz_image, 0, MAX_PATH_LEN);
  memset(qemu_kvm_exe, 0, MAX_PATH_LEN);
  if (kvm->vm_config_flags & VM_CONFIG_FLAG_ARM)
    {
    snprintf(qemu_kvm_exe, MAX_PATH_LEN-1, "%s/server/qemu/%s/%s", 
             cfg_get_bin_dir(), QEMU_BIN_DIR, QEMU_ARM_EXE);
    snprintf(bz_image, MAX_PATH_LEN-1, "%s/%s", cfg_get_bulk(), bzimage);
    }
  else if (kvm->vm_config_flags & VM_CONFIG_FLAG_AARCH64)
    {
    snprintf(qemu_kvm_exe, MAX_PATH_LEN-1, "%s/server/qemu/%s/%s", 
             cfg_get_bin_dir(), QEMU_BIN_DIR, QEMU_AARCH64_EXE);
    snprintf(bz_image, MAX_PATH_LEN-1, "%s/%s", cfg_get_bulk(), bzimage);
    }
  else
    {
    snprintf(qemu_kvm_exe, MAX_PATH_LEN-1, "%s/server/qemu/%s/%s", 
             cfg_get_bin_dir(), QEMU_BIN_DIR, QEMU_EXE);
    }
  if (test_dev_kvm(info))
    result = -1;
  else if (!file_exists(qemu_kvm_exe, F_OK))
    {
    sprintf(info, "File: \"%s\" not found\n", qemu_kvm_exe);
    result = -1;
    }
  else if (strlen(bz_image) && (!file_exists(bz_image, F_OK)))
    {
    sprintf(info, "File: \"%s\" not found\n", bz_image);
    result = -1;
    }
  else if (!file_exists(rootfs, F_OK))
    {
    sprintf(info, "File: \"%s\" not found \n", rootfs);
    result = -1;
    }
  else if ((kvm->vm_config_flags & VM_CONFIG_FLAG_PERSISTENT) &&
           (!file_exists(rootfs, W_OK)))
    {
    sprintf(info, "Persistent write rootfs file: \"%s\" not writable \n", rootfs);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int test_topo_kvm(t_topo_kvm *kvm, int vm_id, char *info)
{
  int result = 0;
  char rootfs[2*MAX_PATH_LEN];
  if (kvm->cpu == 0)
    kvm->cpu =  1;
  if (kvm->mem == 0)
    kvm->mem =  128;
  if (kvm->vm_config_flags & VM_CONFIG_FLAG_INSTALL_CDROM)
    {
    if (!file_exists(kvm->install_cdrom, F_OK))
      {
      sprintf(info, "File: \"%s\" not found\n", kvm->install_cdrom);
      result = -1;
      }
    }
  if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_CDROM)
    {
    if (!file_exists(kvm->added_cdrom, F_OK))
      {
      sprintf(info, "File: \"%s\" not found\n", kvm->added_cdrom);
      result = -1;
      }
    }
  if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK)
    {
    if (!file_exists(kvm->added_disk, F_OK))
      {
      sprintf(info, "File: \"%s\" not found\n", kvm->added_disk);
      result = -1;
      }
    }
  if (result == 0)
    {
    memset(rootfs, 0, 2*MAX_PATH_LEN);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)
      {
      if (file_exists(kvm->rootfs_input, F_OK))
        {
        strncpy(rootfs, kvm->rootfs_input, MAX_PATH_LEN-1);
        strncpy(kvm->rootfs_used, rootfs, MAX_PATH_LEN-1);
        }
      else
        {
        snprintf(rootfs, 2*MAX_PATH_LEN, "%s/%s", 
                 cfg_get_bulk(), kvm->rootfs_input);
        rootfs[MAX_PATH_LEN-1] = 0;
        strncpy(kvm->rootfs_used, rootfs, MAX_PATH_LEN-1);
        }
      }
    else if (kvm->vm_config_flags & VM_CONFIG_FLAG_EVANESCENT)
      {
      kvm->vm_config_flags |= VM_FLAG_DERIVED_BACKING;
      if (file_exists(kvm->rootfs_input, F_OK))
        {
        snprintf(kvm->rootfs_backing, MAX_PATH_LEN, 
                 "%s", kvm->rootfs_input);
        kvm->rootfs_backing[MAX_PATH_LEN-1] = 0;
        }
      else
        {
        snprintf(kvm->rootfs_backing, MAX_PATH_LEN, 
                 "%s", utils_get_root_fs(kvm->rootfs_input));
        kvm->rootfs_backing[MAX_PATH_LEN-1] = 0;
        }
      snprintf(kvm->rootfs_used,MAX_PATH_LEN,"%s/derived.qcow2",
               utils_get_disks_path_name(vm_id));
      kvm->rootfs_used[MAX_PATH_LEN-1] = 0;
      strncpy(rootfs, kvm->rootfs_backing, MAX_PATH_LEN-1); 
      }
    else
      KOUT(" ");
    if (!strlen(rootfs))
      {
      result = -1;
      sprintf(info, "BAD rootfs\n");
      }
    else
      {
      result = test_qemu_kvm_wanted_files(kvm, rootfs,
                                          kvm->linux_kernel, info);
      }
    }
    if (result == 0)
      sprintf(info, 
      "Rx Req add kvm machine %s with dpdk=%d eth=%d wlan=%d, FLAGS:%s",
       kvm->name, kvm->nb_dpdk, kvm->nb_eth, kvm->nb_wlan, 
       prop_flags_ascii_get(kvm->vm_config_flags));
  if (!result)
    {
    if (kvm->nb_dpdk > MAX_DPDK_VM)
      {
      sprintf(info, "Maximum dpdk ethernet %d per machine", MAX_DPDK_VM);
      result = -1;
      }
    if (kvm->nb_eth > MAX_ETH_VM)
      {
      sprintf(info, "Maximum classic ethernet %d per machine", MAX_ETH_VM);
      result = -1;
      }
    if (kvm->nb_wlan > MAX_WLAN_VM)
      {
      sprintf(info, "Maximum wifi lan %d per machine", MAX_WLAN_VM);
      result = -1;
      }
    }
  event_print(info);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cow_look_clone_msg(void *data, char *msg)
{
  char *ptr;
  t_add_vm_cow_look *add_vm = (t_add_vm_cow_look *) data;
  if (!strncmp(msg, "backing file", strlen("backing file")))
    {
    ptr = strchr(msg, '/');
    if (ptr)
      {
      strncpy(add_vm->msg, ptr, MAX_PRINT_LEN-1);
      KERR("%s", add_vm->msg);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cow_look_clone_death(void *data, int status, char *name)
{
  t_add_vm_cow_look *add_vm = (t_add_vm_cow_look *) data;
  char msg[MAX_PATH_LEN];
  if (add_vm->msg[0] == '/')
    {
    if (add_vm->kvm.rootfs_backing[0])
      KERR("%s %s", add_vm->msg,  add_vm->kvm.rootfs_backing);
    else
      {
      memset(msg, 0, MAX_PATH_LEN);
      strncpy(msg, add_vm->msg, MAX_PATH_LEN-1);
      snprintf(add_vm->kvm.rootfs_backing, MAX_PATH_LEN, "%s", msg);
      add_vm->kvm.rootfs_backing[MAX_PATH_LEN-1] = 0;
      add_vm->kvm.vm_config_flags |= VM_FLAG_DERIVED_BACKING;
      }
    }
  machine_recv_add_vm(add_vm->llid, add_vm->tid, 
                      &(add_vm->kvm), add_vm->vm_id);
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int cow_look_clone(void *data)
{
  t_add_vm_cow_look *add_vm = (t_add_vm_cow_look *) data;
  char *cmd = utils_get_qemu_img();
  char rootfs[MAX_PATH_LEN];
  char *argv[] = { cmd, "info", rootfs, NULL, };
  memset(rootfs, 0, MAX_PATH_LEN);
  if (add_vm->kvm.vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)
    {
    snprintf(rootfs, MAX_PATH_LEN, "%s", add_vm->kvm.rootfs_used);
    rootfs[MAX_PATH_LEN-1] = 0;
    }
  else if (add_vm->kvm.vm_config_flags & VM_FLAG_DERIVED_BACKING)
    {
    snprintf(rootfs, MAX_PATH_LEN, "%s", add_vm->kvm.rootfs_backing); 
    rootfs[MAX_PATH_LEN-1] = 0;
    }
  else
    KOUT("%X", add_vm->kvm.vm_config_flags);
  my_popen(cmd, argv);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int adjust_for_nb_dpdk(int mem, int nb_dpdk)
{
  int mod = mem % nb_dpdk;
  int result = mem + (nb_dpdk - mod);
  if (result % nb_dpdk)
    KOUT("%d %d %d", mem, nb_dpdk, mod);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_add_vm(int llid, int tid, t_topo_kvm *kvm)
{
  int i, vm_id, result = 0;
  char mac[6];
  char info[MAX_PRINT_LEN];
  t_add_vm_cow_look *cow_look;
  char use[MAX_PATH_LEN];
  t_vm   *vm = cfg_get_vm(kvm->name);
  info[0] = 0;
  memset(mac, 0, 6);
  if (get_inhib_new_clients())
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
  else if (cfg_name_is_in_use(0, kvm->name, use))
    send_status_ko(llid, tid, use);
  else if (vm)
    {
    sprintf(info, "Machine: \"%s\" already exists", kvm->name);
    event_print("%s", info);
    send_status_ko(llid, tid, info);
    }
  else if (cfg_is_a_zombie(kvm->name))
    {
    sprintf( info, "Machine: \"%s\" is a zombie", kvm->name);
    event_print("%s", info);
    send_status_ko(llid, tid, info);
    }
  else if (cfg_is_a_newborn(kvm->name))
    {
    sprintf( info, "Machine: \"%s\" is a newborn", kvm->name);
    event_print("%s", info);
    send_status_ko(llid, tid, info);
    }
  else if ((kvm->vm_config_flags & VM_CONFIG_FLAG_VMWARE) &&
           ((kvm->nb_eth != 3) || (kvm->nb_dpdk != 0))) 
    {
    sprintf( info, "Hardcoded 3 eth in VMWARE TYPE for  \"%s\"", kvm->name);
    event_print("%s", info);
    send_status_ko(llid, tid, info);
    }
  else
    {
    cfg_add_newborn(kvm->name);
    vm_id = cfg_alloc_vm_id();
    event_print("%s was allocated number %d", kvm->name, vm_id);
    for (i=0; i<kvm->nb_dpdk + kvm->nb_eth; i++)
      {
      if (!memcmp(kvm->eth_params[i].mac_addr, mac, 6))
        { 
        if (g_in_cloonix)
          {
          kvm->vm_config_flags |= VM_FLAG_IS_INSIDE_CLOONIX;
          kvm->eth_params[i].mac_addr[0] = 0x72;
          }
        else
          {
          kvm->eth_params[i].mac_addr[0] = 0x2;
          }
        kvm->eth_params[i].mac_addr[1] = 0xFF & rand();
        kvm->eth_params[i].mac_addr[2] = 0xFF & rand();
        kvm->eth_params[i].mac_addr[3] = 0xFF & rand();
        kvm->eth_params[i].mac_addr[4] = vm_id%100;
        kvm->eth_params[i].mac_addr[5] = i;
        }
      }
    result = test_topo_kvm(kvm, vm_id, info);
    if (result)
      {
      send_status_ko(llid, tid, info);
      cfg_del_newborn(kvm->name);
      }
    else 
      {
      recv_coherency_lock();
      if (kvm->nb_dpdk)
        {
        kvm->mem = adjust_for_nb_dpdk(kvm->mem, kvm->nb_dpdk);
        recv_coherency_lock();
        dpdk_ovs_start_vm(kvm->name, kvm->nb_dpdk, MAX_VM * vm_id);
        }
      cow_look = (t_add_vm_cow_look *) 
                 clownix_malloc(sizeof(t_add_vm_cow_look), 7);
      memset(cow_look, 0, sizeof(t_add_vm_cow_look));
      cow_look->llid = llid;
      cow_look->tid = tid;
      cow_look->vm_id = vm_id;
      memcpy(&(cow_look->kvm), kvm, sizeof(t_topo_kvm));
      pid_clone_launch(cow_look_clone, cow_look_clone_death,
                       cow_look_clone_msg, cow_look, cow_look, cow_look, 
                       kvm->name, -1, 1);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void recv_del_vm(int llid, int tid, char *name)
{
  t_vm   *vm = cfg_get_vm(name);
  if (vm)
    {
    event_print("Rx Req del machine %s", name);
    if (machine_death(name, error_death_noerr))
      send_status_ko(llid, tid, "ZOMBI MACHINE");
    else
      send_status_ok(llid, tid, "delvm");
    }
  else
    send_status_ko(llid, tid, "MACHINE NOT FOUND");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_list_commands_req(int llid, int tid)
{
  t_list_commands *list;
  int qty, alloc_len = MAX_LIST_COMMANDS_QTY * sizeof(t_list_commands);
  list = (t_list_commands *) clownix_malloc(alloc_len, 7);
  memset(list, 0, alloc_len);
  qty = produce_list_commands(list);
  send_list_commands_resp(llid, tid, qty, list);
  clownix_free(list, __FILE__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_pid_lst *create_list_pid(int *nb)
{
  int i,j, nb_vm, nb_endp, nb_sum, nb_mulan, nb_ovs;
  t_lst_pid *ovs_pid = NULL;
  t_lst_pid *endp_pid = NULL;
  t_lst_pid *mulan_pid = NULL;
  t_vm *vm = cfg_get_first_vm(&nb_vm);
  t_pid_lst *lst;
  nb_endp  = endp_mngt_get_all_pid(&endp_pid);
  nb_mulan = mulan_get_all_pid(&mulan_pid);
  nb_ovs   = dpdk_ovs_get_all_pid(&ovs_pid);
  nb_sum   = nb_ovs + nb_vm + nb_endp + nb_mulan + 10;
  lst = (t_pid_lst *)clownix_malloc(nb_sum*sizeof(t_pid_lst),18);
  memset(lst, 0, nb_sum*sizeof(t_pid_lst));
  for (i=0, j=0; i<nb_vm; i++)
    {
    if (!vm)
      KOUT(" ");
    strncpy(lst[j].name, vm->kvm.name, MAX_NAME_LEN-1);
    lst[j].pid = machine_read_umid_pid(vm->kvm.vm_id);
    j++; 
    vm = vm->next;
    }
  if (vm)
    KOUT(" ");
  for (i=0 ; i<nb_endp; i++)
    {
    strncpy(lst[j].name, endp_pid[i].name, MAX_NAME_LEN-1);
    lst[j].pid = endp_pid[i].pid;
    j++;
    }
  clownix_free(endp_pid, __FUNCTION__);
  for (i=0 ; i<nb_mulan; i++)
    {
    strncpy(lst[j].name, mulan_pid[i].name, MAX_NAME_LEN-1);
    lst[j].pid = mulan_pid[i].pid;
    j++;
    }
  clownix_free(mulan_pid, __FUNCTION__);
  for (i=0 ; i<nb_ovs; i++)
    {
    strncpy(lst[j].name, ovs_pid[i].name, MAX_NAME_LEN-1);
    lst[j].pid = ovs_pid[i].pid;
    j++;
    }
  clownix_free(ovs_pid, __FUNCTION__);
  strcpy(lst[j].name, "doors");
  lst[j].pid = doorways_get_distant_pid();
  j++;
  strcpy(lst[j].name, "xwy");
  lst[j].pid = xwy_pid();
  j++;
  strcpy(lst[j].name, "switch");
  lst[j].pid = getpid();
  j++;
  *nb = j;
  return lst;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_list_pid_req(int llid, int tid)
{
  int nb;
  t_pid_lst *lst = create_list_pid(&nb);
  event_print("Rx Req list pid");
  send_list_pid_resp(llid, tid, nb, lst);
  clownix_free(lst, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_sys_sub(int llid, int tid)
{
  event_print("Rx Req subscribing to system counters for client: %d", llid);
  if (msg_exist_channel(llid))
    {
    event_subscribe(sub_evt_sys, llid, tid);
    send_status_ok(llid, tid, "syssub");
    }
  else
    send_status_ko(llid, tid, "Abnormal!");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_sys_unsub(int llid, int tid)
{
  event_print("Rx Req unsubscribing from system for client: %d", llid);
  if (msg_exist_channel(llid))
    {
    event_unsubscribe(sub_evt_sys, llid);
    send_status_ok(llid, tid, "sysunsub");
    }
  else
    send_status_ko(llid, tid, "Abnormal!");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_topo_unsub(int llid, int tid)
{
  event_print("Rx Req unsubscribing from topo modif for client: %d", llid);
  if (msg_exist_channel(llid))
    {
    event_unsubscribe(sub_evt_topo, llid);
    send_status_ok(llid, tid, "topounsub");
    }
  else
    send_status_ko(llid, tid, "Abnormal!");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_evt_print_unsub(int llid, int tid)
{
  event_print("Rx Req unsubscribing from print for client: %d", llid);
  if (msg_exist_channel(llid))
    {
    event_unsubscribe(sub_evt_print, llid);
    send_status_ok(llid, tid, "printunsub");
    }
  else
    send_status_ko(llid, tid, "Abnormal!");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_del_all(int llid, int tid)
{
  event_print("Rx Req Delete ALL");
  mulan_del_all();
  c2c_free_all();
  machine_recv_kill_clownix();
  endp_mngt_stop_all_sat();
  dpdk_ovs_urgent_client_destruct();
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
  send_status_ok(llid, tid, "delall");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_topo_small_event_sub(int llid, int tid)
{
  int i, nb;
  t_vm *cur;
  event_print("Req subscribing to Machine poll event for client: %d", llid);
  if (msg_exist_channel(llid))
    {
    event_subscribe(topo_small_event, llid, tid);
    send_status_ok(llid, tid, "vmpollsub");
    cur = cfg_get_first_vm(&nb);
    for (i=0; i<nb; i++)
      {
      if (!cur)
        KOUT(" ");

      if (cur->qmp_conn == 1)
        send_topo_small_event(llid, tid, cur->kvm.name, 
                              NULL, NULL, vm_evt_qmp_conn_ok);
      else
        send_topo_small_event(llid, tid, cur->kvm.name,
                              NULL, NULL, vm_evt_qmp_conn_ko);

      if (cur->kvm.vm_config_flags & VM_FLAG_CLOONIX_AGENT_PING_OK)
        send_topo_small_event(llid, tid, cur->kvm.name,
                              NULL, NULL, vm_evt_cloonix_ga_ping_ok);
      else
        send_topo_small_event(llid, tid, cur->kvm.name,
                              NULL, NULL, vm_evt_cloonix_ga_ping_ko);

      cur = cur->next;
      }
    if (cur)
      KOUT(" ");
    }
  else
    send_status_ko(llid, tid, "Abnormal!");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_topo_small_event_unsub(int llid, int tid)
{
  event_print("Req unsubscribing from Machine poll event for client: %d", llid);
  if (msg_exist_channel(llid))
    {
    event_unsubscribe(topo_small_event, llid);
    send_status_ok(llid, tid, "vmpollunsub");
    }
  else
    send_status_ko(llid, tid, "tid, Abnormal!");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void local_add_sat(int llid, int tid, char *name, int type,
                   t_c2c_req_info *c2c_info)
{
  char info[MAX_PATH_LEN];
  char recpath[MAX_PATH_LEN];
  int capture_on=0;
  snprintf(recpath,MAX_PATH_LEN-1,"%s/%s.pcap",utils_get_snf_pcap_dir(),name);
  if (type == endp_type_dpdk_tap)
    {
    recv_coherency_lock();
    if (dpdk_ovs_add_tap(llid, tid, name))
      {
      snprintf( info, MAX_PATH_LEN-1, "Error dpdk_tap %s", name);
      send_status_ko(llid, tid, info);
      }
    }
  else if (endp_mngt_start(llid, tid, name, 0, type))
    {
    snprintf( info, MAX_PATH_LEN-1, "Bad start of %s", name);
    send_status_ko(llid, tid, info);
    }
  else
    {
    if (type == endp_type_snf)
      {
      endp_mngt_snf_set_name(name, 0);
      endp_mngt_snf_set_capture(name, 0, capture_on);
      endp_mngt_snf_set_recpath(name, 0, recpath);
      }
    else if (type == endp_type_c2c)
      {
      if (!c2c_info)
        {
        KERR("%s", name);
        sprintf( info, "Bad c2c param info %s", name);
        send_status_ko(llid, tid, info);
        endp_mngt_stop(name, 0);
        }
      else  if (c2c_create_master_begin(name, c2c_info->ip_slave,
                                              c2c_info->port_slave,
                                              c2c_info->passwd_slave))
        {
        KERR("%s", name);
        sprintf( info, "Bad c2c begin %s", name);
        send_status_ko(llid, tid, info);
        endp_mngt_stop(name, 0);
        }
      }
    else if (type == endp_type_a2b)
      {
      if (endp_mngt_start(llid, tid, name, 1, type))
        {
        endp_mngt_stop(name, 0);
        sprintf( info, "Bad start of %s", name);
        send_status_ko(llid, tid, info);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_add_sat(int llid, int tid, char *name, int type, 
                  t_c2c_req_info *c2c_info)
{
  char info[MAX_PRINT_LEN];
  int mutype;
  char use[MAX_PATH_LEN];
  event_print("Rx Req add %s %d", name, type);
  if (get_inhib_new_clients())
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
  else if (cfg_name_is_in_use(0, name, use))
    send_status_ko(llid, tid, use);
  else
    {
    if ((endp_mngt_exists(name, 0, &mutype)) ||
        ((type == endp_type_a2b) && (endp_mngt_exists(name, 1, &mutype))))
      {
      sprintf( info, "NOT NORMAL!  %s 1 already exists", name);
      send_status_ko(llid, tid, info);
      }
    else if ((type != endp_type_tap) &&
             (type != endp_type_dpdk_tap) &&
             (type != endp_type_snf) &&
             (type != endp_type_c2c) &&
             (type != endp_type_nat) &&
             (type != endp_type_a2b) &&
             (type != endp_type_raw) &&
             (type != endp_type_wif))
      {
      sprintf(info, "%s Bad type: %d", name, type);
      send_status_ko(llid, tid, info);
      }
    else 
      {
      local_add_sat(llid, tid, name, type, c2c_info); 
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_del_sat(int llid, int tid, char *name)
{
  int type;
  char info[MAX_PATH_LEN];
  event_print("Rx Req del %s", name);
  if (dpdk_ovs_exist_tap(name))
    {
    if (dpdk_ovs_del_tap(llid, tid, name))
      {
      snprintf( info, MAX_PATH_LEN-1, "Error dpdk_tap %s", name);
      send_status_ko(llid, tid, info);
      }
    }
  else if (!endp_mngt_exists(name, 0, &type))
    {
    snprintf(info, MAX_PATH_LEN-1, "sat %s not found", name);
    send_status_ko(llid, tid, info);
    }
  else if ((type == endp_type_kvm_eth) || (type == endp_type_kvm_wlan))
    {
    snprintf(info, MAX_PATH_LEN-1, "%s is a kvm", name);
    send_status_ko(llid, tid, info);
    }
  else 
    {
    if (type == endp_type_a2b)
      {
      if (endp_mngt_stop(name, 1))
        KERR("%s", name);
      }
    if (endp_mngt_stop(name, 0))
      KERR("%s", name);
    send_status_ok(llid, tid, "del usat");
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_get_name_list_doors(int llid, int tid)
{
  int nb;
  t_hop_list *list = hop_get_name_list(&nb);
  send_hop_name_list_doors(llid, tid, nb, list);
  hop_free_name_list(list);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_sav_vm(int llid, int tid, char *name, int stype, char *path)
{
  t_vm   *vm = cfg_get_vm(name);
  char *dir_path = mydirname(path);
  if (!vm)
    {
    send_status_ko(llid, tid, "error MACHINE NOT FOUND");
    }
  else if (file_exists(path, F_OK))
    {
    send_status_ko(llid, tid, "error FILE ALREADY EXISTS");
    }
  else if (!file_exists(dir_path, W_OK))
    {
    send_status_ko(llid, tid, "error DIRECTORY NOT WRITABLE OR NOT FOUND");
    }
  else
    {
    qmp_request_save_rootfs(name, path, llid, tid, stype);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_sav_vm_all(int llid, int tid, int stype, char *path)
{
  char info[MAX_PRINT_LEN];
  char *dir_path = mydirname(path);
  int nb;
  t_vm *vm = cfg_get_first_vm(&nb);
  if (!vm)
    {
    send_status_ko(llid, tid, "error MACHINE NOT FOUND");
    }
  else if (file_exists(path, F_OK))
    {
    send_status_ko(llid, tid, "error DIRECTORY ALREADY EXISTS");
    }
  else if (!file_exists(dir_path, W_OK))
    {
    sprintf(info, "error DIRECTORY %s NOT WRITABLE", dir_path);
    send_status_ko(llid, tid, info);
    }
  else
    {
    if (mkdir(path, 0700))
      {
      if (errno == EEXIST)
        {
        sprintf(info, "error %s ALREADY EXISTS", path);
        send_status_ko(llid, tid, info);
        }
      else
        {
        sprintf(info, "error DIR %s CREATE ERROR %d", path, errno);
        send_status_ko(llid, tid, info);
        }
      }
    else
      {
      qmp_request_save_rootfs_all(nb, vm, path, llid, tid, stype);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void recv_reboot_vm(int llid, int tid, char *name)
{
  t_vm   *vm = cfg_get_vm(name);
  if (vm)
    qmp_request_qemu_reboot(name, llid, tid);
  else
    send_status_ko(llid, tid, "error MACHINE NOT FOUND");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_qmp_sub(int llid, int tid, char *name)
{
  t_vm *vm;
  if (!name)
    qmp_request_sub(NULL, llid, tid);
  else
    {
    vm = cfg_get_vm(name);
    if (vm)
      qmp_request_sub(name, llid, tid);
    else
      send_qmp_resp(llid, tid, name, "error MACHINE NOT FOUND", -1);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_qmp_req(int llid, int tid, char *name, char *msg)
{
  t_vm *vm = cfg_get_vm(name);
  if (vm)
    qmp_request_snd(name, llid, tid, msg);
  else
    send_qmp_resp(llid, tid, name, "error MACHINE NOT FOUND", -1);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void recv_halt_vm(int llid, int tid, char *name)
{
  recv_del_vm(llid, tid, name);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_init(void)
{
  glob_coherency = 0;
  g_head_coherency = NULL;
  g_coherency_abs_beat_timer = 0;
  g_coherency_ref_timer = 0;
  g_in_cloonix = inside_cloonix(&g_cloonix_vm_name);
  g_inhib_new_clients = 0;
}
/*---------------------------------------------------------------------------*/


