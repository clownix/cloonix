/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include "machine_create.h"
#include "heartbeat.h"
#include "system_callers.h"
#include "automates.h"
#include "util_sock.h"
#include "utils_cmd_line_maker.h"
#include "qmp.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "file_read_write.h"
#include "ovs.h"
#include "suid_power.h"
#include "qga_dialog.h"

#define DRIVE_PARAMS_CISCO " -device virtio-scsi-pci,id=scsi"\
                           " -device scsi-hd,drive=hd"\
                           " -drive if=none,id=hd,file=%s"

#define DRIVE_PARAMS " -drive file=%s,index=%d,media=disk,if=virtio"

#define DRIVE_FULL_VIRT " -blockdev node-name=%s,driver=qcow2,"\
                        "file.driver=file,file.node-name=file,file.filename=%s"\
                        " -device ide-hd,bus=ide.0,unit=0,drive=%s"

#define INSTALL_DISK " -boot d -drive file=%s,index=%d,media=disk,if=virtio"

#define AGENT_CDROM " -drive file=%s,if=none,media=cdrom,id=cd"\
                    " -device virtio-scsi-pci"\
                    " -device scsi-cd,drive=cd"

#define AGENT_DISK " -drive file=%s,if=virtio,media=disk"

#define ADDED_CDROM " -drive file=%s,media=cdrom"

#define BLOCKNAME " -device virtio-scsi-pci,id=virtio_scsi_pci0"\
                  " -blockdev driver=file,cache.direct=off,cache.no-flush=on,filename=%s,node-name=lol%s"\
                  " -blockdev driver=qcow2,node-name=%s,file=lol%s"\
                  " -device scsi-hd,drive=%s"





typedef struct t_cprootfs_config
{
  char name[MAX_NAME_LEN];
  char msg[MAX_PRINT_LEN];
  char backing[MAX_PATH_LEN];
  char used[MAX_PATH_LEN];
} t_cprootfs_config;

/*--------------------------------------------------------------------------*/

int inside_cloon(char **name);

void qemu_vm_automaton(void *unused_data, int status, char *name);


/****************************************************************************/
static int get_wake_up_eths(char *name, t_vm **vm,
                            t_wake_up_eths **wake_up_eths)
{
  *vm = cfg_get_vm(name);
  if (!(*vm))
    {
    KERR("WAKE ERROR NO VM %s", name);
    return -1;
    }
  *wake_up_eths = (*vm)->wake_up_eths;
  if (!(*wake_up_eths))
    {
    KERR("WAKE ERROR NO ETH %s", name);
    return -1;
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void static_vm_timeout(void *data)
{
  char *name = (char *) data;
  t_wake_up_eths *wake_up_eths;
  t_vm *vm;
  if (get_wake_up_eths(name, &vm, &wake_up_eths))
    KERR("%s", name);
  else
    qemu_vm_automaton(NULL, 0, name);
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cprootfs_clone_death(void *data, int status, char *name)
{
  t_cprootfs_config *cprootfs = (t_cprootfs_config *) data;
  t_vm   *vm;
  t_wake_up_eths *wake_up_eths;

  event_print("%s %s", __FUNCTION__, name);
  if (get_wake_up_eths(name, &vm, &wake_up_eths))
    {
    KERR("POSSIBLE? %s", name);
    recv_coherency_unlock();
    }
  else
    {
    if (strcmp(name, cprootfs->name))
      KOUT("%s %s", name, cprootfs->name);
    if (strstr(cprootfs->msg, "OK"))
      {
      if (status)
        KOUT("%d", status);
      }
    else if (strstr(cprootfs->msg, "KO"))
      {
      if (!status)
        KOUT("%d", status);
      snprintf(wake_up_eths->error_report, MAX_PRINT_LEN, 
               "%s", cprootfs->msg);
      wake_up_eths->error_report[MAX_PRINT_LEN-1] = 0;
      KERR("%s", name);
      }
    else
      KERR("%s %d", cprootfs->msg, status);
    qemu_vm_automaton(NULL, status, name);
    }
  clownix_free(cprootfs, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cprootfs_clone_msg(void *data, char *msg)
{
  int pid;
  t_cprootfs_config *cprootfs = (t_cprootfs_config *) data;
  t_vm   *vm;
  t_wake_up_eths *wake_up_eths;
  if (!get_wake_up_eths(cprootfs->name, &vm, &wake_up_eths))
    {
    if (!strncmp(msg, "pid=", strlen("pid=")))
      {
      if (!strncmp(msg, "pid=start", strlen("pid=start")))
        {
        if (sscanf(msg, "pid=start:%d", &(vm->pid_of_cp_clone)) != 1)
          KOUT("%s", msg);
        }
      else if (!strncmp(msg, "pid=end", strlen("pid=end")))
        {
        if (sscanf(msg, "pid=end:%d", &pid) != 1)
          KOUT("%s", msg);
        if (pid != vm->pid_of_cp_clone)
          KERR(" %s %d", msg, vm->pid_of_cp_clone);
        vm->pid_of_cp_clone = 0;
        }
      else
        KERR(" %s", msg);
      }
    else
      strncpy(cprootfs->msg, msg, MAX_NAME_LEN-1);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int local_cmd_system (char *cmd, char *par1, char *par2, char *par3,
                             char *par4, char *par5, char *par6)
{
  pid_t pid;
  int   status;
  char *argv[8];
  char msg_dad[MAX_NAME_LEN];
  if ((pid = fork ()) < 0)
    return (-1);
  if (pid == 0)
    {
    argv[0] = cmd;
    argv[1] = par1;
    argv[2] = par2;
    argv[3] = par3;
    argv[4] = par4;
    argv[5] = par5;
    argv[6] = par6;
    argv[7] = NULL;
    execv(cmd, argv);
    exit (127);
    }
  memset(msg_dad, 0, MAX_NAME_LEN);
  snprintf(msg_dad, MAX_NAME_LEN - 1, "pid=start:%d", pid);
  send_to_daddy(msg_dad);
  while (1)
    {
    if (waitpid (pid, &status, 0) == -1)
      return (-1);
    else
      {
      memset(msg_dad, 0, MAX_NAME_LEN);
      snprintf(msg_dad, MAX_NAME_LEN - 1, "pid=end:%d", pid);
      send_to_daddy(msg_dad);
      return (status);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int cprootfs_clone(void *data)
{
  int result;
  char *qimg = "/usr/libexec/cloonix/server/cloonix-qemu-img";
  char parm[2*MAX_PATH_LEN];
  t_cprootfs_config *cprootfs = (t_cprootfs_config *) data;
  memset(parm, 0, 2*MAX_PATH_LEN);
  snprintf(parm, 2*MAX_PATH_LEN-1,
           "backing_file=%s,backing_fmt=qcow2", cprootfs->backing);
  result = local_cmd_system(qimg, "create", "-f", "qcow2", "-o",
                            parm, cprootfs->used);
  if (result)
    {
    KERR("ERROR %s create -f qcow2 -o %s %s", qimg, parm, cprootfs->used);
    send_to_daddy("KO");
    }
  else
    {
    send_to_daddy("OK");
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void derived_file_creation_request(t_vm *vm)
{
  char *name;
  t_cprootfs_config *cprootfs;
  if (!vm)
    KOUT(" ");
  name = vm->kvm.name;
  cprootfs=(t_cprootfs_config *)clownix_malloc(sizeof(t_cprootfs_config),13);
  memset(cprootfs, 0, sizeof(t_cprootfs_config));
  strncpy(cprootfs->name, name, MAX_NAME_LEN-1);
  strcpy(cprootfs->msg, "NO_MSG");

  strncpy(cprootfs->used, vm->kvm.rootfs_used, MAX_PATH_LEN-1);
  strncpy(cprootfs->backing, vm->kvm.rootfs_backing, MAX_PATH_LEN-1);

  event_print("%s %s", __FUNCTION__, name);
  pid_clone_launch(cprootfs_clone, cprootfs_clone_death,
                   cprootfs_clone_msg, cprootfs, 
                   cprootfs, cprootfs, name, -1, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
#define QEMU_OPTS_BASE \
   " -m %d"\
   " -name %s"\
   " -serial stdio"\
   " -nodefaults"\
   " -rtc base=utc,driftfix=slew"\
   " -global kvm-pit.lost_tick_policy=delay"\
   " -machine hpet=off -boot strict=on"

#define QEMU_OPTS_QMP \
   " -chardev socket,id=qmp1,path=%s,server=on,wait=off"\
   " -mon chardev=qmp1,mode=control"

#define QEMU_OPTS_CLOON \
   " -device virtio-serial-pci"\
   " -device virtio-mouse-pci"\
   " -device virtio-keyboard-pci"\
   " -chardev socket,path=%s,server=on,wait=off,id=cloon0"\
   " -device virtserialport,chardev=cloon0,name=net.cloon.0"\
   " -chardev socket,path=%s,server=on,wait=off,id=qga0" \
   " -device virtserialport,chardev=qga0,name=org.qemu.guest_agent.0" \
   " -chardev spicevmc,id=spice0,name=vdagent" \
   " -device virtserialport,chardev=spice0,name=com.redhat.spice.0"\
   " -device virtio-balloon-pci,id=balloon0"\
   " -object rng-random,filename=/dev/urandom,id=rng0"\
   " -device virtio-rng-pci,rng=rng0"

#define QEMU_SPICE \
   " -device virtio-vga"\
   " -audiodev driver=spice,id=hda0"\
   " -device intel-hda,id=hda0"\
   " -device hda-duplex,bus=hda0.0,audiodev=hda0"\
   " -device qemu-xhci"\
   " -device usb-tablet"\
   " -chardev spicevmc,id=charredir0,name=usbredir"\
   " -device usb-redir,chardev=charredir0"\
   " -chardev spicevmc,id=charredir1,name=usbredir"\
   " -device usb-redir,chardev=charredir1"\
   " -spice unix=on,addr=%s,disable-ticketing=on"


/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_linux_cmd_kvm(t_vm *vm, char *qemu_cmd)
{
  int i, nb_cpu, len = 0;
  char cpu_type[MAX_NAME_LEN];
  char *rootfs, *added_disk, *gname;
  char *spice_path, *cdrom;
  if (!vm)
    KOUT(" ");
  spice_path = utils_get_spice_path(vm->kvm.vm_id);
  nb_cpu = vm->kvm.cpu;
  if (inside_cloon(&gname))
    strcpy(cpu_type, "kvm64");
  else
    strcpy(cpu_type, "host,-aes");
  len += sprintf(qemu_cmd+len, QEMU_OPTS_BASE, vm->kvm.mem, vm->kvm.name);
  for (i = 0; i < vm->kvm.nb_tot_eth; i++)
    {
    if ((vm->kvm.eth_table[i].endp_type == endp_type_eths) ||
        (vm->kvm.eth_table[i].endp_type == endp_type_ethv))
      {
      len += sprintf(qemu_cmd+len, "%s",
             ovs_format_ethv(vm, i, vm->kvm.eth_table[i].vhost_ifname));
      }
    else
      KERR("ERROR %d", vm->kvm.eth_table[i].endp_type);
    }
  if (!(vm->kvm.vm_config_flags & VM_CONFIG_FLAG_NO_QEMU_GA))
    {
    len += sprintf(qemu_cmd+len, QEMU_OPTS_CLOON, 
                   utils_get_qbackdoor_path(vm->kvm.vm_id),
                   utils_get_qga_path(vm->kvm.vm_id));
    }
  len += sprintf(qemu_cmd+len,
                 " -pidfile %s/%s/pid"
                 " -enable-kvm"
                 " -machine q35,accel=kvm,usb=off,dump-guest-core=off,vmport=off"
                 " -cpu %s"
                 " -smp %d,maxcpus=%d,cores=1",
                 cfg_get_work_vm(vm->kvm.vm_id), DIR_UMID,
                 cpu_type, nb_cpu, nb_cpu);
  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_WITH_PXE)
    len += sprintf(qemu_cmd+len," -nographic -vga none");
  else
    len += sprintf(qemu_cmd+len, QEMU_SPICE, spice_path);
  rootfs = vm->kvm.rootfs_used;
  added_disk = vm->kvm.added_disk;
  if  (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_NO_REBOOT)
    {
    len += sprintf(qemu_cmd+len, " -no-reboot");
    }
  if  (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_WITH_PXE)
    {
    len += sprintf(qemu_cmd+len, " -boot n");
    }
  if  (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_INSTALL_CDROM)
    {
    len += sprintf(qemu_cmd+len, INSTALL_DISK, rootfs, 0);
    len += sprintf(qemu_cmd+len, ADDED_CDROM, vm->kvm.install_cdrom);
    }
  else
    {
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_FULL_VIRT)
      len += sprintf(qemu_cmd+len, DRIVE_FULL_VIRT, 
                     vm->kvm.name, rootfs, vm->kvm.name );
    else
      {
      if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_NO_QEMU_GA)
        {
        len += sprintf(qemu_cmd+len, DRIVE_PARAMS_CISCO, rootfs);
        len += sprintf(qemu_cmd+len,
          " -uuid 1c54ff10-774c-4e63-9896-4c18d66b50b1");
        }
      else
        {
        len += sprintf(qemu_cmd+len, BLOCKNAME, rootfs, vm->kvm.name, 
                       vm->kvm.name, vm->kvm.name, vm->kvm.name);
        }
      } 
    cdrom = utils_get_cdrom_path_name(vm);
    len += sprintf(qemu_cmd+len, AGENT_CDROM, cdrom);
  
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK)
      {
      len += sprintf(qemu_cmd+len, DRIVE_PARAMS, added_disk, 1);
      }
    }
  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_ADDED_CDROM)
    {
    len += sprintf(qemu_cmd+len, ADDED_CDROM, vm->kvm.added_cdrom);
    }
  len += sprintf(qemu_cmd+len, QEMU_OPTS_QMP,
                 utils_get_qmp_path(vm->kvm.vm_id));
}
/*--------------------------------------------------------------------------*/
              
/****************************************************************************/
static char *alloc_argv(char *str)
{
  int len = strlen(str);
  char *argv = (char *)clownix_malloc(len + 1, 15);
  memset(argv, 0, len + 1);
  strncpy(argv, str, len);
  return argv;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char **create_qemu_argv(t_vm *vm)
{
  int i = 0;
  static char **argv;
  char qemu_cmd[10*MAX_PATH_LEN];
  char namespace[MAX_PATH_LEN];
  char qemu_bin[MAX_PATH_LEN];
  char qemu_data[MAX_PATH_LEN];
  char *ptr1, *ptr2;
  memset(qemu_cmd, 0, 10*MAX_PATH_LEN);
  memset(namespace, 0, MAX_PATH_LEN);
  memset(qemu_bin, 0, MAX_PATH_LEN);
  memset(qemu_data, 0, MAX_PATH_LEN);

  snprintf(namespace, MAX_PATH_LEN-1, "%s_%s",
           BASE_NAMESPACE, cfg_get_cloonix_name());
  snprintf(qemu_bin, MAX_PATH_LEN-1, "%s/server/cloonix-qemu-system-x86_64",
           cfg_get_bin_dir());
  snprintf(qemu_data, MAX_PATH_LEN-1, "%s/server/qemu", cfg_get_bin_dir());
  create_linux_cmd_kvm(vm, qemu_cmd);

  argv = (char **)clownix_malloc(200 * sizeof(char *), 13);
  memset(argv, 0, 200 * sizeof(char *));
  argv[i++] = alloc_argv(utils_get_dtach_bin_path());
  argv[i++] = alloc_argv("-n");
  argv[i++] = alloc_argv(utils_get_dtach_sock_path(vm->kvm.name));
  argv[i++] = alloc_argv(IP_BIN);
  argv[i++] = alloc_argv("netns");
  argv[i++] = alloc_argv("exec");
  argv[i++] = alloc_argv(namespace);
  argv[i++] = alloc_argv(qemu_bin);
  argv[i++] = alloc_argv("-L");
  argv[i++] = alloc_argv(qemu_data);
  ptr1 = qemu_cmd;
  ptr1 += strspn(ptr1, " "); 
  while(ptr1)
    {
    if (i>190)
      KOUT("ERROR");
    ptr2 = ptr1 + strcspn(ptr1, " ");
    if (ptr2)
      {
      *ptr2 = 0;
      ptr2 += 1;
      }
    argv[i++] = alloc_argv(ptr1);
    if ((ptr2) && (strlen(ptr2)))
      {
      ptr1 = ptr2;
      ptr1 += strspn(ptr1, " "); 
      }
    else
      ptr1 = NULL;
    }
  argv[i++] = NULL;
  return argv;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_launch_end(void *data)
{
  char *name = (char *) data;
  char err[MAX_PRINT_LEN];
  t_vm   *vm = cfg_get_vm(name);
  t_wake_up_eths *wake_up_eths;
  if (vm)
    {
    wake_up_eths = vm->wake_up_eths;
    if (wake_up_eths)
      {
      if (strcmp(wake_up_eths->name, name))
        KERR(" ");
      else
        {
        if (!file_exists(utils_get_dtach_sock_path(name), F_OK))
          {
          sprintf(err, "ERROR QEMU UNEXPECTED STOP %s do \n"
                       "sudo cat /var/log/user.log | grep qemu\n", name);
          event_print(err);
          send_status_ko(wake_up_eths->llid, wake_up_eths->tid, err);
          free_wake_up_eths_and_delete_vm(vm, error_death_qemu_quiterr);
          }
        }
      }
    }
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_qemu_monitor_name(void *data)
{
  char *name = (char *) data;
  t_vm   *vm = cfg_get_vm(name);
  int pid;
  if (vm)
    {
    pid = suid_power_get_pid(vm->kvm.vm_id);
    if ((pid == 0) && (vm->vm_to_be_killed == 0))
      {
      if (vm->pid == 0)
        {
        vm->count_no_pid_yet += 1;
        if (vm->count_no_pid_yet > 20)
          {
          KERR("ERROR QEMU PID ABSENT");
          poweroff_vm(0, 0, vm);
          }
        }
      else
        {
        poweroff_vm(0, 0, vm);
        }
      }
    else
      clownix_timeout_add(10, timer_qemu_monitor_name, data, NULL, NULL);
    }
  else
    {
    clownix_free(data, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void launch_qemu_vm_end(char *name)
{
  int i;
  char *time_name;
  char *qemu_monitor_name;
  t_vm   *vm = cfg_get_vm(name);
  if (!vm)
    {
    KERR("%s", name);
    }
  else
   {
    for (i=0; vm->launcher_argv[i] != NULL; i++)
      clownix_free(vm->launcher_argv[i], __FUNCTION__);
    clownix_free(vm->launcher_argv, __FUNCTION__);
    vm->launcher_argv = NULL;
    time_name = clownix_malloc(MAX_NAME_LEN, 5);
    memset(time_name, 0, MAX_NAME_LEN);
    strncpy(time_name, vm->kvm.name, MAX_NAME_LEN-1);
    qemu_monitor_name = clownix_malloc(MAX_NAME_LEN, 5);
    memset(qemu_monitor_name, 0, MAX_NAME_LEN);
    strncpy(qemu_monitor_name, vm->kvm.name, MAX_NAME_LEN-1);
    clownix_timeout_add(10, timer_qemu_monitor_name,
                        (void *) qemu_monitor_name, NULL, NULL);
    clownix_timeout_add(3000, timer_launch_end, (void *) time_name, NULL, NULL);
   }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void launch_qemu_vm_start(t_vm *vm)
{
  if (vm->launcher_argv != NULL)
    KOUT("%s", vm->kvm.name);
  vm->launcher_argv = create_qemu_argv(vm);
  utils_send_creation_info(vm->kvm.name, vm->launcher_argv);
  suid_power_launch_vm(vm, launch_qemu_vm_end);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void timer_utils_finish_vm_init(int vm_id, int val)
{
  unsigned long ul_vm_id = (unsigned long) vm_id;
  clownix_timeout_add(val,utils_finish_vm_init,(void *)ul_vm_id,NULL,NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void arm_static_vm_timeout(char *name, int delay)
{
  char *nm;
  nm = (char *) clownix_malloc(MAX_NAME_LEN, 9);
  memset(nm, 0, MAX_NAME_LEN);
  strncpy(nm, name, MAX_NAME_LEN-1);
  clownix_timeout_add(delay, static_vm_timeout, (void *)nm, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qemu_vm_automaton(void *unused_data, int status, char *name) 
{
  char err[MAX_PRINT_LEN];
  int time_end, i, state, nb_eth = 0;
  t_vm   *vm = cfg_get_vm(name);
  t_wake_up_eths *wake_up;
  if (!vm)
    {
    KERR("POSSIBLE ? %s", name);
    return;
    }
  wake_up = vm->wake_up_eths;
  if (!wake_up)
    {
    KERR("POSSIBLE ? %s", name);
    return;
    }
  if (strcmp(wake_up->name, name))
    KOUT(" ");
  state = wake_up->state;
  if (status)
    {
    sprintf(err, "ERROR when creating %s\n", name);
    event_print(err);
    KERR("%s", err);
    send_status_ko(wake_up->llid, wake_up->tid, err);
    free_wake_up_eths_and_delete_vm(vm, error_death_qemuerr);
    recv_coherency_unlock();
    return;
    }
  switch (state)
    {
    case auto_idle:
      wake_up->state = auto_delay_possible_ovs_start;
      if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)
        arm_static_vm_timeout(name, 1);
      else
        derived_file_creation_request(vm);
      break;
    case auto_delay_possible_ovs_start:
      for (i = 0; i < vm->kvm.nb_tot_eth; i++)
        {
        if ((vm->kvm.eth_table[i].endp_type == endp_type_eths) ||
            (vm->kvm.eth_table[i].endp_type == endp_type_ethv))
          nb_eth += 1;
        }
      if (nb_eth)
        {
        wake_up->eth_count += 1;
        if (wake_up->eth_count > 100)
          {
          KERR("ERROR ovs start when creating %s\n", name);
          sprintf(err, "ERROR ovs start when creating %s\n", name);
          event_print(err);
          send_status_ko(wake_up->llid, wake_up->tid, err);
          free_wake_up_eths_and_delete_vm(vm, error_death_qemuerr);
          recv_coherency_unlock();
          }
        else
          {
          arm_static_vm_timeout(name, 100);
          }
        wake_up->state = auto_create_vm_launch;
        }
      else
        {
        wake_up->state = auto_create_vm_launch;
        arm_static_vm_timeout(name, 1);
        }
      break;
    case auto_create_vm_launch:
      wake_up->state = auto_create_vm_connect;
      launch_qemu_vm_start(vm);
      arm_static_vm_timeout(name, 100);
      break;
    case auto_create_vm_connect:
      time_end = util_get_max_tempo_fail();
      time_end += 200;
      if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_NO_QEMU_GA)
        timer_utils_finish_vm_init(vm->kvm.vm_id, 1);
      else
        timer_utils_finish_vm_init(vm->kvm.vm_id, time_end);
      qmp_begin_qemu_unix(name, 1);
      recv_coherency_unlock();
      break;
    default:
      KOUT(" ");
    }
}
/*--------------------------------------------------------------------------*/



