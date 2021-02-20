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
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "heartbeat.h"
#include "machine_create.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "system_callers.h"
#include "lan_to_name.h"
#include "doors_rpc.h"
#include "utils_cmd_line_maker.h"
#include "automates.h"
#include "llid_trace.h"
#include "cdrom_creation_clone.h"
#include "qmonitor.h"
#include "qmp.h"
#include "qhvc0.h"
#include "doorways_mngt.h"
#include "c2c.h"
#include "endp_mngt.h"
#include "stats_counters.h"
#include "stats_counters_sysinfo.h"
#include "dpdk_ovs.h"
#include "suid_power.h"
#include "endp_evt.h"
#include "edp_mngt.h"





void uml_vm_automaton(void *unused_data, int status, char *name);
void qemu_vm_automaton(void *unused_data, int status, char *name);


/*****************************************************************************/
typedef struct t_action_rm_dir
{
  int llid;
  int tid;
  int vm_id;
  char name[MAX_NAME_LEN];
} t_action_rm_dir;
/*---------------------------------------------------------------------------*/
typedef struct t_vm_building
{
  int llid;
  int tid;
  t_topo_kvm kvm;
  int vm_id;
  int ref_jfs;
  void *jfs;
} t_vm_building;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void death_of_rmdir_clone(void *data, int status, char *name)
{
  t_action_rm_dir *act = (t_action_rm_dir *) data;
  int i, pid, result;
  t_eth_table *eth_tab;
  t_vm *vm = find_vm_with_id(act->vm_id);
  if (vm)
    {
    pid = suid_power_get_pid(vm->kvm.vm_id);
    if (pid)
      {
      suid_power_kill_vm(vm->kvm.vm_id);
      }
    eth_tab = vm->kvm.eth_table;
    for (i=0; i<vm->kvm.nb_tot_eth; i++)
      {
      if (eth_tab[i].eth_type == eth_type_sock)
        endp_mngt_stop(vm->kvm.name, i);
      }
    if (cfg_unset_vm(vm) != act->vm_id)
      KOUT(" ");
    cfg_free_vm_id(act->vm_id);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
  if (cfg_is_a_zombie(act->name))
    {
    pid = suid_power_get_pid(act->vm_id);
    if (pid)
      KERR("%s %d", name, act->vm_id);
    cfg_del_zombie(act->name);
    result = status;
    }
  else
    KOUT(" ");
  event_print("End erasing %s data status %d", act->name, result);
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
  dec_lock_self_destruction_dir();
  clownix_free(act, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int rmdir_clone(void *data)
{
  int result = 0;
  char info[MAX_PRINT_LEN];
  t_action_rm_dir *act = (t_action_rm_dir *) data;
  info[0] = 0;
  suid_power_kill_vm(act->vm_id);
  if (rm_machine_dirs(act->vm_id, info))
    {
    sleep(1);
    if (rm_machine_dirs(act->vm_id, info))
      {
      sleep(1);
      if (rm_machine_dirs(act->vm_id, info))
        {
        KERR(" %s ", info);
        result = -1;
        }
      }
    }
  sleep(1);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void action_rm_dir_timed(void *data)
{
  t_action_rm_dir *act = (t_action_rm_dir *) data;
  pid_clone_launch(rmdir_clone, death_of_rmdir_clone, NULL,
                   (void *) act, (void *) act, NULL, act->name, -1, 1);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void timeout_erase_dir_zombie(int vm_id, char *name)
{
  t_action_rm_dir *act;
  act = (t_action_rm_dir *) clownix_malloc(sizeof(t_action_rm_dir),12);
  memset(act, 0, sizeof(t_action_rm_dir));
  act->vm_id = vm_id; 
  strcpy(act->name, name);
  clownix_timeout_add(200, action_rm_dir_timed, (void *) act, NULL, NULL);
  inc_lock_self_destruction_dir();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void timeout_start_vm_create_automaton(void *data)
{
  char *vm_name = (char *) data;
  t_vm   *vm = cfg_get_vm(vm_name);
  t_wake_up_eths *wake_up_eths;
  if (!vm)
    {
    KERR("POSSIBLE?");
    recv_coherency_unlock();
    }
  else
    {
    wake_up_eths = vm->wake_up_eths;
    if (!wake_up_eths)
      {
      KERR("POSSIBLE?");
      recv_coherency_unlock();
      }
    else
      {
      if (strcmp(wake_up_eths->name, vm_name))
        KOUT("%s %s", wake_up_eths->name, vm_name);
      else 
        qemu_vm_automaton(NULL, 0, vm->kvm.name);
      }
    }
  clownix_free(data, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void start_lock_and_watchdog(int llid, int tid, t_vm *vm, char *err)
{
  t_wake_up_eths *data;
  if (!vm)
    KOUT(" ");
  utils_chk_my_dirs(vm);
  event_print("Making cmd line for %s", vm->kvm.name);
  data = (t_wake_up_eths *) clownix_malloc(sizeof(t_wake_up_eths), 13);
  memset(data, 0, sizeof(t_wake_up_eths));
  data->state = 0;
  data->llid = llid;
  data->tid = tid;
  strcpy(data->name, vm->kvm.name);
  vm->wake_up_eths = data;
  cfg_set_vm_locked(vm);
  cdrom_config_creation_request(vm, vm->kvm.vm_config_flags);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int run_linux_virtual_machine(int llid, int tid, char *name, 
                              t_vm *vm, char *err)
{
  int result = -1;
  start_lock_and_watchdog(llid, tid, vm, err);
  result = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int kerr_upon_missing_dir(char *name, char *dir)
{
  int result = 0;
  struct stat stat_file;
  if (stat(dir, &stat_file))
    {
    KERR("%s %s %d", name, dir, errno);
    result = -1;
    }
  else if (!S_ISDIR(stat_file.st_mode))
    {
    KERR("%s %s", name, dir);
    result = -1;
    }
  else if ((stat_file.st_mode & S_IRWXU) != S_IRWXU)
    {
    KERR("%s %s %X", name, dir, stat_file.st_mode);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int missing_dir(char *name, int vm_id)
{
  int result = 0;
  char path[MAX_PATH_LEN];
  snprintf(path, MAX_PATH_LEN-1, "%s", cfg_get_work_vm(vm_id));
  if (kerr_upon_missing_dir(name, path))
    result = 1;
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", cfg_get_work_vm(vm_id), DIR_CONF);
  if (kerr_upon_missing_dir(name, path))
    result = 1;
  snprintf(path, MAX_PATH_LEN-1, "%s", utils_dir_conf_tmp(vm_id));
  if (kerr_upon_missing_dir(name, path))
    result = 1;
  snprintf(path, MAX_PATH_LEN-1, "%s", utils_get_disks_path_name(vm_id));
  if (kerr_upon_missing_dir(name, path))
    result = 1;
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", cfg_get_work_vm(vm_id),  DIR_UMID);
  if (kerr_upon_missing_dir(name, path))
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void death_of_mkdir_clone(void *data, int status, char *name)
{
  char err[MAX_PATH_LEN];
  t_vm *vm;
  t_vm_building *vm_building = (t_vm_building *) data;
  cfg_del_newborn(name);
  if (status)
    {
    sprintf(err,"Path: \"%s\" pb creating directory", 
                cfg_get_work_vm(vm_building->vm_id));
    send_status_ko(vm_building->llid, vm_building->tid, err);
    KERR("%s", err);
    clownix_free(vm_building,  __FUNCTION__);
    recv_coherency_unlock();
    }
  else if (missing_dir(vm_building->kvm.name, vm_building->vm_id))
    {
    sprintf(err,"Bad vm %s dir creation", vm_building->kvm.name);
    send_status_ko(vm_building->llid, vm_building->tid, err);
    KERR("%s", err);
    clownix_free(vm_building,  __FUNCTION__);
    recv_coherency_unlock();
    }
  else
    {
    event_print("Directories for %s created", vm_building->kvm.name);
    cfg_set_vm(&(vm_building->kvm),
                vm_building->vm_id, vm_building->llid);
    vm = cfg_get_vm(vm_building->kvm.name);
    if (!vm)
      KOUT(" ");

    if (!run_linux_virtual_machine(vm_building->llid, vm_building->tid,
                                   vm_building->kvm.name, vm, err))
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    else
      {
      send_status_ko(vm_building->llid, vm_building->tid, err);
      KERR("%s", err);
      machine_death(vm_building->kvm.name, error_death_run);
      recv_coherency_unlock();
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int mkdir_clone(void *data)
{
  int result;
  t_vm_building *vm_building = (t_vm_building *) data;
  result = mk_machine_dirs(vm_building->kvm.name, vm_building->vm_id);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void machine_recv_add_vm(int llid, int tid, t_topo_kvm *kvm, int vm_id)
{
  t_vm_building *vm_building;
  vm_building = (t_vm_building *) clownix_malloc(sizeof(t_vm_building), 19);
  memset(vm_building, 0, sizeof(t_vm_building));
  vm_building->llid = llid;
  vm_building->tid  = tid;
  memcpy(&(vm_building->kvm), kvm, sizeof(t_topo_kvm));
  vm_building->vm_id  = vm_id;
  pid_clone_launch(mkdir_clone, death_of_mkdir_clone, NULL,
                   (void *) vm_building, (void *) vm_building, NULL, 
                   vm_building->kvm.name, -1, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void machine_death( char *name, int error_death)
{
  int i;
  t_vm *vm = cfg_get_vm(name);
  t_eth_table *eth_tab;
  if ((!vm) || (vm->vm_to_be_killed == 1))
    {
    KERR("%s %d", name, error_death);
    }
  else
    {
    vm->vm_to_be_killed = 1;
    if ((error_death) && 
        (error_death != error_death_qmp) &&
        (error_death != error_death_nopid) &&
        (error_death != error_death_pid_diseapeared) &&
        (error_death != error_death_qmonitor))
      KERR("%s %s %d ", __FUNCTION__, vm->kvm.name, error_death);
    else if ((error_death == error_death_noovs) ||
             (error_death == error_death_noovstime) ||
             (error_death == error_death_noovseth))
      {
      KERR("%s REQ KILL %s %d ", __FUNCTION__, vm->kvm.name, error_death);
      suid_power_kill_vm(vm->kvm.vm_id);
      }
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_CISCO)
      {
      edp_mngt_cisco_nat_destroy(name);
      }
    eth_tab = vm->kvm.eth_table;
    for (i=0; i<vm->kvm.nb_tot_eth; i++)
      {
      if (eth_tab[i].eth_type == eth_type_sock)
        endp_mngt_stop(vm->kvm.name, i);
      }
    for (i=0; i<vm->kvm.nb_tot_eth; i++)
      {
      if (eth_tab[i].eth_type == eth_type_dpdk)
        {
        dpdk_ovs_del_vm(vm->kvm.name);
        break;
        }
      else if (eth_tab[i].eth_type == eth_type_wlan)
        {
        endp_mngt_stop(vm->kvm.name, i);
        }
      }

    doors_send_del_vm(get_doorways_llid(), 0, vm->kvm.name);
    qhvc0_end_qemu_unix(vm->kvm.name);
    qmonitor_end_qemu_unix(vm->kvm.name);
    qmp_request_qemu_halt(vm->kvm.name, 0, 0);
    if (vm->pid_of_cp_clone)
      {
      KERR("CP ROOTFS SIGKILL %s, PID %d", vm->kvm.name, vm->pid_of_cp_clone);
      kill(vm->pid_of_cp_clone, SIGKILL);
      vm->pid_of_cp_clone = 0;
      }
    if (!cfg_is_a_zombie(vm->kvm.name))
      {
      stats_counters_sysinfo_vm_death(vm->kvm.name);
      cfg_add_zombie(vm->kvm.vm_id, vm->kvm.name);
      timeout_erase_dir_zombie(vm->kvm.vm_id, vm->kvm.name);
      if (!cfg_get_vm_locked(vm))
        {
        if (vm->wake_up_eths != NULL)
          KOUT(" ");
        }
      else
        {
        if (vm->wake_up_eths == NULL)
          KOUT(" ");
        vm->wake_up_eths->destroy_requested = 1;
        clownix_timeout_del(vm->wake_up_eths->abs_beat,
                            vm->wake_up_eths->ref, __FILE__, __LINE__);
        vm->wake_up_eths->abs_beat = 0;
        vm->wake_up_eths->ref = 0;
        free_wake_up_eths_and_delete_vm(vm, error_death_abort);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void dtach_duplicate_clone_msg(void *data, char *msg)
{
  char dtach_name[2*MAX_NAME_LEN];
  t_check_dtach_duplicate *dtach = (t_check_dtach_duplicate *) data;
  memset(dtach_name, 0, 2*MAX_NAME_LEN);
  snprintf(dtach_name, 2*MAX_NAME_LEN, "%s: ", dtach->name);
  dtach_name[2*MAX_NAME_LEN-1] = 0;
  if (!strncmp(msg, dtach_name, strlen(dtach_name)))
    strcpy(dtach->msg, "DUPLICATE");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void dtach_duplicate_clone_death(void *data, int status, char *name)
{
  t_check_dtach_duplicate *dtach = (t_check_dtach_duplicate *) data;
  if (!dtach->cb)
    KOUT(" ");
  if (!strcmp(dtach->msg, "DUPLICATE"))
    dtach->cb(-1, dtach->name);
  else
    dtach->cb(0, dtach->name);
  clownix_free(data, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int dtach_duplicate_clone(void *data)
{
  t_check_dtach_duplicate *dtach = (t_check_dtach_duplicate *) data;
  char *cmd = utils_get_dtach_bin_path();
  char *sock = utils_get_dtach_sock_path(dtach->name);

  char *argv[] = { cmd, "-n", sock, "ls", NULL, };
  my_popen(cmd, argv);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void dtach_duplicate_check(char *name, t_dtach_duplicate_callback cb)
{
  t_check_dtach_duplicate *dtach;
  int len = sizeof(t_check_dtach_duplicate);
  dtach = (t_check_dtach_duplicate *) clownix_malloc(len, 7);
  memset(dtach, 0, len);
  strncpy(dtach->name, name, MAX_NAME_LEN-1);
  dtach->cb = cb;
  pid_clone_launch(dtach_duplicate_clone, dtach_duplicate_clone_death,
                   dtach_duplicate_clone_msg, NULL, dtach, dtach, name, -1, 1);
}
/*---------------------------------------------------------------------------*/


