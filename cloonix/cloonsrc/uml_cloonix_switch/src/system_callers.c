/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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


int my_mkdir(char *dst_dir, int wr_all)
{
  int result;
  struct stat stat_file;
  if (wr_all)
    result = mkdir(dst_dir, 0777);
  else
    result = mkdir(dst_dir, 0700);
  if (result)
    {
    if (errno != EEXIST)
      KERR("ERROR %s, %d", dst_dir, errno);
    else
      {
      if (stat(dst_dir, &stat_file))
        KERR("ERROR %s, %d", dst_dir, errno);
      else if (!S_ISDIR(stat_file.st_mode))
        {
        KERR("ERROR %s", dst_dir);
        unlink(dst_dir);
        if (mkdir(dst_dir, 0777))
          KERR("ERROR %s, %d", dst_dir, errno);
        }
      else
        result = 0;
      }
    }
  return result;
}

void mk_cnt_dir(void)
{
  my_mkdir(utils_get_cnt_dir(), 1);
  my_mkdir(utils_get_mnt_dir(), 1);
}

void mk_endp_dir(void)
{
  my_mkdir(utils_get_run_dir(), 1);
  my_mkdir(utils_get_crun_dir(), 1);
  my_mkdir(utils_get_log_dir(), 1);
  my_mkdir(utils_get_snf_pcap_dir(), 1);
  my_mkdir(utils_get_c2c_dir(), 1);
  my_mkdir(utils_get_nat_main_dir(), 1);
  my_mkdir(utils_get_nat_proxy_dir(), 1);
  my_mkdir(utils_get_a2b_dir(), 1);
  my_mkdir(utils_get_run_config_dir(), 1);
}

void mk_dtach_screen_dir(void)
{
  char err[2*MAX_PATH_LEN];
  if (rm_tree(utils_get_dtach_sock_dir(), err))
    KERR("ERROR %s", err);
  my_mkdir(utils_get_dtach_sock_dir(), 1);
}

void mk_ovs_db_dir(void)
{
  char pth[MAX_PATH_LEN+MAX_NAME_LEN];
  DIR *dirptr;
  struct dirent *ent;
  char *ovsdb_dir = utils_get_ovs_dir();
  if (!access(ovsdb_dir, R_OK))
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
        if(ent->d_type != DT_DIR)
          {
          snprintf(pth,MAX_PATH_LEN+MAX_NAME_LEN,"%s/%s",ovsdb_dir,ent->d_name);
          pth[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
          if (unlink(pth))
            KERR("File: %s could not be deleted\n", pth);
          }
        }
      if (closedir(dirptr))
        KOUT("%d", errno);
      }
   }
  my_mkdir(ovsdb_dir, 1);
}

int mk_machine_dirs(char *name, int vm_id)
{
  char err[MAX_PRINT_LEN];
  char path[MAX_PATH_LEN];

  snprintf(path, sizeof(path),"%s", cfg_get_work_vm(vm_id));
  my_mkdir(path, 0);

  snprintf(path, sizeof(path),"%s/%s", cfg_get_work_vm(vm_id), DIR_CONF);
  my_mkdir(path, 0);

  my_mkdir(utils_dir_conf_tmp(vm_id), 0);
  my_mkdir(utils_get_disks_path_name(vm_id), 0);

  snprintf(path, sizeof(path),"%s/%s", cfg_get_work_vm(vm_id),  DIR_UMID);
  my_mkdir(path, 0);

  snprintf(path, sizeof(path),"%s/%s", cfg_get_work_vm(vm_id),  CLOONIX_FILE_NAME);
  if (write_whole_file(path, name, strlen(name) + 1, err))
    KERR("%s", err);
  return 0;
}

int mk_cnt_dirs(char *dir_path, char *name)
{
  int result = 0;
  char path[MAX_PATH_LEN];
  snprintf(path, sizeof(path),"%s/%s", dir_path, name);
  if (my_mkdir(path, 1))
    {
    result = -1;
    KERR("ERROR %s %s", dir_path, name);
    }
  return result;
}

int rm_work_dir(char *dir, char *err)
{
  int result = rm_tree(dir, err);
  if (result)
    KERR("ERR DEL: %s %s", dir, err);
  return result;
}

int rm_machine_dirs(int vm_id, char *err)
{
  int result = 0;
  char dir[MAX_PATH_LEN];
  char pth[MAX_PATH_LEN+MAX_NAME_LEN];
  DIR *dirptr;
  struct dirent *ent;
  int maxlenpth = MAX_PATH_LEN+MAX_NAME_LEN;
  strcpy(err, "NO_ERROR\n");
  snprintf(dir, sizeof(dir),"%s", cfg_get_work_vm(vm_id));
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
        result = rm_tree(utils_dir_conf_tmp(vm_id), err);
        if (!result)
          {
          snprintf(pth, maxlenpth, "%s/%s", cfg_get_work_vm(vm_id), DIR_CONF);
          pth[maxlenpth-1] = 0;
          result = rm_tree(pth, err);
          }
        }
      else if (!strncmp(FILE_COW, ent->d_name, strlen(FILE_COW)))
        {
        snprintf(pth, maxlenpth, "%s/%s", cfg_get_work_vm(vm_id), ent->d_name);
        pth[maxlenpth-1] = 0;
        if (unlink(pth))
          {
          snprintf(err, MAX_PATH_LEN, "File: %s could not be deleted\n", pth);
          result = -1;
          }
        }
      else if (!strcmp(DIR_CLOONIX_DISKS, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_disks_path_name(vm_id));
        pth[maxlenpth-1] = 0;
        result = rm_tree(pth, err);
        }
      else if (!strcmp(DIR_UMID, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s/%s", cfg_get_work_vm(vm_id), DIR_UMID);
        pth[maxlenpth-1] = 0;
        result = rm_tree(pth, err);
        }
      else if (!strcmp(CLOONIX_FILE_NAME, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s/%s", cfg_get_work_vm(vm_id),  CLOONIX_FILE_NAME);
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
     else if (!strcmp(QMP_UNIX, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_qmp_path(vm_id));
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
     else if (!strcmp(QGA_UNIX, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_qga_path(vm_id));
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
     else if (!strcmp(QBACKDOOR_UNIX, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_qbackdoor_path(vm_id));
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
     else if (!strcmp(VNC_SOCK, ent->d_name))
        {
        snprintf(pth, maxlenpth, "%s", utils_get_vnc_path(vm_id));
        pth[maxlenpth-1] = 0;
        unlink(pth);
        }
      else
        {
        snprintf(err, MAX_PATH_LEN, "UNEXPECTED: %s found\n", ent->d_name);
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
        snprintf(err, MAX_PATH_LEN, "Dir %s could not be deleted\n", dir);
        err[MAX_PRINT_LEN-1] = 0;
        result = -1;
        }
      }
    }
  return result;
}

