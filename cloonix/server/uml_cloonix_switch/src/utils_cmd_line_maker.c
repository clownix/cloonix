/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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


char **get_saved_environ(void);
static char *glob_ptr_uname_r;
void dec_creation_counter(t_wake_up_eths *wake_up_eths);
void give_back_creation_counter(void);
static uid_t glob_uid_user;
static uid_t glob_gid_user;

/*****************************************************************************/
char *utils_get_qemu_img(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/server/qemu/%s/%s",
           cfg_get_bin_dir(), QEMU_BIN_DIR, QEMU_IMG);
  return path;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
char *utils_qemu_img_derived(char *backing_file, char *derived_file)
{
  static char cmd[2*MAX_PATH_LEN];
  memset(cmd, 0,  2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1, "%s create -f qcow2 -b %s %s", 
                                   utils_get_qemu_img(), backing_file, 
                                   derived_file);
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
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,"%s/%s", cfg_get_root_work(), SNF_PCAP_DIR);
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
      (type == endp_type_raw)  ||
      (type == endp_type_wif))
    sprintf(path, "%s/server/muswitch/mutap/cloonix_mutap", cfg_get_bin_dir());
  else if (type == endp_type_snf)
    sprintf(path, "%s/server/muswitch/musnf/cloonix_musnf", cfg_get_bin_dir());
  else if (type == endp_type_c2c)
    sprintf(path, "%s/server/muswitch/muc2c/cloonix_muc2c", cfg_get_bin_dir());
  else if (type == endp_type_a2b)
    sprintf(path, "%s/server/muswitch/mua2b/cloonix_mua2b", cfg_get_bin_dir());
  else if (type == endp_type_nat)
    sprintf(path, "%s/server/muswitch/munat/cloonix_munat", cfg_get_bin_dir());
  else if ((type == endp_type_ovs) || (type == endp_type_ovsdb))
    sprintf(path, "%s/server/muswitch/muovs/cloonix_muovs", cfg_get_bin_dir());
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
int utils_get_pid_of_machine(t_vm *vm)
{
  int pid = 0;
  if (!vm)
    KOUT(" ");
  pid = machine_read_umid_pid(vm->kvm.vm_id);
  return pid;
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
  clownix_free(vm->wake_up_eths, __FUNCTION__);
  vm->wake_up_eths = NULL;
  cfg_reset_vm_locked(vm);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_wake_up_eths_and_delete_vm(t_vm *vm, int error_death)
{
  int llid, tid;
  char err[MAX_PATH_LEN];
  event_print("DELETE VM %s", vm->kvm.name);
  llid = vm->wake_up_eths->llid;
  tid = vm->wake_up_eths->tid;
  if (cfg_is_a_zombie(vm->kvm.name))
    cfg_del_zombie(vm->kvm.name);
  free_wake_up_eths(vm);
  machine_death(vm->kvm.name, error_death);
  if (llid)
    {
    memset(err, 0, MAX_PATH_LEN);
    if (error_death == error_death_timeout_hvc0_silent)
      snprintf(err, MAX_PATH_LEN-1, "ERROR, hvc0 of machine silent"); 
    else if (error_death == error_death_timeout_hvc0_conf)
      snprintf(err, MAX_PATH_LEN-1, "ERROR, hvc0 of machine not usable"); 
    else
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
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void utils_launched_vm_death(char *nm, int error_death)
{
  t_vm   *vm = cfg_get_vm(nm);
  char info[MAX_PRINT_LEN];
  sprintf(info, "DEATH OF %s", nm);
  event_print(info);
  if (vm)
    {
    if (!vm->wake_up_eths)
      {
      if (cfg_is_a_zombie(vm->kvm.name))
        cfg_del_zombie(vm->kvm.name);
      machine_death(vm->kvm.name, error_death);
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      }
    else
      {
      free_wake_up_eths_and_delete_vm(vm, error_death);
      }
    }
  else
    KERR(" ");
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void utils_vm_create_fct_abort(void *data)
{
  t_vm *vm = (t_vm *) data;
  if (!vm)
    KOUT(" ");
  if (!vm->wake_up_eths)
    KOUT(" ");
  event_print("ABORT %s %s", __FUNCTION__, vm->kvm.name);
  free_wake_up_eths_and_delete_vm(vm, error_death_abort);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void utils_finish_vm_init(void *vname)
{
  char *name = (char *) vname;
  t_vm *vm = cfg_get_vm(name);
  if ((vm) && (vm->wake_up_eths))
    {
    if (vm->wake_up_eths->destroy_requested)
      free_wake_up_eths_and_delete_vm(vm, error_death_wakeup);
    else
      {
      vm->pid = utils_get_pid_of_machine(vm);
      if (!vm->pid)
        {
        KERR("PID of machine %s not found", name);
        event_print("PID of machine %s not found", name);
        free_wake_up_eths_and_delete_vm(vm, error_death_timeout_no_pid);
        }
      else
        {
        event_print("VM %s has a main pid: %d",vm->wake_up_eths->name,vm->pid);
        free_wake_up_eths_and_vm_ok(vm);
        }
      }
    }
  clownix_free(vname, __FUNCTION__);
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
int spice_libs_exists(void)
{
  int result = 0;
  char lib_path[MAX_PATH_LEN];
  memset(lib_path, 0, MAX_PATH_LEN);
  snprintf(lib_path, MAX_PATH_LEN-1,
           "%s/common/spice/spice_lib/pkgconfig", cfg_get_bin_dir());
  if (file_exists(lib_path, F_OK))
    result = 1;
  else
    KERR("%s", lib_path);
  return result;
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
char *util_get_genisoimage(void)
{
  static char path[MAX_PATH_LEN];
  snprintf(path, MAX_PATH_LEN-1, "/usr/bin/genisoimage");
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


