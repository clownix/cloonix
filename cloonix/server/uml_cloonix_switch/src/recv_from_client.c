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
#include "qmp.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "xwy.h"
#include "hop_event.h"
#include "ovs.h"
#include "kvm.h"
#include "suid_power.h"
#include "list_commands.h"
#include "cnt.h"
#include "ovs_snf.h"
#include "ovs_nat_main.h"
#include "ovs_a2b.h"
#include "ovs_tap.h"
#include "ovs_phy.h"
#include "ovs_c2c.h"
#include "msg.h"
#include "crun.h"
#include "novnc.h"
#include "proxymous.h"
#include "cloonix_conf_info.h"


t_cloonix_conf_info *get_cloonix_conf_info(void);
static void recv_promiscious(int llid, int tid, char *name, int eth, int on);
int inside_cloonix(char **name);
extern int clownix_server_fork_llid;
static int g_inside_cloonix;
static char *g_cloonix_vm_name;

int get_uml_cloonix_started(void);
void shadown_lan_del_all(void);

/*****************************************************************************/
typedef struct t_timer_kvm_delete
{
  char name[MAX_NAME_LEN];
  int llid;
  int tid;
  int count;
} t_timer_kvm_delete;
/*---------------------------------------------------------------------------*/
typedef struct t_timer_del
{
  int llid;
  int tid;
  int tzcount;
  int kill_cloon;
} t_timer_del;
/*---------------------------------------------------------------------------*/
typedef struct t_timer_zombie
{
  int llid;
  int tid;
  int nb_try;
  int count;
  t_topo_kvm kvm;
} t_timer_zombie;
/*---------------------------------------------------------------------------*/
typedef struct t_add_vm_cow_look
{
  char msg[MAX_PATH_LEN];
  int llid;
  int tid;
  int vm_id;
  t_topo_kvm kvm;
} t_add_vm_cow_look;
/*---------------------------------------------------------------------------*/
static int g_inhib_new_clients;

/*****************************************************************************/
static int g_killing_cloonix;
int get_killing_cloonix(void)
{
  return g_killing_cloonix;
}

/*****************************************************************************/
int get_inhib_new_clients(void)
{
  if (g_inhib_new_clients)
    {
    KERR("ERROR Server being killed, no new client");
    event_print("Server being killed, no new client");
    }
  return g_inhib_new_clients;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_novnc_on_off(int llid, int tid, int on)
{
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (on)
    {
    if (start_novnc() == 0)
      send_status_ok(llid, tid, "novnc_on");
    else
      send_status_ko(llid, tid, "Error novnc_on");
    }
  else
    {
    if (end_novnc(1) == 0)
      send_status_ok(llid, tid, "novnc_off");
    else
      send_status_ko(llid, tid, "Error novnc_off");
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
  char info[MAX_PATH_LEN];
  t_vm *vm;
  vm = cfg_get_vm(name);
  event_print("Rx Req promisc %d for  %s eth%d", on, name, eth);
  if (!vm)
    {
    sprintf( info, "Machine %s does not exist", name);
    send_status_ko(llid, tid, info);
    }
  else if ((eth < 0) || (eth >= vm->kvm.nb_tot_eth))
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
static void timer_poweroff_eth(void *data)
{
  t_timer_kvm_delete *kd = (t_timer_kvm_delete *) data;
  t_vm *vm = cfg_get_vm(kd->name);
  t_eth_table *eth_tab;
  int i, can_delete, glob_can_delete;
  if (vm == NULL)
    {
    KERR("ERROR timer_kvm_delete %s", kd->name);
    clownix_free(kd, __FILE__);
    }
  else if (msg_ovsreq_get_qty() > 50)
    clownix_timeout_add(10, timer_poweroff_eth, (void *) kd, NULL, NULL);
  else
    {
    eth_tab = vm->kvm.eth_table;
    glob_can_delete = 1;
    for (i=0; i<vm->kvm.nb_tot_eth; i++)
      {
      kvm_del(0, 0, vm->kvm.name, i, eth_tab[i].endp_type, &can_delete);
      if (can_delete == 0)
        glob_can_delete = 0;
      if (kvm_lan_added_exists(kd->name, i))
        glob_can_delete = 0;
      }

    if (msg_ovsreq_exists_name(kd->name))
      glob_can_delete = 0;

    if ((glob_can_delete == 0) && (kd->count < 2000))
      {
      clownix_timeout_add(10, timer_poweroff_eth, (void *) kd, NULL, NULL);
      kd->count += 1;
      }
    else
      {
      if (glob_can_delete == 0)
        KERR("ERROR %s", kd->name);
      if (vm->vm_to_be_killed == 0)
        machine_death(kd->name, error_death_noerr);
      if (kd->llid)
        send_status_ok(kd->llid, kd->tid, "OK");
      clownix_free(kd, __FILE__);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void poweroff_vm(int llid, int tid, t_vm *vm)
{
  t_timer_kvm_delete *kd;
  if (vm->vm_poweroff_done == 0)
    {
    vm->vm_poweroff_done = 1;
    kd = (t_timer_kvm_delete *) clownix_malloc(sizeof(t_timer_kvm_delete), 3);
    memset(kd, 0, sizeof(t_timer_kvm_delete));
    kd->llid = llid;
    kd->tid  = tid;
    strncpy(kd->name, vm->kvm.name, MAX_NAME_LEN-1);
    timer_poweroff_eth((void *) kd);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_vmcmd(int llid, int tid, char *name, int cmd, int param)
{
  t_vm *vm = cfg_get_vm(name);
  char err[MAX_PATH_LEN];
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  switch(cmd)
    {
    case vmcmd_reboot_with_qemu:
      if (vm)
        {
        qmp_request_qemu_reboot(name);
        send_status_ok(llid, tid, "reboot");
        }
      else
        {
        snprintf(err, MAX_PATH_LEN-1, "ERROR: %s NOT FOUND", name);
        send_status_ko(llid, tid, err);
        }
      break;

    case vmcmd_promiscious_flag_set:
      recv_promiscious(llid, tid, name, param, 1);
      break;

    case vmcmd_promiscious_flag_unset:
      recv_promiscious(llid, tid, name, param, 0);
      break;

    default:
      KERR("ERROR %d", cmd);
      break;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_evt_print_sub(int llid, int tid)
{
  event_print("Rx Req subscribing to print for client: %d", llid);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

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
    cfg_hysteresis_send_topo_info();
    }
  else
    send_status_ko(llid, tid, "Abnormal!");
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
  if (file_exists(SYS_KVM_DEV, R_OK))
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
static void inside_cloonix_test_dev_kvm(char *err)
{
  int major, minor;
  char min[MAX_NAME_LEN];
  char *argv[10];
  memset(argv, 0, 10*sizeof(char *));
  memset(min, 0, MAX_NAME_LEN);
  if (!get_dev_kvm_major_minor(&major, &minor, err))
    {
    snprintf(min, MAX_NAME_LEN-1, "%d", minor);
    if (major == 10)
      {
      argv[0] = pthexec_mknod_bin();
      argv[1] = "/dev/kvm"; 
      argv[2] = "c"; 
      argv[3] = "10"; 
      argv[4] = min; 
      argv[5] = NULL; 
      if (!lio_system(argv))
        {
        argv[0] = pthexec_chmod_bin();
        argv[1] = "666"; 
        argv[2] = "/dev/kvm"; 
        argv[3] = NULL; 
        if (!lio_system(argv))
          KERR("ERROR %s 666 /dev/kvm", pthexec_chmod_bin());
        }
      else
        {
        sprintf(err, "%s /dev/kvm c %d %d", pthexec_mknod_bin(), major, minor);
        KERR("ERROR %s", err);
        }
      }
    else
      {
      sprintf(err, "/dev/kvm: %d:%d major not 10, wrong\n", major, minor);
      KERR("ERROR %s", err);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int module_access_is_ko(char *dev_file)
{
  int result = -1;
  int fd;
  if (access( dev_file, R_OK))
    KERR("ERROR %s not found", dev_file);
  else if (!i_have_read_write_access(dev_file))
    KERR("ERROR %s not writable", dev_file);
  else
    {
    fd = open(dev_file, O_RDWR);
    if (fd >= 0)
      {
      result = 0;
      close(fd);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int test_dev_kvm(char *info)
{
  int result = -1;
  if (test_machine_is_kvm_able())
    {
    if (g_inside_cloonix)
      inside_cloonix_test_dev_kvm(info);
    if (module_access_is_ko("/dev/kvm"))
      {
      KERR("ERROR KO /dev/kvm, Maybe: chmod 0666 /dev/kvm");
      KERR("OR: sudo setfacl -m u:${USER}:rw /dev/kvm");
      }
    else
      result = 0;
    }
  else
    {
    sprintf(info, "No hardware virtualisation! kvm needs it!\n");
    KERR("ERROR KO hardware virtualisation");
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
  snprintf(qemu_kvm_exe, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/cloonix-qemu-system", cfg_get_bin_dir());
  if (test_dev_kvm(info))
    result = -1;
  else if (!file_exists(qemu_kvm_exe, R_OK))
    {
    sprintf(info, "File: \"%s\" not found\n", qemu_kvm_exe);
    result = -1;
    }
  else if (strlen(bz_image) && (!file_exists(bz_image, R_OK)))
    {
    sprintf(info, "File: \"%s\" not found\n", bz_image);
    result = -1;
    }
  else if (!file_exists(rootfs, R_OK))
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
  else if (kvm->vm_config_flags & VM_CONFIG_FLAG_I386)
    {
    KERR("ERROR NOT MANAGED I386");
    sprintf(info, "ERROR NOT MANAGED I386\n");
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int test_topo_kvm(t_topo_kvm *kvm, int vm_id, char *info, int nb_eth)
{
  int result = 0;
  char rootfs[2*MAX_PATH_LEN];
  if (kvm->cpu == 0)
    kvm->cpu =  1;
  if (kvm->mem == 0)
    kvm->mem =  128;
  if (kvm->vm_config_flags & VM_CONFIG_FLAG_INSTALL_CDROM)
    {
    if (!file_exists(kvm->install_cdrom, R_OK))
      {
      sprintf(info, "File: \"%s\" not found\n", kvm->install_cdrom);
      result = -1;
      }
    }
  if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_CDROM)
    {
    if (!file_exists(kvm->added_cdrom, R_OK))
      {
      sprintf(info, "File: \"%s\" not found\n", kvm->added_cdrom);
      result = -1;
      }
    }
  if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK)
    {
    if (!file_exists(kvm->added_disk, R_OK))
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
      if (file_exists(kvm->rootfs_input, R_OK))
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
    else
      {
      kvm->vm_config_flags |= VM_FLAG_DERIVED_BACKING;
      if (file_exists(kvm->rootfs_input, R_OK))
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
      "Rx Req add kvm machine %s with tot_eth=%d FLAGS:%s",
       kvm->name, kvm->nb_tot_eth, prop_flags_ascii_get(kvm->vm_config_flags));
  if (!result)
    {
    if (nb_eth > MAX_ETH_VM)
      {
      sprintf(info, "Maximum ethernet %d per machine", MAX_ETH_VM);
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
      strncpy(add_vm->msg, ptr, MAX_PATH_LEN-1);
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
  char *cmd = pthexec_cloonfs_qemu_img();
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
  clone_popen(cmd, argv);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delayed_add_vm(t_timer_zombie *tz)
{
  int i, vm_id, result = -1;
  int nb_eth = 0;
  char mac[6];
  char info[MAX_PATH_LEN];
  char natplug_name[2*MAX_NAME_LEN];
  char lan_natplug_name[2*MAX_NAME_LEN];
  char use[MAX_PATH_LEN];
  t_add_vm_cow_look *cow_look;
  int llid = tz->llid, tid = tz->tid;
  t_topo_kvm *kvm = &(tz->kvm);
  t_vm   *vm = cfg_get_vm(kvm->name);
  char *npn = natplug_name;
  char *lnpn = lan_natplug_name;
  info[0] = 0;
  memset(mac, 0, 6);
  memset(natplug_name, 0, 2*MAX_NAME_LEN);
  memset(lan_natplug_name, 0, 2*MAX_NAME_LEN);
  snprintf(natplug_name, 2*MAX_NAME_LEN-1, "nat_%s", kvm->name);
  snprintf(lan_natplug_name, 2*MAX_NAME_LEN-1, "lan_nat_%s", kvm->name);
  natplug_name[MAX_NAME_LEN-1] = 0;
  lan_natplug_name[MAX_NAME_LEN-1] = 0;
  if (cfg_is_a_zombie(kvm->name))
    KOUT("%s", kvm->name);
  if (vm)
    KOUT("%s", kvm->name);
  if (get_inhib_new_clients())
    {
    recv_coherency_unlock();
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    KERR("ERROR %s", kvm->name);
    }
  else if (cfg_name_is_in_use(0, kvm->name, use))
    {
    recv_coherency_unlock();
    send_status_ko(llid, tid, use);
    KERR("ERROR %s", kvm->name);
    }
  else if (cfg_is_a_newborn(kvm->name))
    {
    sprintf( info, "Machine: \"%s\" is a newborn", kvm->name);
    event_print("%s", info);
    recv_coherency_unlock();
    send_status_ko(llid, tid, info);
    KERR("ERROR %s", kvm->name);
    }
  else if ((kvm->vm_config_flags & VM_CONFIG_FLAG_NATPLUG) && 
           ((cfg_name_is_in_use(0, natplug_name, use)) ||
            (cfg_name_is_in_use(0, lan_natplug_name, use))))
    {
    sprintf(info, "Cisco needs nat \"%s\" or \"%s\", exists", npn, lnpn);
    event_print("%s", info);
    recv_coherency_unlock();
    send_status_ko(llid, tid, info);
    KERR("ERROR %s", kvm->name);
    }
  else if (utils_get_eth_numbers(kvm->nb_tot_eth, kvm->eth_table,
                                 &nb_eth))
    {
    sprintf(info, "Bad eth_table %s", kvm->name);
    event_print("%s", info);
    recv_coherency_unlock();
    send_status_ko(llid, tid, info);
    KERR("ERROR %s", kvm->name);
    }
  else
    {
    cfg_add_newborn(kvm->name);
    vm_id = cfg_alloc_obj_id();
    event_print("%s was allocated number %d", kvm->name, vm_id);
    for (i=0; i < kvm->nb_tot_eth; i++)
      {
      if (!memcmp(kvm->eth_table[i].mac_addr, mac, 6))
        { 
        if (g_inside_cloonix)
          {
          kvm->vm_config_flags |= VM_FLAG_IS_INSIDE_CLOON;
          kvm->eth_table[i].mac_addr[0] = 0x72;
          }
        else
          {
          kvm->eth_table[i].mac_addr[0] = 0x2;
          }
        kvm->eth_table[i].mac_addr[1] = 0xFF & rand();
        kvm->eth_table[i].mac_addr[2] = 0xFF & rand();
        kvm->eth_table[i].mac_addr[3] = 0xFF & rand();
        kvm->eth_table[i].mac_addr[4] = vm_id%100;
        kvm->eth_table[i].mac_addr[5] = i;
        kvm->eth_table[i].randmac = 1;
        }
      memset(kvm->eth_table[i].vhost_ifname, 0, MAX_NAME_LEN);
      snprintf(kvm->eth_table[i].vhost_ifname, (MAX_NAME_LEN-1),
               "%s%d_%d", OVS_BRIDGE_PORT, vm_id, i);
      }
    result = test_topo_kvm(kvm, vm_id, info, nb_eth);
    if (result)
      {
      send_status_ko(llid, tid, info);
      recv_coherency_unlock();
      cfg_del_newborn(kvm->name);
      KERR("ERROR %s %s", kvm->name, info);
      }
    else
      {
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
  clownix_free(tz, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_zombie(void *data)
{
  char err[MAX_PATH_LEN];
  t_timer_zombie *tz = (t_timer_zombie *) data;
  tz->count++;
  if (tz->count >= 30)
    {
    sprintf(err, "zombie too long to die: %s", tz->kvm.name);
    send_status_ko(tz->llid, tz->tid, err);
    clownix_free(tz, __FUNCTION__);
    recv_coherency_unlock();
    KERR("ERROR %s", tz->kvm.name);
    }
  else
    {
    if (cfg_is_a_zombie(tz->kvm.name))
      clownix_timeout_add(20, timer_zombie, (void *) tz, NULL, NULL);
    else
      delayed_add_vm(tz);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_add_vm(int llid, int tid, t_topo_kvm *kvm)
{
  t_vm   *vm = cfg_get_vm(kvm->name);
  t_timer_zombie *tz;
  char err[MAX_PATH_LEN];
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (vm)
    {
    sprintf(err, "Machine: \"%s\" already exists", kvm->name);
    event_print("%s", err);
    KERR("ERROR %s", err);
    send_status_ko(llid, tid, err);
    }
  else
    {
    tz = (t_timer_zombie *) clownix_malloc(sizeof(t_timer_zombie), 3);
    memset(tz, 0, sizeof(t_timer_zombie));
    tz->llid = llid;
    tz->tid = tid;
    memcpy(&(tz->kvm), kvm, sizeof(t_topo_kvm));
    clownix_timeout_add(1, timer_zombie, (void *) tz, NULL, NULL);
    recv_coherency_lock();
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_list_commands_req(int llid, int tid, int is_layout)
{
  t_list_commands *list;
  int qty, alloc_len = MAX_LIST_COMMANDS_QTY * sizeof(t_list_commands);
  list = (t_list_commands *) clownix_malloc(alloc_len, 7);
  memset(list, 0, alloc_len);
  qty = produce_list_commands(list, is_layout);
  send_list_commands_resp(llid, tid, qty, list);
  clownix_free(list, __FILE__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_pid_lst *create_list_pid(int *nb)
{
  int i,j, nb_vm, nb_cnt, nb_snf, nb_a2b, nb_c2c, nb_sum, nb_ovs;
  int nb_nat_main;
  t_lst_pid *proxymous_pid = NULL;
  t_lst_pid *ovs_pid = NULL;
  t_lst_pid *cnt_pid = NULL;
  t_lst_pid *snf_pid = NULL;
  t_lst_pid *nat_main_pid = NULL;
  t_lst_pid *a2b_pid = NULL;
  t_lst_pid *c2c_pid = NULL;
  t_vm *vm = cfg_get_first_vm(&nb_vm);
  t_pid_lst *lst;
  nb_cnt = cnt_get_all_pid(&cnt_pid);
  nb_ovs = ovs_get_all_pid(&ovs_pid);
  nb_snf = ovs_snf_get_all_pid(&snf_pid);
  nb_nat_main = ovs_nat_main_get_all_pid(&nat_main_pid);
  nb_a2b = ovs_a2b_get_all_pid(&a2b_pid);
  nb_c2c = ovs_c2c_get_all_pid(&c2c_pid);
  nb_sum = nb_ovs + nb_vm + nb_cnt + nb_snf + nb_a2b + nb_c2c + 10;
  nb_sum += nb_nat_main ;
  lst = (t_pid_lst *)clownix_malloc(nb_sum*sizeof(t_pid_lst),18);
  proxymous_get_pid(&proxymous_pid);
  memset(lst, 0, nb_sum*sizeof(t_pid_lst));
  for (i=0, j=0; i<nb_vm; i++)
    {
    if (!vm)
      KOUT(" ");
    strcpy(lst[j].name, "vmpid:");
    strncpy(lst[j].name, vm->kvm.name, MAX_NAME_LEN-7);
    lst[j].pid = suid_power_get_pid(vm->kvm.vm_id);
    j++; 
    vm = vm->next;
    }
  if (vm)
    KOUT(" ");

  for (i=0 ; i<nb_cnt; i++)
    {
    strncpy(lst[j].name, cnt_pid[i].name, MAX_NAME_LEN-1);
    lst[j].pid = cnt_pid[i].pid;
    j++;
    }

  for (i=0 ; i<nb_snf; i++)
    {
    strncpy(lst[j].name, snf_pid[i].name, MAX_NAME_LEN-1);
    lst[j].pid = snf_pid[i].pid;
    j++;
    }

  for (i=0 ; i<nb_nat_main; i++)
    {
    strncpy(lst[j].name, nat_main_pid[i].name, MAX_NAME_LEN-1);
    lst[j].pid = nat_main_pid[i].pid;
    j++;
    }

  for (i=0 ; i<nb_a2b; i++)
    {
    strncpy(lst[j].name, a2b_pid[i].name, MAX_NAME_LEN-1);
    lst[j].pid = a2b_pid[i].pid;
    j++;
    }

  for (i=0 ; i<nb_c2c; i++)
    {
    strncpy(lst[j].name, c2c_pid[i].name, MAX_NAME_LEN-1);
    lst[j].pid = c2c_pid[i].pid;
    j++;
    }

  if (proxymous_pid)
    {
    strncpy(lst[j].name, proxymous_pid[0].name, MAX_NAME_LEN-1);
    lst[j].pid = proxymous_pid[0].pid;
    j++;
    }

  clownix_free(cnt_pid, __FUNCTION__);
  clownix_free(ovs_pid, __FUNCTION__);
  clownix_free(snf_pid, __FUNCTION__);
  clownix_free(nat_main_pid, __FUNCTION__);
  if (proxymous_pid)
    clownix_free(proxymous_pid, __FUNCTION__);
  clownix_free(a2b_pid, __FUNCTION__);
  clownix_free(c2c_pid, __FUNCTION__);
  strcpy(lst[j].name, "doors");
  lst[j].pid = doorways_get_distant_pid();
  j++;
  strcpy(lst[j].name, "xwy");
  lst[j].pid = xwy_pid();
  j++;
  strcpy(lst[j].name, "suid_power");
  lst[j].pid = suid_power_pid();
  j++;
  strcpy(lst[j].name, "cloonix_server");
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
  t_pid_lst *lst;
  event_print("Rx Req list pid");
  if (get_uml_cloonix_started())
    {
    lst = create_list_pid(&nb);
    }
  else
    {
    nb = 1;
    lst = (t_pid_lst *)clownix_malloc(sizeof(t_pid_lst),18);
    memset(lst, 0, sizeof(t_pid_lst));
    strcpy(lst[0].name, "server_not_ready");
    lst[0].pid = 0;
    }
  send_list_pid_resp(llid, tid, nb, lst);
  clownix_free(lst, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_sys_sub(int llid, int tid)
{
  event_print("Rx Req subscribing to system counters for client: %d", llid);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (msg_exist_channel(llid))
    {
    event_subscribe(sub_evt_sys, llid, tid);
    }
  else
    send_status_ko(llid, tid, "Abnormal!");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_sys_unsub(int llid, int tid)
{
  event_print("Rx Req unsubscribing from system for client: %d", llid);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

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
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

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
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

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
static void del_all_start(void)
{
  int i, nb;
  t_vm *vm = cfg_get_first_vm(&nb);
  g_inhib_new_clients = 1;
  event_print("Rx Req Self-Destruction");
  for (i=0; i<nb; i++)
    {
    poweroff_vm(0, 0, vm);
    vm = vm->next;
    }
  suid_power_delete_cnt_all();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_del_all_end(void *data)
{
  t_timer_del *td = (t_timer_del *) data;;
  if (td->kill_cloon)
    {
    ovs_destroy();
    auto_self_destruction(td->llid, td->tid);
    }
  else
    {
    g_inhib_new_clients = 0;
    send_status_ok(td->llid, td->tid, "delall");
    cfg_hysteresis_send_topo_info();
    clownix_free(td, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_del_all(void *data)
{ 
  t_ovs_nat_main *nat, *nnat;
  t_ovs_a2b *a2b, *na2b;
  t_ovs_c2c *c2c, *nc2c;
  t_ovs_tap *tap, *ntap;
  t_ovs_phy *phy, *nphy;
  t_vm *vm;
  int nb_vm, nb_cnt, nb_zombies, nb_nat, nb_a2b, nb_c2c, nb_tap, nb_phy;
  int i, found = 0;
  t_timer_del *td = (t_timer_del *) data;;
  vm = cfg_get_first_vm(&nb_vm);
  nb_cnt = suid_power_delete_cnt_all();
  nb_zombies = cfg_zombie_qty();
  tap = ovs_tap_get_first(&nb_tap);
  phy = ovs_phy_get_first(&nb_phy);
  nat = ovs_nat_main_get_first(&nb_nat);
  a2b = ovs_a2b_get_first(&nb_a2b);
  c2c = ovs_c2c_get_first(&nb_c2c);

  shadown_lan_del_all();

  for (i=0; i<nb_vm; i++)
    {
    poweroff_vm(0, 0, vm);
    vm = vm->next;
    }
  while(tap)
    {
    ntap = tap->next;
    ovs_tap_del(0, 0, tap->name); 
    tap = ntap;
    }
  while(phy)
    {
    nphy = phy->next;
    ovs_phy_del(0, 0, phy->vhost);
    phy = nphy;
    }
  while(nat)
    {
    nnat = nat->next;
    ovs_nat_main_del(0, 0, nat->name);
    nat = nnat;
    }
  while(a2b)
    {
    na2b = a2b->next;
    ovs_a2b_del(0, 0, a2b->name);
    a2b = na2b;
    }
  while(c2c)
    {
    nc2c = c2c->next;
    ovs_c2c_del(0, 7777, c2c->name);
    c2c = nc2c;
    }
  found = nb_vm+nb_cnt+nb_zombies+nb_tap+nb_phy+nb_nat+nb_a2b+nb_c2c;
  if (found)
    {
    td->tzcount += 1;
    if (td->tzcount == 300)
      KERR("WARNING %d %d %d", nb_vm, nb_cnt, nb_zombies);
    if (td->tzcount > 500)
      {
      KERR("ERROR %d %d %d", nb_vm, nb_cnt, nb_zombies);
      clownix_timeout_add(1, timer_del_all_end, (void *) td, NULL, NULL);
      }
    else
      clownix_timeout_add(100, timer_del_all, (void *) td, NULL, NULL);
    }
  else
    clownix_timeout_add(100, timer_del_all_end, (void *) td, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_del_all(int llid, int tid)
{
  t_timer_del *td;
  del_all_start();
  td = (t_timer_del *) clownix_malloc(sizeof(t_timer_del), 3);
  memset(td, 0, sizeof(t_timer_del));
  td->llid = llid;
  td->tid = tid;
  clownix_timeout_add(100, timer_del_all, (void *) td, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_kill_uml_clownix(int llid, int tid)
{ 
  t_timer_del *td;
  del_all_start();
  td = (t_timer_del *) clownix_malloc(sizeof(t_timer_del), 3);
  memset(td, 0, sizeof(t_timer_del));
  td->llid = llid;
  td->tid = tid;
  td->kill_cloon = 1;
  g_killing_cloonix = 1;
  clownix_timeout_add(100, timer_del_all, (void *) td, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_topo_small_event_sub(int llid, int tid)
{
  int i, nb;
  t_vm *cur;
  event_print("Req subscribing to Machine poll event for client: %d", llid);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

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
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (msg_exist_channel(llid))
    {
    event_unsubscribe(topo_small_event, llid);
    send_status_ok(llid, tid, "vmpollunsub");
    }
  else
    send_status_ko(llid, tid, "tid, Abnormal!");
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void recv_del_name(int llid, int tid, char *name)
{
  int nb_eth;
  event_print("Rx Req del %s", name);
  t_vm *vm = cfg_get_vm(name);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (vm)
    {
    event_print("Rx Req del machine %s", name);
    poweroff_vm(llid, tid, vm);
    }
  else if (cnt_name_exists(name, &nb_eth))
    {
    suid_power_delete_cnt(llid, tid, name);
    }
  else if (ovs_tap_exists(name))
    {
    ovs_tap_del(llid, tid, name);
    }
  else if (ovs_phy_exists_vhost(name))
    {
    ovs_phy_del(llid, tid, name);
    }
  else if (ovs_a2b_exists(name))
    {
    ovs_a2b_del(llid, tid, name);
    }
  else if (ovs_nat_main_exists(name))
    {
    ovs_nat_main_del(llid, tid, name);
    }
  else if (ovs_c2c_exists(name))
    {
    ovs_c2c_del(llid, tid, name);
    }
  else
    {
    send_status_ko(llid, tid, "NOT FOUND");
    KERR("ERROR %s", name);
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
void recv_sav_vm(int llid, int tid, char *name, char *path)
{
  t_vm   *vm = cfg_get_vm(name);
  char *dir_path = mydirname(path);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (!vm)
    {
    send_status_ko(llid, tid, "error MACHINE NOT FOUND");
    }
  else if (file_exists(path, R_OK))
    {
    send_status_ko(llid, tid, "error FILE ALREADY EXISTS");
    }
  else if (!file_exists(dir_path, W_OK))
    {
    send_status_ko(llid, tid, "error DIRECTORY NOT WRITABLE OR NOT FOUND");
    }
  else
    {
    qmp_request_save_rootfs(name, path, llid, tid);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_nat_add(int llid, int tid, char *name)
{
  char *locnet = cfg_get_cloonix_name();
  char err[MAX_PATH_LEN];
  event_print("Rx Req add nat %s", name);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (get_inhib_new_clients())
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    }
  else if (cfg_name_is_in_use(0, name, err))
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, err);
    }
  else
    {
    ovs_nat_main_add(llid, tid, name);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_a2b_add(int llid, int tid, char *name)
{
  char *locnet = cfg_get_cloonix_name();
  char err[MAX_PATH_LEN];
  event_print("Rx Req add a2b %s", name);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (get_inhib_new_clients())
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    }
  else if (cfg_name_is_in_use(0, name, err))
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, err);
    }
  else
    {
    ovs_a2b_add(llid, tid, name);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_c2c_add(int llid, int tid, char *name, uint32_t loc_udp_ip,
                  char *dist, uint32_t dist_ip, uint16_t dist_port,
                  char *dist_passwd, uint32_t dist_udp_ip,
                  uint16_t c2c_udp_port_low)
{
  char *locnet = cfg_get_cloonix_name();
  t_cloonix_conf_info *conf = get_cloonix_conf_info();
  char err[MAX_PATH_LEN];
  event_print("Rx Req add c2c %s", name);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (get_inhib_new_clients())
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    }
  else if (cfg_name_is_in_use(0, name, err))
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, err);
    }
  else
    {
    if (conf->c2c_udp_port_low != c2c_udp_port_low)
      KERR("ERROR %hu %hu", conf->c2c_udp_port_low, c2c_udp_port_low);
    ovs_c2c_add(llid, tid, name, loc_udp_ip,
                dist, dist_ip, dist_port, dist_passwd,
                dist_udp_ip, c2c_udp_port_low); 
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_snf_add(int llid, int tid, char *name, int num, int val)
{
  char *locnet = cfg_get_cloonix_name();
  char err[MAX_PATH_LEN];
  t_vm   *vm = cfg_get_vm(name);
  int nb_eth;
  t_ovs_tap *tap_exists = ovs_tap_exists(name);
  t_ovs_phy *phy_exists = ovs_phy_exists_vhost(name);
  t_ovs_nat_main *nat_exists = ovs_nat_main_exists(name);
  t_ovs_a2b *a2b_exists = ovs_a2b_exists(name);
  t_ovs_c2c *c2c_exists = ovs_c2c_exists(name);
  event_print("Rx Req add snf %s %d %d", name, num, val);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (get_inhib_new_clients())
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    }
  else if (nat_exists)
    {
    if (num != 0)
      {
      KERR("ERROR %s %s", locnet, name);
      send_status_ko(llid, tid, "bad num");
      }
    else if (ovs_nat_main_dyn_snf(name, val))
      {
      KERR("ERROR %s %s", locnet, name);
      send_status_ko(llid, tid, "error");
      }
    else
      {
      send_status_ok(llid, tid, "ok");
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (a2b_exists)
    {
    if ((num != 0) && ((num != 1)))
      {
      KERR("ERROR %s %s", locnet, name);
      send_status_ko(llid, tid, "bad num");
      }
    else if (ovs_a2b_dyn_snf(name, num, val))
      {
      KERR("ERROR %s %s", locnet, name);
      send_status_ko(llid, tid, "error");
      }
    else
      {
      send_status_ok(llid, tid, "ok");
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (tap_exists)
    {
    if (num != 0)
      {
      KERR("ERROR %s %s", locnet, name);
      send_status_ko(llid, tid, "bad num");
      }
    else if (ovs_tap_dyn_snf(name, val))
      {
      KERR("ERROR %s %s", locnet, name);
      send_status_ko(llid, tid, "error");
      }
    else
      {
      send_status_ok(llid, tid, "ok");
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (phy_exists)
    {
    if (ovs_phy_dyn_snf(name, val))
      {
      KERR("ERROR %s %s %d", locnet, name, val);
      send_status_ko(llid, tid, "error");
      }
    else
      {
      send_status_ok(llid, tid, "ok");
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (c2c_exists)
    {
    if (num != 0)
      {
      KERR("ERROR %s %s", locnet, name);
      send_status_ko(llid, tid, "bad num");
      }
    else if (ovs_c2c_dyn_snf(name, val))
      {
      KERR("ERROR %s %s", locnet, name);
      send_status_ko(llid, tid, "error");
      }
    else
      {
      send_status_ok(llid, tid, "ok");
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (vm)
    {
    if ((num < 0) || (num >= vm->kvm.nb_tot_eth))
      {
      sprintf( err, "eth%d for machine %s does not exist", num, name);
      send_status_ko(llid, tid, err);
      }
    else if (kvm_dyn_snf(name, num, val))
      {
      KERR("ERROR %s %s %d", locnet, name, num);
      send_status_ko(llid, tid, "error");
      }
    else
      {
      send_status_ok(llid, tid, "ok");
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (cnt_name_exists(name, &nb_eth))
    {
    if ((num < 0) || (num >= nb_eth))
      {
      sprintf( err, "eth%d for machine %s does not exist", num, name);
      send_status_ko(llid, tid, err);
      }
    else if (cnt_dyn_snf(name, num, val))
      {
      KERR("ERROR %s %s %d", locnet, name, num);
      send_status_ko(llid, tid, "error");
      }
    else
      {
      send_status_ok(llid, tid, "ok");
      cfg_hysteresis_send_topo_info();
      }
    }
  else
    {
    KERR("ERROR %s %s %d", locnet, name, num);
    send_status_ko(llid, tid, "error");
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_phy_add(int llid, int tid, char *name, int type)
{
  char *locnet = cfg_get_cloonix_name();
  char err[MAX_PATH_LEN];
  event_print("Rx Req add phy %s", name);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (get_inhib_new_clients())
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    }
  else if (cfg_name_is_in_use(0, name, err))
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, err);
    }
  else if (!suid_power_info_phy_exists(name))
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, "recv_phy_add name unknown");
    }
  else
    {
    ovs_phy_add(llid, tid, name, type);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_tap_add(int llid, int tid, char *name)
{
  char *locnet = cfg_get_cloonix_name();
  char err[MAX_PATH_LEN];
  event_print("Rx Req add tap %s", name);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (get_inhib_new_clients())
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    }
  else if (cfg_name_is_in_use(0, name, err))
    {
    KERR("ERROR %s %s", locnet, name);
    send_status_ko(llid, tid, err);
    }
  else
    {
    ovs_tap_add(llid, tid, name);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_c2c_cnf(int llid, int tid, char *name, char *cmd)
{
  uint8_t mac[6];
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (strncmp("mac_mangle=", cmd, strlen("mac_mangle=")))
    {
    send_status_ko(llid, tid, cmd);
    KERR("ERROR %s %s ", name, cmd);
    }
  else if (sscanf(cmd, "mac_mangle=%hhX:%hhX:%hhX:%hhX:%hhX:%hhX",
                  &(mac[0]), &(mac[1]), &(mac[2]),
                  &(mac[3]), &(mac[4]), &(mac[5])) != 6)
    {
    send_status_ko(llid, tid, cmd);
    KERR("ERROR %s %s ", name, cmd);
    }
  else if (ovs_c2c_mac_mangle(name, mac))
    {
    send_status_ko(llid, tid, name);
    KERR("ERROR %s %s ", name, cmd);
    }
  else
    send_status_ok(llid, tid, "OK");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_nat_cnf(int llid, int tid, char *name, char *cmd)
{
  t_vm *vm;
  char *ptr;
  event_print("Rx Req cnf nat %s %s", name, cmd);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (!ovs_nat_main_exists(name))
    {
    send_status_ko(llid, tid, "error nat not found");
    KERR("ERROR %s %s ", name, cmd);
    }
  else if (strncmp("whatip=", cmd, strlen("whatip=")))
    {
    send_status_ko(llid, tid, "error nat bad cmd");
    KERR("ERROR %s %s ", name, cmd);
    }
  else
    {
    ptr = &(cmd[strlen("whatip=")]);
    if (strlen(ptr) <= 1)
      {
      send_status_ko(llid, tid, "error nat cnf cmd");
      KERR("ERROR %s %s ", name, cmd);
      }
    else
      {
      vm = cfg_get_vm(ptr);
      if (vm == NULL)
        {
        send_status_ko(llid, tid, "error nat vm not found");
        KERR("ERROR %s %s ", name, cmd);
        }
      else  if (ovs_nat_main_whatip(llid, tid, name, ptr))
        {
        send_status_ko(llid, tid, "error nat cnf");
        KERR("ERROR %s %s ", name, cmd);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_a2b_cnf(int llid, int tid, char *name, char *cmd)
{
  event_print("Rx Req cnf a2b %s %s", name, cmd);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (!ovs_a2b_exists(name))
    {
    send_status_ko(llid, tid, "error a2b not found");
    KERR("ERROR %s %s ", name, cmd);
    }
  else
    {
    if (ovs_a2b_param_config(llid, tid, name, cmd))
      {
      send_status_ko(llid, tid, "error a2b cnf cmd");
      KERR("ERROR %s %s ", name, cmd);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_lan_cnf(int llid, int tid, char *name, char *cmd)
{
  event_print("Rx Req cnf lan %s %s", name, cmd);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (!lan_get_with_name(name))
    {
    send_status_ko(llid, tid, "error lan not found");
    KERR("ERROR %s %s ", name, cmd);
    }
  else if (strcmp(cmd, "rstp"))
    {
    send_status_ko(llid, tid, "error cmd");
    KERR("ERROR %s %s ", name, cmd);
    }
  else
    {
    msg_lan_add_rstp(name);
    send_status_ok(llid, tid, "ok");
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_c2c_peer_create(int llid, int tid, char *name,
                          int is_ack, char *dist, char *loc)
{
  char *locnet = cfg_get_cloonix_name();
  t_cloonix_conf_info *conf = get_cloonix_conf_info();
  char err[MAX_PATH_LEN];
  t_ovs_c2c *c2c;
  int res;
  if (is_ack == 0)
    {
    if (get_inhib_new_clients())
      {
      KERR("ERROR %s %s %s %s", locnet, name, dist, loc);
      }
    else 
      {
      res = cfg_name_is_in_use(0, name, err);
      if (res == 0)
        ovs_c2c_peer_add(llid, tid, name, dist, loc, conf->c2c_udp_port_low);
      else if (res != 2)  
        KERR("ERROR %s %s %s %s %s", locnet, name, dist, loc, err);
      }
    }
  else
    {
    c2c = find_c2c(name);
    if (c2c)
      {
      ovs_c2c_peer_add_ack(llid, tid, name, dist, loc, is_ack);
      }
    else
      KERR("ERROR %s %s %s %s", locnet, name, dist, loc);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_c2c_peer_conf(int llid, int tid, char *name,
                        int status, char *dist, char *loc,
                        uint32_t dist_udp_ip,   uint32_t loc_udp_ip,
                        uint16_t dist_udp_port, uint16_t loc_udp_port)
{
  ovs_c2c_peer_conf(llid, tid, name, status, dist, loc, dist_udp_ip,
                    loc_udp_ip, dist_udp_port, loc_udp_port);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_c2c_peer_ping(int llid, int tid, char *name, int status_peer_ping)
{
  ovs_c2c_peer_ping(llid, tid, name, status_peer_ping);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_color_item(int llid, int tid, char *name, int color)
{
  char info[MAX_PATH_LEN];
  t_vm *vm = cfg_get_vm(name);
  t_eth_table *eth_tab;
  int nb_eth;
  int cnt_exists = cnt_info(name, &nb_eth, &eth_tab);
  event_print("Rx Req color %d for  %s", color, name);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if ((!cnt_exists) && (!vm))
    {
    sprintf( info, "Machine %s does not exist", name);
    send_status_ko(llid, tid, info);
    }
  else
    {
    send_status_ok(llid, tid, "ok");
    if (vm)
      {
      vm->kvm.color = color;
      }
    else
      {
      cnt_set_color(name, color);
      }
    cfg_hysteresis_send_topo_info();
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_cnt_add(int llid, int tid, t_topo_cnt *cnt)
{
  char *locnet = cfg_get_cloonix_name();
  char err[MAX_PATH_LEN];
  int i, vm_id;

  event_print("Rx Req add cnt name:%s brandtype:%s is_persistent:%d "
              "is_privileged:%d image:%s startup_env:%s",
              cnt->name, cnt->brandtype, cnt->is_persistent,
              cnt->is_privileged, cnt->image, cnt->startup_env);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }


  if (get_inhib_new_clients())
    {
    KERR("ERROR %s %s", locnet, cnt->name);
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    }
  else if (cfg_name_is_in_use(0, cnt->name, err))
    {
    KERR("ERROR %s %s", locnet, cnt->name);
    send_status_ko(llid, tid, err);
    }
  else if ((strcmp(cnt->brandtype, "brandzip")) &&
           (strcmp(cnt->brandtype, "brandcvm")))
    {
    snprintf(err, MAX_PATH_LEN-1, "%s %s bad brandtype:%s",
             locnet, cnt->name, cnt->brandtype);
    KERR("ERROR %s", err);
    send_status_ko(llid, tid, err);
    }
  else
    {
    vm_id = cfg_alloc_obj_id();
    for (i=0; i < cnt->nb_tot_eth; i++)
      {
      if (cnt->eth_table[i].mac_addr[0] == 0)
        {
        cnt->eth_table[i].mac_addr[0] = 0x2;
        cnt->eth_table[i].mac_addr[1] = 0xFF & rand();
        cnt->eth_table[i].mac_addr[2] = 0xFF & rand();
        cnt->eth_table[i].mac_addr[3] = 0xFF & rand();
        cnt->eth_table[i].mac_addr[4] = vm_id%100;
        cnt->eth_table[i].mac_addr[5] = i;
        cnt->eth_table[i].randmac = 1;
        }
      }
    if (suid_power_create_cnt(llid, tid, vm_id, cnt, err))
      {
      KERR("ERROR %s", err);
      cfg_free_obj_id(vm_id);
      send_status_ko(llid, tid, err);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_sync_wireshark_req(int llid, int tid, char *name, int num, int cmd)
{
  ovs_snf_sync_wireshark_req(llid, tid, name, num, cmd);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_fix_display(int llid, int tid, char *disp, char *buf)
{
  char wauth[MAX_PATH_LEN];
  char cmd[2 * MAX_PATH_LEN];
  char *net = cfg_get_cloonix_name();

  memset(wauth, 0, MAX_PATH_LEN);
  snprintf(wauth, MAX_PATH_LEN-1, "/var/lib/cloonix/%s/.Xauthority", net);
    memset(cmd, 0, 2 * MAX_PATH_LEN);
    snprintf(cmd, 2 * MAX_PATH_LEN - 1, "%s %s", pthexec_touch_bin(), wauth); 
    system(cmd);
    memset(cmd, 0, 2 * MAX_PATH_LEN);
    snprintf(cmd, 2 * MAX_PATH_LEN - 1,
             "%s -f %s add %s MIT-MAGIC-COOKIE-1 %s",
             pthexec_xauth_bin(), wauth, disp, buf); 
    if (system(cmd))
      {
      KERR("ERROR %s", cmd);
      send_status_ko(llid, tid, "cmd err");
      }
    else
      {
      if (chmod(wauth, 0777))
        {
        KERR("ERROR chmod %s", wauth);
        send_status_ko(llid, tid, "err");
        }
      else
        {
        send_status_ok(llid, llid, "ok");
        }
      }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_init(void)
{
  g_killing_cloonix = 0;
  g_inside_cloonix = inside_cloonix(&g_cloonix_vm_name);
  g_inhib_new_clients = 0;
}
/*---------------------------------------------------------------------------*/


