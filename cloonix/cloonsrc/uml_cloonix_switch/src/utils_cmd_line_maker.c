/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "system_callers.h"
#include "event_subscriber.h"
#include "machine_create.h"
#include "utils_cmd_line_maker.h"
#include "doors_rpc.h"
#include "file_read_write.h"
#include "doorways_mngt.h"
#include "suid_power.h"
#include "qmp.h"
#include "cnt.h"
#include "ovs_nat_main.h"
#include "qga_dialog.h"
#include "uml_clownix_switch.h"

static char *glob_ptr_uname_r;
void timer_utils_finish_vm_init(int vm_id, int val);
void dec_creation_counter(t_wake_up_eths *wake_up_eths);
void give_back_creation_counter(void);
static uid_t glob_uid_user;
static uid_t glob_gid_user;

int lio_system(char *argv[])
{
  char *acmd = lio_linear(argv);
  int chld_state, wstatus, result = -1;
  int exited_pid, timeout_pid, worker_pid, pid;
  if ((pid = fork()) < 0)
    KOUT("ERROR");
  if (pid == 0)
    {
    lio_clean_all_llid();
    worker_pid = fork();
    if (worker_pid == 0)
      {
      execv(argv[0], argv);
      KOUT("ERROR execv");
      }
    timeout_pid = fork();
    if (timeout_pid == 0)
      {
      sleep(5);
      KERR("WARNING TIMEOUT SLOW CMD 1 %s", acmd);
      exit(1);
      }
    exited_pid = wait(&chld_state);
    if (exited_pid == worker_pid)
      {
      if (WIFEXITED(chld_state))
        wstatus = WEXITSTATUS(chld_state);
      if (WIFSIGNALED(chld_state))
        KERR("WARNING Child exited via signal %d", WTERMSIG(chld_state));
      kill(timeout_pid, SIGKILL);
      }
    else
      {
      KERR("WARNING %s", acmd);
      kill(worker_pid, SIGKILL);
      }
    wait(NULL);
    exit(wstatus);
    }
  else
    {
    if (waitpid(pid, &wstatus, 0))
      result = 0;
    else if (wstatus)
      KERR("ERROR %s", acmd);
    else
      result = 0;
    }
  return result;
}

void utils_send_status_ko(int *llid, int *tid, char *err)
{
  if (msg_exist_channel(*llid))
    send_status_ko(*llid, *tid, err);
  *llid = 0;
  *tid = 0;
}

void utils_send_status_ok(int *llid, int *tid)
{
  if (msg_exist_channel(*llid))
    send_status_ok(*llid, *tid, "");
  *llid = 0;
  *tid = 0;
}

int utils_get_next_tid(void)
{
  static int tid = 0;
  tid += 1;
  if (tid == 0xFFFF)
    tid = 1;
  return tid;
}

int utils_get_eth_numbers(int nb_tot_eth, t_eth_table *eth_tab, int *eth_nb)
{
  int i, result = 0;
  char info[MAX_PATH_LEN];
  (*eth_nb) = 0;
  for (i=0; i<nb_tot_eth; i++)
    {
    if ((eth_tab[i].endp_type == endp_type_eths) ||
        (eth_tab[i].endp_type == endp_type_ethv))
      (*eth_nb)++;
    else
      {
      KERR("ERROR eth %d of %d has the kind %d, not an eth", i, nb_tot_eth,
             eth_tab[i].endp_type);
      snprintf(info, sizeof(info), "Bad input %d for endp_type", eth_tab[i].endp_type);
      result = -1;
      }
    }
  return result;
}

void utils_get_snf_tap_name(char *tap, char *vhost)
{
  if (strlen(vhost) >= MAX_NAME_LEN - 1)
    KOUT("ERROR vhost name too long for an snf tap: %s", vhost);
  memset(tap, 0, MAX_NAME_LEN);
  tap[0] = 's';
  strncpy(tap + 1, vhost, MAX_NAME_LEN - 2);
}

char *utils_get_snf_pcap_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, SNF_DIR);
  return path;
}

char *utils_get_crun_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, CRUN_DIR);
  return path;
}

char *utils_get_log_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, LOG_DIR);
  return path;
}

char *utils_get_cnt_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, CNT_DIR);
  return path;
}

char *utils_get_mnt_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, MNT_DIR);
  return path;
}

char *utils_get_c2c_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, C2C_DIR);
  return path;
}

char *utils_get_nat_main_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, NAT_MAIN_DIR);
  return path;
}

char *utils_get_nat_proxy_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *net = cfg_get_cloonix_name();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s_%s/%s",PROXYSHARE_IN,net,NAT_PROXY_DIR);
  return path;
}

char *utils_get_a2b_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, A2B_DIR);
  return path;
}

char *utils_get_run_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, RUN_DIR);
  return path;
}

char *utils_get_run_config_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s/.config", root, RUN_DIR);
  return path;
}

char *utils_get_qmp_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s/%s", cfg_get_work_vm(vm_id), QMP_UNIX);
  return path;
}

char *utils_get_qga_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s/%s", cfg_get_work_vm(vm_id), QGA_UNIX);
  return path;
}

char *utils_get_cloonix_switch_path(void)
{
  static char path[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s/%s", cfg_get_root_work(), CLOONIX_SWITCH);
  return path;
}

char *utils_get_qbackdoor_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s/%s", cfg_get_work_vm(vm_id), QBACKDOOR_UNIX);
  return path;
}

char *utils_get_dtach_bin_path(void)
{
  static char dtach[MAX_PATH_LEN];
  snprintf(dtach, sizeof(dtach), "%s/cloonfs/bin/cloonix-dtach", cfg_get_bin_dir());
  return dtach;
}

char *utils_get_dtach_sock_dir(void)
{
  static char dtach_sock[MAX_PATH_LEN];
  snprintf(dtach_sock, sizeof(dtach_sock), "%s/%s", cfg_get_root_work(), DTACH_SOCK);
  return dtach_sock;
}

char *utils_get_ovs_dir(void)
{
  static char ovs[MAX_PATH_LEN];
  snprintf(ovs, sizeof(ovs), "%s", cfg_get_root_work());
  return ovs;
}

char *utils_get_ovs_bin_dir(void)
{
  static char ovs[MAX_PATH_LEN];
  snprintf(ovs, MAX_PATH_LEN-1, "%s/cloonfs/bin", cfg_get_bin_dir());
  return ovs;
}

char *utils_get_ovs_drv_bin_dir(void)
{
  static char ovs_drv[MAX_PATH_LEN];
  snprintf(ovs_drv, MAX_PATH_LEN-1, 
           "%s/cloonfs/bin/cloonix-ovs-drv", cfg_get_bin_dir());
  return ovs_drv;
}

char *utils_get_ovs_snf_bin_dir(void)
{
  static char ovs_snf[MAX_PATH_LEN];
  snprintf(ovs_snf, MAX_PATH_LEN-1, 
           "%s/cloonfs/bin/cloonix-ovs-snf", cfg_get_bin_dir());
  return ovs_snf;
}

char *utils_get_dumpcap_bin_dir(void)
{
  static char dumpcap[MAX_PATH_LEN];
  snprintf(dumpcap, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/jscloonix/cloonix-dumpcap", cfg_get_bin_dir());
  return dumpcap;
}

char *utils_get_ovs_nat_main_bin_dir(void)
{
  static char ovs_nat_main[MAX_PATH_LEN];
  snprintf(ovs_nat_main, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/cloonix-ovs-nat", cfg_get_bin_dir());
  return ovs_nat_main;
}

char *utils_get_proxymous_bin(void)
{
  static char proxymous[MAX_PATH_LEN];
  snprintf(proxymous, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/cloonix-proxymous", cfg_get_bin_dir());
  return proxymous;
}

char *utils_get_ovs_a2b_bin_dir(void)
{
  static char ovs_a2b[MAX_PATH_LEN];
  snprintf(ovs_a2b, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/cloonix-ovs-a2b", cfg_get_bin_dir());
  return ovs_a2b;
}

char *utils_get_ovs_c2c_bin_dir(void)
{
  static char ovs_c2c[MAX_PATH_LEN];
  snprintf(ovs_c2c, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/cloonix-ovs-c2c", cfg_get_bin_dir());
  return ovs_c2c;
}

char *utils_get_ovs_path(char *name)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", utils_get_ovs_dir(), name);
  return path;
}

char *utils_get_dtach_sock_path(char *name)
{
  static char dtach_sock[MAX_PATH_LEN];
  snprintf(dtach_sock, sizeof(dtach_sock), "%s/%s/%s", cfg_get_root_work(), DTACH_SOCK, name);
  return dtach_sock;
}

/* Per-VM qemu stderr logfile. */
char *utils_get_qemu_stderr_log_path(char *name)
{
  static char log_path[MAX_PATH_LEN];
  snprintf(log_path, sizeof(log_path), "%s/%s/%s.stderr", cfg_get_root_work(), DTACH_SOCK, name);
  return log_path;
}

/* Per-vm launch script: the netns and qemu command, stderr redirected. */
char *utils_get_qemu_launch_script_path(char *name)
{
  static char script_path[MAX_PATH_LEN];
  snprintf(script_path, sizeof(script_path), "%s/%s/%s.sh", cfg_get_root_work(), DTACH_SOCK, name);
  return script_path;
}

char *utils_get_vnc_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", cfg_get_work_vm(vm_id), VNC_SOCK);
  return path;
}

char *utils_dir_conf_tmp(int vm_id)
{
  static char dir_conf_tmp[MAX_PATH_LEN];
  snprintf(dir_conf_tmp, sizeof(dir_conf_tmp), "%s/%s", cfg_get_work_vm(vm_id), DIR_CONF);  
  return dir_conf_tmp;
}

char *utils_get_root_fs(char *rootfs)
{
  static char root_fs[MAX_PATH_LEN];
  char host_root_fs[MAX_PATH_LEN];
  memset(root_fs, 0, MAX_PATH_LEN);
  if (rootfs[0] == '/')
    snprintf(root_fs, sizeof(root_fs), "%s", rootfs);
  else
    {
    snprintf(root_fs, sizeof(root_fs), "%s/%s", cfg_get_bulk(), rootfs);
    if (get_running_in_crun())
      {
      snprintf(host_root_fs, sizeof(host_root_fs), "%s/%s", cfg_get_bulk_host(), rootfs);
      if ((!file_exists(root_fs, R_OK)) &&
          (file_exists(host_root_fs, R_OK)))
        {
        memset(root_fs, 0, MAX_PATH_LEN);
        strncpy(root_fs, host_root_fs, MAX_PATH_LEN-1);
        }
      }
    }
  return root_fs;
}

void utils_chk_my_dirs(t_vm *vm)
{
  int vm_id;
  char path[MAX_PATH_LEN];
  if (!vm)
    KOUT(" ");
  vm_id = vm->kvm.vm_id;
  snprintf(path, sizeof(path), "%s", cfg_get_work_vm(vm_id)); 
  if (!file_exists(path, R_OK))
    KOUT(" ");
  if (!file_exists(utils_dir_conf_tmp(vm_id), R_OK))
    KOUT(" ");
  snprintf(path, sizeof(path),"%s/%s", cfg_get_work_vm(vm_id),  DIR_UMID);
  if (!file_exists(path, R_OK))
    KOUT(" ");
}

void free_wake_up_eths(t_vm *vm)
{
  if (vm->wake_up_eths->automate_abs_beat)
    {
    void *nm = clownix_timeout_del(vm->wake_up_eths->automate_abs_beat,
                                   vm->wake_up_eths->automate_ref,
                                   __FILE__, __LINE__);
    if (nm)
      clownix_free(nm, __FUNCTION__);
    }
  vm->wake_up_eths->automate_abs_beat = 0;
  vm->wake_up_eths->automate_ref = 0;

  clownix_timeout_del(vm->wake_up_eths->abs_beat, vm->wake_up_eths->ref,
                      __FILE__, __LINE__);
  vm->wake_up_eths->abs_beat = 0;
  vm->wake_up_eths->ref = 0;
  if (!vm->wake_up_eths->coherency_unlocked)
    {
    vm->wake_up_eths->coherency_unlocked = 1;
    recv_coherency_unlock();
    KTRC("%s: create ended in automaton state %d", vm->kvm.name,
         vm->wake_up_eths->state);
    }
  clownix_free(vm->wake_up_eths, __FUNCTION__);
  vm->wake_up_eths = NULL;
  cfg_reset_vm_locked(vm);
}

/* The one ko of a creation that failed: why given, or the generic line. */
void free_wake_up_eths_and_delete_vm(t_vm *vm, int error_death, char *why)
{
  int llid, tid;
  char err[MAX_PATH_LEN];
  llid = vm->wake_up_eths->llid;
  tid = vm->wake_up_eths->tid;
  free_wake_up_eths(vm);
  if (!cfg_is_a_zombie(vm->kvm.name))
    poweroff_vm(0, 0, vm);
  if (llid)
    {
    if (why == NULL)
      {
      memset(err, 0, MAX_PATH_LEN);
      snprintf(err, MAX_PATH_LEN-1,
               "stopped while being created (step %d), its qemu stderr is "
               "\"%s\"", error_death,
               utils_get_qemu_stderr_log_path(vm->kvm.name));
      why = err;
      }
    KERR("ERROR %s: %s", vm->kvm.name, why);
    send_status_ko(llid, tid, why);
    }
  cfg_hysteresis_send_topo_info();
}

static void free_wake_up_eths_and_vm_ok(t_vm *vm)
{
  int llid, tid;
  if ((!vm) || (!vm->wake_up_eths))
    KOUT(" ");
  llid = vm->wake_up_eths->llid;
  tid = vm->wake_up_eths->tid;
  free_wake_up_eths(vm);
  if (llid)
    send_status_ok(llid, tid, "addvm");
  ovs_nat_main_vm_event();
}

/* The launch script appends "qemu exit N" when qemu ends: the line before it
   is what qemu said. */
static int qemu_ended(char *name, char *why, int len)
{
  char buf[4096], *last, *prev;
  size_t rd;
  FILE *lf = fopen(utils_get_qemu_stderr_log_path(name), "r");
  memset(why, 0, len);
  if (!lf)
    return 0;
  if (fseek(lf, -((long) sizeof(buf) - 1), SEEK_END))
    rewind(lf);
  rd = fread(buf, 1, sizeof(buf) - 1, lf);
  fclose(lf);
  buf[rd] = 0;
  while (rd && ((buf[rd-1] == '\n') || (buf[rd-1] == '\r')))
    buf[--rd] = 0;
  last = strrchr(buf, '\n');
  last = last ? last + 1 : buf;
  if (strncmp(last, "qemu exit ", 10))
    return 0;
  prev = NULL;
  if (last > buf)
    {
    last[-1] = 0;
    prev = strrchr(buf, '\n');
    prev = prev ? prev + 1 : buf;
    }
  snprintf(why, len-1, "qemu ended at its start: %s (%s)",
           (prev && prev[0]) ? prev : "it said nothing", last);
  return 1;
}

void utils_finish_vm_init(void *ul_vm_id)
{
  int vm_id = (unsigned long) ul_vm_id;
  char why[MAX_PATH_LEN];
  t_vm *vm = find_vm_with_id(vm_id);
  if ((vm) && (vm->wake_up_eths))
    {
    if (vm->wake_up_eths->destroy_requested)
      free_wake_up_eths_and_delete_vm(vm, error_death_wakeup, NULL);
    else
      {
      /* A pid is not a running qemu: the ok waits for the monitor to answer. */
      vm->pid = suid_power_get_pid(vm->kvm.vm_id);
      if (((!vm->pid) || (!qmp_greeting_seen(vm->kvm.name))) &&
          (qemu_ended(vm->kvm.name, why, MAX_PATH_LEN)))
        {
        KERR("ERROR %s refused: %s", vm->kvm.name, why);
        free_wake_up_eths_and_delete_vm(vm, error_death_timeout_no_pid, why);
        }
      else if ((!vm->pid) || (!qmp_greeting_seen(vm->kvm.name)))
        {
        vm->wake_up_eths->pid_count += 1;
        if (vm->wake_up_eths->pid_count < 100)
          {
          timer_utils_finish_vm_init(vm->kvm.vm_id, 20);
          }
        else
          {
          KERR("ERROR %s: no pid or no monitor answer in 20 s (pid %d)",
               vm->kvm.name, vm->pid);
          free_wake_up_eths_and_delete_vm(vm, error_death_timeout_no_pid, NULL);
          }
        }
      else
        {
        if (vm->wake_up_eths->pid_count)
          KTRC("%s alive after %d looks", vm->kvm.name,
               vm->wake_up_eths->pid_count);
        free_wake_up_eths_and_vm_ok(vm);
        }
      }
    }
}

char *utils_get_disks_path_name(int vm_id)
{
  static char rootfs[MAX_PATH_LEN];
  snprintf(rootfs, sizeof(rootfs),"%s/%s", cfg_get_work_vm(vm_id), DIR_CLOONIX_DISKS);
  return rootfs;
}

char *utils_get_cdrom_path_name(t_vm *vm)
{
  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_WINDOWS)
    return(pthexec_agent_iso_win());
  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_FREEBSD)
    return(pthexec_agent_iso_bsd());
  return(pthexec_agent_iso_amd64());
}

void utils_format_gene(char *start, char *err, char *name, char **argv)
{
  int argc = 0;
  int len = 0;
  int cut = 0;
  int i, ln;
  ln = sprintf(err, "%s %s ", start, name);
  while (argv[argc])
    {
    len += strlen(argv[argc]) + 2;
    if (len + ln + 10 > MAX_PRINT_LEN)
      {
      cut = 1;
      break;
      }
    argc++;
    }
  for (i=0; i<argc; i++)
    {
    if (strchr(argv[i], ' '))
      ln += sprintf(err+ln, "\"%s\" ", argv[i]);
    else
      ln += sprintf(err+ln, "%s ", argv[i]);
    }
  if (cut)
    ln += sprintf(err+ln, "...");
}

void utils_send_creation_info(char *name, char **argv)
{
  char info[MAX_PRINT_LEN];
  utils_format_gene("CREATION", info, name, argv);
}

int utils_execv(void *ptr)
{
  char **argv = (char **) ptr;
  execv(argv[0], argv);
  KOUT("ERROR execv");
  return -1;
}

static void utils_init_uname_r(void)
{
  static struct utsname buf;
  if (!uname(&buf))
    {
    glob_ptr_uname_r = buf.release;
    }
  else
    glob_ptr_uname_r = NULL;
}

int util_get_max_tempo_fail(void)
{
  int nb, nb_vm, nb_cnt;
  t_vm  *vm  = cfg_get_first_vm(&nb_vm);
  t_cnt *cnt = cnt_get_first_cnt(&nb_cnt);
  (void) vm;
  (void) cnt;
  nb = nb_vm + nb_cnt/3;
  return (nb*250 + 10000); 
}

void utils_init(void)
{
  utils_init_uname_r();
  glob_uid_user = getuid();
  glob_gid_user = getgid();
}

void kil_req_and_check(int llid, int type, char *name)
{
  if (llid == 0)
    KERR("ERROR %s: no rpc link to its helper process, the "
         "kill request cannot be delivered and the process "
         "will be left running", name);
  else
    rpct_send_kil_req(llid, type);
}

/* fnv-1a of one string, carried on from h */
static unsigned int fnv_str(unsigned int h, char *str)
{
  unsigned char *ptr = (unsigned char *) str;
  while (*ptr)
    {
    h = (h ^ (*ptr)) * 16777619U;
    ptr += 1;
    }
  return h;
}

/* a mac of an interface of a guest, from its net, its name and its number:
   the net keeps two nets of one host, whose radios share one hwsim, apart */
void utils_derive_mac(unsigned char *mac, unsigned char lead,
                      char *name, int eth)
{
  unsigned int h = 2166136261U;
  h = fnv_str(h, cfg_get_cloonix_name());
  h = fnv_str(h, "/");
  h = fnv_str(h, name);
  mac[0] = lead;
  mac[1] = (h >> 24) & 0xFF;
  mac[2] = (h >> 16) & 0xFF;
  mac[3] = (h >>  8) & 0xFF;
  mac[4] = h & 0xFF;
  mac[5] = eth & 0xFF;
}
