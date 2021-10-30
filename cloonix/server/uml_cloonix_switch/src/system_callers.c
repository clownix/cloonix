/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <termios.h>
#include <pty.h>
#include <sys/mman.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "system_callers.h"
#include "event_subscriber.h"
#include "utils_cmd_line_maker.h"
#include "file_read_write.h"



/****************************************************************************/
void my_cp_file(char *dsrc, char *ddst, char *name)
{
  char err[MAX_PRINT_LEN];
  char src_file[MAX_PATH_LEN+MAX_NAME_LEN];
  char dst_file[MAX_PATH_LEN+MAX_NAME_LEN];
  struct stat stat_file;
  int len;
  char *buf;
  err[0] = 0;
  snprintf(src_file, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", dsrc, name);
  snprintf(dst_file, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", ddst, name);
  src_file[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
  dst_file[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
  if (!stat(src_file, &stat_file))
    {
    buf = read_whole_file(src_file, &len, err);
    if (buf)
      {
      unlink(dst_file);
      if (!write_whole_file(dst_file, buf, len, err))
        chmod(dst_file, stat_file.st_mode);
      }
    clownix_free(buf, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void my_cp_link(char *dir_src, char *dir_dst, char *name)
{
  char link_name[MAX_PATH_LEN+MAX_NAME_LEN];
  char link_val[MAX_PATH_LEN+MAX_NAME_LEN];
  int len;
  memset(link_val, 0, MAX_PATH_LEN+MAX_NAME_LEN);
  snprintf(link_name, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", dir_src, name);
  link_name[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
  len = readlink(link_name, link_val, MAX_PATH_LEN-1);
  if ((len != -1) && (len < MAX_PATH_LEN-1))
    {
    sprintf(link_name, "%s/%s", dir_dst, name);
    if (symlink (link_val, link_name))
      syslog(LOG_ERR, "ERROR writing link:  %s  %d", link_name, errno);
    }
  else
    syslog(LOG_ERR, "ERROR reading link: %s  %d", link_name, errno);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void my_mv_link(char *dir_src, char *dir_dst, char *name)
{
  char link_name[MAX_PATH_LEN+MAX_NAME_LEN];
  char link_val[MAX_PATH_LEN+MAX_NAME_LEN];
  int len;
  memset(link_val, 0, MAX_PATH_LEN+MAX_NAME_LEN);
  snprintf(link_name, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", dir_src, name);
  link_name[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
  len = readlink(link_name, link_val, MAX_PATH_LEN-1);
  unlink(link_name);
  if ((len != -1) && (len < MAX_PATH_LEN-1))
    {
    sprintf(link_name, "%s/%s", dir_dst, name);
    if (symlink (link_val, link_name))
      syslog(LOG_ERR, "ERROR writing link:  %s  %d", link_name, errno);
    }
  else
    syslog(LOG_ERR,"ERROR reading link: %s  %d", link_name, errno);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void my_mv_file(char *dsrc, char *ddst, char *name)
{
  char err[MAX_PRINT_LEN];
  char src_file[MAX_PATH_LEN];
  char dst_file[MAX_PATH_LEN];
  struct stat stat_file;
  int len;
  char *buf;
  err[0] = 0;
  sprintf(src_file,"%s/%s", dsrc, name);
  sprintf(dst_file,"%s/%s", ddst, name);
  if (!stat(src_file, &stat_file))
    {
    buf = read_whole_file(src_file, &len, err);
    if (buf)
      {
      unlink(dst_file);
      if (!write_whole_file(dst_file, buf, len, err))
        chmod(dst_file, stat_file.st_mode);
      unlink(src_file);
      }
    clownix_free(buf, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void my_mkdir(char *dst_dir, int wr_all)
{
  int result;
  mode_t old_mask, mode_mkdir;
  struct stat stat_file;
  if (wr_all)
    {
    old_mask = umask (0000);
    mode_mkdir = 0777;
    result = mkdir(dst_dir, mode_mkdir);
    }
  else
    {
    old_mask = umask (0077);
    mode_mkdir = 0700;
    result = mkdir(dst_dir, mode_mkdir);
    }
  if (result)
    {
    if (errno != EEXIST)
      KERR("ERROR %s, %d", dst_dir, errno);
    else
      {
      if (stat(dst_dir, &stat_file))
        KERR("ERROR %s, %d", dst_dir, errno);
      if (!S_ISDIR(stat_file.st_mode))
        {
        KERR("ERROR %s", dst_dir);
        unlink(dst_dir);
        if (mkdir(dst_dir, mode_mkdir))
          KERR("ERROR %s, %d", dst_dir, errno);
        }
      }
    }
  umask (old_mask);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void my_cp_dir(char *src_dir, char *dst_dir, char *src_name, char *dst_name)
{
  DIR *dirptr;
  struct dirent *ent;
  char src_sub_dir[MAX_PATH_LEN];
  char dst_sub_dir[MAX_PATH_LEN];
  sprintf(src_sub_dir, "%s/%s", src_dir, src_name);
  sprintf(dst_sub_dir, "%s/%s", dst_dir, dst_name);
  my_mkdir(dst_sub_dir, 0);
  dirptr = opendir(src_sub_dir);
  if (dirptr)
    {
    while ((ent = readdir(dirptr)) != NULL)
      {
      if (!strcmp(ent->d_name, "."))
        continue;
      if (!strcmp(ent->d_name, ".."))
        continue;
      if (ent->d_type == DT_REG)
        my_cp_file(src_sub_dir, dst_sub_dir, ent->d_name);
      else if(ent->d_type == DT_DIR)
        my_cp_dir(src_sub_dir, dst_sub_dir, ent->d_name, ent->d_name);
      else if(ent->d_type == DT_LNK)
        my_cp_link(src_sub_dir, dst_sub_dir, ent->d_name);
      else
        event_print("Wrong type of file %s/%s", src_sub_dir, ent->d_name);
      }
    if (closedir(dirptr))
      KOUT("%d", errno);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void my_mv_dir(char *src_dir,char *dst_dir, char *src_name,char *dst_name)
{
  DIR *dirptr;
  struct dirent *ent;
  char src_sub_dir[MAX_PATH_LEN];
  char dst_sub_dir[MAX_PATH_LEN];
  sprintf(src_sub_dir, "%s/%s", src_dir, src_name);
  sprintf(dst_sub_dir, "%s/%s", dst_dir, dst_name);
  my_mkdir(dst_sub_dir, 0);
  dirptr = opendir(src_sub_dir);
  if (dirptr)
    {
    while ((ent = readdir(dirptr)) != NULL)
      {
      if (!strcmp(ent->d_name, "."))
        continue;
      if (!strcmp(ent->d_name, ".."))
        continue;
      if (ent->d_type == DT_REG)
        my_mv_file(src_sub_dir, dst_sub_dir, ent->d_name);
      else if(ent->d_type == DT_DIR)
        my_mv_dir(src_sub_dir, dst_sub_dir, ent->d_name, ent->d_name);
      else if(ent->d_type == DT_LNK)
        my_mv_link(src_sub_dir, dst_sub_dir, ent->d_name);
      else
        event_print("Wrong type of file %s/%s", src_sub_dir, ent->d_name);
      }
    if (closedir(dirptr))
      KOUT("%d", errno);
    }
  rmdir(src_sub_dir);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void mk_endp_dir(void)
{
  my_mkdir(utils_get_snf_pcap_dir(), 1);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void mk_dtach_dir(void)
{
  char err[2*MAX_PATH_LEN];
  if (unlink_sub_dir_files(utils_get_dtach_sock_dir(), err))
    KERR("%s", err);
  my_mkdir(utils_get_dtach_sock_dir(), 0);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void mk_dpdk_ovs_db_dir(void)
{
  char pth[MAX_PATH_LEN+MAX_NAME_LEN];
  DIR *dirptr;
  struct dirent *ent;
  char *ovsdb_dir = utils_get_dpdk_ovs_db_dir();
  if (!access(ovsdb_dir, F_OK))
    {
    dirptr = opendir(ovsdb_dir);
    if (dirptr)
      {
      while ((ent = readdir(dirptr)) != NULL)
        {
        if (!strcmp(ent->d_name, "."))
          continue;
        if (!strcmp(ent->d_name, ".."))
          continue;
        if (!strcmp(ent->d_name, "cloonix_diag.log"))
          continue;
        if (!strcmp(ent->d_name, "ovs-vswitchd.log"))
          continue;
        if (!strcmp(ent->d_name, "cloonix_ovs_req.log"))
          continue;
        snprintf(pth,MAX_PATH_LEN+MAX_NAME_LEN,"%s/%s",ovsdb_dir,ent->d_name);
        pth[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
        if(ent->d_type == DT_DIR)
          KERR("%s Directory Found: %s will not delete\n", ovsdb_dir, pth);
        else if (unlink(pth))
          KERR("File: %s could not be deleted\n", pth);
        }
      if (closedir(dirptr))
        KOUT("%d", errno);
      }
   }
  my_mkdir(ovsdb_dir, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void mk_dpdk_dir(void)
{
  my_mkdir(utils_get_dpdk_qemu_dir(), 0);
  my_mkdir(utils_get_dpdk_nat_dir(), 0);
  my_mkdir(utils_get_dpdk_xyx_dir(), 0);
  my_mkdir(utils_get_dpdk_a2b_dir(), 0);
  my_mkdir(utils_get_dpdk_d2d_dir(), 0);
  my_mkdir(utils_get_dpdk_cloonix_dir(), 0);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int mk_machine_dirs(char *name, int vm_id)
{
  char err[MAX_PRINT_LEN];
  char path[MAX_PATH_LEN];

  sprintf(path,"%s", cfg_get_work_vm(vm_id));
  my_mkdir(path, 0);

  sprintf(path,"%s/%s", cfg_get_work_vm(vm_id), DIR_CONF);
  my_mkdir(path, 0);

  my_mkdir(utils_dir_conf_tmp(vm_id), 0);
  my_mkdir(utils_get_disks_path_name(vm_id), 0);

  sprintf(path,"%s/%s", cfg_get_work_vm(vm_id),  DIR_UMID);
  my_mkdir(path, 0);

  sprintf(path,"%s/%s", cfg_get_work_vm(vm_id),  CLOONIX_FILE_NAME);
  if (write_whole_file(path, name, strlen(name) + 1, err))
    KERR("%s", err);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int unlink_sub_dir_files_except_dir(char *dir, char *err)
{
  int result = 0;
  int end_result = 0;
  char pth[MAX_PATH_LEN+MAX_NAME_LEN];
  DIR *dirptr;
  struct dirent *ent;
  dirptr = opendir(dir);
  if (dirptr)
    {
    while ((result == 0) && ((ent = readdir(dirptr)) != NULL))
      {
      if (!strcmp(ent->d_name, "."))
        continue;
      if (!strcmp(ent->d_name, ".."))
        continue;
      snprintf(pth, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", dir, ent->d_name);
      pth[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
      if(ent->d_type == DT_DIR)
        {
        end_result = unlink_sub_dir_files(pth, err);
        }
      else if (unlink(pth))
        {
        sprintf(err, "File: %s could not be deleted\n", pth);
        result = -1;
        }
      }
    if (closedir(dirptr))
      KOUT("%d", errno);
    if (result == 0)
      {
      if (end_result)
        result = end_result;
      else if (rmdir(dir))
        {
        sprintf(err, "Dir: %s could not be deleted\n", dir);
        result = -1;
        }
      }
    }
  if (result)
    KERR("ERR DEL: %s %s", dir, err);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int unlink_sub_dir_files(char *dir, char *err)
{
  int result = 0;
  char pth[MAX_PATH_LEN+MAX_NAME_LEN];
  DIR *dirptr;
  struct dirent *ent;
  dirptr = opendir(dir);
  if (dirptr)
    {
    while ((result == 0) && ((ent = readdir(dirptr)) != NULL))
      {
      if (!strcmp(ent->d_name, "."))
        continue;
      if (!strcmp(ent->d_name, ".."))
        continue;
      snprintf(pth, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", dir, ent->d_name);
      pth[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
      if(ent->d_type == DT_DIR)
        result = unlink_sub_dir_files(pth, err);
      else if (unlink(pth))
        {
        sprintf(err, "File: %s could not be deleted\n", pth);
        result = -1;
        }
      }
    if (closedir(dirptr))
      KOUT("%d", errno);
    if (rmdir(dir))
      {
      sprintf(err, "Dir: %s could not be deleted\n", dir);
      result = -1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
int rm_machine_dirs(int vm_id, char *err)
{
  int result = 0;
  char dir[MAX_PATH_LEN];
  char pth[MAX_PATH_LEN+MAX_NAME_LEN];
  DIR *dirptr;
  struct dirent *ent;
  int maxlenpth = MAX_PATH_LEN+MAX_NAME_LEN;
  strcpy(err, "NO_ERROR\n");
  sprintf(dir,"%s", cfg_get_work_vm(vm_id));
  dirptr = opendir(dir);
  if (dirptr)
    {
    while ((result == 0) && ((ent = readdir(dirptr)) != NULL))
      {
      if (!strcmp(ent->d_name, "."))
        continue;
      if (!strcmp(ent->d_name, ".."))
        continue;
      if (!strcmp(DIR_CONF, ent->d_name))
        {
        result = unlink_sub_dir_files(utils_dir_conf_tmp(vm_id), err);
        if (!result)
          {
          snprintf(pth, maxlenpth, "%s/%s", cfg_get_work_vm(vm_id), DIR_CONF);
          pth[maxlenpth-1] = 0;
          result = unlink_sub_dir_files(pth, err);
          }
        }
      else if (!strncmp(FILE_COW, ent->d_name, strlen(FILE_COW)))
        {
        snprintf(pth, maxlenpth, "%s/%s", cfg_get_work_vm(vm_id), ent->d_name);
        pth[maxlenpth-1] = 0;
        if (unlink(pth))
          {
          sprintf(err, "File: %s could not be deleted\n", pth);
          result = -1;
          }
        }
      else if (!strcmp(DIR_CLOONIX_DISKS, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_disks_path_name(vm_id));
        pth[maxlenpth-1] = 0;
        result = unlink_sub_dir_files(pth, err);
        }
      else if (!strcmp(DIR_UMID, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s/%s", cfg_get_work_vm(vm_id), DIR_UMID);
        pth[maxlenpth-1] = 0;
        result = unlink_sub_dir_files(pth, err);
        }
      else if (!strcmp(CLOONIX_FILE_NAME, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s/%s", cfg_get_work_vm(vm_id),  CLOONIX_FILE_NAME);
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
      else if (!strcmp(QMONITOR_UNIX, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_qmonitor_path(vm_id));
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
     else if (!strcmp(QMP_UNIX, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_qmp_path(vm_id));
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
     else if (!strcmp(QHVCO_UNIX, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_qhvc0_path(vm_id));
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
     else if (!strcmp(QBACKDOOR_UNIX, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_qbackdoor_path(vm_id));
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
     else if (!strcmp(QBACKDOOR_HVCO_UNIX, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_qbackdoor_hvc0_path(vm_id));
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
     else if (!strcmp(SPICE_SOCK, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_spice_path(vm_id));
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
      else
        {
        snprintf(err, MAX_PRINT_LEN, "UNEXPECTED: %s found\n", ent->d_name);
        err[MAX_PRINT_LEN-1] = 0;
        result = -1;
        }
      }
    if (closedir(dirptr))
      KOUT("%d", errno);
    if (result == 0)
      {
      if (rmdir(dir))
        {
        snprintf(err, MAX_PRINT_LEN, "Dir %s could not be deleted\n", dir);
        err[MAX_PRINT_LEN-1] = 0;
        result = -1;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_pty(int *master_fdp, int *slave_fdp, char *slave_name)
{
    int  mfd, sfd = -1;
    char pty_name[MAX_NAME_LEN];
    struct termios tios;
    openpty(&mfd, &sfd, pty_name, NULL, NULL);
  if (sfd > 0)
      {
      nonblock_fd(mfd);
      event_print("pty/tty pair: got %s", pty_name);
      strcpy(slave_name, pty_name);
      chmod(slave_name, 0666);
      *master_fdp = mfd;
      *slave_fdp = sfd;
      if (tcgetattr(sfd, &tios) == 0)
        {
          tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
          tios.c_cflag |= CS8 | CREAD | CLOCAL;
          tios.c_iflag  = IGNPAR;
          tios.c_oflag  = 0;
          tios.c_lflag  = 0;
          if (tcsetattr(sfd, TCSAFLUSH, &tios) < 0)
              event_print("couldn't set attributes on pty: %m");
        }
      else
        event_print("couldn't get attributes on pty: %m");
      return 0;
      }
  return -1;
}
/*---------------------------------------------------------------------------*/


