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
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/prctl.h>

#include "io_clownix.h"
#include "config_json.h"
#include "crun.h"
#include "tar_img.h"
#include "net_phy.h"
#include "crun_utils.h"
#include "utils.h"
#include "vwifi.h"


#define CGROUP_SYS "--cgroup-manager=systemd"
#define CGROUP_DIS "--cgroup-manager=disabled"

char *get_net_name(void);


/****************************************************************************/
int dirs_agent_create_mnt_tmp(char *mountbear, char *mounttmp)
{
  int result = -1;
  char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s", mounttmp);
  if (my_mkdir(path))
    KERR("ERROR %s", path);
  else
    {
    chmod(path, 0777);
    memset(path, 0, MAX_PATH_LEN);
    snprintf(path, MAX_PATH_LEN-1, "%s", mountbear);
    if (my_mkdir(path))
      KERR("ERROR %s", path);
    else
      {
      chmod(path, 0777);
      memset(path, 0, MAX_PATH_LEN);
      snprintf(path, MAX_PATH_LEN-1, "%s/mountbear", mountbear);
      if (my_mkdir(path))
        KERR("ERROR %s", path);
      else
        {
        chmod(path, 0777);
        memset(path, 0, MAX_PATH_LEN);
        snprintf(path, MAX_PATH_LEN-1, "%s/cnf_fs", mountbear);
        if (my_mkdir(path))
          KERR("ERROR %s", path);
        else
          {
          chmod(path, 0777);
          memset(path, 0, MAX_PATH_LEN);
          snprintf(path,MAX_PATH_LEN-1,"%s/cnf_fs/lib",mountbear);
          if (my_mkdir(path))
            KERR("ERROR %s", path);
          else
            {
            chmod(path, 0777);
            result = 0;
            }
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int dirscnt_upper(char *tmpfs)
{
  int result = -1;
  char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s/", tmpfs, UPPER);
  if (my_mkdir(path))
    KERR("ERROR %s", path);
  else
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int dirscnt(char *mountbear, char *mounttmp, char *tmpfs,
                   char *cnt_dir, char *name)
{
  int result = -1;
  char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", tmpfs, WORKDIR);
  if (my_mkdir(path))
    KERR("ERROR %s", path);
  else
    {
    memset(path, 0, MAX_PATH_LEN);
    snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s", cnt_dir, name, ROOTFS);
    if (my_mkdir(path))
      KERR("ERROR %s", path);
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int create_all_dirs(char *image, char *cnt_dir, char *name,
                           char *brandtype, int is_persistent,
                           int is_privileged)

{
  int result = -1;
  char path[MAX_PATH_LEN];
  char cmd[2*MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s/", cnt_dir, name, TMPFS);
  if (my_mkdir(path))
    {
    KERR("WARNING %s", path);
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s %s/%s/%s/",
             pthexec_umount_bin(), cnt_dir, name, TMPFS); 
    if (execute_cmd(cmd, 1, 0))
      KERR("WARNING %s", cmd);
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s %s/%s/%s",
             pthexec_umount_bin(), cnt_dir, name, ROOTFS);
    if (execute_cmd(cmd, 1, 0))
      KERR("WARNING %s", cmd);
    if (my_mkdir(path))
      KERR("ERROR %s", path);
    else
      result = 0;
    }
  else
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *get_insider_agent(void)
{
  struct stat sb;
  char *result = NULL;

  if (lstat(pthexec_cloonfs_lib64(), &sb) == 0)
    {
    if (lstat(pthexec_agent_iso_amd64(), &sb) == 0)
      result = pthexec_agent_iso_amd64();
    else
      KERR("ERROR %s NOT FOUND", pthexec_agent_iso_amd64());
    }
  else
    KERR("ERROR %s NOT FOUND", pthexec_cloonfs_lib64());
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int dirs_agent_copy_starter(char *mountbear, char *agent_dir)
{
  int result = -1;
  char path[MAX_PATH_LEN];
  char cmd[2*MAX_PATH_LEN];
  char line[MAX_NAME_LEN];
  char *etc_path = pthexec_cloonfs_etc();
  char *iso = get_insider_agent();
  if (iso)
    {
    memset(path, 0, MAX_PATH_LEN);
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(path, MAX_PATH_LEN-1, "%s/cnf_fs", mountbear);
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s -osirrox on -indev %s -extract / %s",
             pthexec_osirrox_bin(), iso, path); 
    if (execute_cmd(cmd, 1, 0))
      KERR("ERROR %s", cmd);
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int read_crun_create_pid(char *name, int should_not_exist,
                                int uid_image, int is_privileged)
{
  FILE *fp;
  char *ptr = get_var_crun_log();
  int child_pid, wstatus, count= 0, create_pid = 0;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  int nb_tries = 15;
  char *cgroupman;
  cgroupman = CGROUP_DIS;
  if (should_not_exist)
    nb_tries = 1;
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "%s %s --log=%s --root=/var/lib/cloonix/%s/%s state %s | "
  "%s \"\\\"pid\\\"\" | %s '{ print $2 }'",
  pthexec_crun_bin(), cgroupman, ptr, get_net_name(), CRUN_DIR,
  name, pthexec_grep_bin(), pthexec_awk_bin());
  while(create_pid == 0)
    {
    count += 1;
    if (count > nb_tries)
      {
      if (should_not_exist == 0)
        KERR("ERROR PID %s", name);
      break;
      }
    usleep(100000);
    memset(line, 0, MAX_PATH_LEN);
    fp = cmd_lio_popen(cmd, &child_pid, 0);
    if (fp == NULL)
      {
      KERR("ERROR %s", cmd);
      log_write_req("PREVIOUS CMD KO 2");
      }
    else
      {
      if(fgets(line, MAX_PATH_LEN-1, fp))
        {
        ptr = strchr(line, ',');
        if (ptr)
          {
          *ptr = 0;
          create_pid = atoi(line);
          }
        else
          KERR("ERROR PID %s %s", name, line);
        }
      else if (should_not_exist == 0)
        {
        KERR("ERROR PID %s", name);
        log_write_req("PREVIOUS CMD KO 3");
        }
      pclose(fp);
      if (force_waitpid(child_pid, &wstatus))
        KERR("ERROR");
      }
    }
  return create_pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int read_crun_create_pid_while_delete(char *name, int uid_image,
                                             int is_privileged)
{ 
  FILE *fp;
  char *ptr = get_var_crun_log();
  int child_pid, wstatus, create_pid = 0;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char *cgroupman;
  cgroupman = CGROUP_DIS;
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "%s %s --log=%s --root=/var/lib/cloonix/%s/%s state %s | "
  "%s \"\\\"pid\\\"\" | %s '{ print $2 }'",
  pthexec_crun_bin(), cgroupman, ptr, get_net_name(), CRUN_DIR,
  name, pthexec_grep_bin(), pthexec_awk_bin());
  memset(line, 0, MAX_PATH_LEN);
  fp = cmd_lio_popen(cmd, &child_pid, 0);
  if (fp == NULL)
    {
    KERR("ERROR %s", cmd);
    log_write_req("PREVIOUS CMD KO 2");
    }
  else
    {
    if(fgets(line, MAX_PATH_LEN-1, fp))
      {
      ptr = strchr(line, ',');
      if (ptr)
        {
        *ptr = 0;
        create_pid = atoi(line);
        KERR("ERROR PID EXISTS %s %s", name, line);
        }
      else
        KERR("ERROR LINE PID %s %s", name, line);
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    }
  return create_pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_create_overlay(char *mountbear, char *mounttmp,
                              char *name, char *cnt_dir, char *lower,
                              char *image, int uid_image, char *agent_dir,
                              char *bulk_rootfs, int is_persistent,
                              int is_privileged, char *brandtype)
{
  int result = -1;
  char cmd[2*MAX_PATH_LEN];
  char path[MAX_PATH_LEN];
  if (is_persistent)
    {
    memset(path, 0, MAX_PATH_LEN);
    if (dirs_agent_copy_starter(mountbear, agent_dir))
      KERR("ERROR %s %s", mountbear, mounttmp);
    else
      {
      if (!strcmp(brandtype, "brandzip"))
        result = 0;
      else if (!strcmp(brandtype, "brandcvm"))
        {
        memset(cmd, 0, 2*MAX_PATH_LEN);
        snprintf(cmd, 2*MAX_PATH_LEN-1, "%s -R %d:%d %s",
                 pthexec_chown_bin(), uid_image, uid_image, bulk_rootfs);
        execute_cmd(cmd, 1, 0);
        result = 0;
        }
      else
        KOUT("ERROR %s", brandtype);
      }
    }
  else
    {
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s -t tmpfs -o mode=06777 tmpfs %s/%s/%s",
             pthexec_mount_bin(), cnt_dir, name, TMPFS);
    if (execute_cmd(cmd, 1, 0))
      KERR("ERROR %s", cmd);
    else
      {
      memset(path, 0, MAX_PATH_LEN);
      snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s", cnt_dir, name, TMPFS);
      if (dirscnt_upper(path))
        KERR("ERROR %s %s %s", path, name, agent_dir);
      else if (dirscnt(mountbear, mounttmp, path, cnt_dir, name))
        KERR("ERROR %s %s %s", path, name, agent_dir);
      else if (dirs_agent_copy_starter(mountbear, agent_dir))
        KERR("ERROR %s %s %s", path, name, agent_dir);
      else
        {
        if ((!strcmp(brandtype, "brandcvm")) &&
            (strcmp(image, "self_rootfs")))
          {
          memset(cmd, 0, 2*MAX_PATH_LEN);
          snprintf(cmd, 2*MAX_PATH_LEN-1, "%s -R %d:%d %s",
                   pthexec_chown_bin(), uid_image, uid_image, lower);
          execute_cmd(cmd, 1, 0);
          }
        memset(cmd, 0, 2*MAX_PATH_LEN);
        snprintf(cmd, 2*MAX_PATH_LEN-1,
          "%s -t overlay overlay "
          "-o index=off,xino=off,redirect_dir=nofollow,metacopy=off,uuid=off "
          "-o lowerdir=%s,upperdir=%s/%s/%s/%s,workdir=%s/%s/%s/%s %s/%s/%s",
          pthexec_mount_bin(), lower, cnt_dir, name, TMPFS, UPPER,
          cnt_dir, name, TMPFS, WORKDIR, cnt_dir, name, ROOTFS);
        if (execute_cmd(cmd, 1, 0))
          KERR("ERROR %s", cmd);
        else
          result = 0;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_get_vswitchd_pid(char *name)
{
  FILE *fp;
  int child_pid, wstatus, pid, result = 0;
  char *ident1 = pthexec_cloonfs_ovs_vswitchd();
  char ident2[MAX_PATH_LEN];
  char cmd[2*MAX_PATH_LEN];
  char line[2*MAX_PATH_LEN];
  memset(ident2, 0, MAX_PATH_LEN);
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(ident2, MAX_PATH_LEN-1,
  "unix:/var/lib/cloonix/%s/ovsdb_server.sock", name);
  snprintf(cmd, 2*MAX_PATH_LEN-1,
           "%s axo pid,args | grep %s | grep %s | grep -v grep",
           pthexec_ps_bin(), ident1, ident2);
  fp = cmd_lio_popen(cmd, &child_pid, 0);
  if (fp == NULL)
    {
    KERR("ERROR %s", cmd);
    log_write_req("PREVIOUS CMD KO 2");
    }
  else
    {
    if(fgets(line, 2*MAX_PATH_LEN-1, fp))
      {
      if (sscanf(line, "%d", &pid) == 1)
        result = pid;
      }
    pclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR %s", cmd);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_create_crun_create(char *cnt_dir, char *name, int ovspid,
                                  int uid_image, int is_privileged)
{
  int pid = 0, len = 0;
  char cmd[4*MAX_PATH_LEN];
  char *cgroupman;
  if (is_privileged)
    cgroupman = CGROUP_DIS;
  else
    cgroupman = CGROUP_SYS;
  memset(cmd, 0, 4*MAX_PATH_LEN);
  len += sprintf(cmd+len, "%s %s", pthexec_crun_bin(), cgroupman); 
  len += sprintf(cmd+len, " --debug"); 
  len += sprintf(cmd+len, " --log=%s", get_var_crun_log()); 
  len += sprintf(cmd+len, " --root=/var/lib/cloonix/%s/%s",
                 get_net_name(), CRUN_DIR); 
  len += sprintf(cmd+len, " create"); 
  len += sprintf(cmd+len, " --config=%s/%s/config.json", cnt_dir, name);
  len += sprintf(cmd+len, " %s", name); 
  if (execute_cmd(cmd, 1, 0))
    KERR("ERROR %s", cmd);
  pid = read_crun_create_pid(name, 0, uid_image, is_privileged);
  if (pid == 0)
    KERR("ERROR DID NOT GET CREATE PID FOR %s", name);
  return pid;
}
/*--------------------------------------------------------------------------*/
   
/****************************************************************************/
static int crun_utils_create_veth(int vm_id, char *nspacecrun,
                                  int nb_eth, t_eth_mac *eth_mac)
{
  char *mac;
  char cmd[MAX_PATH_LEN];
  int i, result = 0;
  for (i=0; i<nb_eth; i++)
    {
    mac = eth_mac[i].mac;
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s link add name %s%d_%d type veth peer name itp%d%d "
    "address %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
    pthexec_ip_bin(), OVS_BRIDGE_PORT, vm_id, i, vm_id, i,
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (execute_cmd(cmd, 1, 0))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      break;
      }
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s link set itp%d%d netns %s", pthexec_ip_bin(), vm_id, i, nspacecrun);
    if (execute_cmd(cmd, 1, 0))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      break;
      }
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s link set %s%d_%d netns %s_%s",
    pthexec_ip_bin(), OVS_BRIDGE_PORT, vm_id, i,
    BASE_NAMESPACE, get_net_name());
    if (execute_cmd(cmd, 1, 0))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      break;
      }
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s netns exec %s %s link set itp%d%d name eth%d",
    pthexec_ip_bin(), nspacecrun, pthexec_ip_bin(), vm_id, i, i);
    if (execute_cmd(cmd, 1, 0))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      break;
      }
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s netns exec %s %s link set eth%d up",
    pthexec_ip_bin(), nspacecrun, pthexec_ip_bin(), i);
    if (execute_cmd(cmd, 1, 0))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      break;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void crun_utils_prepare_with_delete(char *name, char *nspacecrun,
                                    int vm_id, int nb_eth,
                                    int uid_image, int is_privileged)
{
  char cmd[MAX_PATH_LEN];
  int i, pid = read_crun_create_pid(name, 1, uid_image, is_privileged);
  char *ptr = get_var_crun_log();
  char *cgroupman;
  cgroupman = CGROUP_DIS;
  if (pid)
    {
    KERR("ERROR %s", name);
    kill(pid, 9);
    }
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
           "%s %s --log=%s --root=/var/lib/cloonix/%s/%s delete %s",
           pthexec_crun_bin(), cgroupman, ptr, get_net_name(), CRUN_DIR, name);
  execute_cmd(cmd, 0, 0);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns del %s >/dev/null",
           pthexec_ip_bin(), nspacecrun);
  execute_cmd(cmd, 0, 0);
  for (i=0; i<nb_eth; i++)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s link del name %s%d_%d",
             pthexec_ip_bin(), OVS_BRIDGE_PORT, vm_id, i);
    execute_cmd(cmd, 0, 0);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_user_nspacecrun_init(char *nspacecrun, int crun_pid)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns attach %s %d",
           pthexec_ip_bin(), nspacecrun, crun_pid);
  if (execute_cmd(cmd, 1, 0))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_user_eth_create(char *name, char *nspacecrun,
                               int cloonix_rank, int vm_id,
                               int nb_eth, t_eth_mac *eth_mac)
{
  char cmd[MAX_PATH_LEN];
  int nb, res, result;
  char *ptr = get_var_crun_log();

  result = crun_utils_create_veth(vm_id, nspacecrun, nb_eth, eth_mac);
  if (result == 0)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1,
             "%s netns exec %s %s link set lo up",
             pthexec_ip_bin(), nspacecrun, pthexec_ip_bin());
    if (execute_cmd(cmd, 1, 0))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_create_net(char *brandtype, int is_persistent,
                          int is_privileged, char *mountbear,
                          char *image, char *name, char *cnt_dir,
                          char *nspacecrun, int cloonix_rank,
                          int vm_id, int nb_eth, t_eth_mac *eth_mac,
                          char *agent_dir)
{
  int result;
  result = create_all_dirs(image, cnt_dir, name, brandtype,
                           is_persistent, is_privileged);
  if (result)
    KERR("ERROR %s %s %s", cnt_dir, name, nspacecrun);
  else if (dirs_agent_copy_starter(mountbear, agent_dir))
    {
    KERR("ERROR %s %s %s", cnt_dir, name, nspacecrun);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_delete_overlay1(char *name, char *cnt_dir)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s %s/%s/%s",
           pthexec_umount_bin(), cnt_dir, name, ROOTFS);
  if (execute_cmd(cmd, 1, 0))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  memset(cmd, 0, MAX_PATH_LEN); 
  snprintf(cmd, MAX_PATH_LEN-1, "%s/%s/%s", cnt_dir, name, ROOTFS);
  if (rmdir(cmd))
    {
    KERR("ERROR Dir: %s could not be deleted\n", cmd);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_delete_overlay2(char *name, char *cnt_dir)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s %s/%s/%s",
           pthexec_umount_bin(), cnt_dir, name, TMPFS);
  if (execute_cmd(cmd, 1, 0))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s/%s/%s", cnt_dir, name, TMPFS);
  if (rmdir(cmd))
    {
    KERR("ERROR Dir: %s could not be deleted\n", cmd);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void crun_utils_delete_crun_stop(char *name, int pid, int uid_image,
                                 int is_privileged, char *mountbear)
{
  char cmd[MAX_PATH_LEN];
  char *ptr = get_var_crun_log();
  char *cgroupman;
  cgroupman = CGROUP_DIS;
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
           "%s %s --log=%s --root=/var/lib/cloonix/%s/%s kill %s",
           pthexec_crun_bin(), cgroupman, ptr, get_net_name(), CRUN_DIR, name);
  if (execute_cmd(cmd, 1, 0))
    KERR("ERROR %s", cmd);
  if (pid)
    kill(pid, 9);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s %s/tmp", pthexec_umount_bin(), mountbear);
  execute_cmd(cmd, 0, 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void crun_utils_delete_crun_delete(char *name, char *nspacecrun,
                                   int vm_id, int nb_eth,
                                   int uid_image, int is_privileged)
{
  int i, pid;
  char cmd[MAX_PATH_LEN];
  char *ptr = get_var_crun_log();
  char *cgroupman;
  cgroupman = CGROUP_DIS;
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
           "%s %s --log=%s --root=/var/lib/cloonix/%s/%s delete %s",
           pthexec_crun_bin(), cgroupman, ptr, get_net_name(), CRUN_DIR, name);
  if (execute_cmd(cmd, 1, 0))
    KERR("ERROR %s", cmd);
  pid = read_crun_create_pid_while_delete(name, uid_image, is_privileged);
  if (pid > 0)
    {
    KERR("ERROR PID EXISTS %s %d", name, pid);
    kill(pid, 9);
    }
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns del %s >/dev/null",
           pthexec_ip_bin(), nspacecrun);
  execute_cmd(cmd, 1, 0);
  for (i=0; i<nb_eth; i++)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s link del name %s%d_%d",
             pthexec_ip_bin(), OVS_BRIDGE_PORT, vm_id, i);
    execute_cmd(cmd, 1, 0);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_create_crun_start(char *name, int ovspid, int uid_image,
                                 int is_privileged)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  char crun_dir[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char *ptr = get_var_crun_log();
  char *cgroupman;
  cgroupman = CGROUP_DIS;
  memset(cmd, 0, MAX_PATH_LEN);
  memset(crun_dir, 0, MAX_PATH_LEN);
  memset(line, 0, MAX_PATH_LEN);
  snprintf(crun_dir, MAX_PATH_LEN-1,
  "/var/lib/cloonix/%s/%s", get_net_name(), CRUN_DIR);
  snprintf(cmd, MAX_PATH_LEN-1,
  "%s -t %d --net -- %s %s --debug --log=%s --root=%s start %s",
  pthexec_nsenter_bin(), ovspid, pthexec_crun_bin(),
  cgroupman, ptr, crun_dir, name);
  if (execute_cmd(cmd, 1, 0))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

