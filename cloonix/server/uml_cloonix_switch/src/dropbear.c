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
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"


typedef struct t_dropbear_params
{
  char bin[MAX_PATH_LEN];
  char unix_sock[MAX_PATH_LEN];
  char pid_file[MAX_PATH_LEN];
  char cloonix_tree[MAX_PATH_LEN];
} t_dropbear_params;


static void launch_dropbear(void);
static t_dropbear_params g_dropbear_params;
static int g_dropbear_killed_requested;

/*****************************************************************************/
/*
VIP
static void dump_dropbear_creation_info(t_dropbear_params *dp)
{
  char info[MAX_PRINT_LEN];
  char *argv[] = {dp->bin,
                  dp->unix_sock,
                  dp->pid_file,
                  dp->cloonix_tree,
                  NULL};
  utils_format_gene("CREATION", info, "dropbear", argv);
  KERR("%s", info);
}
*/
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int start_dropbear(void *data)
{
  t_dropbear_params *dp = (t_dropbear_params *) data;
  char *argv[] = {dp->bin, 
                  dp->unix_sock, 
                  dp->pid_file,  
                  dp->cloonix_tree,
                  NULL};
//VIP
  execv(dp->bin, argv);
  return 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void launch_dropbear(void)
{
  if (g_dropbear_killed_requested == 0)
    {
//VIP
//    dump_dropbear_creation_info(&g_dropbear_params);
    pid_clone_launch(start_dropbear, NULL, NULL,
                    (void *) (&g_dropbear_params),
                    NULL, NULL, "dropbear", -1, 1);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int check_pid_is_dropbear(int pid)
{
  int result = 0;
  int len, fd;
  char path[MAX_PATH_LEN];
  char *ptr;
  static char cmd[MAX_BIG_BUF];
  sprintf(path, "/proc/%d/cmdline", pid);
  fd = open(path, O_RDONLY);
  if (fd > 0)
    {
    memset(cmd, 0, MAX_BIG_BUF);
    len = read(fd, cmd, MAX_BIG_BUF-2);
    ptr = cmd;
    if (len > 0)
      {
      while ((ptr = memchr(cmd, 0, len)) != NULL)
        *ptr = ' ';
      if (strstr(cmd, "dropbear_cloonix_sshd"))
        result = 1;
      }
    if (close(fd))
      KOUT("%d", errno);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_dropbear_pid(char *path)
{
  FILE *fhd;
  int pid;
  int result = 0;
  fhd = fopen(path, "r");
  if (fhd)
    {
    if (fscanf(fhd, "%d", &pid) == 1)
      {
      if (check_pid_is_dropbear(pid))
        result = pid;
      }
    if (fclose(fhd))
      KOUT("%d", errno);
    }
return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_monitor_dropbear_pid(void *data)
{
  int pid;
  if (g_dropbear_killed_requested == 0)
    {
    pid = get_dropbear_pid(g_dropbear_params.pid_file);
    if (!pid)
      {
      KERR();
      launch_dropbear();
      }
    clownix_timeout_add(2000, timer_monitor_dropbear_pid, NULL, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void kill_dropbear(void)
{
  int pid = get_dropbear_pid(g_dropbear_params.pid_file);
  g_dropbear_killed_requested = 1;
  if (pid)
    {
    kill(pid, SIGTERM);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dropbear_pid(void)
{
  int pid = get_dropbear_pid(g_dropbear_params.pid_file);
  return pid;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void init_dropbear(void)
{
  memset(&g_dropbear_params, 0, sizeof(t_dropbear_params));
  snprintf(g_dropbear_params.bin, MAX_PATH_LEN-1, 
           "%s/common/agent_dropbear/agent_bin/dropbear_cloonix_sshd", 
           cfg_get_bin_dir());
  snprintf(g_dropbear_params.unix_sock, MAX_PATH_LEN-1, 
           "%s/%s", cfg_get_root_work(), DROPBEAR_SOCK);
  snprintf(g_dropbear_params.pid_file, MAX_PATH_LEN-1,
           "%s/%s", cfg_get_root_work(), DROPBEAR_PID);
  snprintf(g_dropbear_params.cloonix_tree, MAX_PATH_LEN-1, 
           "%s", cfg_get_bin_dir());
  g_dropbear_killed_requested = 0;
  launch_dropbear();
  clownix_timeout_add(2000, timer_monitor_dropbear_pid, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/




