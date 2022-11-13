/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>

#include "io_clownix.h"
#include "launcher.h"
#include "config_json.h"
#include "cnt.h"
#include "loop_img.h"
#include "cnt_utils.h"


void clean_all_llid(void);
char *get_net_name(void);
char *get_bin_dir(void);

static char losetup_bin[MAX_PATH_LEN];
static char ext4fuse_bin[MAX_PATH_LEN];
static char ln_bin[MAX_PATH_LEN];
static char mount_bin[MAX_PATH_LEN];
static char umount_bin[MAX_PATH_LEN];

FILE *my_popen(const char *command, const char *type)
{
//  KERR("COMMAND:%s", command);
  return popen(command, type);
}

/****************************************************************************/
static int get_file_len(char *file_name)
{
  int result = -1;
  struct stat statbuf;
  if (stat(file_name, &statbuf) == 0)
    result = statbuf.st_size;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *read_whole_file(char *file_name, int *len)
{
  char *buf = NULL;
  int fd, readlen;
  *len = get_file_len(file_name);
  if (*len == -1)
    KERR("ERROR Cannot get size of file %s\n", file_name);
  else if (*len)
    {
    fd = open(file_name, O_RDONLY);
    if (fd > 0)
      {
      buf = (char *) malloc((*len+1) *sizeof(char));
      readlen = read(fd, buf, *len);
      if (readlen != *len)
        {
        KERR("ERROR Length of file error %s\n", file_name);
        free(buf);
        buf = NULL;
        }
      else
        buf[*len] = 0;
      close (fd);
      }
    else
      KERR("ERROR Cannot open file  %s\n", file_name);
    }
  else
    {
    buf = (char *) malloc(sizeof(char));
    buf[0] = 0;
    }
  return buf;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int write_whole_file(char *file_name, char *buf, int len)
{
  int result = -1;
  int fd, writelen;
  if (!access(file_name, F_OK))
    {
    KERR("ERROR %s already exists", file_name);
    unlink(file_name);
    }
  fd = open(file_name, O_CREAT|O_WRONLY, 00666);
  if (fd > 0)
    {
    writelen = write(fd, buf, len);
    if (writelen != len)
      KERR("ERROR write of file %s", file_name);
    else
      result = 0;
    close (fd);
    }
  else
    KERR("ERROR open of file %s", file_name);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void my_cp_file(char *dsrc, char *ddst, char *name)
{
  char src_file[MAX_PATH_LEN+MAX_NAME_LEN];
  char dst_file[MAX_PATH_LEN+MAX_NAME_LEN];
  struct stat stat_file;
  int len;
  char *buf;
  snprintf(src_file, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", dsrc, name);
  snprintf(dst_file, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", ddst, name);
  src_file[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
  dst_file[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
  if (!stat(src_file, &stat_file))
    {
    buf = read_whole_file(src_file, &len);
    if (buf)
      {
      unlink(dst_file);
      if (!write_whole_file(dst_file, buf, len))
        chmod(dst_file, stat_file.st_mode);
      }
    free(buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void my_cp_and_sed_file(char *dsrc, char *ddst, char *name,
                               char *customer_launch)
{
  char src_file[MAX_PATH_LEN+MAX_NAME_LEN];
  char dst_file[MAX_PATH_LEN+MAX_NAME_LEN];
  struct stat stat_file;
  int len;
  char *buf1, *buf2, *ptr1, *ptr2, *ptr3;
  snprintf(src_file, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", dsrc, name);
  snprintf(dst_file, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", ddst, name);
  src_file[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
  dst_file[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
  if (!stat(src_file, &stat_file))
    {
    buf1 = read_whole_file(src_file, &len);
    if (buf1)
      {
      unlink(dst_file);
      ptr1 = buf1;
      ptr2 = strstr(ptr1, "__CUSTOMER_LAUNCHER_SCRIPT__");
      ptr3 = ptr2 + strlen("__CUSTOMER_LAUNCHER_SCRIPT__");
      if (ptr2 == NULL)
        {
        KERR("ERROR: %s %s %s", name, customer_launch, ptr1);
        if (!write_whole_file(dst_file, buf1, len))
          chmod(dst_file, stat_file.st_mode);
        free(buf1);
        }
      else
        {
        len += strlen(customer_launch);
        buf2 = (char *) malloc((len+1) *sizeof(char));
        memset(buf2, 0, len+1);
        *ptr2 = 0;
        if ((strlen(ptr1) + strlen(customer_launch) + strlen(ptr3)) >= len)
          {
          KERR("ERROR: %s %s", name, customer_launch);
          if (!write_whole_file(dst_file, buf1, len))
            chmod(dst_file, stat_file.st_mode);
          }
        else 
          {
          strncpy(buf2, ptr1, len);
          strcat(buf2, customer_launch);
          strcat(buf2, ptr3);
          if (!write_whole_file(dst_file, buf2, len))
            chmod(dst_file, stat_file.st_mode);
          }
        free(buf1);
        free(buf2);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *get_config_json(char *rootfs, char *nspace,
                             char *cloonix_dropbear, char *mounttmp,
                             int is_persistent)
{
  char *buf;
  int len = strlen(CONFIG_JSON) + (5 * strlen(CONFIG_JSON_CAPA));
  char *starter;
  starter = "\"/mnt/cloonix_config_fs/cloonix_init_starter.sh\"\n";
  len += (2 * MAX_PATH_LEN);
  buf = (char *) malloc(len);
  memset(buf, 0, len);
  snprintf(buf, len-1, CONFIG_JSON, starter,
                                    CONFIG_JSON_CAPA, CONFIG_JSON_CAPA,
                                    CONFIG_JSON_CAPA, CONFIG_JSON_CAPA,
                                    CONFIG_JSON_CAPA, rootfs, 
                                    cloonix_dropbear, mounttmp, nspace); 
  return buf;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int my_mkdir(char *dst_dir)
{
  int result = -1;
  mode_t old_mask, mode_mkdir;
  mode_mkdir = 0700;
  if (cnt_utils_unlink_sub_dir_files(dst_dir) == 0)
    {
    old_mask = umask (0077);
    result = mkdir(dst_dir, mode_mkdir);
    if (result)
      {
      if (errno != EEXIST)
        KERR("ERROR %s, %d", dst_dir, errno);
      else
        KERR("WARNING ALREADY EXISTS %s", dst_dir);
      }
    umask (old_mask);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int my_mkdir_if_not_exists(char *dst_dir)
{
  int result = -1;
  mode_t old_mask, mode_mkdir;
  mode_mkdir = 0700;
  old_mask = umask (0077);
  result = mkdir(dst_dir, mode_mkdir);
  if (result)
    {
    if (errno != EEXIST)
      KERR("ERROR %s, %d", dst_dir, errno);
    else
      result = 0;
    }
  umask (old_mask);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int execute_cmd(char *cmd, int trace)
{
  int result = 0;
  FILE *fp;
  char line[MAX_PATH_LEN];
  char *ptr;
  fp = my_popen(cmd, "r");
  if (fp == NULL)
    result = -1;
  else
    {
    while ( fgets( line, sizeof line, fp))
      {
      ptr = strchr(line, '\r');
      if (ptr)
        *ptr = 0;
      ptr = strchr(line, '\n');
      if (ptr)
        *ptr = 0;
      if (trace)
        KERR("%s %s", cmd, line);
      result = -1;
      }
    pclose(fp);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_mount_does_not_exist(char *path)
{
  int result = 0;
  char cmd[2*MAX_PATH_LEN];
  char *mount = mount_bin;
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1, "%s | grep %s 2>&1", mount, path);
  if (execute_cmd(cmd, 1))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
static int create_mnt_tmp_dirs(char *cnt_dir, char *name)
{
  int result = -1;
  char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s/tmp", cnt_dir, name);
  if (my_mkdir(path))
    KERR("ERROR %s", path);
  else
    {
    chmod(path, 0777);
    memset(path, 0, MAX_PATH_LEN);
    snprintf(path, MAX_PATH_LEN-1, "%s/%s/mnt", cnt_dir, name);
    if (my_mkdir(path))
      KERR("ERROR %s", path);
    else
      {
      chmod(path, 0777);
      memset(path, 0, MAX_PATH_LEN);
      snprintf(path, MAX_PATH_LEN-1, "%s/%s/mnt/mountbear",
               cnt_dir, name);
      if (my_mkdir(path))
        KERR("ERROR %s", path);
      else
        {
        chmod(path, 0777);
        memset(path, 0, MAX_PATH_LEN);
        snprintf(path, MAX_PATH_LEN-1, "%s/%s/mnt/cloonix_config_fs",
                 cnt_dir, name);
        if (my_mkdir(path))
          KERR("ERROR %s", path);
        else
          {
          memset(path, 0, MAX_PATH_LEN);
          snprintf(path, MAX_PATH_LEN-1,
                   "%s/%s/mnt/cloonix_config_fs/lib", cnt_dir, name);
          if (my_mkdir(path))
            KERR("ERROR %s", path);
          else
            {
            snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s",
                     cnt_dir, name, WORKDIR);
            if (my_mkdir(path))
              KERR("ERROR %s", path);
            else
              {
              memset(path, 0, MAX_PATH_LEN);
              snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s",
                       cnt_dir, name, ROOTFS);
              if (my_mkdir(path))
                KERR("ERROR %s", path);
              else
                result = 0;
              }
            }
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int create_all_dirs(char *bulk, char *image, char *cnt_dir, char *name)
{
  int result = -1;
  char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/mnt/%s", bulk, image);
  if (my_mkdir_if_not_exists(path))
    KERR("ERROR %s", path);
  else
    {
    memset(path, 0, MAX_PATH_LEN);
    snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s/", cnt_dir, name, UPPER);
    if (my_mkdir(path))
      KERR("ERROR %s", path);
    else
      {
      memset(path, 0, MAX_PATH_LEN);
      snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s/usr", cnt_dir, name, UPPER);
      if (my_mkdir(path))
        KERR("ERROR %s", path);
      else
        {
        memset(path, 0, MAX_PATH_LEN);
        snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s/usr/bin",cnt_dir,name,UPPER);
        if (my_mkdir(path))
          KERR("ERROR %s", path);
        else
          result = create_mnt_tmp_dirs(cnt_dir, name);
        }
      }
    }
  return result;
}
 
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int dir_cnt_create(char *bulk, char *image, char *cnt_dir,
                          char *name, char *agent_dir, char *launch)
{
  char path[MAX_PATH_LEN];
  char libpath[MAX_PATH_LEN];
  int result = create_all_dirs(bulk, image, cnt_dir, name);
  if (result == 0)
    {
    memset(path, 0, MAX_PATH_LEN);
    snprintf(path, MAX_PATH_LEN-1, "%s/%s/mnt/cloonix_config_fs",
             cnt_dir, name);
    my_cp_and_sed_file(agent_dir, path, "cloonix_init_starter.sh", launch);
    my_cp_file(agent_dir, path, "cloonix_agent");
    my_cp_file(agent_dir, path, "dropbear_cloonix_sshd");

    snprintf(path, MAX_PATH_LEN-1, "%s/%s/mnt/cloonix_config_fs/lib",
             cnt_dir, name);
    snprintf(libpath, MAX_PATH_LEN-1, "%s/lib", agent_dir);
    my_cp_file(libpath, path, "ld-linux-x86-64.so.2");
    my_cp_file(libpath, path, "libc.so.6");
    my_cp_file(libpath, path, "libutil.so.1");
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int read_crun_create_pid(char *name)
{
  FILE *fp;
  char *ptr;
  int count= 0, create_pid = 0;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  int nb_tries = 6;
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "crun state %s |grep \"\\\"pid\\\"\" |awk '{ print $2 }'", name);
  while(create_pid == 0)
    {
    count += 1;
    if (count > nb_tries)
      break;
    memset(line, 0, MAX_PATH_LEN);
    fp = my_popen(cmd, "r");
    if (fp == NULL)
      KERR("ERROR %s", cmd);
    else
      {
      if(fgets(line, MAX_PATH_LEN-1, fp))
        {
        ptr = strchr(line, ',');
        if (ptr)
          *ptr = 0;
        create_pid = atoi(line);
        }
      pclose(fp);
      }
    usleep(3000);
    }
  return create_pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void check_netns_and_clean(char *name, char *nspace,
                                  int vm_id, int nb_eth)
{
  int i;
  char cmd[MAX_PATH_LEN];
  int pid = read_crun_create_pid(name);
  if (pid)
    kill(pid, 9);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "/usr/bin/crun delete %s", name);
  execute_cmd(cmd, 0);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns del %s 2>&1 >/dev/null",
           SBIN_IP, nspace);
  execute_cmd(cmd, 0);
  for (i=0; i<nb_eth; i++)
    {
    snprintf(cmd, MAX_PATH_LEN-1, "%s link del name %s%d_%d 2>&1",
             SBIN_IP, OVS_BRIDGE_PORT, vm_id, i);
    execute_cmd(cmd, 0);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int cnt_utils_unlink_sub_dir_files(char *dir)
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
        result = cnt_utils_unlink_sub_dir_files(pth);
      else if (unlink(pth))
        {
        KERR("ERROR File: %s could not be deleted\n", pth);
        result = -1;
        }
      }
    if (closedir(dirptr))
      KOUT("%d", errno);
    if (rmdir(dir))
      {
      KERR("ERROR Dir: %s could not be deleted\n", pth);
      result = -1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int cnt_utils_create_overlay(char *path, char *lower, int is_persistent)
{
  int result = -1;
  char cmd[2*MAX_PATH_LEN];
//  char *mount = mount_bin;

  if (check_mount_does_not_exist(path))
    KERR("%s", path);
  else
    {
    memset(cmd, 0, 2*MAX_PATH_LEN);
    if (is_persistent)
      {
      result = 0;
      }
    else
      {
//      snprintf(cmd, 2*MAX_PATH_LEN-1,
//      "%s none -t overlay -o lowerdir=%s,upperdir=%s/%s,workdir=%s/%s %s/%s 2>&1",
//      mount,  lower, path, UPPER, path, WORKDIR, path, ROOTFS);

      snprintf(cmd, 2*MAX_PATH_LEN-1,
      "/usr/bin/fuse-overlayfs -o lowerdir=%s -o upperdir=%s/%s -o workdir=%s/%s %s/%s 2>&1",
      lower, path, UPPER, path, WORKDIR, path, ROOTFS);
      if (execute_cmd(cmd, 1))
        KERR("%s", cmd);
      else
        result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int cnt_utils_create_crun_create(char *cnt_dir, char *name)
{
  char cnf[MAX_PATH_LEN];
  char *argv[5];
  int pid, wstatus;
  memset(cnf, 0, MAX_PATH_LEN);
  snprintf(cnf, MAX_PATH_LEN-1, "--config=%s/%s/config.json", cnt_dir, name);
  argv[0] = "/usr/bin/crun";
  argv[1] = "create";
  argv[2] = cnf;
  argv[3] = name;
  argv[4] = NULL;
  if ((pid = fork()) < 0)
    KOUT(" ");
  if (pid == 0)
    {
    clean_all_llid();
    execv(argv[0], argv);
    }
  else
    {
    waitpid(pid, &wstatus, 0);
    pid = read_crun_create_pid(name);
    if (pid == 0)
      KERR("ERROR DID NOT GET CREATE PID FOR %s", name);
    }
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int cnt_utils_create_config_json(char *path, char *rootfs, char *nspace,
                                 char *cloonix_dropbear, 
                                 char *mounttmp, int is_persistent)
{
  int len, result;
  char json_path[MAX_PATH_LEN];
  char *buf = get_config_json(rootfs, nspace, cloonix_dropbear,
                              mounttmp, is_persistent);
  memset(json_path, 0, MAX_PATH_LEN);
  snprintf(json_path, MAX_PATH_LEN-1, "%s/config.json", path); 
  len = strlen(buf);
  result = write_whole_file(json_path, buf, len);
  free(buf);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int nspace_create(char *name, char *nspace, int cloonix_rank,
                         int vm_id, int nb_eth, t_eth_mac *eth_mac)
{
  int i, result = -1;
  char cmd[MAX_PATH_LEN];         
  char *mac;
  check_netns_and_clean(name, nspace, vm_id, nb_eth);
  memset(cmd, 0, MAX_PATH_LEN);         
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns add %s 2>&1",
           SBIN_IP, nspace);
  if (execute_cmd(cmd, 1))
    KERR("ERROR %s", cmd);
  else
    {
    usleep(10000);
    result = 0;
    for (i=0; i<nb_eth; i++)
      {
      mac = eth_mac[i].mac;
      snprintf(cmd, MAX_PATH_LEN-1,
      "%s link add name %s%d_%d type veth peer name tmpcnt%d "
      "address %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx  2>&1",
      SBIN_IP, OVS_BRIDGE_PORT, vm_id, i, i, mac[0], mac[1], mac[2],
      mac[3], mac[4], mac[5]);
      if (execute_cmd(cmd, 1))
        {
        KERR("%s", cmd);
        result = -1;
        break;
        }
      snprintf(cmd, MAX_PATH_LEN-1,
      "%s link set tmpcnt%d netns %s 2>&1", SBIN_IP, i, nspace);
      if (execute_cmd(cmd, 1))
        {
        KERR("ERROR %s", cmd);
        result = -1;
        break;
        }
      snprintf(cmd, MAX_PATH_LEN-1,
      "%s link set %s%d_%d netns %s_%s 2>&1",
      SBIN_IP, OVS_BRIDGE_PORT, vm_id, i, BASE_NAMESPACE, get_net_name());
      if (execute_cmd(cmd, 1))
        {
        KERR("ERROR %s", cmd);
        result = -1;
        break;
        }

      snprintf(cmd, MAX_PATH_LEN-1,
      "%s netns exec %s %s link set tmpcnt%d name eth%d 2>&1",
      SBIN_IP, nspace, SBIN_IP, i, i);
      if (execute_cmd(cmd, 1))
        {
        KERR("ERROR %s", cmd);
        result = -1;
        break;
        }

      snprintf(cmd, MAX_PATH_LEN-1,
      "%s netns exec %s %s link set eth%d up 2>&1",
      SBIN_IP, nspace, SBIN_IP, i);
      if (execute_cmd(cmd, 1))
        {
        KERR("ERROR %s", cmd);
        result = -1;
        break;
        }
      }
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s netns exec %s %s link set lo up 2>&1", SBIN_IP, nspace, SBIN_IP);
    if (execute_cmd(cmd, 1))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int cnt_utils_create_net(char *bulk, char *image, char *name, char *cnt_dir,
                         char *nspace, int cloonix_rank, int vm_id,
                         int nb_eth, t_eth_mac *eth_mac, char *agent_dir,
                         char *customer_launch)
{
  int result = -1;
  if (dir_cnt_create(bulk, image, cnt_dir, name, agent_dir, customer_launch))
    KERR("ERROR %s %s %s %s", cnt_dir, name, nspace, customer_launch);
  else if (nspace_create(name, nspace, cloonix_rank, vm_id, nb_eth, eth_mac))
    KERR("ERROR %s %s %s", cnt_dir, name, nspace);
  else
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int cnt_utils_delete_net(char *nspace)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns del %s 2>&1", SBIN_IP, nspace);
  if (execute_cmd(cmd, 1))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int cnt_utils_delete_overlay(char *name, char *cnt_dir, char *bulk,
                             char *image, int is_persistent)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  char *umount = umount_bin;

  if (is_persistent == 0)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s %s/%s/%s 2>&1",
             umount, cnt_dir, name, ROOTFS);
    if (execute_cmd(cmd, 1))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      }
    }
  if (loop_img_del(name, bulk, image, cnt_dir, is_persistent))
    {
    KERR("ERROR %s", name);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int cnt_utils_delete_crun_stop(char *name, int crun_pid)
{
  int pid, result = -1;
  char cmd[MAX_PATH_LEN];
  if (crun_pid > 0)
    result = kill(crun_pid, 9);
  else
    {
    pid = read_crun_create_pid(name);
    if (pid > 0)
      result = kill(pid, 9);
    }
  memset(cmd, 0, MAX_PATH_LEN); 
  snprintf(cmd, MAX_PATH_LEN-1, "/usr/bin/crun delete %s", name);
  if (execute_cmd(cmd, 1))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_ext4fuse_bin(void)
{
  return ext4fuse_bin;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_losetup_bin(void)
{
  return losetup_bin;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_mount_bin(void)
{
  return mount_bin;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_umount_bin(void)
{
  return umount_bin;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int cnt_utils_create_crun_start(char *name)
{
  int result = -1;
  FILE *fp;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  memset(line, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "/usr/bin/crun start %s 2>&1", name);
  fp = my_popen(cmd, "r");
  if (fp == NULL)
    KERR("ERROR %s", cmd);
  else 
    {
    if (!fgets(line, MAX_PATH_LEN-1, fp))
      result = 0;
    else
      KERR("ERROR %s %s", cmd, line);
    pclose(fp);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *utils_get_suid_power_bin_path(void)
{
  static char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  return path;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void cnt_utils_init(void)
{
  memset(mount_bin, 0, MAX_PATH_LEN);
  memset(umount_bin, 0, MAX_PATH_LEN);
  memset(ln_bin, 0, MAX_PATH_LEN);
  memset(losetup_bin, 0, MAX_PATH_LEN);
  memset(ext4fuse_bin, 0, MAX_PATH_LEN);

  if (!access("/sbin/losetup", F_OK))
    strcpy(losetup_bin, "/sbin/losetup");
  else if (!access("/bin/losetup", F_OK))
    strcpy(losetup_bin, "/bin/losetup");
  else
    KERR("ERROR ERROR ERROR losetup binary not found");

  snprintf(ext4fuse_bin, MAX_PATH_LEN-1,
           "%s/server/gerard_lledo/ext4fuse",
           get_bin_dir());

  if (!access("/sbin/ln", F_OK))
    strcpy(ln_bin, "/sbin/ln");
  else if (!access("/bin/ln", F_OK))
    strcpy(ln_bin, "/bin/ln");
  else
    KERR("ERROR ERROR ERROR ln binary not found");
  if (!access("/sbin/mount", F_OK))
    strcpy(mount_bin, "/sbin/mount");
  else if (!access("/bin/mount", F_OK))
    strcpy(mount_bin, "/bin/mount");
  else
    KERR("ERROR ERROR ERROR mount binary not found");
  if (!access("/sbin/umount", F_OK))
    strcpy(umount_bin, "/sbin/umount");
  else if (!access("/bin/umount", F_OK))
    strcpy(umount_bin, "/bin/umount");
  else
    KERR("ERROR ERROR ERROR umount binary not found");
  if (access("/usr/bin/crun", F_OK))
    KERR("ERROR ERROR ERROR /usr/bin/crun binary not found");
}
/*--------------------------------------------------------------------------*/
