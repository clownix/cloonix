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
#define MAX_ARG_LEN 400
#define CLOONIX_CMD_LOG  "cloonix_ovs_req.log"

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
int my_popen(char *dpdk_db_dir, char *argv[], char *env[], int *child_pid)
{
  int chld_state, pid, status=97;
  pid_t rc_pid;
  if ((pid = fork()) < 0)
    KOUT(" ");
  if (pid == 0)
    {
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

