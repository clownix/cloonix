/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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
#include "qmp.h"
#include "doorways_mngt.h"
#include "ovs.h"
#include "kvm.h"
#include "suid_power.h"
#include "ovs_nat_main.h"
#include "wifi_air.h"

void uml_vm_automaton(void *unused_data, int status, char *name);
void qemu_vm_automaton(void *unused_data, int status, char *name);

typedef struct t_action_rm_dir
{
  int llid;
  int tid;
  int vm_id;
  int wait_count;
  char name[MAX_NAME_LEN];
} t_action_rm_dir;
typedef struct t_vm_building
{
  int llid;
  int tid;
  t_topo_kvm kvm;
  int vm_id;
  int ref_jfs;
  void *jfs;
} t_vm_building;

static void death_of_rmdir_clone(void *data, int status, char *name)
{
  t_action_rm_dir *act = (t_action_rm_dir *) data;
  int pid;
  t_vm *vm = find_vm_with_id(act->vm_id);
  if (vm)
    {
    pid = suid_power_get_pid(vm->kvm.vm_id);
    if (pid)
      KERR("ERROR %s: its directory is erased while qemu pid %d is still "
           "reported", name, pid);
    if (cfg_unset_vm(vm) != act->vm_id)
      KOUT(" ");
    cfg_free_obj_id(act->vm_id);
    cfg_hysteresis_send_topo_info();
    }
  if (cfg_is_a_zombie(act->name))
    cfg_del_zombie(act->name);
  else
    KOUT(" ");
  cfg_hysteresis_send_topo_info();
  dec_lock_self_destruction_dir();
  clownix_free(act, __FUNCTION__);
}

static int rmdir_clone(void *data)
{
  int result = 0;
  char info[MAX_PRINT_LEN];
  t_action_rm_dir *act = (t_action_rm_dir *) data;
  info[0] = 0;
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

static void action_rm_dir_timed(void *data)
{
  t_action_rm_dir *act = (t_action_rm_dir *) data;
  if(!suid_power_get_pid(act->vm_id))
    pid_clone_launch(rmdir_clone, death_of_rmdir_clone, NULL,
                     (void *) act, (void *) act, NULL, act->name, -1, 1);
  else
    {
    act->wait_count += 1;
    if ((act->wait_count % 30) == 0)
      {
      KERR("WARNING %s: qemu pid %d still alive %d s after its kill, its "
           "directory cannot be erased yet", act->name,
           suid_power_get_pid(act->vm_id), act->wait_count);
      suid_power_kill_vm(act->vm_id);
      }
    clownix_timeout_add(100, action_rm_dir_timed, (void *) act, NULL, NULL);
    }
}

static void timeout_erase_dir_zombie(int vm_id, char *name)
{
  t_action_rm_dir *act;
  act = (t_action_rm_dir *) clownix_malloc(sizeof(t_action_rm_dir),12);
  memset(act, 0, sizeof(t_action_rm_dir));
  act->vm_id = vm_id; 
  strcpy(act->name, name);
  clownix_timeout_add(100, action_rm_dir_timed, (void *) act, NULL, NULL);
  inc_lock_self_destruction_dir();
}

static void run_linux_virtual_machine(int llid, int tid, t_vm *vm)
{
  t_wake_up_eths *data;
  if (!vm)
    KOUT(" ");
  utils_chk_my_dirs(vm);
  data = (t_wake_up_eths *) clownix_malloc(sizeof(t_wake_up_eths), 22);
  memset(data, 0, sizeof(t_wake_up_eths));
  data->state = 0;
  data->llid = llid;
  data->tid = tid;
  strcpy(data->name, vm->kvm.name);
  vm->wake_up_eths = data;
  cfg_set_vm_locked(vm);
  qemu_vm_automaton(NULL, 0, vm->kvm.name);
}

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

static void death_of_mkdir_clone(void *data, int status, char *name)
{
  char err[MAX_PATH_LEN];
  t_vm *vm;
  t_vm_building *vm_building = (t_vm_building *) data;
  cfg_del_newborn(name);
  if (status)
    {
    snprintf(err, sizeof(err),"Path: \"%s\" pb creating directory", 
                cfg_get_work_vm(vm_building->vm_id));
    send_status_ko(vm_building->llid, vm_building->tid, err);
    KERR("ERROR %s", err);
    cfg_free_obj_id(vm_building->vm_id);
    clownix_free(vm_building,  __FUNCTION__);
    recv_coherency_unlock();
    }
  else if (missing_dir(vm_building->kvm.name, vm_building->vm_id))
    {
    snprintf(err, sizeof(err),"Bad vm %s dir creation", vm_building->kvm.name);
    send_status_ko(vm_building->llid, vm_building->tid, err);
    KERR("ERROR %s", err);
    cfg_free_obj_id(vm_building->vm_id);
    clownix_free(vm_building,  __FUNCTION__);
    recv_coherency_unlock();
    }
  else
    {
    cfg_set_vm(&(vm_building->kvm),
                vm_building->vm_id, vm_building->llid);
    vm = cfg_get_vm(vm_building->kvm.name);
    if (!vm)
      KOUT(" ");
    run_linux_virtual_machine(vm_building->llid, vm_building->tid, vm);
    kvm_add_whole_vm(vm->kvm.name, vm->kvm.nb_tot_eth, vm->kvm.eth_table);
    cfg_hysteresis_send_topo_info();
    /* The success path frees vm_building too: cfg_set_vm copied the kvm
       struct into the config. */
    clownix_free(vm_building, __FUNCTION__);
    }
}

static int mkdir_clone(void *data)
{
  int result;
  t_vm_building *vm_building = (t_vm_building *) data;
  result = mk_machine_dirs(vm_building->kvm.name, vm_building->vm_id);
  return result;
}

void machine_recv_add_vm(int llid, int tid, t_topo_kvm *kvm, int vm_id)
{
  t_vm_building *vm_building;
  vm_building = (t_vm_building *) clownix_malloc(sizeof(t_vm_building), 31);
  memset(vm_building, 0, sizeof(t_vm_building));
  vm_building->llid = llid;
  vm_building->tid  = tid;
  memcpy(&(vm_building->kvm), kvm, sizeof(t_topo_kvm));
  vm_building->vm_id  = vm_id;
  pid_clone_launch(mkdir_clone, death_of_mkdir_clone, NULL,
                   (void *) vm_building, (void *) vm_building, NULL, 
                   vm_building->kvm.name, -1, 1);
}

void machine_death( char *name, int error_death)
{
  t_vm *vm = cfg_get_vm(name);
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
        (error_death != error_death_pid_diseapeared))
      KERR("%s %s %d ", __FUNCTION__, vm->kvm.name, error_death);
    suid_power_kill_vm(vm->kvm.vm_id);
    wifi_air_kvm_gone(vm);
    doors_send_del_vm(get_doorways_llid(), 0, vm->kvm.name);
    if (!cfg_is_a_zombie(vm->kvm.name))
      {
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
        free_wake_up_eths_and_delete_vm(vm, error_death_abort, NULL);
        }
      }
    }
}

