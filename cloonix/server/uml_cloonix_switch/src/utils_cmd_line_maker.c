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
#include "cnt.h"
#include "ovs_nat_main.h"
#include "qga_dialog.h"
#include "uml_clownix_switch.h"


static char *glob_ptr_uname_r;
void dec_creation_counter(t_wake_up_eths *wake_up_eths);
void give_back_creation_counter(void);
static uid_t glob_uid_user;
static uid_t glob_gid_user;

/****************************************************************************/
void lio_clean_all_llid(void)
{
  int llid, nb_chan, i;
  clownix_timeout_del_all();
  nb_chan = get_clownix_channels_nb();
  for (i=0; i<=nb_chan; i++)
    {
    llid = channel_get_llid_with_cidx(i);
    if (llid)
      {
      if (msg_exist_channel(llid))
        msg_delete_channel(llid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *lio_linear(char *argv[])
{
  int i;
  static char result[3*MAX_PATH_LEN];
  memset(result, 0, 3*MAX_PATH_LEN);
  for (i=0;  (argv[i] != NULL); i++)
    {
    strcat(result, argv[i]);
    if (strlen(result) >= 2*MAX_PATH_LEN)
      {
      KERR("NOT POSSIBLE");
      break;
      }
    strcat(result, " ");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
        KERR("WARNING Child exited via signal %d\n", WTERMSIG(chld_state));
      kill(timeout_pid, SIGKILL);
      }
    else
      {
      KERR("WARNING %s\n", acmd);
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
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void utils_send_status_ko(int *llid, int *tid, char *err)
{
  if (msg_exist_channel(*llid))
    send_status_ko(*llid, *tid, err);
  *llid = 0;
  *tid = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void utils_send_status_ok(int *llid, int *tid)
{
  if (msg_exist_channel(*llid))
    send_status_ok(*llid, *tid, "");
  *llid = 0;
  *tid = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int utils_get_next_tid(void)
{
  static int tid = 0;
  tid += 1;
  if (tid == 0xFFFF)
    tid = 1;
  return tid;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
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
      KERR("%d %d %d", nb_tot_eth, i, eth_tab[i].endp_type);
      sprintf(info, "Bad input %d for endp_type", eth_tab[i].endp_type);
      event_print("%s", info);
      result = -1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_suid_power_bin_path(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/cloonix-suid-power", cfg_get_bin_dir());
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int utils_get_uid_user(void)
{
  int result = (int) glob_uid_user;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int utils_get_gid_user(void)
{
  int result = (int) glob_gid_user;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_snf_pcap_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, SNF_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_crun_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, CRUN_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_nginx_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, NGINX_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_nginx_conf_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s/conf", root, NGINX_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_nginx_logs_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s/logs", root, NGINX_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_nginx_client_body_temp_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s/client_body_temp", root, NGINX_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
char *utils_get_log_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, LOG_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_cnt_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, CNT_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_mnt_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, MNT_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_c2c_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, C2C_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_nat_main_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, NAT_MAIN_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_nat_proxy_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *net = cfg_get_cloonix_name();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s_%s/%s",PROXYSHARE_IN,net,NAT_PROXY_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_a2b_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, A2B_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_run_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, RUN_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_run_config_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s/.config", root, RUN_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_qmp_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/%s", cfg_get_work_vm(vm_id), QMP_UNIX);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_qga_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/%s", cfg_get_work_vm(vm_id), QGA_UNIX);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_cloonix_switch_path(void)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/%s", cfg_get_root_work(), CLOONIX_SWITCH);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_qbackdoor_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/%s", cfg_get_work_vm(vm_id), QBACKDOOR_UNIX);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dtach_bin_path(void)
{
  static char dtach[MAX_PATH_LEN];
  sprintf(dtach, "%s/cloonfs/bin/cloonix-dtach", cfg_get_bin_dir());
  return dtach;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dtach_sock_dir(void)
{
  static char dtach_sock[MAX_PATH_LEN];
  sprintf(dtach_sock, "%s/%s", cfg_get_root_work(), DTACH_SOCK);
  return dtach_sock;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_ovs_dir(void)
{
  static char ovs[MAX_PATH_LEN];
  sprintf(ovs, "%s", cfg_get_root_work());
  return ovs;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_ovs_bin_dir(void)
{
  static char ovs[MAX_PATH_LEN];
  snprintf(ovs, MAX_PATH_LEN-1, "%s/cloonfs/bin", cfg_get_bin_dir());
  return ovs;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_ovs_drv_bin_dir(void)
{
  static char ovs_drv[MAX_PATH_LEN];
  snprintf(ovs_drv, MAX_PATH_LEN-1, 
           "%s/cloonfs/bin/cloonix-ovs-drv", cfg_get_bin_dir());
  return ovs_drv;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_ovs_snf_bin_dir(void)
{
  static char ovs_snf[MAX_PATH_LEN];
  snprintf(ovs_snf, MAX_PATH_LEN-1, 
           "%s/cloonfs/bin/cloonix-ovs-snf", cfg_get_bin_dir());
  return ovs_snf;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_ovs_nat_main_bin_dir(void)
{
  static char ovs_nat_main[MAX_PATH_LEN];
  snprintf(ovs_nat_main, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/cloonix-ovs-nat-main", cfg_get_bin_dir());
  return ovs_nat_main;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_proxymous_bin(void)
{
  static char proxymous[MAX_PATH_LEN];
  snprintf(proxymous, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/cloonix-proxymous", cfg_get_bin_dir());
  return proxymous;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_ovs_a2b_bin_dir(void)
{
  static char ovs_a2b[MAX_PATH_LEN];
  snprintf(ovs_a2b, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/cloonix-ovs-a2b", cfg_get_bin_dir());
  return ovs_a2b;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_ovs_c2c_bin_dir(void)
{
  static char ovs_c2c[MAX_PATH_LEN];
  snprintf(ovs_c2c, MAX_PATH_LEN-1,
           "%s/cloonfs/bin/cloonix-ovs-c2c", cfg_get_bin_dir());
  return ovs_c2c;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_ovs_path(char *name)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", utils_get_ovs_dir(), name);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dtach_sock_path(char *name)
{
  static char dtach_sock[MAX_PATH_LEN];
  sprintf(dtach_sock, "%s/%s/%s", cfg_get_root_work(), DTACH_SOCK, name);
  return dtach_sock;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_spice_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", cfg_get_work_vm(vm_id), SPICE_SOCK);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_dir_conf(int vm_id)
{
  static char dir_conf[MAX_PATH_LEN];
  sprintf(dir_conf, "%s/%s", cfg_get_work_vm(vm_id), DIR_CONF);
  return dir_conf;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_dir_conf_tmp(int vm_id)
{
  static char dir_conf_tmp[MAX_PATH_LEN];
  sprintf(dir_conf_tmp, "%s/%s", cfg_get_work_vm(vm_id), DIR_CONF);  
  return dir_conf_tmp;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_root_fs(char *rootfs)
{
  static char root_fs[MAX_PATH_LEN];
  char host_root_fs[MAX_PATH_LEN];
  memset(root_fs, 0, MAX_PATH_LEN);
  if (rootfs[0] == '/')
    sprintf(root_fs, "%s", rootfs);
  else
    {
    sprintf(root_fs, "%s/%s", cfg_get_bulk(), rootfs);
    if (get_running_in_crun())
      {
      sprintf(host_root_fs, "%s/%s", cfg_get_bulk_host(), rootfs);
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void utils_chk_my_dirs(t_vm *vm)
{
  int vm_id;
  char path[MAX_PATH_LEN];
  if (!vm)
    KOUT(" ");
  vm_id = vm->kvm.vm_id;
  sprintf(path, "%s", cfg_get_work_vm(vm_id)); 
  if (!file_exists(path, R_OK))
    KOUT(" ");
  if (!file_exists(utils_dir_conf_tmp(vm_id), R_OK))
    KOUT(" ");
  sprintf(path,"%s/%s", cfg_get_work_vm(vm_id),  DIR_UMID);
  if (!file_exists(path, R_OK))
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void free_wake_up_eths(t_vm *vm)
{
  clownix_timeout_del(vm->wake_up_eths->automate_abs_beat,
                      vm->wake_up_eths->automate_ref, 
                      __FILE__, __LINE__);
  vm->wake_up_eths->automate_abs_beat = 0;
  vm->wake_up_eths->automate_ref = 0;

  clownix_timeout_del(vm->wake_up_eths->abs_beat, vm->wake_up_eths->ref,
                      __FILE__, __LINE__);
  vm->wake_up_eths->abs_beat = 0;
  vm->wake_up_eths->ref = 0;
  if (vm->wake_up_eths->state != auto_create_vm_connect)
    KERR("WRONG AUTO STATE: %d", vm->wake_up_eths->state);
  clownix_free(vm->wake_up_eths, __FUNCTION__);
  vm->wake_up_eths = NULL;
  cfg_reset_vm_locked(vm);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void free_wake_up_eths_and_delete_vm(t_vm *vm, int error_death)
{
  int llid, tid;
  char err[MAX_PATH_LEN];
  event_print("DELETE VM %s", vm->kvm.name);
  llid = vm->wake_up_eths->llid;
  tid = vm->wake_up_eths->tid;
  free_wake_up_eths(vm);
  if (!cfg_is_a_zombie(vm->kvm.name))
    poweroff_vm(0, 0, vm);
  if (llid)
    {
    memset(err, 0, MAX_PATH_LEN);
    snprintf(err, MAX_PATH_LEN-1, 
             "ERROR %d WHILE CREATION try \"sudo cat /var/log/user.log\"",
               error_death); 
    send_status_ko(llid, tid, err);
    }
  cfg_hysteresis_send_topo_info();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_cisco_add(void *data)
{
  char *name = (char *) data;
  t_vm *vm = cfg_get_vm(name);
  if (!vm)
    KERR("ERROR %s", name);
  else
    ovs_nat_main_cisco_add(vm->kvm.name);
  free(data);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void free_wake_up_eths_and_vm_ok(t_vm *vm)
{
  int llid, tid;
  char *nm;
  if ((!vm) || (!vm->wake_up_eths))
    KOUT(" ");
  llid = vm->wake_up_eths->llid;
  tid = vm->wake_up_eths->tid;
  free_wake_up_eths(vm);
  if (llid)
    send_status_ok(llid, tid, "addvm");
  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_NATPLUG)
    {
    nm = (char *) malloc(MAX_NAME_LEN);
    memset(nm, 0, MAX_NAME_LEN);
    strncpy(nm, vm->kvm.name, MAX_NAME_LEN);
    clownix_timeout_add(500, timer_cisco_add, (void *) nm, NULL, NULL);
    }
  ovs_nat_main_vm_event();
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void utils_finish_vm_init(void *ul_vm_id)
{
  int vm_id = (unsigned long) ul_vm_id;
  t_vm *vm = find_vm_with_id(vm_id);
  if ((vm) && (vm->wake_up_eths))
    {
    if (vm->wake_up_eths->destroy_requested)
      free_wake_up_eths_and_delete_vm(vm, error_death_wakeup);
    else
      {
      vm->pid = suid_power_get_pid(vm->kvm.vm_id);
      if (!vm->pid)
        {
        KERR("PID of machine %s not found", vm->kvm.name);
        event_print("PID of machine %s not found", vm->kvm.name);
        free_wake_up_eths_and_delete_vm(vm, error_death_timeout_no_pid);
        }
      else
        {
        event_print("VM %s has a main pid: %d", vm->kvm.name, vm->pid);
        free_wake_up_eths_and_vm_ok(vm);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_cow_path_name(int vm_id)
{
  static char cow[MAX_PATH_LEN];
  sprintf(cow,"%s/%s", utils_get_disks_path_name(vm_id), FILE_COW);
  return cow;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_disks_path_name(int vm_id)
{
  static char rootfs[MAX_PATH_LEN];
  sprintf(rootfs,"%s/%s", cfg_get_work_vm(vm_id), DIR_CLOONIX_DISKS);
  return rootfs;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_cdrom_path_name(t_vm *vm)
{
  return(AGENT_ISO_AMD64);
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void utils_send_creation_info(char *name, char **argv)
{
  char info[MAX_PRINT_LEN];
  utils_format_gene("CREATION", info, name, argv);
  event_print("%s", info);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int utils_execv(void *ptr)
{
  char **argv = (char **) ptr;
  execv(argv[0], argv);
  KOUT("ERROR execv");
  return -1;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_uname_r_mod_path(void)
{
  char *result = NULL;
  static char mod_path[MAX_PATH_LEN];
  if (glob_ptr_uname_r)
    {
    sprintf(mod_path, "/lib/modules/%s", glob_ptr_uname_r);
    result = mod_path;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void utils_init(void)
{
  utils_init_uname_r();
  glob_uid_user = getuid();
  glob_gid_user = getgid();
}
/*---------------------------------------------------------------------------*/


