/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include <ctype.h>


#include "io_clownix.h"
#include "config_json.h"
#include "crun.h"
#include "loop_img.h"
#include "crun_utils.h"

#define MAX_ARGS_POPEN 100
#define MAX_CMD_POPEN 5000


char *get_net_name(void);
char *get_bin_dir(void);
char g_var_log[MAX_PATH_LEN];
char g_var_crun_log[MAX_PATH_LEN];


/*****************************************************************************/
int force_waitpid(int pid, int *status)
{
  int count = 0, result = 0;
  if (pid <= 0)
    KOUT("ERROR %d", pid);
  do
    {
    if (waitpid(pid, status, 0) != pid)
      {
      count++;
      if (count == 10)
        KOUT("ERROR");
      KERR("ERROR waitpid");
      usleep(1000);
      result = -1;
      }
    else if (!WIFEXITED(*status))
      {
      count++;
      if (count == 10)
        KOUT("ERROR");
      usleep(1000);
      KERR("ERROR waitpid");
      result = -1;
      }
    } while(!WIFEXITED(*status));
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void log_write_req(char *line)
{
  FILE *fp_log;
  fp_log = fopen(g_var_log, "a+");
  if (fp_log)
    {
    fprintf(fp_log, "%s\n", line);
    fflush(fp_log);
    fclose(fp_log);
    }
}
/*---------------------------------------------------------------------------*/

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
static FILE *lio_popen(char *argv[], int *child_pid)
{
  int exited_pid, timeout_pid, worker_pid, chld_state, pid, status;
  int pdes[2];
  FILE *iop = NULL;
  if (pipe(pdes))
    KOUT("ERROR");
  if ((pid = fork()) < 0)
    KOUT("ERROR");
  if (pid == 0)
    {
    lio_clean_all_llid();
    (void) close(pdes[0]);
    if (pdes[1] != STDOUT_FILENO)
      {
      (void)dup2(pdes[1], STDOUT_FILENO);
      (void)close(pdes[1]);
      }
    worker_pid = fork();
    if (worker_pid == 0)
      {
      execv(argv[0], argv);
      KOUT("ERROR FORK %s %s", strerror(errno), lio_linear(argv));
      }
    timeout_pid = fork();
    if (timeout_pid == 0)
      {
      sleep(25);
      KERR("WARNING TIMEOUT SLOW CMD 1 %s", lio_linear(argv));
      exit(1);
      }
    exited_pid = wait(&chld_state);
    if (exited_pid == worker_pid)
      {
      status = 0;
      kill(timeout_pid, SIGKILL);
      }
    else
      {
      status = -1;
      KERR("WARNING EXITED TIMER\n");
      kill(worker_pid, SIGKILL);
      }
    wait(NULL);
    exit(status);
    }
  else
    {
    iop = fdopen(pdes[0], "r");
    if (iop == NULL)
      KOUT("ERROR");
    (void)close(pdes[1]);
    *child_pid = pid;
    }
  return iop;
}
/*---------------------------------------------------------------------------*/

// /usr/libexec/cloonix/server/cloonix-suid-power
// /usr/libexec/cloonix/server/cloonix-ext4fuse
// /usr/libexec/cloonix/server/cloonix-fuse-overlayfs
// /usr/libexec/cloonix/server/cloonix-crun


/*****************************************************************************/
FILE *cmd_lio_popen(char *cmd, int *child_pid)
{
//  char *ptr1, *ptr2;
//  char *ptr[MAX_ARGS_POPEN];
  static char *argv[MAX_ARGS_POPEN];
  char cpcmd[MAX_CMD_POPEN];
//  int nb = 0;
  FILE *iop = NULL;
  if (!cmd)
    KOUT("ERROR");
  if (strlen(cmd) == 0)
    KOUT("ERROR");
  if (strlen(cmd) >= MAX_CMD_POPEN)
    KOUT("ERROR %lu", strlen(cmd));
  strncpy(cpcmd, cmd, MAX_CMD_POPEN - 1);
  strcat(cpcmd, " 2>&1");
/*
    ptr1 = cpcmd;
    ptr[nb++] = ptr1;
    ptr2 = strchr(ptr1, '|');
    while(ptr2)
      {
      *ptr2 = 0;
      ptr1 = ptr2 + 1;
      ptr1 += strspn(ptr1, " ");
      ptr[nb++] = ptr1;
      ptr2 = strchr(ptr1, '|');
      }
*/
  argv[0] = BASH_BIN;
  argv[1] = "-c";
  argv[2] = cpcmd;
  argv[3] = NULL;
  iop = lio_popen(argv, child_pid);
  log_write_req(cmd);
  return iop;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int execute_cmd(char *cmd, int synchro)
{
  int child_pid, wstatus, result = -1;
  FILE *fp = cmd_lio_popen(cmd, &child_pid);
  if (fp == NULL)
    KERR("ERROR %s", cmd);
  else
    {
    if (synchro)
      {
      if (force_waitpid(child_pid, &wstatus))
        KERR("ERROR");
      result = wstatus;    
      }
    else
      {
      KOUT("ERROR NOT USED");
      result = child_pid;
      }
    pclose(fp);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

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
  char cmd[MAX_PATH_LEN];
  if (!access(file_name, F_OK))
    {
    KERR("ERROR %s already exists", file_name);
    unlink(file_name);
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "unlink %s", file_name);
    log_write_req(cmd);
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
static void my_cp_and_sed_file(char *dsrc, char *ddst, char *name,
                               char *customer_launch)
{
  char src_file[MAX_PATH_LEN+MAX_NAME_LEN];
  char dst_file[MAX_PATH_LEN+MAX_NAME_LEN];
  char cmd[2*MAX_PATH_LEN];
  struct stat stat_file;
  int len, len_to_sub, len_to_add;
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
      memset(cmd, 0, 2*MAX_PATH_LEN);
      snprintf(cmd, 2*MAX_PATH_LEN-1, "unlink %s", dst_file);
      log_write_req(cmd);
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
        len_to_sub = strlen("__CUSTOMER_LAUNCHER_SCRIPT__");
        len_to_add = 0;
        len_to_add = strlen(customer_launch);
        len -= len_to_sub;
        len += len_to_add;
        buf2 = (char *) malloc((len+1) *sizeof(char));
        memset(buf2, 0, len+1);
        *ptr2 = 0;
        if ((strlen(ptr1) + len_to_add + strlen(ptr3)) > len)
          {
          KERR("ERROR: %s %lu %d %lu %d", src_file,
               strlen(ptr1), len_to_add, strlen(ptr3), len);
          KERR("ERROR: %s %s %s %s", name, customer_launch, ptr1, ptr3);
          if (!write_whole_file(dst_file, buf1, len))
            chmod(dst_file, stat_file.st_mode);
          }
        else 
          {
          strncpy(buf2, ptr1, len-len_to_add);
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
  starter = "\"/mnt/cloonix_config_fs/crun_init_starter.sh\"\n";
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
  char cmd[MAX_PATH_LEN];
  int result = -1;
  if (crun_utils_unlink_sub_dir_files(dst_dir) == 0)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "mkdir %s", dst_dir);
    log_write_req(cmd);
    result = mkdir(dst_dir, 0777);
    if (result)
      {
      if (errno != EEXIST)
        KERR("ERROR %s, %d", dst_dir, errno);
      else
        KERR("WARNING ALREADY EXISTS %s", dst_dir);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int my_mkdir_if_not_exists(char *dst_dir)
{
  int result = -1;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "mkdir %s", dst_dir);
  log_write_req(cmd);
  result = mkdir(dst_dir, 0777);
  if (result)
    {
    if (errno != EEXIST)
      KERR("ERROR %s, %d", dst_dir, errno);
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_mount_does_not_exist(char *path)
{
  int result = 0;
  char cmd[2*MAX_PATH_LEN];
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd,2*MAX_PATH_LEN-1,"%s | %s %s",MOUNT_BIN,GREP_BIN,path);
  if (execute_cmd(cmd, SYNCHRO))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
int dirs_agent_create_mnt_tmp(char *cnt_dir, char *name)
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
          chmod(path, 0777);
          memset(path, 0, MAX_PATH_LEN);
          snprintf(path, MAX_PATH_LEN-1,
                   "%s/%s/mnt/cloonix_config_fs/lib", cnt_dir, name);
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
static int create_all_dirs(char *image, char *cnt_dir, char *name)
{
  int result = -1;
  char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", get_mnt_loop_dir(), image);
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
          {
          result = dirs_agent_create_mnt_tmp(cnt_dir, name);
          if (result == 0)
            {
            result = -1;
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
void dirs_agent_copy_starter(char *cnt_dir, char *name,
                             char *agent_dir, char *launch)
{
  char path[MAX_PATH_LEN];
  char cmd[2*MAX_PATH_LEN];
  char *iso="/usr/libexec/cloonix/server/insider_agents/insider_agent_x86_64.iso";
  memset(path, 0, MAX_PATH_LEN);
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s/mnt/cloonix_config_fs", cnt_dir, name);
  snprintf(cmd, 2*MAX_PATH_LEN-1, "%s -osirrox on -indev %s -extract / %s",
           OSIRROX_BIN, iso, path); 
  if (execute_cmd(cmd, SYNCHRO))
    KERR("ERROR %s", cmd);
  my_cp_and_sed_file(agent_dir, path, "crun_init_starter.sh", launch);
  my_cp_and_sed_file(agent_dir, path, "docker_init_starter.sh", launch);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int read_crun_create_pid(char *name)
{
  FILE *fp;
  char *ptr = g_var_crun_log;
  int child_pid, wstatus, count= 0, create_pid = 0;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  int nb_tries = 3;
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
  "%s --log=%s --root=/var/lib/cloonix/%s/crun "
  "state %s | %s \"\\\"pid\\\"\" | %s '{ print $2 }'",
  CRUN_BIN, ptr, get_net_name(), name, GREP_BIN, AWK_BIN);
  while(create_pid == 0)
    {
    usleep(10000);
    count += 1;
    if (count > nb_tries)
      break;
    memset(line, 0, MAX_PATH_LEN);
    fp = cmd_lio_popen(cmd, &child_pid);
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
      if (force_waitpid(child_pid, &wstatus))
        KERR("ERROR");
      }
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
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns del %s >/dev/null",
           IP_BIN, nspace);
  execute_cmd(cmd, SYNCHRO);
  for (i=0; i<nb_eth; i++)
    {
    snprintf(cmd, MAX_PATH_LEN-1, "%s link del name %s%d_%d",
             IP_BIN, OVS_BRIDGE_PORT, vm_id, i);
    execute_cmd(cmd, SYNCHRO);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int crun_utils_unlink_sub_dir_files(char *dir)
{
  int result = 0;
  char pth[MAX_PATH_LEN+MAX_NAME_LEN];
  DIR *dirptr;
  struct dirent *ent;
  char cmd[2*MAX_PATH_LEN];
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
        result = crun_utils_unlink_sub_dir_files(pth);
      else if (unlink(pth))
        {
        KERR("ERROR File: %s could not be deleted\n", pth);
        result = -1;
        }
      else
        {
        memset(cmd, 0, 2*MAX_PATH_LEN);
        snprintf(cmd, 2*MAX_PATH_LEN-1, "unlink %s", pth);
        log_write_req(cmd);
        }
      }
    if (closedir(dirptr))
      KOUT("%d", errno);
    if (rmdir(dir))
      {
      KERR("ERROR Dir: %s could not be deleted\n", pth);
      result = -1;
      }
    else
      {
      log_write_req(dir);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_create_overlay(char *path, char *lower, int is_persistent)
{
  int result = -1;
  char cmd[2*MAX_PATH_LEN];

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
      snprintf(cmd, 2*MAX_PATH_LEN-1,
      "%s -o lowerdir=%s -o upperdir=%s/%s -o workdir=%s/%s %s/%s",
      FUSE_OVERLAY_FS_BIN, lower, path, UPPER, path, WORKDIR, path, ROOTFS);
      if (execute_cmd(cmd, SYNCHRO))
        KERR("%s", cmd);
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_create_crun_create(char *cnt_dir, char *name)
{
  int pid, len = 0;
  char cmd[4*MAX_PATH_LEN];
  memset(cmd, 0, 4*MAX_PATH_LEN);
  len += sprintf(cmd+len, "%s", CRUN_BIN); 
  len += sprintf(cmd+len, " --log=%s", g_var_crun_log); 
  len += sprintf(cmd+len, " --root=/var/lib/cloonix/%s/crun",get_net_name()); 
  len += sprintf(cmd+len, " create"); 
  len += sprintf(cmd+len, " --config=%s/%s/config.json", cnt_dir, name);
  len += sprintf(cmd+len, " %s", name); 
  if (execute_cmd(cmd, SYNCHRO))
    KERR("ERROR %s", cmd);
  pid = read_crun_create_pid(name);
  if (pid == 0)
    KOUT("ERROR DID NOT GET CREATE PID FOR %s", name);
  return pid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_create_config_json(char *path, char *rootfs, char *nspace,
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
  if (access(json_path, F_OK))
    KERR("ERROR %s", json_path);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int crun_utils_create_veth(int vm_id, char *nspace,
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
    IP_BIN, OVS_BRIDGE_PORT, vm_id, i, vm_id, i,
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (execute_cmd(cmd, SYNCHRO))
      {
      KERR("%s", cmd);
      result = -1;
      break;
      }
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s link set itp%d%d netns %s", IP_BIN, vm_id, i, nspace);
    if (execute_cmd(cmd, SYNCHRO))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      break;
      }
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s link set %s%d_%d netns %s_%s",
    IP_BIN, OVS_BRIDGE_PORT, vm_id, i, BASE_NAMESPACE, get_net_name());
    if (execute_cmd(cmd, SYNCHRO))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      break;
      }
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s netns exec %s %s link set itp%d%d name eth%d",
    IP_BIN, nspace, IP_BIN, vm_id, i, i);
    if (execute_cmd(cmd, SYNCHRO))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      break;
      }
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s netns exec %s %s link set eth%d up",
    IP_BIN, nspace, IP_BIN, i);
    if (execute_cmd(cmd, SYNCHRO))
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
static int nspace_create(char *name, char *nspace, int cloonix_rank,
                         int vm_id, int nb_eth, t_eth_mac *eth_mac)
{
  int result = -1;
  char cmd[MAX_PATH_LEN];
  int pid = read_crun_create_pid(name);
  char *ptr = g_var_crun_log;
  if (pid)
    kill(pid, 9);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, 
           "%s --log=%s --root=/var/lib/cloonix/%s/crun delete %s",
           CRUN_BIN, ptr, get_net_name(), name);
  execute_cmd(cmd, SYNCHRO);
  check_netns_and_clean(name, nspace, vm_id, nb_eth);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns add %s", IP_BIN, nspace);
  if (execute_cmd(cmd, SYNCHRO))
    KERR("ERROR %s", cmd);
  else
    {
    usleep(10000);
    result = crun_utils_create_veth(vm_id, nspace, nb_eth, eth_mac);
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s netns exec %s %s link set lo up", IP_BIN, nspace, IP_BIN);
    if (execute_cmd(cmd, SYNCHRO))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_docker_veth(char *name, int pid, int vm_id,
                           int nb_eth, t_eth_mac *eth_mac)
{
  int i, result = -1;
  char path[MAX_PATH_LEN];
  char nspace[MAX_PATH_LEN];
  char *ns=nspace;
  char cmd[MAX_PATH_LEN];

  memset(path, 0, MAX_PATH_LEN);
  memset(nspace, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/ns/net", pid);
  if (access(path, F_OK))
    KERR("ERROR %s not found bad pid", path);
  else
    {
    snprintf(nspace, MAX_PATH_LEN-1, "clons%s", name);
    for (i=0; i<strlen(nspace); i++)
      nspace[i] = tolower(nspace[i]);
    check_netns_and_clean(name, nspace, vm_id, nb_eth);
    snprintf(cmd, MAX_PATH_LEN-1,
    "%s netns attach %s %d", IP_BIN, ns, pid); 
    if (execute_cmd(cmd, SYNCHRO))
      KERR("ERROR %s", cmd);
    else
      {
      result = crun_utils_create_veth(vm_id, nspace, nb_eth, eth_mac);
      memset(cmd, 0, MAX_PATH_LEN);
      snprintf(cmd, MAX_PATH_LEN-1, "%s netns delete %s", IP_BIN, ns);
      if (execute_cmd(cmd, SYNCHRO))
        KERR("ERROR %s", cmd);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_create_net(char *image, char *name, char *cnt_dir,
                         char *nspace, int cloonix_rank, int vm_id,
                         int nb_eth, t_eth_mac *eth_mac, char *agent_dir,
                         char *customer_launch)
{
  int result = create_all_dirs(image, cnt_dir, name);
  if (result)
    KERR("ERROR %s %s %s %s", cnt_dir, name, nspace, customer_launch);
  else
    {
    dirs_agent_copy_starter(cnt_dir, name, agent_dir, customer_launch);
    if (nspace_create(name, nspace, cloonix_rank, vm_id, nb_eth, eth_mac))
      {
      KERR("ERROR %s %s %s", cnt_dir, name, nspace);
      result = -1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_delete_net_nspace(char *nspace)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns del %s", IP_BIN, nspace);
  if (execute_cmd(cmd, SYNCHRO))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_delete_overlay(char *name, char *cnt_dir, char *bulk,
                             char *image, int is_persistent)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];

  if (is_persistent == 0)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s %s/%s/%s",
             UMOUNT_BIN, cnt_dir, name, ROOTFS);
    if (execute_cmd(cmd, SYNCHRO))
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
int crun_utils_delete_crun_stop(char *name, int crun_pid)
{
  int pid, result = -1;
  char cmd[MAX_PATH_LEN];
  char *ptr = g_var_crun_log;
  if (crun_pid > 0)
    result = kill(crun_pid, 9);
  else
    {
    pid = read_crun_create_pid(name);
    if (pid > 0)
      result = kill(pid, 9);
    }
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
           "%s --log=%s --root=/var/lib/cloonix/%s/crun delete %s",
           CRUN_BIN, ptr, get_net_name(), name);
  if (execute_cmd(cmd, SYNCHRO))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_create_crun_start(char *name)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char *ptr = g_var_crun_log;
  memset(cmd, 0, MAX_PATH_LEN);
  memset(line, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1,
           "%s --log=%s --root=/var/lib/cloonix/%s/crun start %s",
           CRUN_BIN, ptr, get_net_name(), name);
  if (execute_cmd(cmd, SYNCHRO))
    {
    KERR("ERROR %s", cmd);
    result = -1;
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
void crun_utils_init(char *log)
{
  if (access(FUSE_OVERLAY_FS_BIN, X_OK))
    KERR("ERROR %s", FUSE_OVERLAY_FS_BIN);
  if (access(EXT4FUSE_BIN, X_OK))
    KERR("ERROR %s", EXT4FUSE_BIN);
  if (access(CRUN_BIN, X_OK))
    KERR("ERROR %s", CRUN_BIN);
  if (access(IP_BIN, X_OK))
    KERR("ERROR %s", IP_BIN);
  if (access(MOUNT_BIN, X_OK))
    KERR("ERROR %s", MOUNT_BIN);
  if (access(UMOUNT_BIN, X_OK))
    KERR("ERROR %s", UMOUNT_BIN);
  if (access(LOSETUP_BIN, X_OK))
    KERR("ERROR %s", LOSETUP_BIN);
  if (access(MKNOD_BIN, X_OK))
    KERR("ERROR %s", MKNOD_BIN);
  if (access(CHMOD_BIN, X_OK))
    KERR("ERROR %s", CHMOD_BIN);
  memset(g_var_log, 0, MAX_PATH_LEN);
  memset(g_var_crun_log, 0, MAX_PATH_LEN);
  snprintf(g_var_log,MAX_PATH_LEN-1,"%s/log/%s",log,DEBUG_LOG_SUID);
  snprintf(g_var_crun_log,MAX_PATH_LEN-1,"%s/log/%s",log,DEBUG_LOG_CRUN);
}
/*--------------------------------------------------------------------------*/
