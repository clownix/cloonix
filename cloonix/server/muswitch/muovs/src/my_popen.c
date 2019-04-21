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
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "ioc.h"

#include "ovs_execv.h"

#define CLOONIX_CMD_LOG  "cloonix_ovs_req.log"

/*****************************************************************************/
int get_cloonix_listen_fd(void);
int get_cloonix_fd(void);
/*---------------------------------------------------------------------------*/

static char *g_environ[NB_ENV];

/*****************************************************************************/
static char *make_cmd_str(char **argv)
{
  int i; 
  static char result[10*MAX_ARG_LEN];
  memset(result, 0, 10*MAX_ARG_LEN);
  for (i=0;  (argv[i] != NULL); i++)
    {
    strcat(result, argv[i]);
    if (strlen(result) >= 8*MAX_ARG_LEN)
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
static void add_cmd_to_log(char *dpdk_db_dir, char *argv[])
{
  FILE *fp_log;
  char pth[MAX_ARG_LEN];
  snprintf(pth, MAX_ARG_LEN-1, "%s/%s", dpdk_db_dir, CLOONIX_CMD_LOG);
  fp_log = fopen(pth, "a+");
  if (!fp_log) 
    KERR("%s", strerror(errno));
  fprintf(fp_log, "\n\n");
  fprintf(fp_log, "%s", make_cmd_str(argv));
  fflush(fp_log);
  fclose(fp_log);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int my_popen(char *dpdk_db_dir, char *argv[], char *env[],
                    int *child_pid)
{
  int chld_state, pid, status=97;
  pid_t rc_pid;
  if ((pid = fork()) < 0)
    KOUT(" ");
  if (pid == 0)
    {
    close(get_cloonix_listen_fd());
    close(get_cloonix_fd());
    execve(argv[0], argv, env);
    KOUT("FORK error %s", strerror(errno));
    }
  *child_pid = pid;
  add_cmd_to_log(dpdk_db_dir, argv);
  rc_pid = waitpid(pid, &chld_state, 0);
  if (rc_pid > 0)
    {
    if (WIFEXITED(chld_state))
      status = WEXITSTATUS(chld_state);
    if (WIFSIGNALED(chld_state))
      KERR("Child exited via signal %d\n",WTERMSIG(chld_state));
    }
  else
    {
    if (errno != ECHILD)
      KERR("Unexpected error %s", strerror(errno));
    }
  return status;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int call_my_popen(char *dpdk_db_dir, int nb, char arg[NB_ARG][MAX_ARG_LEN])
{
  int i, child_pid, result = 0;
  char *argv[NB_ARG];
  memset(argv, 0, NB_ARG * sizeof(char *));
  for (i=0; i<nb; i++)
    argv[i] = arg[i];
  if (my_popen(dpdk_db_dir, argv, g_environ, &child_pid))
    {
    KERR("%s %d", make_cmd_str(argv), child_pid);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void init_environ(char *dpdk_db_dir)
{
  int i;
  static char env[NB_ENV][MAX_ENV_LEN];
  memset(env, 0, NB_ENV * MAX_ENV_LEN * sizeof(char));
  memset(g_environ, 0, NB_ENV * sizeof(char *));
  snprintf(env[0], MAX_ENV_LEN-1, "OVS_BINDIR=%s", dpdk_db_dir);
  snprintf(env[1], MAX_ENV_LEN-1, "OVS_RUNDIR=%s", dpdk_db_dir);
  snprintf(env[2], MAX_ENV_LEN-1, "OVS_LOGDIR=%s", dpdk_db_dir);
  snprintf(env[3], MAX_ENV_LEN-1, "OVS_DBDIR=%s", dpdk_db_dir);
  snprintf(env[4], MAX_ENV_LEN-1, "XDG_RUNTIME_DIR=%s", dpdk_db_dir);
  for (i=0; i<NB_ENV-1; i++)
    g_environ[i] = env[i];
}
/*---------------------------------------------------------------------------*/


