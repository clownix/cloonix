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
#include "tar_img.h"
#include "crun_utils.h"

#define MAX_ARGS_POPEN 100
#define MAX_CMD_POPEN 5000

#define CGROUP_SYS "--cgroup-manager=systemd"
#define CGROUP_DIS "--cgroup-manager=disabled"

char *get_net_name(void);
char *get_bin_dir(void);

static char g_var_log[MAX_PATH_LEN];
static char g_var_crun_log[MAX_PATH_LEN];
static char g_var_crun_json[MAX_PATH_LEN];


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
static FILE *lio_popen(char *argv[], int *child_pid, int uid)
{
  int exited_pid, timeout_pid, worker_pid, wstatus, pid, status;
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
      if (uid)
        {
        if (setgid(uid))
          KERR("ERROR uid %d  %s", uid, lio_linear(argv));
        if (setuid(uid))
          KERR("ERROR uid %d  %s", uid, lio_linear(argv));
        }
      execv(argv[0], argv);
      KOUT("ERROR FORK %s %s", strerror(errno), lio_linear(argv));
      }
    timeout_pid = fork();
    if (timeout_pid == 0)
      {
      sleep(6);
      KERR("WARNING TIMEOUT SLOW CMD 1 %s", lio_linear(argv));
      exit(1);
      }
    exited_pid = wait(&wstatus);
    if (exited_pid == worker_pid)
      {
      status = -1;
      if (WIFEXITED(wstatus))
        {
        status = WEXITSTATUS(wstatus);
        }
      else
        KERR("WARNING CMD %s", lio_linear(argv));
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

/*****************************************************************************/
FILE *cmd_lio_popen(char *cmd, int *child_pid, int uid)
{
  static char *argv[MAX_ARGS_POPEN];
  char cpcmd[MAX_CMD_POPEN];
  FILE *iop = NULL;
  if (!cmd)
    KOUT("ERROR");
  if (strlen(cmd) == 0)
    KOUT("ERROR");
  if (strlen(cmd) >= MAX_CMD_POPEN)
    KOUT("ERROR %lu", strlen(cmd));
  log_write_req(cmd);
  strncpy(cpcmd, cmd, MAX_CMD_POPEN - 1);
  strcat(cpcmd, " 2>&1");
  argv[0] = pthexec_bash_bin();
  argv[1] = "-c";
  argv[2] = cpcmd;
  argv[3] = NULL;
  iop = lio_popen(argv, child_pid, uid);
  return iop;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int execute_cmd(char *cmd, int do_log, int uid)
{
  int child_pid, wstatus, result = -1;
  FILE *fp = cmd_lio_popen(cmd, &child_pid, uid);
  if (fp == NULL)
    KERR("ERROR %s", cmd);
  else
    {
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    result = wstatus;    
    if (result)
      {
      if (do_log)
        log_write_req("PREVIOUS CMD KO 1");
      }
    pclose(fp);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int write_whole_file(char *file_name, char *buf, int len)
{
  int result = -1;
  int fd, writelen;
  char cmd[MAX_PATH_LEN];
  if (!access(file_name, R_OK))
    {
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
static void format_startup_env(char *startup_env, char *env)
{
  char *ptr_start, *ptr;
  int i, nb = 0;
  char tmp[2*MAX_NAME_LEN];
  memset(env, 0, MAX_PATH_LEN+MAX_NAME_LEN);
  ptr = startup_env;
  if (strlen(ptr))
    {
    nb = 1;
    while(ptr)
      {
      ptr = strchr(ptr, ' ');
      if (ptr)
        {
        nb += 1;
        ptr += 1;
        }
      }
    ptr_start = startup_env;
    for (i=0; i<nb; i++)
      {
      ptr = strchr(ptr_start, ' ');
      if (ptr)
        {
        *ptr = 0;
        ptr += 1;
        }
      memset(tmp, 0, 2*MAX_NAME_LEN);
      snprintf(tmp, 2*MAX_NAME_LEN-1, ",\"%s\"", ptr_start);
      strcat(env, tmp);
      ptr_start = ptr;
      }
    }
  else
    strcat(env, " ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void extract_vmount(char *vmount,
                           char vmountunit[MAX_VMOUNT][MAX_PATH_LEN])
{
  char *ptrs = vmount;
  char *ptre;
  int i = 0;
  memset(vmountunit, 0, MAX_VMOUNT * MAX_PATH_LEN);
  while (strlen(ptrs))
    {
    ptre = strchr(ptrs, ' ');
    if (ptre)
      *ptre = 0;
    strncpy(vmountunit[i], ptrs, MAX_PATH_LEN);
    i += 1;
    if ((i == MAX_VMOUNT) || (ptre == NULL))
      break;
    ptrs = ptre + 1;
    }
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int directory_exists(char *path)
{
  struct stat sb;
  int result = 0;
  if (lstat(path, &sb) == 0) 
    {
    if ((sb.st_mode & S_IFMT) == S_IFDIR)
      result = 1;
    }
  return result; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *format_buf_mount(char *image, char *rootfs, char *mountbear,
                              char *mounttmp, char *vmount, char *brandtype)
{
  int i;
  char *self_rootfs_var_etc;
  char *libmod = " ";
  char *buf_mount, *ptrs, *ptre;
  char vmountunit[MAX_VMOUNT][MAX_PATH_LEN];
  char vmountjson[MAX_VMOUNT][4*MAX_PATH_LEN];
  char path_libmod[MAX_PATH_LEN];
  int len_mount = strlen(CONFIG_JSON_MOUNT) +
           MAX_VMOUNT*(strlen(CONFIG_JSON_MOUNT_ITEM)+(2*MAX_PATH_LEN)); 
  memset(path_libmod, 0, MAX_PATH_LEN);
  snprintf(path_libmod, MAX_PATH_LEN-1, "%s/lib/modules", rootfs);
  if (directory_exists(path_libmod))
    libmod = CONFIG_JSON_LIBMOD;
  extract_vmount(vmount, vmountunit);
  for (i=0; i<MAX_VMOUNT; i++) 
    {
    memset(vmountjson[i], 0, 4*MAX_PATH_LEN);
    strcpy(vmountjson[i], " ");
    }
  for (i=0; i<MAX_VMOUNT; i++) 
    {
    ptrs = vmountunit[i];
    if (!strlen(ptrs))
      {
      break;
      }
    else
      {
      ptre = strchr(vmountunit[i], ':');
      if (!ptre)
        {
        KERR("WARNING PARAM VMOUNT %d %s", i, vmountunit[i]);
        break;
        }
      else
        {
        *ptre = 0;
        ptre += 1;
        if (!strlen(ptre))
          {
          KERR("WARNING PARAM VMOUNT %d %s", i, vmountunit[i]);
          break;
          }
        else
          {
          snprintf(vmountjson[i], 4*MAX_PATH_LEN - 1,
                   CONFIG_JSON_MOUNT_ITEM, ptre, ptrs);
          }
        }
      }
    }
  if (!strcmp(image, "self_rootfs"))
    self_rootfs_var_etc = CONFIG_JSON_SELF_ROOTFS;
  else
    self_rootfs_var_etc = " ";
  buf_mount = (char *) malloc(len_mount);
  memset(buf_mount, 0, len_mount);
  snprintf(buf_mount, len_mount-1,  CONFIG_JSON_MOUNT,
         vmountjson[0], vmountjson[1], vmountjson[2], vmountjson[3],
         vmountjson[4], vmountjson[5], vmountjson[6], vmountjson[7],
         libmod, mountbear, self_rootfs_var_etc, mounttmp);
  return buf_mount;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *get_config_json(char *image, char *rootfs, char *nspacecrun,
                             char *mountbear, char *mounttmp,
                             int is_persistent, int is_privileged,
                             char *brandtype,
                             char *startup_env, char *vmount,
                             char *name, int ovspid, int uid_image,
                             char *sbin_init, char *sh_starter,
                             int subuid_start, int subuid_qty,
                             int subgid_start, int subgid_qty)
{
  char *buf, *buf_mount, *buf_uid_gid;
  char sbin_init_starter[MAX_PATH_LEN];
  int len = strlen(CONFIG_MAIN_JSON) +
            strlen(CONFIG_JSON_MOUNT) +
            strlen(UID_GID_MAPPING) +
            strlen(NAMESPACES_PRIVILEGED) +
            (5*strlen(CONFIG_JSON_CAPA)) + strlen(CONFIG_JSON_MOUNT) +
            (4*strlen(CONFIG_JSON_MOUNT_ITEM)) + 10*MAX_PATH_LEN;
  char env[MAX_PATH_LEN+MAX_NAME_LEN];
  char log_json[MAX_PATH_LEN];
  char *namespaces, *memory;
  int len_uid_gid = 2 * strlen(UID_GID_MAPPING);

  if (is_privileged)
    {
    namespaces = NAMESPACES_PRIVILEGED;
    memory = " ";
    }
  else
    {
    namespaces = NAMESPACES_NOT_PRIVILEGED;
    memory = MEMORY_NOT_LIMITED;
    }
  memset(sbin_init_starter, 0, MAX_PATH_LEN);
  memset(log_json, 0, MAX_PATH_LEN);
  snprintf(log_json, MAX_PATH_LEN-1, "%s/%s", g_var_crun_json, name); 
  snprintf(sbin_init_starter, MAX_PATH_LEN-1,
           "\"%s\", \"-c\", \"%s\"", sh_starter, sbin_init);
  format_startup_env(startup_env, env);
  buf_mount = format_buf_mount(image, rootfs, mountbear, mounttmp,
                               vmount, brandtype);
  buf = (char *) malloc(len);
  buf_uid_gid = (char *) malloc(len_uid_gid); 
  memset(buf, 0, len);
  memset(buf_uid_gid, 0, len_uid_gid);
  if ((is_privileged == 0) && (uid_image) &&
      (subuid_start && subuid_qty && subgid_start && subgid_qty))
    {
    snprintf(buf_uid_gid, len_uid_gid-1, UID_GID_MAPPING,
             0, uid_image, 1, 1, subuid_start, subuid_qty,
             0, uid_image, 1, 1, subgid_start, subgid_qty);
    }
  else
    {
    snprintf(buf_uid_gid, len_uid_gid-1, " ");
    }
  snprintf(buf, len-1, CONFIG_MAIN_JSON, 0, 0, sbin_init_starter, env,
                                    CONFIG_JSON_CAPA, CONFIG_JSON_CAPA,
                                    CONFIG_JSON_CAPA, CONFIG_JSON_CAPA,
                                    CONFIG_JSON_CAPA, rootfs, buf_uid_gid,
                                    namespaces, memory, buf_mount);

  write_whole_file(log_json, buf, strlen(buf));
  free(buf_mount);
  free(buf_uid_gid);
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
static int dirs_agent_create_mnt_tmp(char *mountbear, char *mounttmp)
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
  if (dirs_agent_create_mnt_tmp(mountbear, mounttmp))
    KERR("ERROR %s %s", mountbear, mounttmp);
  else
    {
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
  char *ptr = g_var_crun_log;
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
  char *ptr = g_var_crun_log;
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
      KERR("ERROR Dir: %s could not be deleted\n", dir);
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
    if (dirs_agent_create_mnt_tmp(mountbear, mounttmp))
      KERR("ERROR %s %s", mountbear, mounttmp);
    else if (dirs_agent_copy_starter(mountbear, agent_dir))
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
        if (strcmp(image, "self_rootfs"))
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
  len += sprintf(cmd+len, " --log=%s", g_var_crun_log); 
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
void crun_utils_startup_env(char *mountbear, char *startup_env, int nb_eth)
{     
  char path[MAX_PATH_LEN];
  char line[MAX_NAME_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path,MAX_PATH_LEN-1, "%s/cnf_fs/startup_env", mountbear);
  write_whole_file(path, startup_env, strlen(startup_env));
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path,MAX_PATH_LEN-1, "%s/cnf_fs/startup_nb_eth", mountbear);
  memset(line, 0, MAX_NAME_LEN);
  snprintf(line, MAX_NAME_LEN-1, "%d", nb_eth);
  write_whole_file(path, line, strlen(line));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int crun_utils_create_config_json(char *image, char *path, char *rootfs,
                                 char *nspacecrun,
                                 char *mountbear, char *mounttmp,
                                 int is_persistent, int is_privileged,
                                 char *brandtype,
                                 char *startup_env, char *vmount,
                                 char *name, int ovspid, int uid_image,
                                 char *sbin_init, char *sh_starter,
                                 int subuid_start, int subuid_qty,
                                 int subgid_start, int subgid_qty)
{
  int len, result;
  char json_path[MAX_PATH_LEN];
  char tmp_json_path[MAX_PATH_LEN];
  char *buf = get_config_json(image, rootfs, nspacecrun, mountbear, mounttmp,
                              is_persistent, is_privileged, brandtype,
                              startup_env, vmount, name, ovspid,
                              uid_image, sbin_init, sh_starter,
                              subuid_start, subuid_qty,
                              subgid_start, subgid_qty);
  memset(json_path, 0, MAX_PATH_LEN);
  snprintf(json_path, MAX_PATH_LEN-1, "%s/config.json", path); 
  len = strlen(buf);
  result = write_whole_file(json_path, buf, len);
  memset(tmp_json_path, 0, MAX_PATH_LEN);
  sprintf(tmp_json_path, "/tmp/config.json.%s", name); 
  write_whole_file(tmp_json_path, buf, len); 
  free(buf);
  if (access(json_path, R_OK))
    KERR("ERROR %s", json_path);
  return result;
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
  char *ptr = g_var_crun_log;
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
                               int cloonix_rank, int vm_id, int nb_eth,
                               t_eth_mac *eth_mac)
{
  char cmd[MAX_PATH_LEN];
  int result;
  char *ptr = g_var_crun_log;
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
  usleep(200000);
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
  char *ptr = g_var_crun_log;
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
  char *ptr = g_var_crun_log;
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
  char *ptr = g_var_crun_log;
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
  if (access(pthexec_fusezip_bin(), X_OK))
    KERR("ERROR %s", pthexec_fusezip_bin());
  if (access(pthexec_crun_bin(), X_OK))
    KERR("ERROR %s", pthexec_crun_bin());
  if (access(pthexec_ip_bin(), X_OK))
    KERR("ERROR %s", pthexec_ip_bin());
  if (access(pthexec_mount_bin(), X_OK))
    KERR("ERROR %s", pthexec_mount_bin());
  if (access(pthexec_umount_bin(), X_OK))
    KERR("ERROR %s", pthexec_umount_bin());
  if (access(pthexec_mknod_bin(), X_OK))
    KERR("ERROR %s", pthexec_mknod_bin());
  if (access(pthexec_chmod_bin(), X_OK))
    KERR("ERROR %s", pthexec_chmod_bin());
  memset(g_var_log, 0, MAX_PATH_LEN);
  memset(g_var_crun_log, 0, MAX_PATH_LEN);
  snprintf(g_var_log,MAX_PATH_LEN-1,"%s/log/%s",log,DEBUG_LOG_SUID);
  snprintf(g_var_crun_log,MAX_PATH_LEN-1,"%s/log/%s",log,DEBUG_LOG_CRUN);
  snprintf(g_var_crun_json,MAX_PATH_LEN-1,"%s/log/%s",log,DEBUG_LOG_JSON);
  my_mkdir(g_var_crun_json);
}
/*--------------------------------------------------------------------------*/
