/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
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
#include "uml_clownix_switch.h"
#include "commun_daemon.h"
#include "heartbeat.h"
#include "machine_create.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "qcow2_hdr.h"
#include "system_callers.h"
#include "automates.h"
#include "lan_to_name.h"
#include "utils_cmd_line_maker.h"
#include "wifi_air.h"
#include "pcap_relay.h"
#include "file_read_write.h"
#include "qmp.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "hop_event.h"
#include "ovs.h"
#include "kvm.h"
#include "suid_power.h"
#include "slowperiodic.h"
#include "targz.h"
#include "list_commands.h"
#include "cnt.h"
#include "ovs_snf.h"
#include "ack_pending.h"
#include "ovs_nat_main.h"
#include "ovs_a2b.h"
#include "ovs_tap.h"
#include "ovs_phy.h"
#include "ovs_c2c.h"
#include "msg.h"
#include "crun.h"
#include "proxymous.h"
#include "gui_server_mngt.h"
#include "cloonix_conf_info.h"
#include "pthexec.h"
#include "layout_store.h"

t_cloonix_conf_info *get_cloonix_conf_info(void);
int inside_cloonix(char **name);
extern int clownix_server_fork_llid;
static int g_inside_cloonix;
static char *g_cloonix_vm_name;

int get_uml_cloonix_started(void);
void shadown_lan_del_all(void);

typedef struct t_timer_kvm_delete
{
  char name[MAX_NAME_LEN];
  int llid;
  int tid;
  int count;
  int busy_count;
} t_timer_kvm_delete;
typedef struct t_timer_del
{
  int llid;
  int tid;
  int tzcount;
  int kill_cloon;
} t_timer_del;
typedef struct t_timer_zombie
{
  int llid;
  int tid;
  int nb_try;
  int count;
  t_topo_kvm kvm;
} t_timer_zombie;
typedef struct t_add_vm_cow_look
{
  char msg[MAX_PATH_LEN];
  int llid;
  int tid;
  int vm_id;
  t_topo_kvm kvm;
} t_add_vm_cow_look;
static int g_inhib_new_clients;

static int g_killing_cloonix;
int get_killing_cloonix(void)
{
  return g_killing_cloonix;
}

int get_inhib_new_clients(void)
{
  if (g_inhib_new_clients)
    {
    KERR("ERROR Server being killed, no new client");
    }
  return g_inhib_new_clients;
}

/* Refusals shared by every request: the server is not started, the net is  */
/* in autodestruct, or the name already belongs to an object. 1 = refused.  */
/* A refusal: one reason, the same to syslog and to the client. */
static void refuse(int llid, int tid, const char *fmt, ...)
  __attribute__ ((format (printf, 3, 4)));
static void refuse(int llid, int tid, const char *fmt, ...)
{
  char reason[MAX_PATH_LEN];
  char *r = reason, *nl;
  va_list ap;
  memset(reason, 0, MAX_PATH_LEN);
  va_start(ap, fmt);
  vsnprintf(reason, MAX_PATH_LEN-1, fmt, ap);
  va_end(ap);
  while ((nl = strchr(reason, '\n')) != NULL)
    *nl = ' ';
  if (!strncmp(r, "ERROR ", 6))
    r += 6;
  KERR("ERROR %s", r);
  send_status_ko(llid, tid, r);
}

static int refuse_not_ready(int llid, int tid)
{
  if (get_uml_cloonix_started())
    return 0;
  send_status_ko(llid, tid, "SERVER NOT READY");
  KERR("ERROR SERVER NOT READY");
  return 1;
}

static int refuse_new_name(int llid, int tid, char *name)
{
  char err[MAX_PATH_LEN];
  if (get_inhib_new_clients())
    {
    KERR("ERROR %s refused: the net is stopping", name);
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    return 1;
    }
  if (cfg_name_is_in_use(0, name, err))
    {
    KERR("ERROR %s refused: %s", name, err);
    send_status_ko(llid, tid, err);
    return 1;
    }
  return 0;
}

void recv_work_dir_req(int llid, int tid)
{
  t_topo_clc *conf = cfg_get_topo_clc();
  send_work_dir_resp(llid, tid, conf);
}

static int kvm_gone_test(void *ctx, char *err)
{
  if (cfg_get_vm((char *) ctx))
    return 0;
  return 1;
}

static void timer_poweroff_eth(void *data)
{
  t_timer_kvm_delete *kd = (t_timer_kvm_delete *) data;
  t_vm *vm = cfg_get_vm(kd->name);
  t_eth_table *eth_tab;
  int i, can_delete, glob_can_delete;
  if (vm == NULL)
    {
    KTRC("%s: gone before its poweroff finished", kd->name);
    if (kd->llid)
      send_status_ok(kd->llid, kd->tid, "OK");
    clownix_free(kd, __FILE__);
    }
  else if (msg_ovsreq_get_qty() > 50)
    {
    /* as the delete of a cnt: waits, said after 30 s only */
    kd->busy_count += 1;
    if ((kd->busy_count % 300) == 0)
      KERR("WARNING %s: still waiting to be deleted, the ovs queue has %d "
           "requests in it and has been busy for %d tries",
           kd->name, msg_ovsreq_get_qty(), kd->busy_count);
    clownix_timeout_add(10, timer_poweroff_eth, (void *) kd, NULL, NULL);
    }
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
        KERR("ERROR %s: its eths could not be released after %d tries, "
             "deleting it anyway", kd->name, kd->count);
      if (vm->vm_to_be_killed == 0)
        machine_death(kd->name, error_death_noerr);
      if (kd->llid)
        {
        char *ctx = (char *) malloc(MAX_NAME_LEN);
        memset(ctx, 0, MAX_NAME_LEN);
        strncpy(ctx, kd->name, MAX_NAME_LEN-1);
        ack_pending(kd->llid, kd->tid, 100, 25000, "del kvm",
                    kvm_gone_test, NULL, ctx);
        }
      clownix_free(kd, __FILE__);
      }
    }
}

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

void recv_vmcmd(int llid, int tid, char *name, int cmd, int param)
{
  t_vm *vm = cfg_get_vm(name);
  char err[MAX_PATH_LEN];
  if (refuse_not_ready(llid, tid))
    return;

  switch(cmd)
    {
    case vmcmd_reboot_with_qemu:
      if (vm)
        {
        vm->reboot_asked_ms = cloonix_get_msec();
        doors_send_command(get_doorways_llid(), 0, name, REBOOT_ANNOUNCED);
        qmp_request_qemu_reboot(name);
        send_status_ok(llid, tid, "reboot");
        }
      else
        {
        KERR("WARNING reboot refused: %s is not a machine", name);
        snprintf(err, MAX_PATH_LEN-1, "ERROR: %s NOT FOUND", name);
        send_status_ko(llid, tid, err);
        }
      break;

    default:
      KERR("ERROR %d", cmd);
      break;
    }
}

void recv_trace_sub(int llid, int tid, int kind, int sub_on)
{
  char err[MAX_PATH_LEN];
  int sub_evt = event_trace_sub_evt(kind);
  if (sub_evt == -1)
    {
    KERR("WARNING trace refused: unknown kind %d", kind);
    snprintf(err, MAX_PATH_LEN-1, "ERROR: UNKNOWN TRACE KIND %d", kind);
    send_status_ko(llid, tid, err);
    return;
    }
  if (!msg_exist_channel(llid))
    {
    KERR("WARNING a trace request arrived on llid %d whose channel is "
         "already gone", llid);
    return;
    }
  if (sub_on)
    {
    event_subscribe(sub_evt, llid, tid);
    send_status_ok(llid, tid, "tracesub");
    }
  else
    {
    event_unsubscribe(sub_evt, llid);
    send_status_ok(llid, tid, "traceunsub");
    }
}

void recv_event_topo_sub(int llid, int tid)
{
  if (msg_exist_channel(llid))
    {
    event_subscribe(sub_evt_topo, llid, tid);
    cfg_hysteresis_send_topo_info();
    layout_replay(llid, tid);
    }
  else
    {
    KERR("WARNING a subscribe request arrived on llid %d whose channel is "
         "already gone", llid);
    send_status_ko(llid, tid, "Abnormal!");
    }
}

static char *read_sys_file(char *file_name, char *err)
{
  char *buf = NULL;
  int fd, len;
  fd = open(file_name, O_RDONLY);
  if (fd >= 0)
    {
    buf = (char *) clownix_malloc(100,13);
    memset(buf, 0, 100);
    len = (int) read(fd, buf, 99);
    if (len < 0)
      len = 0;
    buf[len] = 0;
    close (fd);
    }
  else
    sprintf(err, "Cannot open file %s\n", file_name);
  return buf;
}

#define SYS_KVM_DEV "/sys/devices/virtual/misc/kvm/dev"
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
        sprintf(info, "UNEXPECTED %s", SYS_KVM_DEV);
      }
    else
      snprintf(info, MAX_PATH_LEN, "ERR %s %s", SYS_KVM_DEV, err);
    clownix_free(buf, __FUNCTION__);
    }
  else
    sprintf(info,
            "/dev/kvm not found, \"modprobe kvm_intel/kvm_amd nested=1\""
            " and \"chmod 666 /dev/kvm\", (on the real host!)\n");
  return result;
}

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

static int test_dev_kvm(char *info)
{
  char why[MAX_NAME_LEN];
  int result = -1;
  if (machine_is_kvm_able())
    {
    if (g_inside_cloonix)
      inside_cloonix_test_dev_kvm(info);
    if (dev_file_rw_ko("/dev/kvm", why))
      {
      KERR("ERROR KO /dev/kvm %s, Maybe: chmod 0666 /dev/kvm", why);
      KERR("OR: sudo setfacl -m u:${USER}:rw /dev/kvm");
      }
    else
      result = 0;
    }
  else
    {
    sprintf(info, "No hardware virtualisation! kvm needs it!");
    KERR("ERROR KO hardware virtualisation");
    }
  return result;
}

static int image_missing(char *image, int is_qcow2, char *info)
{
  char path[MAX_PATH_LEN];
  char base[MAX_NAME_LEN];
  char *dot;
  /* An image named, not a path: the name rule of every other object. */
  if ((int) strlen(image) >= MAX_NAME_LEN)
    {
    snprintf(info, MAX_PATH_LEN, "image name refused: \"%.40s...\" has %d "
             "chars, %d at most", image, (int) strlen(image), MAX_NAME_LEN-1);
    return -1;
    }
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", cfg_get_bulk(), image);
  if (file_exists(path, R_OK))
    return 0;
  if (get_running_in_crun())
    {
    snprintf(path, MAX_PATH_LEN-1, "%s/%s", cfg_get_bulk_host(), image);
    if (file_exists(path, R_OK))
      return 0;
    }
  memset(base, 0, MAX_NAME_LEN);
  strncpy(base, image, MAX_NAME_LEN-1);
  dot = strstr(base, ".qcow2");
  if (dot)
    *dot = 0;
  if (is_qcow2)
    {
    if (!strcmp(base, "c8000v"))
      snprintf(info, MAX_PATH_LEN-1,
               "%s is not in %s: build it from the cisco iso with "
               "vm_create/cisco/doitall.sh", image, cfg_get_bulk());
    else if (!strcmp(base, "win"))
      snprintf(info, MAX_PATH_LEN-1,
               "%s is not in %s: convert a microsoft disk as the windows "
               "chapter of the user doc says", image, cfg_get_bulk());
    else
      snprintf(info, MAX_PATH_LEN-1,
               "%s is not in %s: build it with kvm_%s.sh of vm_create, "
               "or take %s.gz from https://clownix.net/downloads.html "
               "and gunzip it there",
               image, cfg_get_bulk(), base, image);
    }
  else
    snprintf(info, MAX_PATH_LEN-1,
             "%s is not in %s, as a directory or as %s.tar.gz: build it "
             "with cnt_%s.sh of vm_create, or take %s.tar.gz from "
             "https://clownix.net/downloads.html "
             "and put it there, cloonix extracts it", image, cfg_get_bulk(),
             image, base, image);
  return -1;
}

/* Reason first, path last and elided: info is MAX_PATH_LEN and a path can
   fill it on its own, the reason must still reach the user. */
static void info_path(char *info, char *reason, char *path)
{
  int room = MAX_PATH_LEN - 8 - (int) strlen(reason);
  if (room < 16)
    room = 16;
  if ((int) strlen(path) > room)
    snprintf(info, MAX_PATH_LEN, "%s: \"%.*s...\"", reason, room, path);
  else
    snprintf(info, MAX_PATH_LEN, "%s: \"%s\"", reason, path);
}

static int test_qemu_kvm_wanted_files(t_topo_kvm *kvm, char *rootfs,
                                      char *info)
{
  int result = 0;
  char qemu_kvm_exe[MAX_PATH_LEN];
  char backing[MAX_PATH_LEN];
  memset(qemu_kvm_exe, 0, MAX_PATH_LEN);
  snprintf(qemu_kvm_exe, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/cloonix-qemu-system", cfg_get_bin_dir());
  if (test_dev_kvm(info))
    result = -1;
  else if (!file_exists(qemu_kvm_exe, R_OK))
    {
    info_path(info, "not found", qemu_kvm_exe);
    result = -1;
    }
  else if (!file_exists(rootfs, R_OK))
    {
    if (file_exists(kvm->rootfs_input, R_OK) ||
        strchr(kvm->rootfs_input, '/'))
      info_path(info, "not found", rootfs);
    else if (!image_missing(kvm->rootfs_input, 1, info))
      info_path(info, "not found", rootfs);
    result = -1;
    }
  else if ((kvm->vm_config_flags & VM_CONFIG_FLAG_PERSISTENT) &&
           (!file_exists(rootfs, W_OK)))
    {
    info_path(info, "--persistent needs a writable rootfs, not writable",
              rootfs);
    result = -1;
    }
  else if ((!(kvm->vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)) &&
           (qcow2_get_backing(rootfs, backing, MAX_PATH_LEN)))
    {
    /* The derived disk is a qcow2 over this file: it has to be a qcow2. */
    info_path(info, "not a qcow2", rootfs);
    result = -1;
    }
  else if (kvm->vm_config_flags & VM_CONFIG_FLAG_I386)
    {
    KERR("ERROR NOT MANAGED I386");
    sprintf(info, "ERROR NOT MANAGED I386");
    result = -1;
    }
  return result;
}

/* Optional kvm disc: path made absolute in bulk, -1 and info if absent. */
static int bulk_disc(char *path, char *info)
{
  int result = 0;
  strncpy(path, utils_get_root_fs(path), MAX_PATH_LEN-1);
  if (!file_exists(path, R_OK))
    {
    info_path(info, "not found", path);
    result = -1;
    }
  return result;
}

static int test_topo_kvm(t_topo_kvm *kvm, int vm_id, char *info, int nb_eth)
{
  int result = 0;
  char rootfs[2*MAX_PATH_LEN];
  if (kvm->cpu == 0)
    kvm->cpu =  1;
  if (kvm->mem == 0)
    kvm->mem =  128;
  if (kvm->vm_config_flags & VM_CONFIG_FLAG_INSTALL_CDROM)
    result |= bulk_disc(kvm->install_cdrom, info);
  if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_CDROM)
    result |= bulk_disc(kvm->added_cdrom, info);
  if (kvm->vm_config_flags & VM_CONFIG_FLAG_ADDED_DISK)
    result |= bulk_disc(kvm->added_disk, info);
  if (result == 0)
    {
    memset(rootfs, 0, 2*MAX_PATH_LEN);
    /* as given when readable, else looked up in bulk */
    if (file_exists(kvm->rootfs_input, R_OK))
      strncpy(rootfs, kvm->rootfs_input, MAX_PATH_LEN-1);
    else
      strncpy(rootfs, utils_get_root_fs(kvm->rootfs_input), MAX_PATH_LEN-1);
    if (kvm->vm_config_flags & VM_CONFIG_FLAG_PERSISTENT)
      strncpy(kvm->rootfs_used, rootfs, MAX_PATH_LEN-1);
    else
      {
      kvm->vm_config_flags |= VM_FLAG_DERIVED_BACKING;
      snprintf(kvm->rootfs_backing, MAX_PATH_LEN, "%s", rootfs);
      snprintf(kvm->rootfs_used,MAX_PATH_LEN,"%s/derived.qcow2",
               utils_get_disks_path_name(vm_id));
      }
    if (!strlen(rootfs))
      {
      result = -1;
      sprintf(info, "BAD rootfs");
      }
    else
      {
      result = test_qemu_kvm_wanted_files(kvm, rootfs, info);
      }
    }
    if (result == 0)
      snprintf(info, MAX_PATH_LEN,
      "Rx Req add kvm machine %s with tot_eth=%d FLAGS:%s",
       kvm->name, kvm->nb_tot_eth, prop_flags_ascii_get(kvm->vm_config_flags));
  if (!result)
    {
    if (nb_eth > MAX_ETH_VM)
      {
      sprintf(info, "Maximum ethernet %d per machine", MAX_ETH_VM);
      result = -1;
      }
    else if ((kvm->nb_tot_nb_vwif < 0) ||
             (kvm->nb_tot_nb_vwif > MAX_VWIF))
      {
      sprintf(info, "wlan=%d refused: a kvm has %d radios at most",
              kvm->nb_tot_nb_vwif, MAX_VWIF);
      result = -1;
      }
    else if ((kvm->nb_tot_nb_vwif) && (kvm->vm_config_flags &
             (VM_CONFIG_FLAG_NO_QEMU_GA | VM_CONFIG_FLAG_WINDOWS |
              VM_CONFIG_FLAG_FREEBSD)))
      {
      sprintf(info, "wlan=%d refused: the radio of a kvm is made by the "
              "cloonix agent of a linux guest", kvm->nb_tot_nb_vwif);
      result = -1;
      }
    }
  return result;
}

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

static int cow_look_clone(void *data)
{
  t_add_vm_cow_look *add_vm = (t_add_vm_cow_look *) data;
  char rootfs[MAX_PATH_LEN];
  char backing[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  memset(rootfs, 0, MAX_PATH_LEN);
  memset(backing, 0, MAX_PATH_LEN);
  memset(line, 0, MAX_PATH_LEN);
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
  if (!qcow2_get_backing(rootfs, backing, MAX_PATH_LEN))
    {
    if (backing[0])
      {
      snprintf(line, MAX_PATH_LEN-1, "backing file: %s\n", backing);
      send_to_daddy(line);
      }
    }
  else
    KERR("ERROR qcow2 header read %s", rootfs);
  return 0;
}

static void setup_mac_addr_table(t_topo_kvm *kvm, int vm_id)
{
  int i, max;
  char mac[6];
  memset(mac, 0, 6);
  if (kvm->nb_tot_eth == 0)
    max = 1;
  else
    max = kvm->nb_tot_eth;
  for (i=0; i < max; i++)
    {
    if (!memcmp(kvm->eth_table[i].mac_addr, mac, 6))
      {
      if (g_inside_cloonix)
        {
        kvm->vm_config_flags |= VM_FLAG_IS_INSIDE_CLOON;
        utils_derive_mac(kvm->eth_table[i].mac_addr, 0x72, kvm->name, i);
        }
      else
        {
        utils_derive_mac(kvm->eth_table[i].mac_addr, 0x2, kvm->name, i);
        }
      kvm->eth_table[i].randmac = 1;
      }
    memset(kvm->eth_table[i].vhost_ifname, 0, MAX_NAME_LEN);
    snprintf(kvm->eth_table[i].vhost_ifname, (MAX_NAME_LEN-1),
             "%s", pthexec_ovs_port(vm_id, i));
    }
}

static void delayed_add_vm(t_timer_zombie *tz)
{
  int vm_id, result = -1;
  int nb_eth = 0;
  char info[MAX_PATH_LEN];
  char natplug_name[2*MAX_NAME_LEN];
  char lan_natplug_name[2*MAX_NAME_LEN];
  char use[MAX_PATH_LEN];
  t_add_vm_cow_look *cow_look;
  int llid = tz->llid, tid = tz->tid;
  t_topo_kvm *kvm = &(tz->kvm);
  t_vm   *vm = cfg_get_vm(kvm->name);
  info[0] = 0;
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
    refuse(llid, tid, "%s refused: the net is stopping", kvm->name);
    }
  else if (cfg_name_is_in_use(0, kvm->name, use))
    {
    recv_coherency_unlock();
    KERR("ERROR %s refused: %s", kvm->name, use);
    send_status_ko(llid, tid, use);
    }
  else if (cfg_is_a_newborn(kvm->name))
    {
    recv_coherency_unlock();
    refuse(llid, tid, "%s refused: a machine of that name is being created",
           kvm->name);
    }
  else if (utils_get_eth_numbers(kvm->nb_tot_eth, kvm->eth_table,
                                 &nb_eth))
    {
    recv_coherency_unlock();
    refuse(llid, tid, "%s refused: bad eth table", kvm->name);
    }
  else
    {
    cfg_add_newborn(kvm->name);
    vm_id = cfg_alloc_obj_id();
    wifi_air_kvm_default(kvm);
    setup_mac_addr_table(kvm, vm_id);
    result = test_topo_kvm(kvm, vm_id, info, nb_eth);
    if (result)
      {
      send_status_ko(llid, tid, info);
      recv_coherency_unlock();
      cfg_del_newborn(kvm->name);
      cfg_free_obj_id(vm_id);
      KERR("ERROR %s refused: %s", kvm->name,
           info);
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

static void timer_zombie(void *data)
{
  char err[MAX_PATH_LEN];
  t_timer_zombie *tz = (t_timer_zombie *) data;
  tz->count++;
  if (tz->count >= 30)
    {
    snprintf(err, sizeof(err), "zombie too long to die: %s", tz->kvm.name);
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

void recv_add_vm(int llid, int tid, t_topo_kvm *kvm)
{
  t_vm   *vm = cfg_get_vm(kvm->name);
  t_timer_zombie *tz;
  char err[MAX_PATH_LEN];
  if (refuse_not_ready(llid, tid))
    return;

  if (vm)
    {
    snprintf(err, sizeof(err), "%s is used by running vm", kvm->name);
    KERR("ERROR %s refused: %s", kvm->name, err);
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

typedef struct t_ovs_tool
{
  char *verb;
  char *allowed;
} t_ovs_tool;

/* One tool: cloonix-ovs-vsctl is the only one the cloonfs ships. */
static t_ovs_tool g_ovs_tool[] = {
  {"ovs-vsctl",
   "list show find get list-br list-ports list-ifaces"},
  {NULL, NULL},
};

static t_ovs_tool *ovs_tool_get(char *verb)
{
  int i;
  for (i = 0; g_ovs_tool[i].verb; i++)
    if (!strcmp(g_ovs_tool[i].verb, verb))
      return (&(g_ovs_tool[i]));
  return NULL;
}

/* The first SUBCOMMAND of args against the allow list, whole word only:
   "list" must not let "list-and-then" through. */
static int ovs_first_word_allowed(t_ovs_tool *tool, char *args)
{
  char first[MAX_NAME_LEN];
  char *ptr;
  int len;
  memset(first, 0, MAX_NAME_LEN);
  for (;;)
    {
    while ((*args == ' ') || (*args == '\t'))
      args += 1;
    if (*args != '-')
      break;
    while ((*args) && (*args != ' ') && (*args != '\t'))
      args += 1;
    if (*args == 0)
      return 0;
    }
  ptr = strchr(args, ' ');
  len = ptr ? (int) (ptr - args) : (int) strlen(args);
  if ((len <= 0) || (len >= MAX_NAME_LEN - 2))
    return 0;
  memcpy(first, args, len);
  ptr = strstr(tool->allowed, first);
  while (ptr)
    {
    if (((ptr == tool->allowed) || (*(ptr-1) == ' ')) &&
        ((ptr[len] == 0) || (ptr[len] == ' ')))
      return 1;
    ptr = strstr(ptr + 1, first);
    }
  return 0;
}

static int ovs_line_store(t_list_commands *list, int qty, char *txt)
{
  if (qty >= MAX_LIST_COMMANDS_QTY)
    return qty;
  strncpy(list[qty].cmd, txt, MAX_LIST_COMMANDS_LEN-1);
  return (qty + 1);
}

t_pid_lst *create_list_pid(int *nb);

void recv_ovs_cmd_req(int llid, int tid, char *tool_name, char *args)
{
  static char line[MAX_LIST_COMMANDS_LEN];
  t_ovs_tool *tool;
  t_list_commands *list;
  int qty = 0, status = 0, rc;
  int alloc_len = MAX_LIST_COMMANDS_QTY * sizeof(t_list_commands);

  list = (t_list_commands *) clownix_malloc(alloc_len, 7);
  memset(list, 0, alloc_len);

  tool = ovs_tool_get(tool_name);
  if (!strcmp(tool_name, "namespaces"))
    {
    /* Read by suid_power: /proc/<pid>/ns of a root process is not readable
       from here. */
    int nb;
    t_pid_lst *pl = create_list_pid(&nb);
    rc = suid_power_ns_cmd(llid, tid, nb, pl);
    clownix_free(pl, __FUNCTION__);
    if (rc == 0)
      {
      clownix_free(list, __FILE__);
      return;
      }
    if (rc == -1)
      qty = ovs_line_store(list, qty,
                           "another read is in flight, try again");
    else
      qty = ovs_line_store(list, qty, "no channel to suid_power");
    status = -1;
    }
  else if (!tool)
    {
    qty = ovs_line_store(list, qty, "unknown tool, one of:");
    qty = ovs_line_store(list, qty, "  ovs-vsctl namespaces");
    status = -1;
    }
  else if (!ovs_first_word_allowed(tool, args))
    {
    qty = ovs_line_store(list, qty,
                         "refused: this passthrough is read only");
    snprintf(line, MAX_LIST_COMMANDS_LEN-1, "  %s takes: %s",
             tool->verb, tool->allowed);
    qty = ovs_line_store(list, qty, line);
    status = -1;
    }
  else
    {
    rc = suid_power_ovs_cmd(llid, tid, tool_name, args);
    if (rc == 0)
      {
      clownix_free(list, __FILE__);
      return;
      }
    if (rc == -1)
      qty = ovs_line_store(list, qty,
                           "another ovs read is in flight, try again");
    else
      qty = ovs_line_store(list, qty, "no channel to suid_power");
    status = -1;
    }
  send_ovs_cmd_resp(llid, tid, tool_name, status, qty, list);
  clownix_free(list, __FILE__);
}

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

int pid_blocks_shutdown(char *name, int pid)
{
  int j, nb_ovs, result = 1;
  t_lst_pid *ovs_pid = NULL;
  if ((!strcmp(name, "doors")) ||
      (!strcmp(name, "suid_power")) ||
      (!strcmp(name, "proxymous")) ||
      (!strcmp(name, "gui_server")) ||
      (!strcmp(name, "cloonix_server")))
    result = 0;
  else
    {
    /* By pid, so a rename of the ovs daemons cannot bring the wait back. */
    nb_ovs = ovs_get_all_pid(&ovs_pid);
    for (j=0; j<nb_ovs; j++)
      {
      if (pid == ovs_pid[j].pid)
        result = 0;
      }
    if (ovs_pid)
      clownix_free(ovs_pid, __FUNCTION__);
    }
  return result;
}

static void pid_measure(int pid, int *cpu_tenth, int *rss_kb)
{
  static long hz = 0;
  static int page_kb = 0;
  char path[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char *ptr;
  FILE *fp;
  unsigned long utime, stime, starttime, uptime_ticks;
  long rss_pages;
  double uptime, alive;
  int i;

  *cpu_tenth = 0;
  *rss_kb = 0;
  if (pid <= 0)
    return;
  if (hz == 0)
    {
    hz = sysconf(_SC_CLK_TCK);
    page_kb = (int) (sysconf(_SC_PAGESIZE) / 1024);
    if ((hz <= 0) || (page_kb <= 0))
      {
      hz = -1;
      return;
      }
    }
  if (hz < 0)
    return;

  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/uptime", 0);
  fp = fopen("/proc/uptime", "r");
  if (fp == NULL)
    return;
  if (fscanf(fp, "%lf", &uptime) != 1)
    uptime = 0;
  fclose(fp);

  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/stat", pid);
  fp = fopen(path, "r");
  if (fp == NULL)
    return;
  if (fgets(line, MAX_PATH_LEN-1, fp) == NULL)
    {
    fclose(fp);
    return;
    }
  fclose(fp);

  /* comm may hold spaces and brackets: count from the last bracket. */
  ptr = strrchr(line, ')');
  if ((ptr == NULL) || (ptr[1] == 0))
    return;
  ptr += 2;
  {
  char *tok, *save;
  unsigned long fields[24];
  memset(fields, 0, sizeof(fields));
  i = 0;
  tok = strtok_r(ptr, " ", &save);
  while (tok && (i < 24))
    {
    fields[i] = strtoul(tok, NULL, 10);
    i++;
    tok = strtok_r(NULL, " ", &save);
    }
  if (i < 22)
    return;
  utime      = fields[11];
  stime      = fields[12];
  starttime  = fields[19];
  rss_pages  = (long) fields[21];
  }

  *rss_kb = (int) (rss_pages * page_kb);
  uptime_ticks = (unsigned long) (uptime * hz);
  if (uptime_ticks <= starttime)
    return;
  alive = ((double) (uptime_ticks - starttime)) / ((double) hz);
  if (alive < 0.01)
    return;
  *cpu_tenth = (int) ((1000.0 * ((double) (utime + stime)) /
                       ((double) hz)) / alive);
}

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
    strncpy(lst[j].name, vm->kvm.name, MAX_NAME_LEN-1);
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

  for (i=0 ; i<nb_ovs; i++)
    {
    strncpy(lst[j].name, ovs_pid[i].name, MAX_NAME_LEN-1);
    lst[j].pid = ovs_pid[i].pid;
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
  strcpy(lst[j].name, "suid_power");
  lst[j].pid = suid_power_pid();
  j++;
  strcpy(lst[j].name, "cloonix_server");
  lst[j].pid = getpid();
  j++;
  if (gui_server_get_pid() > 0)
    {
    strcpy(lst[j].name, "gui_server");
    lst[j].pid = gui_server_get_pid();
    j++;
    }
  *nb = j;
  /* Measured in one loop over the final list, not at each source above. */
  for (i=0; i<j; i++)
    pid_measure(lst[i].pid, &(lst[i].cpu_tenth), &(lst[i].rss_kb));
  return lst;
}

void recv_list_pid_req(int llid, int tid)
{
  int nb;
  t_pid_lst *lst;
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

void recv_images_req(int llid, int tid)
{
  int nb;
  t_image_item *items;
  char runner[MAX_NAME_LEN];
  struct passwd *pw = getpwuid(getuid());
  memset(runner, 0, MAX_NAME_LEN);
  if (pw)
    strncpy(runner, pw->pw_name, MAX_NAME_LEN-1);
  else
    snprintf(runner, MAX_NAME_LEN-1, "uid%d", getuid());
  nb = slowperiodic_images(&items);
  send_images_resp(llid, tid, runner, nb, items);
  clownix_free(items, __FUNCTION__);
}

static void del_all_start(void)
{
  int i, nb;
  t_vm *vm = cfg_get_first_vm(&nb);
  g_inhib_new_clients = 1;
  for (i=0; i<nb; i++)
    {
    poweroff_vm(0, 0, vm);
    vm = vm->next;
    }
  suid_power_delete_cnt_all();
}

static void timer_del_all_end(void *data)
{
  t_timer_del *td = (t_timer_del *) data;;
  if (td->kill_cloon)
    {
    ovs_destroy();
    auto_self_destruction(td->llid, td->tid, td->llid ?
                          "a client asking for stop" :
                          "this server, proxymous lost (the ERROR before)");
    }
  else
    {
    g_inhib_new_clients = 0;
    send_status_ok(td->llid, td->tid, "delall");
    cfg_hysteresis_send_topo_info();
    clownix_free(td, __FUNCTION__);
    }
}

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

void recv_del_all(int llid, int tid)
{
  t_timer_del *td;
  del_all_start();
  layout_del_all();
  td = (t_timer_del *) clownix_malloc(sizeof(t_timer_del), 3);
  memset(td, 0, sizeof(t_timer_del));
  td->llid = llid;
  td->tid = tid;
  clownix_timeout_add(100, timer_del_all, (void *) td, NULL, NULL);
}

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

void recv_topo_small_event_sub(int llid, int tid)
{
  int i, nb;
  t_vm *cur;
  if (refuse_not_ready(llid, tid))
    return;

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
    {
    KERR("WARNING a subscribe request arrived on llid %d whose channel is "
         "already gone", llid);
    send_status_ko(llid, tid, "Abnormal!");
    }
}

void recv_del_name(int llid, int tid, char *name)
{
  int nb_eth;
  t_vm *vm = cfg_get_vm(name);
  if (refuse_not_ready(llid, tid))
    return;

  if (vm)
    {
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
    KERR("ERROR del refused: nothing named %s is a kvm, cnt, tap, nat, "
         "phy, a2b or c2c in this config", name);
    send_status_ko(llid, tid, "NOT FOUND");
    }
}

void recv_sav_vm(int llid, int tid, char *name, char *path)
{
  t_vm   *vm = cfg_get_vm(name);
  char *dir_path = mydirname(path);
  if (refuse_not_ready(llid, tid))
    return;

  if (!vm)
    {
    KERR("WARNING save refused: no machine named %s", name);
    send_status_ko(llid, tid, "error MACHINE NOT FOUND");
    }
  else if (file_exists(path, R_OK))
    {
    KERR("WARNING save of %s refused: %s exists already", name, path);
    send_status_ko(llid, tid, "error FILE ALREADY EXISTS");
    }
  else if (!file_exists(dir_path, W_OK))
    {
    KERR("WARNING save of %s refused: %s is not a writable directory",
         name, dir_path);
    send_status_ko(llid, tid, "error DIRECTORY NOT WRITABLE OR NOT FOUND");
    }
  else
    {
    qmp_request_save_rootfs(name, path, llid, tid);
    }
}

static int nat_ports_are_sane(char *ports)
{
  char tmp[MAX_PATH_LEN];
  char *tok, *sav;
  int nb = 0, host, guest;
  char proto[MAX_NAME_LEN];
  if ((ports == NULL) || (strlen(ports) == 0) || (!strcmp(ports, "none")))
    return 1;
  if (strlen(ports) >= MAX_PATH_LEN - 1)
    return 0;
  memset(tmp, 0, MAX_PATH_LEN);
  strncpy(tmp, ports, MAX_PATH_LEN-1);
  tok = strtok_r(tmp, ",", &sav);
  while (tok)
    {
    memset(proto, 0, MAX_NAME_LEN);
    if (sscanf(tok, "%3[a-z]:%d:%d", proto, &host, &guest) != 3)
      return 0;
    if (strcmp(proto, "tcp") && strcmp(proto, "udp"))
      return 0;
    if ((host < 1) || (host > 65535) || (guest < 1) || (guest > 65535))
      return 0;
    nb += 1;
    if (nb > MAX_NAT_PUBLISHED)
      return 0;
    tok = strtok_r(NULL, ",", &sav);
    }
  return 1;
}

void recv_nat_add(int llid, int tid, char *name, char *ports)
{
  if (refuse_not_ready(llid, tid) || refuse_new_name(llid, tid, name))
    return;
  if (!nat_ports_are_sane(ports))
    {
    refuse(llid, tid, "nat %s refused: bad published ports %s", name, ports);
    }
  else
    ovs_nat_main_add(llid, tid, name, ports);
}

void recv_a2b_add(int llid, int tid, char *name)
{
  if (refuse_not_ready(llid, tid) || refuse_new_name(llid, tid, name))
    return;
  ovs_a2b_add(llid, tid, name);
}

void recv_c2c_add(int llid, int tid, char *name, uint32_t loc_udp_ip,
                  char *dist, uint32_t dist_ip, uint16_t dist_port,
                  char *dist_passwd, uint32_t dist_udp_ip,
                  uint16_t c2c_udp_port_low)
{
  t_cloonix_conf_info *conf = get_cloonix_conf_info();
  if (refuse_not_ready(llid, tid) || refuse_new_name(llid, tid, name))
    return;
  if (conf->c2c_udp_port_low != c2c_udp_port_low)
    KERR("ERROR %hu %hu", conf->c2c_udp_port_low, c2c_udp_port_low);
  ovs_c2c_add(llid, tid, name, loc_udp_ip, dist, dist_ip, dist_port,
              dist_passwd, dist_udp_ip, c2c_udp_port_low);
}

static void pcap_trace_arrival(char *what, int llid, int tid,
                               char *name, int num)
{
  PLOG("%s ARRIVED at the server, llid=%d tid=%d, for %s eth%d",
       what, llid, tid, name, num);
  PLOG("the relay uses %s/%s_%d_pcap_sock",
       utils_get_snf_pcap_dir(), name, num);
  (void) what; (void) llid; (void) tid; (void) name; (void) num;
}

void recv_pcap_open(int llid, int tid, char *name, int num,
                    char *filter)
{
  char err[MAX_PATH_LEN];
  pcap_trace_arrival("pcap_open", llid, tid, name, num);
  if (wifi_air_num(name, num) < 0)
    {
    snprintf(err, sizeof(err), "%s has no%s", name,
             wifi_air_iface(name, num));
    send_status_ko(llid, tid, err);
    }
  else
    pcap_relay_open(llid, tid, name, wifi_air_num(name, num), filter);
}

void recv_pcap_close(int llid, int tid, char *name, int num)
{
  pcap_trace_arrival("pcap_close", llid, tid, name, num);
  pcap_relay_close(llid, tid, name, num);
}

/* Server to client only: arriving here means a client sent it the wrong way. */
void recv_pcap_traffic(int llid, int tid, char *name, int num, int type,
                       uint32_t pkt_nb, uint32_t sec, uint32_t usec,
                       int caplen, int len, uint8_t *data)
{
  (void) llid; (void) tid; (void) type; (void) pkt_nb;
  (void) sec; (void) usec; (void) caplen; (void) len; (void) data;
  KERR("ERROR pcap_traffic %s %d arrived at the server", name, num);
}

typedef struct t_snf_ctx
{
  char name[MAX_NAME_LEN];
  int num;
  int val;
} t_snf_ctx;

/* The record is created at the request: none any more means the object
   went (deleted underneath the wait) or the eth was switched back the other
   way by a later request. Either is answered at once, not after 20 s. */
static int snf_state_test(void *ctx, char *err)
{
  t_snf_ctx *c = (t_snf_ctx *) ctx;
  int state = ovs_snf_state(c->name, c->num);
  t_vm *vm;
  t_cnt *cnt;
  int endp_type = -1;
  if (c->val ? (state == 2) : (state == 0))
    return 1;
  if (c->val && (state == 0))
    {
    vm = cfg_get_vm(c->name);
    cnt = find_cnt(c->name);
    if (vm && (c->num < vm->kvm.nb_tot_eth))
      endp_type = vm->kvm.eth_table[c->num].endp_type;
    else if (cnt && (c->num < cnt->cnt.nb_tot_eth))
      endp_type = cnt->cnt.eth_table[c->num].endp_type;
    if ((vm == NULL) && (cnt == NULL) &&
        (ovs_tap_exists(c->name) == NULL) &&
        (ovs_nat_main_exists(c->name) == NULL) &&
        (ovs_a2b_exists(c->name) == NULL) &&
        (ovs_c2c_exists(c->name) == NULL))
      {
      snprintf(err, MAX_PATH_LEN-1, "%s was deleted while its snf %d was "
               "being started", c->name, c->num);
      return -1;
      }
    if ((endp_type != -1) && (endp_type != endp_type_eths))
      {
      snprintf(err, MAX_PATH_LEN-1, "snf %d of %s was switched off again "
               "before it was up", c->num, c->name);
      return -1;
      }
    }
  ovs_snf_state_explain(c->name, c->num, err);
  return 0;
}

static void snf_state_stalled(void *ctx)
{
  t_snf_ctx *c = (t_snf_ctx *) ctx;
  ovs_snf_state_stalled(c->name, c->num);
}

static void ack_snf(int llid, int tid, char *name, int num, int val)
{
  t_snf_ctx *c = (t_snf_ctx *) malloc(sizeof(t_snf_ctx));
  memset(c, 0, sizeof(t_snf_ctx));
  strncpy(c->name, name, MAX_NAME_LEN-1);
  c->num = num;
  c->val = val;
  ack_pending(llid, tid, 100, 20000, "cnf sniff=", snf_state_test,
              snf_state_stalled, c);
}

/* A sniff=on on an eth not up yet, a guest just added: tried again
   every half second, a ko after a minute. */
#define SNF_WAIT_TRIES 120
typedef struct t_snf_wait
{
  int llid;
  int tid;
  char name[MAX_NAME_LEN];
  int wire_num;
  int val;
  int tries;
} t_snf_wait;

static int snf_eth_up(char *name, int num)
{
  t_vm *vm = cfg_get_vm(name);
  t_cnt *cnt = find_cnt(name);
  if (vm)
    return ((num < 0) || (num >= vm->kvm.nb_tot_eth) ||
            kvm_vhost_up(name, num, 0));
  if (cnt)
    return ((num < 0) || (num >= cnt->cnt.nb_tot_eth) ||
            cnt_vhost_up(name, num, 0));
  return 1;
}

static void snf_wait_timer(void *data)
{
  t_snf_wait *w = (t_snf_wait *) data;
  int num = wifi_air_num(w->name, w->wire_num);
  if (snf_eth_up(w->name, num))
    recv_snf_add(w->llid, w->tid, w->name, w->wire_num, w->val);
  else if (++w->tries >= SNF_WAIT_TRIES)
    refuse(w->llid, w->tid, "snf %s%s refused: the eth is not up after %d s",
           w->name, wifi_air_iface(w->name, num), SNF_WAIT_TRIES / 2);
  else
    {
    clownix_timeout_add(50, snf_wait_timer, (void *) w, NULL, NULL);
    return;
    }
  free(w);
}

void recv_snf_add(int llid, int tid, char *name, int wire_num, int val)
{
  char err[MAX_PATH_LEN];
  t_vm   *vm = cfg_get_vm(name);
  int nb_eth, idx, num = wifi_air_num(name, wire_num);
  t_ovs_tap *tap_exists = ovs_tap_exists(name);
  t_ovs_phy *phy_exists = ovs_phy_exists_vhost(name);
  t_ovs_nat_main *nat_exists = ovs_nat_main_exists(name);
  t_ovs_a2b *a2b_exists = ovs_a2b_exists(name);
  t_ovs_c2c *c2c_exists = ovs_c2c_exists(name);
  t_snf_wait *w;
  if (refuse_not_ready(llid, tid))
    return;

  if ((num >= 0) && (!snf_eth_up(name, num)))
    {
    w = (t_snf_wait *) malloc(sizeof(t_snf_wait));
    if (!w)
      KOUT("ERROR malloc");
    memset(w, 0, sizeof(t_snf_wait));
    w->llid = llid;
    w->tid = tid;
    strncpy(w->name, name, MAX_NAME_LEN-1);
    w->wire_num = wire_num;
    w->val = val;
    clownix_timeout_add(50, snf_wait_timer, (void *) w, NULL, NULL);
    return;
    }
  if (get_inhib_new_clients())
    refuse(llid, tid, "snf %s refused: the net is stopping", name);
  else if (num < 0)
    refuse(llid, tid, "snf %s%s refused: %s has no%s", name,
           wifi_air_iface(name, wire_num), name,
           wifi_air_iface(name, wire_num));
  else if (wifi_air_in_use(name))
    wifi_air_snf(llid, tid, name, val);
  else if (wifi_air_wlan(name, num, &idx))
    refuse(llid, tid, "snf %s wlan%d refused: a wlan is captured with its "
           "air, sniff <air>", name, idx);
  else if (nat_exists)
    {
    if (num != 0)
      refuse(llid, tid, "snf %s %d refused: a nat has one interface",
             name, num);
    else if (ovs_nat_main_dyn_snf(name, val))
      refuse(llid, tid, "snf %s%s refused: the nat did not take it",
             name, wifi_air_iface(name, num));
    else
      {
      ack_snf(llid, tid, name, 0, val);
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (a2b_exists)
    {
    if ((num != 0) && ((num != 1)))
      refuse(llid, tid, "snf %s %d refused: an a2b has sides A and B",
             name, num);
    else if (ovs_a2b_dyn_snf(name, num, val))
      refuse(llid, tid, "snf %s%s refused: the a2b did not take it",
             name, wifi_air_iface(name, num));
    else
      {
      ack_snf(llid, tid, name, num, val);
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (tap_exists)
    {
    if (num != 0)
      refuse(llid, tid, "snf %s %d refused: a tap has one interface",
             name, num);
    else if (ovs_tap_dyn_snf(name, val))
      refuse(llid, tid, "snf %s%s refused: the tap did not take it",
             name, wifi_air_iface(name, num));
    else
      {
      ack_snf(llid, tid, name, 0, val);
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (phy_exists)
    {
    if (ovs_phy_dyn_snf(name, val))
      refuse(llid, tid, "snf %s refused: the phy did not take it", name);
    else
      {
      ack_snf(llid, tid, name, 0, val);
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (c2c_exists)
    {
    if (num != 0)
      refuse(llid, tid, "snf %s %d refused: a c2c has one interface",
             name, num);
    else if (ovs_c2c_dyn_snf(name, val))
      refuse(llid, tid, "snf %s%s refused: the c2c did not take it",
             name, wifi_air_iface(name, num));
    else
      {
      ack_snf(llid, tid, name, 0, val);
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (vm)
    {
    if ((num < 0) || (num >= vm->kvm.nb_tot_eth))
      {
      KERR("WARNING refused: machine %s has no eth%d (it has %d)",
           name, num, vm->kvm.nb_tot_eth);
      snprintf(err, sizeof(err), "eth%d for machine %s does not exist", num, name);
      send_status_ko(llid, tid, err);
      }
    else if (kvm_dyn_snf(name, num, val))
      {
      KERR("ERROR: snf %s on %s %d refused",
           val ? "on" : "off", name, num);
      send_status_ko(llid, tid, "error");
      }
    else
      {
      ack_snf(llid, tid, name, num, val);
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (cnt_name_exists(name, &nb_eth))
    {
    if ((num < 0) || (num >= nb_eth))
      {
      KERR("WARNING refused: machine %s has no eth%d (it has %d)",
           name, num, nb_eth);
      snprintf(err, sizeof(err), "eth%d for machine %s does not exist", num, name);
      send_status_ko(llid, tid, err);
      }
    else if (cnt_dyn_snf(name, num, val))
      {
      KERR("ERROR: snf %s on %s %d refused",
           val ? "on" : "off", name, num);
      send_status_ko(llid, tid, "error");
      }
    else
      {
      ack_snf(llid, tid, name, num, val);
      cfg_hysteresis_send_topo_info();
      }
    }
  else
    {
    KERR("WARNING cnf %s sniff= refused: nothing named so is a kvm or a cnt",
         name);
    snprintf(err, sizeof(err), "nothing named %s is a kvm or a cnt", name);
    send_status_ko(llid, tid, err);
    }
}

void recv_phy_add(int llid, int tid, char *name, int type)
{
  if (refuse_not_ready(llid, tid) || refuse_new_name(llid, tid, name))
    return;
  if (!suid_power_info_phy_exists(name))
    {
    refuse(llid, tid, "phy %s refused: no host interface of that name", name);
    }
  else
    ovs_phy_add(llid, tid, name, type);
}

void recv_tap_add(int llid, int tid, char *name)
{
  char err[MAX_PATH_LEN];
  if (refuse_not_ready(llid, tid) || refuse_new_name(llid, tid, name))
    return;
  /* The tap is an interface of the host: IFNAMSIZ-1 chars. */
  if (strlen(name) > 15)
    {
    snprintf(err, MAX_PATH_LEN-1, "tap %s: a host interface name has 15 "
             "chars at most", name);
    KERR("ERROR %s", err);
    send_status_ko(llid, tid, err);
    return;
    }
  ovs_tap_add(llid, tid, name);
}

void recv_c2c_cnf(int llid, int tid, char *name, char *cmd)
{
  uint8_t mac[6];
  if (refuse_not_ready(llid, tid))
    return;

  if (strncmp("mac_mangle=", cmd, strlen("mac_mangle=")))
    {
    send_status_ko(llid, tid, cmd);
    KERR("ERROR cnf %s %s refused: mac_mangle=<mac> is the one known",
         name, cmd);
    }
  else if (sscanf(cmd, "mac_mangle=%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
                  &(mac[0]), &(mac[1]), &(mac[2]),
                  &(mac[3]), &(mac[4]), &(mac[5])) != 6)
    {
    send_status_ko(llid, tid, cmd);
    KERR("ERROR cnf %s %s: not six hex bytes", name, cmd);
    }
  else if (ovs_c2c_mac_mangle(name, mac))
    {
    send_status_ko(llid, tid, name);
    }
  else
    send_status_ok(llid, tid, "OK");
}

void recv_nat_cnf(int llid, int tid, char *name, char *cmd)
{
  t_vm *vm;
  char *ptr;
  if (refuse_not_ready(llid, tid))
    return;

  if (!ovs_nat_main_exists(name))
    refuse(llid, tid, "cnf %s %s refused: no nat of that name", name, cmd);
  else if (!strncmp("publish=", cmd, strlen("publish=")))
    {
    ptr = &(cmd[strlen("publish=")]);
    if (!nat_ports_are_sane(ptr))
      refuse(llid, tid, "cnf %s %s refused: bad published ports",
             name, cmd);
    else if (ovs_nat_main_publish(llid, tid, name, ptr))
      refuse(llid, tid, "cnf %s %s refused: the nat did not take it",
             name, cmd);
    }
  else if (strncmp("whatip=", cmd, strlen("whatip=")))
    refuse(llid, tid, "cnf %s %s refused: a nat takes publish= or "
           "whatip=", name, cmd);
  else
    {
    ptr = &(cmd[strlen("whatip=")]);
    if (strlen(ptr) <= 1)
      refuse(llid, tid, "cnf %s %s refused: whatip= wants a machine name",
             name, cmd);
    else
      {
      vm = cfg_get_vm(ptr);
      if (vm == NULL)
        refuse(llid, tid, "cnf %s %s refused: no machine %s", name, cmd,
               ptr);
      else if (ovs_nat_main_whatip(llid, tid, name, ptr))
        refuse(llid, tid, "cnf %s %s refused: the nat did not take it",
               name, cmd);
      }
    }
}

void recv_cnt_cnf(int llid, int tid, char *name, char *cmd)
{
  char err[MAX_PATH_LEN];
  if (refuse_not_ready(llid, tid))
    return;
  if (suid_power_limit_cnt(llid, tid, name, cmd, err))
    refuse(llid, tid, "%s", err);
}

void recv_a2b_cnf(int llid, int tid, char *name, char *cmd)
{
  if (refuse_not_ready(llid, tid))
    return;

  if (!ovs_a2b_exists(name))
    {
    refuse(llid, tid, "cnf %s %s refused: no a2b of that name", name, cmd);
    }
  else
    {
    if (ovs_a2b_param_config(llid, tid, name, cmd))
      {
      refuse(llid, tid, "cnf %s %s refused: bad parameter", name, cmd);
      }
    }
}

void recv_lan_cnf(int llid, int tid, char *name, char *cmd)
{
  char word[MAX_NAME_LEN];
  int val = -1;

  if (refuse_not_ready(llid, tid))
    return;

  memset(word, 0, MAX_NAME_LEN);
  /* rstp=on; rstp on from a client of before */
  if ((sscanf(cmd, "rstp=%63s", word) == 1) ||
      (sscanf(cmd, "rstp %63s", word) == 1))
    {
    if (!strcmp(word, "on"))
      val = 1;
    else if (!strcmp(word, "off"))
      val = 0;
    }

  if (!lan_get_with_name(name))
    {
    refuse(llid, tid, "cnf %s %s refused: no lan of that name", name, cmd);
    }
  else if (val == -1)
    {
    refuse(llid, tid, "cnf %s %s refused: a lan takes rstp=on, rstp=off",
           name, cmd);
    }
  else
    msg_lan_rstp(llid, tid, name, val);
}

void recv_c2c_peer_create(int llid, int tid, char *name,
                          int is_ack, char *dist, char *loc)
{
  t_cloonix_conf_info *conf = get_cloonix_conf_info();
  char err[MAX_PATH_LEN];
  t_ovs_c2c *c2c;
  int res;
  if (is_ack == 0)
    {
    if (get_inhib_new_clients())
      {
      KERR("ERROR %s %s %s", name, dist, loc);
      }
    else 
      {
      res = cfg_name_is_in_use(0, name, err);
      if (res == 0)
        ovs_c2c_peer_add(llid, tid, name, dist, loc, conf->c2c_udp_port_low);
      else if (res != 2)  
        KERR("ERROR %s %s %s %s", name, dist, loc, err);
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
      KERR("ERROR %s %s %s", name, dist, loc);
    }
}

void recv_c2c_peer_conf(int llid, int tid, char *name,
                        int status, char *dist, char *loc,
                        uint32_t dist_udp_ip,   uint32_t loc_udp_ip,
                        uint16_t dist_udp_port, uint16_t loc_udp_port)
{
  ovs_c2c_peer_conf(llid, tid, name, status, dist, loc, dist_udp_ip,
                    loc_udp_ip, dist_udp_port, loc_udp_port);
}

void recv_c2c_peer_ping(int llid, int tid, char *name, int status_peer_ping)
{
  ovs_c2c_peer_ping(llid, tid, name, status_peer_ping);
}

/* Every source of "<src>:<dst> <src>:<dst>" must be a directory the server */
/* sees; crun would fail on it later with nothing said.                     */
static int vmount_missing(char *vmount, char *err)
{
  char copy[MAX_SIZE_VMOUNT], src[MAX_PATH_LEN];
  char *tok, *save, *sep;
  struct stat sb;
  if (vmount[0] == 0)
    return 0;
  strncpy(copy, vmount, MAX_SIZE_VMOUNT-1);
  copy[MAX_SIZE_VMOUNT-1] = 0;
  for (tok = strtok_r(copy, " ", &save); tok; tok = strtok_r(NULL, " ", &save))
    {
    sep = strchr(tok, ':');
    if (sep == NULL)
      {
      snprintf(err, MAX_PATH_LEN-1, "vmount %s: <dir>:<guest path> wanted",
               tok);
      return 1;
      }
    memset(src, 0, MAX_PATH_LEN);
    strncpy(src, tok, sep - tok);
    if ((stat(src, &sb)) || (!S_ISDIR(sb.st_mode)))
      {
      snprintf(err, MAX_PATH_LEN-1, "vmount %s: not a directory the server "
               "sees%s", src, get_running_in_crun() ?
               " (a toy sees the host bulk as /root/bulk_host and nothing "
               "else of the host)" : "");
      return 1;
      }
    }
  return 0;
}

void recv_cnt_add(int llid, int tid, t_topo_cnt *cnt)
{
  char *locnet = cfg_get_cloonix_name();
  char err[MAX_PATH_LEN];
  int i, vm_id, max, targz;

  if (refuse_not_ready(llid, tid))
    return;

  if (get_inhib_new_clients())
    {
    KERR("ERROR %s refused: the net is stopping", cnt->name);
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    }
  else if (cfg_name_is_in_use(0, cnt->name, err))
    {
    KERR("ERROR %s refused: %s", cnt->name, err);
    send_status_ko(llid, tid, err);
    }
  else if (strcmp(cnt->brandtype, "brandcnt"))
    {
    snprintf(err, MAX_PATH_LEN-1, "%s %s bad brandtype:%s (only brandcnt)",
             locnet, cnt->name, cnt->brandtype);
    KERR("ERROR %s", err);
    send_status_ko(llid, tid, err);
    }
  else if ((cnt->nb_tot_nb_vwif < 0) || (cnt->nb_tot_nb_vwif > MAX_VWIF))
    refuse(llid, tid, "%s: %d radios asked, maximum %d per container",
           cnt->name, cnt->nb_tot_nb_vwif, MAX_VWIF);
  else if ((cnt->nb_tot_eth < 0) || (cnt->nb_tot_eth > MAX_ETH_VM))
    {
    snprintf(err, MAX_PATH_LEN-1, "%s: %d eth asked, maximum %d per machine",
             cnt->name, cnt->nb_tot_eth, MAX_ETH_VM);
    KERR("ERROR %s", err);
    send_status_ko(llid, tid, err);
    }
  else if (vmount_missing(cnt->vmount, err))
    {
    KERR("ERROR %s: %s", cnt->name, err);
    send_status_ko(llid, tid, err);
    }
  else if ((targz = targz_resolve(llid, tid, cnt, err)) < 0)
    {
    KERR("ERROR %s: %s", cnt->name, err);
    send_status_ko(llid, tid, err);
    }
  else if (targz > 0)
    {
    /* Parked until the tar.gz is extracted; targz.c calls back here. */
    }
  else if (image_missing(cnt->image, 0, err))
    {
    KERR("ERROR %s: %s", cnt->name, err);
    send_status_ko(llid, tid, err);
    }
  else
    {
    vm_id = cfg_alloc_obj_id();
    if (cnt->nb_tot_eth == 0)
      max = 1;
    else
      max = cnt->nb_tot_eth;
    for (i=0; i < max; i++)
      {
      if (cnt->eth_table[i].mac_addr[0] == 0)
        {
        utils_derive_mac(cnt->eth_table[i].mac_addr, 0x2, cnt->name, i);
        cnt->eth_table[i].randmac = 1;
        }
      }
    if (suid_power_create_cnt(llid, tid, vm_id, cnt, err))
      {
      KERR("ERROR %s", err);
      cfg_free_obj_id(vm_id);
      send_status_ko(llid, tid, err);
      }
    /* No answer here: the ok comes from crun.c on crun_start_resp_ok, the ko
       from its resp_ko or from cnt_timer_beat when suid_power never answers. */
    }
}

void recv_sync_wireshark_req(int llid, int tid, char *name, int num, int cmd)
{
  ovs_snf_sync_wireshark_req(llid, tid, name, num, cmd);
}

void recv_init(void)
{
  g_killing_cloonix = 0;
  g_inside_cloonix = inside_cloonix(&g_cloonix_vm_name);
  g_inhib_new_clients = 0;
}

void recv_item_pos(int llid, int tid, char *name, int x, int y)
{
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    }
  else if (!name_is_sane(name))
    {
    KERR("WARNING request refused: bad name %s", name);
    send_status_ko(llid, tid, "Bad name");
    }
  else if (!layout_set_pos(name, x, y))
    send_status_ko(llid, tid, "Layout table full");
  else
    send_status_ok(llid, tid, "ok");
}

void recv_item_poseth(int llid, int tid, char *name, int wire_num, int deg)
{
  char info[MAX_PATH_LEN];
  int num = wifi_air_num(name, wire_num);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    }
  else if (!name_is_sane(name))
    {
    KERR("WARNING request refused: bad name %s", name);
    send_status_ko(llid, tid, "Bad name");
    }
  else if (num < 0)
    {
    snprintf(info, sizeof(info), "%s has no%s", name,
             wifi_air_iface(name, wire_num));
    send_status_ko(llid, tid, info);
    }
  else if (num >= MAX_ETH_VM)
    {
    snprintf(info, sizeof(info), "Eth %d is not between 0 and %d", num, MAX_ETH_VM-1);
    send_status_ko(llid, tid, info);
    }
  else if (!layout_set_eth(name, num, deg))
    send_status_ko(llid, tid, "Layout table full");
  else
    send_status_ok(llid, tid, "ok");
}

