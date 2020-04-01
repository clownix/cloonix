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
#include "machine_create.h"
#include "heartbeat.h"
#include "system_callers.h"
#include "automates.h"
#include "util_sock.h"
#include "utils_cmd_line_maker.h"
#include "qmonitor.h"
#include "qmp.h"
#include "qhvc0.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "file_read_write.h"
#include "endp_mngt.h"
#include "dpdk_ovs.h"
#include "endp_evt.h"

#define NETWORK_PARAMS_VMWARE " -netdev type=tap,id=vmware0,ifname=vmwa0"\
                              " -device e1000,netdev=vmware0"\
                              " -netdev type=tap,id=vmware1,ifname=vmwa1"\
                              " -device e1000,netdev=vmware1"\
                              " -netdev type=tap,id=vmware2,ifname=vmwa2"\
                              " -device e1000,netdev=vmware2"



#define DRIVE_VMWARE0 " -drive file=%s,index=0,media=disk,if=none,id=sata0"\
                      " -device ich9-ahci,id=ahci"\
                      " -device ide-drive,drive=sata0,bus=ahci.0"

#define DRIVE_VMWARE1 " -drive file=%s,index=1,media=disk,if=none,id=sata1"\
                      " -device ide-drive,drive=sata1,bus=ahci.1"

#define DRIVE_PARAMS_CISCO " -drive file=%s,index=%d,media=disk,if=virtio,cache=directsync"
#define DRIVE_PARAMS " -drive file=%s,index=%d,media=disk,if=virtio"

#define VIRTIO_9P " -fsdev local,id=fsdev0,security_model=passthrough,path=%s"\
                  " -device virtio-9p-pci,id=fs0,fsdev=fsdev0,mount_tag=%s"

#define DRIVE_FULL_VIRT " -blockdev node-name=%s,driver=qcow2,"\
                        "file.driver=file,file.node-name=file,file.filename=%s" \
                        " -device ide-hd,bus=ide.0,unit=0,drive=%s"


#define INSTALL_DISK " -boot d -drive file=%s,index=%d,media=disk,if=virtio"

#define AGENT_CDROM " -drive file=%s,if=none,media=cdrom,id=cd"\
                    " -device virtio-scsi-pci"\
                    " -device scsi-cd,drive=cd"

#define AGENT_DISK " -drive file=%s,if=virtio,media=disk"


#define ADDED_CDROM " -drive file=%s,media=cdrom"

#define VHOST_VSOCK " -device vhost-vsock-pci,id=vhost-vsock-pci0,guest-cid=%d"


#define BLOCKNAME " -blockdev node-name=%s,driver=qcow2,"\
                  "file.driver=file,file.node-name=file,file.filename=%s" \
                  " -device virtio-blk,drive=%s"

#define FLAG_UEFI " -bios %s/server/qemu/%s/OVMF.fd"


typedef struct t_cprootfs_config
{
  char name[MAX_NAME_LEN];
  char msg[MAX_PRINT_LEN];
  char backing[MAX_PATH_LEN];
  char used[MAX_PATH_LEN];
} t_cprootfs_config;


enum
  {
  auto_idle = 0,
  auto_create_disk,
  auto_delay_possible_ovs_start,
  auto_create_vm_launch,
  auto_create_vm_connect,
  auto_max,
  };

/*--------------------------------------------------------------------------*/

int inside_cloonix(char **name);

void qemu_vm_automaton(void *unused_data, int status, char *name);

char **get_saved_environ(void);

/****************************************************************************/
static int get_wake_up_eths(char *name, t_vm **vm,
                            t_wake_up_eths **wake_up_eths)
{
  *vm = cfg_get_vm(name);
  if (!(*vm))
    return -1;
  *wake_up_eths = (*vm)->wake_up_eths;
  if (!(*wake_up_eths))
    return -1;
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
  if (!get_wake_up_eths(name, &vm, &wake_up_eths))
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
static int local_clownix_system (char *commande)
{
  pid_t pid;
  int   status;
  char **environ = NULL;
  char * argv [4];
  char msg_dad[MAX_NAME_LEN];
  if (commande == NULL)
    return (1);
  if ((pid = fork ()) < 0)
    return (-1);
  if (pid == 0)
    {
    argv[0] = "/bin/bash";
    argv[1] = "-c";
    argv[2] = commande;
    argv[3] = NULL;
    execve("/bin/bash", argv, environ);
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
  char *cmd;
  t_cprootfs_config *cprootfs = (t_cprootfs_config *) data;
  cmd = utils_qemu_img_derived(cprootfs->backing, cprootfs->used);
  result = local_clownix_system(cmd);
  if (result)
    KERR("%s", cmd);
  snprintf(cmd, 2*MAX_PATH_LEN, "/bin/chmod +w %s", cprootfs->used);
  result = clownix_system(cmd);
  if (result)
    {
    KERR("%s", cmd);
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
static char *format_virtkvm_net(t_vm *vm, int eth)
{
  static char net_cmd[MAX_PATH_LEN*3];
  int len = 0;
  char *mac_addr;
  len+=sprintf(net_cmd+len,
               " -device virtio-muethnet,tx=bh,netdev=eth%d,mac=", eth);
  mac_addr = vm->kvm.eth_params[eth].mac_addr;
  len += sprintf(net_cmd+len,"%02X:%02X:%02X:%02X:%02X:%02X",
                 mac_addr[0] & 0xFF, mac_addr[1] & 0xFF, mac_addr[2] & 0xFF,
                 mac_addr[3] & 0xFF, mac_addr[4] & 0xFF, mac_addr[5] & 0xFF);
  len += sprintf(net_cmd+len,",bus=pci.0,addr=0x%x", eth+5);
  len += sprintf(net_cmd+len, 
  " -netdev mueth,id=eth%d,munetname=%s,muname=%s,munum=%d,sock=%s,mutype=1",
                 eth,cfg_get_cloonix_name(), vm->kvm.name, eth, 
                 utils_get_endp_path(vm->kvm.name, eth)); 
  return net_cmd;
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
   " -no-hpet -boot strict=on"

#define QEMU_OPTS_MON_QMP \
   " -chardev socket,id=mon1,path=%s,server,nowait"\
   " -mon chardev=mon1,mode=readline"\
   " -chardev socket,id=qmp1,path=%s,server,nowait"\
   " -mon chardev=qmp1,mode=control"

#define QEMU_OPTS_CLOONIX \
   " -device virtio-serial-pci"\
   " -device virtio-mouse-pci"\
   " -device virtio-keyboard-pci"\
   " -chardev socket,path=%s,server,nowait,id=cloon0"\
   " -device virtserialport,chardev=cloon0,name=net.cloonix.0"\
   " -chardev socket,path=%s,server,nowait,id=hvc0"\
   " -device virtconsole,chardev=hvc0"\
   " -device virtio-balloon-pci,id=balloon0"\
   " -object rng-random,filename=/dev/urandom,id=rng0"\
   " -device virtio-rng-pci,rng=rng0"

#define QEMU_SPICE \
   " -device virtio-vga"\
   " -device ich9-intel-hda"\
   " -device hda-micro"\
   " -device qemu-xhci"\
   " -device usb-tablet"\
   " -chardev spicevmc,id=charredir0,name=usbredir"\
   " -device usb-redir,chardev=charredir0"\
   " -chardev spicevmc,id=charredir1,name=usbredir"\
   " -device usb-redir,chardev=charredir1"\
   " -spice unix,addr=%s,disable-ticketing"\
   " -device virtserialport,chardev=spicechannel0,name=com.redhat.spice.0"\
   " -chardev spicevmc,id=spicechannel0,name=vdagent"
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int create_linux_cmd_arm(t_vm *vm, char *linux_cmd)
{
  int i, len = 0;
  char *agent = utils_get_cdrom_path_name(vm->kvm.vm_id);
  len += sprintf(linux_cmd+len, 
                 " -m %d -name %s"
                 " -serial stdio"
                 " -nographic"
                 " -nodefaults"
                 " -pidfile %s/%s/pid"
                 " -drive file=%s,if=virtio"
                 " -append \"root=/dev/vda earlyprintk=ttyAMA0 net.ifnames=0\"",
                 vm->kvm.mem, vm->kvm.name,
                 cfg_get_work_vm(vm->kvm.vm_id),
                 DIR_UMID, vm->kvm.rootfs_used);

  for (i=vm->kvm.nb_dpdk; i<vm->kvm.nb_dpdk+vm->kvm.nb_eth; i++)
    len+=sprintf(linux_cmd+len,"%s",format_virtkvm_net(vm,i));


  len += sprintf(linux_cmd+len, 
                " -device virtio-serial-pci"
                " -chardev socket,path=%s,server,nowait,id=cloon0"
                " -device virtserialport,chardev=cloon0,name=net.cloonix.0"
                " -chardev socket,path=%s,server,nowait,id=hvc0"
                " -device virtconsole,chardev=hvc0"
                " -chardev socket,id=mon1,path=%s,server,nowait"
                " -mon chardev=mon1,mode=readline"
                " -chardev socket,id=qmp1,path=%s,server,nowait"
                " -mon chardev=qmp1,mode=control",
                utils_get_qbackdoor_path(vm->kvm.vm_id),
                utils_get_qhvc0_path(vm->kvm.vm_id),
                utils_get_qmonitor_path(vm->kvm.vm_id),
                utils_get_qmp_path(vm->kvm.vm_id));


  len += sprintf(linux_cmd+len, AGENT_DISK, agent);

  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int create_linux_cmd_kvm(t_vm *vm, char *linux_cmd)
{
  int i, nb_cpu, len, tot_dpdk;
  char cmd_start[10000];
  char cpu_type[MAX_NAME_LEN];
  char *rootfs, *added_disk, *gname;
  char *spice_path, *cdrom;
  if (!vm)
    KOUT(" ");
  spice_path = utils_get_spice_path(vm->kvm.vm_id);
  nb_cpu = vm->kvm.cpu;
  if (inside_cloonix(&gname))
    {
    strcpy(cpu_type, "kvm64");
    }
  else
    {
    strcpy(cpu_type, "host,+vmx");
    }

  len = sprintf(cmd_start, QEMU_OPTS_BASE, vm->kvm.mem, vm->kvm.name);

  tot_dpdk = vm->kvm.nb_dpdk;
  for (i=0; i < tot_dpdk; i++)
    len+=sprintf(cmd_start+len, "%s", dpdk_ovs_format_net(vm, i, tot_dpdk));

  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_VMWARE)
    {
    if (vm->kvm.nb_eth != 3)
      KOUT("%d", vm->kvm.nb_eth);
    if (vm->kvm.nb_dpdk != 0)
      KOUT("%d", vm->kvm.nb_dpdk);
    len+=sprintf(cmd_start+len, "%s", NETWORK_PARAMS_VMWARE);
    }
  else
    {
    for (i=vm->kvm.nb_dpdk; i<vm->kvm.nb_dpdk+vm->kvm.nb_eth; i++)
      len+=sprintf(cmd_start+len, "%s",format_virtkvm_net(vm,i));
    }
  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_UEFI)
    len += sprintf(cmd_start+len, FLAG_UEFI, cfg_get_bin_dir(), QEMU_BIN_DIR);
  if (!(vm->kvm.vm_config_flags & VM_CONFIG_FLAG_CISCO))
    {
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_VHOST_VSOCK)
      len += sprintf(cmd_start+len, VHOST_VSOCK, vm->kvm.vm_id+2);
    len += sprintf(cmd_start+len, QEMU_OPTS_CLOONIX, 
                   utils_get_qbackdoor_path(vm->kvm.vm_id),
                   utils_get_qhvc0_path(vm->kvm.vm_id));
    for (i=0; i<vm->kvm.nb_wlan; i++)
      {
       len += sprintf(cmd_start+len, 
              " -chardev socket,path=%s,server,nowait,id=cloon%d"
              " -device virtserialport,chardev=cloon%d,name=net.cloonix.%d",
              utils_get_qbackdoor_wlan_path(vm->kvm.vm_id, i), i+1, i+1, i+1);
      }
    }

  len = sprintf(linux_cmd, " %s"
                        " -pidfile %s/%s/pid"
                        " -machine pc,accel=kvm,usb=off,dump-guest-core=off"
                        " -cpu %s"
                        " -smp %d,maxcpus=%d,cores=1",
                        cmd_start, cfg_get_work_vm(vm->kvm.vm_id),
                        DIR_UMID, cpu_type, nb_cpu, nb_cpu);
  if (spice_libs_exists())
    {
    if ((vm->kvm.vm_config_flags & VM_CONFIG_FLAG_WITH_PXE) ||
        (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_CISCO))
      len += sprintf(linux_cmd+len," -nographic -vga none");
    else
      len += sprintf(linux_cmd+len, QEMU_SPICE, spice_path);
    }


  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_9P_SHARED)
    {
    if (vm->kvm.p9_host_share[0] == 0) 
      KERR(" ");
    else
      {
      if (!is_directory_readable(vm->kvm.p9_host_share))
        KERR("%s", vm->kvm.p9_host_share);
      else
        len += sprintf(linux_cmd+len, VIRTIO_9P, vm->kvm.p9_host_share,
                                                 vm->kvm.name);
      }
    }

  rootfs = vm->kvm.rootfs_used;
  added_disk = vm->kvm.added_disk;

  if  (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_NO_REBOOT)
    {
    len += sprintf(linux_cmd+len, " -no-reboot");
    }
  if  (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_WITH_PXE)
    {
    len += sprintf(linux_cmd+len, " -boot n");
    }
  if  (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_INSTALL_CDROM)
    {
    len += sprintf(linux_cmd+len, INSTALL_DISK, rootfs, 0);
    len += sprintf(linux_cmd+len, ADDED_CDROM, vm->kvm.install_cdrom);
    }
  else
    {
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_FULL_VIRT)
      len += sprintf(linux_cmd+len, DRIVE_FULL_VIRT, 
                     vm->kvm.name, rootfs, vm->kvm.name );
    else
      {
      if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_CISCO)
        {
        len += sprintf(linux_cmd+len, DRIVE_PARAMS_CISCO, rootfs, 0);
        len += sprintf(linux_cmd+len,
          " -uuid 1c54ff10-774c-4e63-9896-4c18d66b50b1");
        }
      else if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_VMWARE)
        {
        len += sprintf(linux_cmd+len, DRIVE_VMWARE0, rootfs);
        if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK)
          len += sprintf(linux_cmd+len, DRIVE_VMWARE1, added_disk);
        }
      else
        {
        len += sprintf(linux_cmd+len, BLOCKNAME, 
                       vm->kvm.name, rootfs, vm->kvm.name );
        }
      } 
    cdrom = utils_get_cdrom_path_name(vm->kvm.vm_id);
    len += sprintf(linux_cmd+len, AGENT_CDROM, cdrom);
  
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK)
      {
      if (!(vm->kvm.vm_config_flags & VM_CONFIG_FLAG_VMWARE))
        len += sprintf(linux_cmd+len, DRIVE_PARAMS, added_disk, 1);
      }
    }


  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_ADDED_CDROM)
    {
    len += sprintf(linux_cmd+len, ADDED_CDROM, vm->kvm.added_cdrom);
    }

  len += sprintf(linux_cmd+len, QEMU_OPTS_MON_QMP,
                 utils_get_qmonitor_path(vm->kvm.vm_id),
                 utils_get_qmp_path(vm->kvm.vm_id));

  return len;
}
/*--------------------------------------------------------------------------*/
              
/****************************************************************************/
static char *qemu_cmd_format(t_vm *vm)
{
  int len = 0;
  char *cmd = (char *) clownix_malloc(MAX_BIG_BUF, 7);
  char mach[MAX_NAME_LEN];
  char path_qemu_exe[MAX_PATH_LEN];
  char path_kern[MAX_PATH_LEN];
  char path_initrd[MAX_PATH_LEN];
  memset(cmd, 0,  MAX_BIG_BUF);
  memset(path_qemu_exe, 0, MAX_PATH_LEN);
  memset(path_kern, 0, MAX_PATH_LEN);
  memset(path_initrd, 0, MAX_PATH_LEN);
  memset(mach, 0, MAX_NAME_LEN);
  if ((vm->kvm.vm_config_flags & VM_CONFIG_FLAG_ARM) ||
      (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_AARCH64))
    {
    snprintf(path_kern, MAX_PATH_LEN-1, "%s/%s",
             cfg_get_bulk(), vm->kvm.linux_kernel);
    snprintf(path_initrd, MAX_PATH_LEN-1, "%s/%s-initramfs",
             cfg_get_bulk(), vm->kvm.linux_kernel);
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_AARCH64)
      {
      snprintf(path_qemu_exe, MAX_PATH_LEN-1, "%s/server/qemu/%s/%s",
               cfg_get_bin_dir(), QEMU_BIN_DIR, QEMU_AARCH64_EXE);
      snprintf(mach, MAX_NAME_LEN-1, "-M virt --cpu cortex-a57");
      }
    else
      {
      snprintf(path_qemu_exe, MAX_PATH_LEN-1, "%s/server/qemu/%s/%s",
               cfg_get_bin_dir(), QEMU_BIN_DIR, QEMU_ARM_EXE);
      snprintf(mach, MAX_NAME_LEN-1, "-M virt");
      }
    if (!file_exists(path_initrd, F_OK))
      {
      len += snprintf(cmd, MAX_BIG_BUF-1,
      "%s -L %s/server/qemu/%s %s -kernel %s",
      path_qemu_exe, cfg_get_bin_dir(), QEMU_BIN_DIR, mach, path_kern); 
      }
    else
      {
      len += snprintf(cmd, MAX_BIG_BUF-1,
      "%s -L %s/server/qemu/%s %s -kernel %s -initrd %s",
      path_qemu_exe, cfg_get_bin_dir(), QEMU_BIN_DIR,
      mach, path_kern, path_initrd); 
      }
    len += create_linux_cmd_arm(vm, cmd+len);
    }
  else
    {
    snprintf(path_qemu_exe, MAX_PATH_LEN-1, "%s/server/qemu/%s/%s",
             cfg_get_bin_dir(), QEMU_BIN_DIR, QEMU_EXE);
    len += snprintf(cmd, MAX_BIG_BUF-1, "%s -L %s/server/qemu/%s ",
                    path_qemu_exe, cfg_get_bin_dir(), QEMU_BIN_DIR);
    len += create_linux_cmd_kvm(vm, cmd+len);
    }
  len += sprintf(cmd+len, " 2>/tmp/qemu_%s", vm->kvm.name);
  return cmd;
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
  char *kvm_exe = qemu_cmd_format(vm);
  argv = (char **)clownix_malloc(10 * sizeof(char *), 13);
  memset(argv, 0, 10 * sizeof(char *));
  argv[i++] = alloc_argv(utils_get_dtach_bin_path());
  argv[i++] = alloc_argv("-n");
  argv[i++] = alloc_argv(utils_get_dtach_sock_path(vm->kvm.name));
  argv[i++] = alloc_argv("/bin/bash");
  argv[i++] = alloc_argv("-c");
  argv[i++] = kvm_exe;
  argv[i++] = NULL;
  return argv;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int start_launch_args(void *ptr)
{  

  return (utils_execve(ptr));
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
          utils_launched_vm_death(name, error_death_qemu_quiterr);
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
    pid = utils_get_pid_of_machine(vm);
    if ((pid == 0) && (vm->vm_to_be_killed == 0))
      {
      KERR("ERROR QEMU PID ABSENT (%d, %d)", vm->pid_returned_clone, vm->pid);
      machine_death(name, error_death_pid_diseapeared);
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
static void launcher_death(void *data, int status, char *name)
{
  int i;
  char *time_name;
  char *qemu_monitor_name;
  char **argv = (char **) data;
  for (i=0; argv[i] != NULL; i++)
    clownix_free(argv[i], __FUNCTION__);
  clownix_free(argv, __FUNCTION__);
  time_name = clownix_malloc(MAX_NAME_LEN, 5);
  memset(time_name, 0, MAX_NAME_LEN);
  strncpy(time_name, name, MAX_NAME_LEN-1);
  qemu_monitor_name = clownix_malloc(MAX_NAME_LEN, 5);
  memset(qemu_monitor_name, 0, MAX_NAME_LEN);
  strncpy(qemu_monitor_name, name, MAX_NAME_LEN-1);
  clownix_timeout_add(10, timer_qemu_monitor_name,
                      (void *) qemu_monitor_name, NULL, NULL);
  clownix_timeout_add(500, timer_launch_end, (void *) time_name, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void launch_qemu_vm(t_vm *vm)
{
  char **argv;
  int i, pid;
  argv = create_qemu_argv(vm);
  utils_send_creation_info(vm->kvm.name, argv);
  pid = pid_clone_launch(start_launch_args, launcher_death, NULL, 
                        (void *)argv, (void *)argv, NULL, 
                        vm->kvm.name, -1, 1);
  if (!pid)
    KERR("%s", vm->kvm.name);
  else
    {
    vm->pid_returned_clone = pid;
    for (i=vm->kvm.nb_dpdk; i<vm->kvm.nb_dpdk+vm->kvm.nb_eth; i++)
      {
      if (endp_mngt_kvm_pid_clone(vm->kvm.name, i, pid))
        KERR("%s %d", vm->kvm.name, i);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void timer_utils_finish_vm_init(char *name, int val)
{
  char *nm;
  nm = (char *) clownix_malloc(MAX_NAME_LEN, 9);
  memset(nm, 0, MAX_NAME_LEN);
  strncpy(nm, name, MAX_NAME_LEN-1);
  clownix_timeout_add(val, utils_finish_vm_init, (void *) nm, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void added_nat_cisco_done(void *data)
{
  char *name = (char * ) data;
  t_vm   *vm = cfg_get_vm(name);
  char cisco_nat_name[2*MAX_NAME_LEN];
  char lan_cisco_nat_name[2*MAX_NAME_LEN];
  int endp_type;
  memset(cisco_nat_name, 0, 2*MAX_NAME_LEN);
  memset(lan_cisco_nat_name, 0, 2*MAX_NAME_LEN);
  snprintf(cisco_nat_name, 2*MAX_NAME_LEN-1, "nat_%s", name);
  snprintf(lan_cisco_nat_name, 2*MAX_NAME_LEN-1, "lan_nat_%s", name);
  cisco_nat_name[MAX_NAME_LEN-1] = 0;
  lan_cisco_nat_name[MAX_NAME_LEN-1] = 0;
  if ((vm) && 
      (endp_mngt_exists(cisco_nat_name, 0, &endp_type)) &&
      (endp_type == endp_type_nat))
    {
    if (endp_evt_add_lan(0,0,name,vm->kvm.vm_config_param,lan_cisco_nat_name,0))
      KERR("%s", name);
    else if (endp_evt_add_lan(0,0,cisco_nat_name,0,lan_cisco_nat_name,0))
      KERR("%s", name);
    }
  free(name);
}
/*--------------------------------------------------------------------------*/

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
  int i, state;
  char *cisco_nat_name;
  t_vm   *vm = cfg_get_vm(name);
  t_wake_up_eths *wake_up;
  if (!vm)
    return;
  wake_up = vm->wake_up_eths;
  if (!wake_up)
    return;
  if (strcmp(wake_up->name, name))
    KOUT(" ");
  state = wake_up->state;
  if (status)
    {
    sprintf(err, "ERROR when creating %s\n", name);
    event_print(err);
    send_status_ko(wake_up->llid, wake_up->tid, err);
    utils_launched_vm_death(name, error_death_qemuerr);
    return;
    }
  switch (state)
    {
    case auto_idle:
      wake_up->state = auto_delay_possible_ovs_start;
      if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)
        arm_static_vm_timeout(name, 1);
      else if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_EVANESCENT)
        derived_file_creation_request(vm);
      else
        KOUT("%X", vm->kvm.vm_config_flags);
      break;
    case auto_delay_possible_ovs_start:
      if (vm->kvm.nb_dpdk)
        {
        wake_up->dpdk_count += 1;
        if (wake_up->dpdk_count > 20)
          {
          KERR("ERROR dpdk ovs start when creating %s\n", name);
          sprintf(err, "ERROR dpdk ovs start when creating %s\n", name);
          event_print(err);
          send_status_ko(wake_up->llid, wake_up->tid, err);
          utils_launched_vm_death(name, error_death_qemuerr);
          }
        else
          {
          arm_static_vm_timeout(name, 100);
          }
        if (dpdk_ovs_muovs_ready())
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
      for (i=vm->kvm.nb_dpdk; i<vm->kvm.nb_dpdk+vm->kvm.nb_eth; i++)
        {
        if (endp_mngt_start(0, 0, vm->kvm.name, i, endp_type_kvm_eth))
          KERR("%s %d", vm->kvm.name, i);
        }
      for (i=vm->kvm.nb_dpdk + vm->kvm.nb_eth; 
           i<vm->kvm.nb_dpdk + vm->kvm.nb_wlan + vm->kvm.nb_eth; i++)
        {
        if (endp_mngt_start(0, 0, vm->kvm.name, i, endp_type_kvm_wlan))
          KERR("WLAN %s %d", vm->kvm.name, i);
        }
      launch_qemu_vm(vm);
      arm_static_vm_timeout(name, 100);
      break;
    case auto_create_vm_connect:
      timer_utils_finish_vm_init(name, 4000);
      qmonitor_begin_qemu_unix(name);
      qmp_begin_qemu_unix(name);
      qhvc0_begin_qemu_unix(name);
      if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_CISCO)
        {
        cisco_nat_name = (char *)malloc(2*MAX_NAME_LEN);
        memset(cisco_nat_name, 0, 2*MAX_NAME_LEN);
        snprintf(cisco_nat_name, 2*MAX_NAME_LEN-1, "nat_%s", name);
        cisco_nat_name[MAX_NAME_LEN-1] = 0;
        if (endp_mngt_start(0, 0, cisco_nat_name, 0, endp_type_nat))
          {
          free(cisco_nat_name);
          KERR("%s", name);
          }
        else
          {
          memset(cisco_nat_name, 0, 2*MAX_NAME_LEN);
          snprintf(cisco_nat_name, 2*MAX_NAME_LEN-1, "%s", name);
          clownix_timeout_add(100, added_nat_cisco_done, 
                              (void *)cisco_nat_name,NULL,NULL);
          }
        }
      break;
    default:
      KOUT(" ");
    }
}
/*--------------------------------------------------------------------------*/



