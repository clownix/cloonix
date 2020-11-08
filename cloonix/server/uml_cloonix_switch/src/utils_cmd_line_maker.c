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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/utsname.h>



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
#include "nat_dpdk_process.h"
#include "edp_mngt.h"


char **get_saved_environ(void);
static char *glob_ptr_uname_r;
void dec_creation_counter(t_wake_up_eths *wake_up_eths);
void give_back_creation_counter(void);
static uid_t glob_uid_user;
static uid_t glob_gid_user;


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
int utils_get_eth_numbers(int nb_tot_eth, t_eth_table *eth_tab,
                          int *sock, int *dpdk, int *wlan)
{
  int i, result = 0;
  char info[MAX_PATH_LEN];
  (*sock) = 0;
  (*dpdk) = 0;
  (*wlan) = 0;
  for (i=0; i<nb_tot_eth; i++)
    {
    if (eth_tab[i].eth_type == eth_type_sock)
      (*sock)++;
    else if (eth_tab[i].eth_type == eth_type_dpdk)
      (*dpdk)++;
    else if (eth_tab[i].eth_type == eth_type_wlan)
      (*wlan)++;
    else
      {
      KERR("%d %d %d", nb_tot_eth, i, eth_tab[i].eth_type);
      sprintf(info, "Bad input %d for eth_type", eth_tab[i].eth_type);
      event_print("%s", info);
      result = -1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_snf_dpdk_bin_path(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,
           "%s/server/dpdk/bin/cloonix_snf_dpdk", cfg_get_bin_dir());
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_a2b_dpdk_bin_path(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,
           "%s/server/dpdk/bin/cloonix_a2b_dpdk", cfg_get_bin_dir());
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_nat_dpdk_bin_path(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,
           "%s/server/dpdk/bin/cloonix_nat_dpdk", cfg_get_bin_dir());
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_d2d_dpdk_bin_path(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,
           "%s/server/dpdk/bin/cloonix_d2d_dpdk", cfg_get_bin_dir());
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_suid_power_bin_path(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,
           "%s/server/suid_power/cloonix_suid_power", cfg_get_bin_dir());
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_qemu_img(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/server/qemu/%s",
           cfg_get_bin_dir(), QEMU_IMG);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_qemu_img_derived(char *backing_file, char *derived_file)
{
  static char cmd[2*MAX_PATH_LEN];
  memset(cmd, 0,  2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1, 
           "%s create -f qcow2 -o backing_file=%s,backing_fmt=qcow2 %s", 
            utils_get_qemu_img(), backing_file, derived_file);
  return cmd;
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
char *utils_get_endp_name(char *name, int num)
{
  static char endp_name[MAX_NAME_LEN];
  snprintf(endp_name, MAX_NAME_LEN-1, "%s_%d", name, num);
  return endp_name;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_endp_sock_dir(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", 
           cfg_get_root_work(), ENDP_SOCK_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_cli_sock_dir(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", cfg_get_root_work(), CLI_SOCK_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_snf_pcap_dir(void)
{
  static char path[MAX_PATH_LEN];
  char *root = cfg_get_root_work();
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", root, SNF_PCAP_DIR);
  return path;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_endp_path(char *name, int num)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", 
           utils_get_endp_sock_dir(), utils_get_endp_name(name, num));
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dpdk_ovs_path(char *name)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", utils_get_dpdk_cloonix_dir(), name);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dpdk_endp_path(char *name, int num)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s_%d", 
                                 utils_get_dpdk_qemu_dir(), name, num);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_qmonitor_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/%s", cfg_get_work_vm(vm_id), QMONITOR_UNIX);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_qmp_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/%s", cfg_get_work_vm(vm_id), QMP_UNIX);
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
char *utils_get_qhvc0_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/%s", cfg_get_work_vm(vm_id), QHVCO_UNIX);
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
char *utils_get_qbackdoor_wlan_path(int vm_id, int wlan)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/%s%d", cfg_get_work_vm(vm_id), QBACKDOOR_WLAN_UNIX, wlan);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_qbackdoor_hvc0_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/%s", cfg_get_work_vm(vm_id), QBACKDOOR_HVCO_UNIX);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_muswitch_bin_path(int is_wlan)
{
  static char path[MAX_PATH_LEN];
  if (is_wlan)
    sprintf(path, "%s/server/muswitch/muwlan/cloonix_muwlan", cfg_get_bin_dir());
  else
    sprintf(path, "%s/server/muswitch/mulan/cloonix_mulan", cfg_get_bin_dir());
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dpdk_ovs_bin_dir(void)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/server/dpdk", cfg_get_bin_dir());
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_endp_bin_path(int type)
{
  static char path[MAX_PATH_LEN];
  if ((type == endp_type_tap)  || 
      (type == endp_type_phy)  ||
      (type == endp_type_wif))
    sprintf(path, "%s/server/muswitch/mutap/cloonix_mutap", cfg_get_bin_dir());
  else if (type == endp_type_snf)
    sprintf(path, "%s/server/muswitch/musnf/cloonix_musnf", cfg_get_bin_dir());
  else if (type == endp_type_c2c)
    sprintf(path, "%s/server/muswitch/muc2c/cloonix_muc2c", cfg_get_bin_dir());
  else if (type == endp_type_nat)
    sprintf(path, "%s/server/muswitch/munat/cloonix_munat", cfg_get_bin_dir());
  else if (type == endp_type_ovsdb)
    sprintf(path, "%s/server/muswitch/muovs/cloonix_muovs", cfg_get_bin_dir());
  else if (type == endp_type_pci)
    KERR("%d", type);
  else
    KOUT("%d", type);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_muswitch_sock_dir(void)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/%s", cfg_get_root_work(), MUSWITCH_SOCK_DIR);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_muswitch_traf_dir(void)
{
  static char path[MAX_PATH_LEN];
  sprintf(path, "%s/%s", cfg_get_root_work(), MUSWITCH_TRAF_DIR);
  return path;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dtach_bin_path(void)
{
  static char dtach[MAX_PATH_LEN];
  sprintf(dtach, "%s/server/dtach/dtach", cfg_get_bin_dir());
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
char *utils_get_dpdk_ovs_db_dir(void)
{
  static char dpdk[MAX_PATH_LEN];
  sprintf(dpdk, "%s/%s", cfg_get_root_work(), DIR_DPDK);
  return dpdk;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dpdk_qemu_dir(void)
{
  static char dpdk[MAX_PATH_LEN];
  sprintf(dpdk, "%s/%s_qemu", cfg_get_root_work(), DIR_DPDK);
  return dpdk;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dpdk_snf_dir(void)
{
  static char dpdk[MAX_PATH_LEN];
  sprintf(dpdk, "%s/%s", cfg_get_root_work(), SNF_DPDK_SOCK_DIR);
  return dpdk;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dpdk_nat_dir(void)
{
  static char dpdk[MAX_PATH_LEN];
  sprintf(dpdk, "%s/%s", cfg_get_root_work(), ENDP_SOCK_DIR);
  return dpdk;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dpdk_a2b_dir(void)
{
  static char dpdk[MAX_PATH_LEN];
  sprintf(dpdk, "%s/%s", cfg_get_root_work(), A2B_DPDK_SOCK_DIR);
  return dpdk;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dpdk_d2d_dir(void)
{
  static char dpdk[MAX_PATH_LEN];
  sprintf(dpdk, "%s/%s", cfg_get_root_work(), D2D_DPDK_SOCK_DIR);
  return dpdk;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_dpdk_cloonix_dir(void)
{
  static char dpdk[MAX_PATH_LEN];
  sprintf(dpdk, "%s/%s_cloonix", cfg_get_root_work(), DIR_DPDK);
  return dpdk;
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
  memset(root_fs, 0, MAX_PATH_LEN);
  if (rootfs[0] == '/')
    sprintf(root_fs, "%s", rootfs);
  else
    sprintf(root_fs, "%s/%s", cfg_get_bulk(), rootfs);
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
  if (!file_exists(path, F_OK))
    KOUT(" ");
  if (!file_exists(utils_dir_conf_tmp(vm_id), F_OK))
    KOUT(" ");
  sprintf(path,"%s/%s", cfg_get_work_vm(vm_id),  DIR_UMID);
  if (!file_exists(path, F_OK))
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
    machine_death(vm->kvm.name, error_death);
  if (llid)
    {
    memset(err, 0, MAX_PATH_LEN);
    snprintf(err, MAX_PATH_LEN-1, 
             "ERROR %d WHILE CREATION try \"sudo cat /var/log/user.log\"",
               error_death); 
    send_status_ko(llid, tid, err);
    }
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void free_wake_up_eths_and_vm_ok(t_vm *vm)
{
  int llid, tid;
  if ((!vm) || (!vm->wake_up_eths))
    KOUT(" ");
  llid = vm->wake_up_eths->llid;
  tid = vm->wake_up_eths->tid;
  free_wake_up_eths(vm);
  if (llid)
    send_status_ok(llid, tid, "addvm");
  if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_CISCO)
    {
    edp_mngt_cisco_nat_create(vm->kvm.name);
    }
  nat_dpdk_vm_event();
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

/****************************************************************************/
char *utils_get_kernel_path_name(char *gkernel)
{
  static char kernel[MAX_PATH_LEN];
  memset(kernel, 0, MAX_PATH_LEN);
  sprintf(kernel, "%s/%s", cfg_get_bulk(), gkernel);
  return kernel;
}
/*---------------------------------------------------------------------------*/

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
char *utils_get_cdrom_path_name(int vm_id)
{
  static char config_iso[MAX_PATH_LEN];
  sprintf(config_iso,"%s/%s", 
          utils_get_disks_path_name(vm_id), CDROM_CONFIG_ISO);
  return config_iso;
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
int utils_execve(void *ptr)
{
  char **argv = (char **) ptr;
  char **environ = get_saved_environ();
  return (execve(argv[0], argv, environ));
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
char *util_get_xorrisofs(void)
{
  static char path[MAX_PATH_LEN];
  snprintf(path, MAX_PATH_LEN-1, "/usr/bin/xorrisofs");
  return path;
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

/****************************************************************************/
char *utils_mulan_get_sock_path(char *lan)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path,MAX_PATH_LEN-1,"%s/%s",utils_get_muswitch_sock_dir(),lan);
  return path;
}
/*--------------------------------------------------------------------------*/


