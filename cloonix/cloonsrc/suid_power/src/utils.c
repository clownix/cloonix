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
#include <sys/prctl.h>

#include "io_clownix.h"
#include "config_json.h"

#define MAX_ARGS_POPEN 100
#define MAX_CMD_POPEN 5000


static char g_var_log[MAX_PATH_LEN];
static char g_var_crun_log[MAX_PATH_LEN];
static char g_var_crun_json[MAX_PATH_LEN];


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
int my_mkdir(char *dst_dir)
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

/*****************************************************************************/
char *get_var_crun_log(void)
{
  return g_var_crun_log;
}
/*---------------------------------------------------------------------------*/

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
  strncpy(cpcmd, cmd, MAX_CMD_POPEN - 10);
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
    KERR("ERROR open of file %s %d", file_name, errno);
  return result;
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
static char *format_buf_mount(char *image, char *rootfs, char *mountbear,
                              char *mounttmp, char *vmount, char *brandtype)
{
  int i;
  char *self_rootfs_var_etc;
  char *buf_mount, *ptrs, *ptre;
  char vmountunit[MAX_VMOUNT][MAX_PATH_LEN];
  char vmountjson[MAX_VMOUNT][4*MAX_PATH_LEN];
  int len_mount = strlen(CONFIG_JSON_MOUNT) +
           MAX_VMOUNT*(strlen(CONFIG_JSON_MOUNT_ITEM)+(2*MAX_PATH_LEN));
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
         CONFIG_JSON_LIBMOD, mountbear, self_rootfs_var_etc, mounttmp);
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
void crun_utils_startup_env(char *mountbear, char *startup_env,
                            int nb_vwif, char *mac, int nb_eth)
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
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path,MAX_PATH_LEN-1, "%s/cnf_fs/startup_nb_vwif", mountbear);
  memset(line, 0, MAX_NAME_LEN);
  snprintf(line, MAX_NAME_LEN-1, "%d", nb_vwif);
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
void utils_init(char *log)
{
  if (access(pthexec_iw_bin(), X_OK))
    KERR("ERROR %s", pthexec_iw_bin());
  if (access(pthexec_lsmod_bin(), X_OK))
    KERR("ERROR %s", pthexec_lsmod_bin());
  if (access(pthexec_modprobe_bin(), X_OK))
    KERR("ERROR %s", pthexec_modprobe_bin());
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
  snprintf(g_var_log, MAX_PATH_LEN-1, "%s/%s/%s",
           log, LOG_DIR, DEBUG_LOG_SUID);
  snprintf(g_var_crun_log, MAX_PATH_LEN-1,"%s/%s/%s",
           log, LOG_DIR, DEBUG_LOG_CRUN);
  snprintf(g_var_crun_json,MAX_PATH_LEN-1,"%s/%s/%s",
           log, LOG_DIR, DEBUG_LOG_JSON);
  my_mkdir(g_var_crun_json);
}
/*--------------------------------------------------------------------------*/



