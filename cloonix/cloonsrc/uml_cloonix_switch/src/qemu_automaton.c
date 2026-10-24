/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/syscall.h>
#include <linux/io_uring.h>

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
#include "qcow2_hdr.h"

#define DRIVE_PARAMS_CISCO " -device virtio-scsi-pci,id=scsi"\
                           " -device scsi-hd,drive=hd"\
                           " -drive if=none,id=hd,file=%s"

#define DRIVE_PARAMS " -drive file=%s,index=%d,media=disk,if=virtio,"\
                     "cache=none,aio=%s,discard=unmap"

#define DRIVE_FULL_VIRT " -blockdev node-name=%s,driver=qcow2,"\
                        "file.driver=file,file.node-name=file,file.filename=%s"\
                        " -device ide-hd,bus=ide.2,unit=0,drive=%s,bootindex=0"

#define INSTALL_DISK " -boot d -drive file=%s,index=%d,media=disk,if=virtio"

#define AGENT_CDROM " -drive file=%s,if=none,media=cdrom,id=cd"\
                    " -device virtio-scsi-pci"\
                    " -device scsi-cd,drive=cd"

#define AGENT_CDROM_IDE " -drive file=%s,if=none,media=cdrom,id=cd"\
                        " -device ide-cd,bus=ide.1,unit=0,drive=cd"

#define ADDED_CDROM " -drive file=%s,media=cdrom"

#define BLOCKNAME " -device virtio-scsi-pci,id=virtio_scsi_pci0"\
                  " -blockdev driver=file,cache.direct=on,aio=%s,filename=%s,node-name=lol%s"\
                  " -blockdev driver=qcow2,cache.direct=on,node-name=%s,file=lol%s"\
                  " -device scsi-hd,drive=%s,discard_granularity=4096"

int get_conf_rank(void);

typedef struct t_cprootfs_config
{
  char name[MAX_NAME_LEN];
  char msg[MAX_PRINT_LEN];
  char backing[MAX_PATH_LEN];
  char used[MAX_PATH_LEN];
} t_cprootfs_config;

void qemu_vm_automaton(void *unused_data, int status, char *name);

static int get_wake_up_eths(char *name, t_vm **vm,
                            t_wake_up_eths **wake_up_eths)
{
  *vm = cfg_get_vm(name);
  if (!(*vm))
    {
    KERR("ERROR wake: no kvm %s", name);
    return -1;
    }
  *wake_up_eths = (*vm)->wake_up_eths;
  if (!(*wake_up_eths))
    {
    KERR("ERROR wake: %s has no eth record", name);
    return -1;
    }
  return 0;
}

static void static_vm_timeout(void *data)
{
  char *name = (char *) data;
  t_wake_up_eths *wake_up_eths;
  t_vm *vm;
  if (get_wake_up_eths(name, &vm, &wake_up_eths))
    KERR("ERROR %s: its create timer fired with no create left", name);
  else
    {
    wake_up_eths->automate_abs_beat = 0;
    wake_up_eths->automate_ref = 0;
    qemu_vm_automaton(NULL, 0, name);
    }
  clownix_free(data, __FUNCTION__);
}

static void cprootfs_clone_death(void *data, int status, char *name)
{
  t_cprootfs_config *cprootfs = (t_cprootfs_config *) data;
  t_vm   *vm;
  t_wake_up_eths *wake_up_eths;

  if (get_wake_up_eths(name, &vm, &wake_up_eths))
    {
    KERR("WARNING %s: no eths waiting to wake", name);
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

static void cprootfs_clone_msg(void *data, char *msg)
{
  t_cprootfs_config *cprootfs = (t_cprootfs_config *) data;
  t_vm   *vm;
  t_wake_up_eths *wake_up_eths;
  if (!get_wake_up_eths(cprootfs->name, &vm, &wake_up_eths))
    strncpy(cprootfs->msg, msg, MAX_NAME_LEN-1);
}

static int cprootfs_clone(void *data)
{
  int result;
  t_cprootfs_config *cprootfs = (t_cprootfs_config *) data;
  result = qcow2_create_derived(cprootfs->backing, cprootfs->used);
  if (result)
    {
    KERR("ERROR qcow2 overlay %s on %s: %s", cprootfs->used,
         cprootfs->backing, strerror(errno));
    send_to_daddy("KO");
    }
  else
    {
    send_to_daddy("OK");
    }
  return result;
}

static void derived_file_creation_request(t_vm *vm)
{
  char *name;
  t_cprootfs_config *cprootfs;
  if (!vm)
    KOUT(" ");
  name = vm->kvm.name;
  cprootfs=(t_cprootfs_config *)clownix_malloc(sizeof(t_cprootfs_config), 27);
  memset(cprootfs, 0, sizeof(t_cprootfs_config));
  strncpy(cprootfs->name, name, MAX_NAME_LEN-1);
  strcpy(cprootfs->msg, "NO_MSG");

  strncpy(cprootfs->used, vm->kvm.rootfs_used, MAX_PATH_LEN-1);
  strncpy(cprootfs->backing, vm->kvm.rootfs_backing, MAX_PATH_LEN-1);

  pid_clone_launch(cprootfs_clone, cprootfs_clone_death,
                   cprootfs_clone_msg, cprootfs, 
                   cprootfs, cprootfs, name, -1, 1);
}

#define QEMU_OPTS_BASE \
   " -m %d"\
   " -name %s"\
   " -nodefaults"\
   " -chardev stdio,id=char0,signal=off"\
   " -serial  chardev:char0"\
   " -boot    strict=on"

#define QEMU_OPTS_QMP \
   " -chardev socket,id=qmp1,path=%s,server=on,wait=off"\
   " -object monitor-qmp,id=mon0,chardev=qmp1"

#define QEMU_OPTS_CLOON \
   " -device  virtio-serial-pci"\
   " -chardev socket,path=%s,server=on,wait=off,id=cloon0"\
   " -device  virtserialport,chardev=cloon0,name=net.cloon.0"\
   " -chardev socket,path=%s,server=on,wait=off,id=qga0" \
   " -device  virtserialport,chardev=qga0,name=org.qemu.guest_agent.0"\
   " -device  virtio-balloon-pci,id=balloon0"\
   " -object  rng-random,filename=/dev/urandom,id=rng0"\
   " -device  virtio-rng-pci,rng=rng0"

#define QEMU_VNC \
   " -device   VGA"\
   " -device   qemu-xhci"\
   " -device   usb-tablet"\
   " -vnc      unix:%s"

/* OVMF as two pflash units: the code read only and shared, the variables
   writable and one copy per vm. */
static int uefi_pflash_format(t_vm *vm, char *qemu_cmd)
{
  char vars[MAX_PATH_LEN];
  char *code = pthexec_ovmf_code();
  char *tmpl = pthexec_ovmf_vars();
  char buf[4096];
  int fdi, fdo, nb, len = 0;

  if (!code)
    {
    KERR("ERROR %s: no OVMF in cloonfs/bin/qemu, uefi needs the shipped "
         "edk2-x86_64-code.fd", vm->kvm.name);
    return 0;
    }
  /* No x86 vars template: OVMF is stateless, code only read-only, the boot
     manager starts from EFI/BOOT/BOOTX64.EFI of the disk. */
  if (!tmpl)
    {
    len += sprintf(qemu_cmd+len,
                   " -drive if=pflash,format=raw,unit=0,readonly=on,file=%s",
                   code);
    return len;
    }
  memset(vars, 0, MAX_PATH_LEN);
  /* Beside the rootfs when the rootfs is kept, in the vm directory when it is
     not. */
  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)
    snprintf(vars, MAX_PATH_LEN-1, "%s.vars.fd", vm->kvm.rootfs_used);
  else
    snprintf(vars, MAX_PATH_LEN-1, "%s/%s/OVMF_VARS.fd",
             cfg_get_work_vm(vm->kvm.vm_id), DIR_UMID);
  if (!file_exists(vars, R_OK))
    {
    fdi = open(tmpl, O_RDONLY);
    if (fdi < 0)
      {
      KERR("ERROR %s: cannot read %s", vm->kvm.name, tmpl);
      return 0;
      }
    fdo = open(vars, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fdo < 0)
      {
      KERR("ERROR %s: cannot write %s", vm->kvm.name, vars);
      close(fdi);
      return 0;
      }
    while ((nb = read(fdi, buf, sizeof(buf))) > 0)
      {
      if (write(fdo, buf, nb) != nb)
        {
        KERR("ERROR %s: short write on %s", vm->kvm.name, vars);
        break;
        }
      }
    close(fdi);
    close(fdo);
    }
  len += sprintf(qemu_cmd+len,
                 " -drive if=pflash,format=raw,unit=0,readonly=on,file=%s"
                 " -drive if=pflash,format=raw,unit=1,file=%s", code, vars);
  return len;
}

/* io_uring may be refused by the kernel (kernel.io_uring_disabled on
   RHEL): probed once, threads then. */
static char *aio_mode(void)
{
  static char *mode = NULL;
  struct io_uring_params p;
  int fd;
  if (mode == NULL)
    {
    memset(&p, 0, sizeof(p));
    fd = syscall(SYS_io_uring_setup, 4, &p);
    if (fd < 0)
      {
      KERR("WARNING io_uring refused by the kernel: %s, qemu disks with "
           "aio=threads", strerror(errno));
      mode = "threads";
      }
    else
      {
      close(fd);
      mode = "io_uring";
      }
    }
  return mode;
}


/* the last line qemu wrote on its stderr */
static void stderr_tail(char *name, char *tail, int len)
{
  char buf[4096], *nl;
  FILE *lf = fopen(utils_get_qemu_stderr_log_path(name), "r");
  size_t rd = 0;
  memset(tail, 0, len);
  if (!lf)
    return;
  if (fseek(lf, -((long) sizeof(buf) - 1), SEEK_END))
    rewind(lf);
  rd = fread(buf, 1, sizeof(buf) - 1, lf);
  fclose(lf);
  buf[rd] = 0;
  while (rd && ((buf[rd-1] == '\n') || (buf[rd-1] == '\r')))
    buf[--rd] = 0;
  nl = strrchr(buf, '\n');
  strncpy(tail, nl ? nl + 1 : buf, len - 1);
}

static void create_linux_cmd_kvm(t_vm *vm, char *qemu_cmd)
{
  int i, nb_cpu, len = 0;
  char cpu_type[2*MAX_NAME_LEN];
  char *rootfs, *added_disk;
  char *vnc_path, *cdrom;
  if (!vm)
    KOUT(" ");
  vnc_path = utils_get_vnc_path(vm->kvm.vm_id);
  nb_cpu = vm->kvm.cpu;

  /* Modern-distro-only guests (recent kernels): maximum performance. */
  strcpy(cpu_type, "host,migratable=off,"
                   "+kvm-pv-eoi,+kvm-pv-unhalt,+kvm-pv-tlb-flush,"
                   "+kvm-pv-ipi,+topoext");
  len += sprintf(qemu_cmd+len, QEMU_OPTS_BASE, vm->kvm.mem, vm->kvm.name);
  if (getuid() != 0)
    len += sprintf(qemu_cmd+len, " -run-with user=%d:%d",
                   (int) getuid(), (int) getgid());
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
                 " -machine q35,accel=kvm,usb=off,dump-guest-core=off,"
                            "vmport=off,hpet=off"
                 " -cpu %s"
                 " -smp %d,maxcpus=%d,sockets=1,cores=%d,threads=1"
                 " -overcommit mem-lock=off"
                 " -global kvm-pit.lost_tick_policy=discard",
                 cfg_get_work_vm(vm->kvm.vm_id), DIR_UMID,
                 cpu_type, nb_cpu, nb_cpu, nb_cpu);
  len += sprintf(qemu_cmd+len, QEMU_VNC, vnc_path);
  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_UEFI)
    len += uefi_pflash_format(vm, qemu_cmd+len);
  rootfs = vm->kvm.rootfs_used;
  added_disk = vm->kvm.added_disk;
  if  (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_NO_REBOOT)
    {
    len += sprintf(qemu_cmd+len, " -no-reboot");
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
        len += sprintf(qemu_cmd+len, BLOCKNAME, aio_mode(), rootfs,
                       vm->kvm.name, vm->kvm.name, vm->kvm.name, vm->kvm.name);
        }
      } 
    cdrom = utils_get_cdrom_path_name(vm);
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_FULL_VIRT)
      len += sprintf(qemu_cmd+len, AGENT_CDROM_IDE, cdrom);
    else
      len += sprintf(qemu_cmd+len, AGENT_CDROM, cdrom);
  
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK)
      {
      len += sprintf(qemu_cmd+len, DRIVE_PARAMS, added_disk, 1, aio_mode());
      }
    }
  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_ADDED_CDROM)
    {
    len += sprintf(qemu_cmd+len, ADDED_CDROM, vm->kvm.added_cdrom);
    }
  len += sprintf(qemu_cmd+len, QEMU_OPTS_QMP,
                 utils_get_qmp_path(vm->kvm.vm_id));
}
              
static char *alloc_argv(char *str)
{
  int len = strlen(str);
  char *argv = (char *)clownix_malloc(len + 1, 15);
  memset(argv, 0, len + 1);
  strncpy(argv, str, len);
  return argv;
}

static char **create_qemu_argv(t_vm *vm)
{
  int i = 0;
  static char **argv;
  char qemu_cmd[30*MAX_PATH_LEN];
  char namespace[MAX_PATH_LEN];
  char qemu_bin[MAX_PATH_LEN];
  char qemu_data[MAX_PATH_LEN];
  char *ptr1;
  memset(qemu_cmd, 0, 10*MAX_PATH_LEN);
  memset(namespace, 0, MAX_PATH_LEN);
  memset(qemu_bin, 0, MAX_PATH_LEN);
  memset(qemu_data, 0, MAX_PATH_LEN);

  snprintf(namespace, MAX_PATH_LEN-1, "%s_%s",
           BASE_NAMESPACE, cfg_get_cloonix_name());
  snprintf(qemu_bin, MAX_PATH_LEN-1, "%s/cloonfs/bin/cloonix-qemu-system",
           cfg_get_bin_dir());
  snprintf(qemu_data, MAX_PATH_LEN-1, "%s/cloonfs/bin/qemu", cfg_get_bin_dir());
  create_linux_cmd_kvm(vm, qemu_cmd);

  argv = (char **)clownix_malloc(200 * sizeof(char *), 27);
  memset(argv, 0, 200 * sizeof(char *));
  argv[i++] = alloc_argv(utils_get_dtach_bin_path());
  argv[i++] = alloc_argv("-n");
  argv[i++] = alloc_argv(utils_get_dtach_sock_path(vm->kvm.name));
  {
    static char launch[MAX_TRACE_LEN];
    char *script  = utils_get_qemu_launch_script_path(vm->kvm.name);
    char *err_log = utils_get_qemu_stderr_log_path(vm->kvm.name);
    FILE *fp;
    ptr1 = qemu_cmd;
    ptr1 += strspn(ptr1, " ");
    /* No exec: bash stays to record how qemu ended, a signal says nothing
       by itself; guard.c of suid_power knows these two lines exactly. */
    if (snprintf(launch, MAX_TRACE_LEN,
                 "%s --net=%s%s %s -L %s %s 2>%s\necho qemu exit $? >>%s",
                 pthexec_nsenter_bin(), PATH_NAMESPACE, namespace,
                 qemu_bin, qemu_data, ptr1, err_log, err_log) >= MAX_TRACE_LEN)
      KOUT("ERROR qemu command line too long for %s", vm->kvm.name);
    /* A qemu exit line in it is then of this launch. */
    unlink(err_log);
    fp = fopen(script, "w");
    if (!fp)
      KOUT("ERROR cannot write qemu launch script %s", script);
    /* The shebang names the bash of cloonfs, not the host's. */
    fprintf(fp, "#!%s\n%s\n", pthexec_bash_bin(), launch);
    fclose(fp);
    if (chmod(script, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH))
      KERR("ERROR chmod %s", script);
    event_trace(trace_kind_qemu, vm->kvm.name, launch);
    argv[i++] = alloc_argv(pthexec_bash_bin());
    argv[i++] = alloc_argv(script);
  }
  argv[i++] = NULL;
  return argv;
}

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
        KERR("ERROR the wake record of %s names %s", name,
                 wake_up_eths->name);
      else
        {
        if (!file_exists(utils_get_dtach_sock_path(name), R_OK))
          {
          char *err_log = utils_get_qemu_stderr_log_path(name);
          char qemu_err[MAX_PRINT_LEN];
          FILE *lf;
          memset(qemu_err, 0, MAX_PRINT_LEN);
          lf = fopen(err_log, "r");
          if (lf)
            {
            size_t rd = fread(qemu_err, 1, MAX_PRINT_LEN - 1, lf);
            qemu_err[rd] = 0;
            fclose(lf);
            }
          snprintf(err, MAX_PRINT_LEN-1,
                   "ERROR QEMU UNEXPECTED STOP %s\nqemu stderr (%s):\n%s",
                   name, err_log,
                   (qemu_err[0]) ? qemu_err : "(empty - see logfile)");
          KERR("%s", err);
          free_wake_up_eths_and_delete_vm(vm, error_death_qemu_quiterr, err);
          }
        }
      }
    }
  clownix_free(data, __FUNCTION__);
}

#define MAX_TRIES_NO_PID_YET 600

static void timer_qemu_monitor_name(void *data)
{
  char tail[MAX_PRINT_LEN];
  char *name = (char *) data;
  t_vm   *vm = cfg_get_vm(name);
  int pid;
  if (!vm)
    {
    clownix_free(data, __FUNCTION__);
    return;
    }
  pid = suid_power_get_pid(vm->kvm.vm_id);
  if ((pid == 0) && (vm->vm_to_be_killed == 0))
    {
    if (vm->pid == 0)
      {
      vm->count_no_pid_yet += 1;
      if (vm->count_no_pid_yet > MAX_TRIES_NO_PID_YET)
        {
        KERR("ERROR QEMU PID ABSENT %s after %d tries (%d seconds), "
             "powering it off", name, vm->count_no_pid_yet,
             (vm->count_no_pid_yet * 10 * 10) / 1000);
        poweroff_vm(0, 0, vm);
        clownix_free(data, __FUNCTION__);
        }
      else
        {
        clownix_timeout_add(10, timer_qemu_monitor_name, data, NULL, NULL);
        }
      }
    else
      {
      if (vm->qemu_shutdown_seen)
        KTRC("%s: qemu pid %d is gone after the guest powered down",
             name, vm->pid);
      else
        {
        stderr_tail(name, tail, sizeof(tail));
        KERR("ERROR %s: qemu pid %d died without a guest shutdown: %s "
             "(\"%s\")", name, vm->pid, tail[0] ? tail : "stderr empty",
             utils_get_qemu_stderr_log_path(name));
        }
      poweroff_vm(0, 0, vm);
      clownix_free(data, __FUNCTION__);
      }
    }
  else
    clownix_timeout_add(10, timer_qemu_monitor_name, data, NULL, NULL);
}

static void launch_qemu_vm_end(char *name, int ok)
{
  int i;
  char err[MAX_PRINT_LEN];
  char *time_name;
  char *qemu_monitor_name;
  t_vm   *vm = cfg_get_vm(name);
  if (!vm)
    {
    KERR("ERROR %s: launch answered for a vm that is gone", name);
    }
  else
   {
    for (i=0; vm->launcher_argv[i] != NULL; i++)
      clownix_free(vm->launcher_argv[i], __FUNCTION__);
    clownix_free(vm->launcher_argv, __FUNCTION__);
    vm->launcher_argv = NULL;
    if (!ok)
      {
      if (vm->wake_up_eths)
        {
        snprintf(err, MAX_PRINT_LEN-1, "ERROR suid_power could not launch "
                 "qemu for %s, see \"%s\"", name,
                 utils_get_qemu_stderr_log_path(name));
        free_wake_up_eths_and_delete_vm(vm, error_death_qemuerr, err);
        }
      else
        poweroff_vm(0, 0, vm);
      return;
      }
    time_name = clownix_malloc(MAX_NAME_LEN, 35);
    memset(time_name, 0, MAX_NAME_LEN);
    strncpy(time_name, vm->kvm.name, MAX_NAME_LEN-1);
    qemu_monitor_name = clownix_malloc(MAX_NAME_LEN, 35);
    memset(qemu_monitor_name, 0, MAX_NAME_LEN);
    strncpy(qemu_monitor_name, vm->kvm.name, MAX_NAME_LEN-1);
    clownix_timeout_add(10, timer_qemu_monitor_name,
                        (void *) qemu_monitor_name, NULL, NULL);
    clownix_timeout_add(3000, timer_launch_end, (void *) time_name, NULL, NULL);
   }
}

static void launch_qemu_vm_start(t_vm *vm)
{
  if (vm->launcher_argv != NULL)
    KOUT("%s", vm->kvm.name);
  vm->launcher_argv = create_qemu_argv(vm);
  utils_send_creation_info(vm->kvm.name, vm->launcher_argv);
  suid_power_launch_vm(vm, launch_qemu_vm_end);
}

void timer_utils_finish_vm_init(int vm_id, int val)
{
  unsigned long ul_vm_id = (unsigned long) vm_id;
  clownix_timeout_add(val,utils_finish_vm_init,(void *)ul_vm_id,NULL,NULL);
}

static void arm_static_vm_timeout(char *name, int delay)
{
  char *nm;
  t_vm *vm = cfg_get_vm(name);
  if ((!vm) || (!vm->wake_up_eths))
    KOUT("%s", name);
  if (vm->wake_up_eths->automate_abs_beat)
    KOUT("%s: create timer armed twice", name);
  nm = (char *) clownix_malloc(MAX_NAME_LEN, 9);
  memset(nm, 0, MAX_NAME_LEN);
  strncpy(nm, name, MAX_NAME_LEN-1);
  clownix_timeout_add(delay, static_vm_timeout, (void *)nm,
                      &(vm->wake_up_eths->automate_abs_beat),
                      &(vm->wake_up_eths->automate_ref));
}

void qemu_vm_automaton(void *unused_data, int status, char *name) 
{
  char err[MAX_PRINT_LEN];
  int i, state, nb_eth = 0;
  t_vm   *vm = cfg_get_vm(name);
  t_wake_up_eths *wake_up;
  if (!vm)
    {
    KERR("WARNING %s is gone before its eths woke", name);
    return;
    }
  wake_up = vm->wake_up_eths;
  if (!wake_up)
    {
    KERR("WARNING %s is gone before its eths woke", name);
    return;
    }
  if (strcmp(wake_up->name, name))
    KOUT(" ");
  state = wake_up->state;
  if (status)
    {
    snprintf(err, sizeof(err), "ERROR when creating %s\n", name);
    KERR("%s", err);
    free_wake_up_eths_and_delete_vm(vm, error_death_qemuerr, err);
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
    /* Wait for ovs only if ovs is not there yet. */
    case auto_delay_possible_ovs_start:
      for (i = 0; i < vm->kvm.nb_tot_eth; i++)
        {
        if ((vm->kvm.eth_table[i].endp_type == endp_type_eths) ||
            (vm->kvm.eth_table[i].endp_type == endp_type_ethv))
          nb_eth += 1;
        }
      if ((nb_eth) && (!get_daemon_done()))
        {
        wake_up->eth_count += 1;
        if (wake_up->eth_count > 100)
          {
          KERR("ERROR ovs never came up while creating %s, gave up after "
               "%d tries", name, wake_up->eth_count);
          snprintf(err, sizeof(err), "ERROR ovs start when creating %s\n", name);
          free_wake_up_eths_and_delete_vm(vm, error_death_qemuerr, err);
          }
        else
          {
          /* Stay in this state on purpose: look again in half a second. */
          arm_static_vm_timeout(name, 100);
          }
        }
      else
        {
        if (wake_up->eth_count)
          KTRC("%s waited %d half-second(s) for ovs before launching",
               name, wake_up->eth_count);
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
      /* The ok means qemu runs: pid looked for every 200 ms, 20 s max. The agent is not part of it. */
      timer_utils_finish_vm_init(vm->kvm.vm_id, 1);
      qmp_begin_qemu_unix(name, 1);
      wake_up->coherency_unlocked = 1;
      recv_coherency_unlock();
      break;
    default:
      KOUT(" ");
    }
}

