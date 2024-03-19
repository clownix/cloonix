/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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

#include "io_clownix.h"
#include "ovs_execv.h"

extern char **environ;


/*****************************************************************************/
int get_cloonix_listen_fd(void);
int get_cloonix_fd(void);
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *make_cmd_str(char **argv)
{
  int i; 
  static char result[NB_ENV*MAX_ARG_LEN];
  memset(result, 0, NB_ENV*MAX_ARG_LEN);
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
static void add_cmd_to_log(char *ovs_dir, char *argv[])
{
  FILE *fp_log;
  char pth[MAX_ARG_LEN];
  snprintf(pth, MAX_ARG_LEN-1, "%s/log/%s", ovs_dir, DEBUG_LOG_OVS_CMD);
  fp_log = fopen(pth, "a+");
  if (!fp_log) 
    KERR("ERROR %s %s", strerror(errno), make_cmd_str(argv));
  else
    {
    fprintf(fp_log, "\n\n");
    fprintf(fp_log, "%s", make_cmd_str(argv));
    fflush(fp_log);
    fclose(fp_log);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_popen(char *ovs_dir, char *argv[])
{
  int exited_pid, timeout_pid, worker_pid, chld_state, pid, status=97;
  pid_t rc_pid;
  add_cmd_to_log(ovs_dir, argv);
  if ((pid = fork()) < 0)
    KOUT(" ");
  if (pid == 0)
    {
    close(get_cloonix_listen_fd());
    close(get_cloonix_fd());
    worker_pid = fork();

    if (worker_pid == 0)
      {
      execv(argv[0], argv);
      KOUT("FORK error %s", strerror(errno));
      }
    timeout_pid = fork();
    if (timeout_pid == 0)
      {
      sleep(10);
      KERR("WARNING TIMEOUT SLOW CMD 1 %s", make_cmd_str(argv));
      sleep(8);
      KERR("WARNING TIMEOUT SLOW CMD 2 %s", make_cmd_str(argv));
      sleep(8);
      KERR("WARNING TIMEOUT SLOW CMD 3 %s", make_cmd_str(argv));
      sleep(8);
      KERR("ERROR TIMEOUT CMD %s", make_cmd_str(argv));
      exit(1);
      }
    exited_pid = wait(&chld_state);
    if (exited_pid == worker_pid)
      {
      if (WIFEXITED(chld_state))
        status = WEXITSTATUS(chld_state);
      if (WIFSIGNALED(chld_state))
        KERR("Child exited via signal %d\n",WTERMSIG(chld_state));
      kill(timeout_pid, SIGKILL);
      }
    else
      {
      kill(worker_pid, SIGKILL);
      }
    wait(NULL);
    exit(status);
    }
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
int call_ovs_popen(char *ovs_dir, char **argv, int quiet,
                   const char *fct, int line)
{
  static int protect_reentry = 0;
  int result = 0;
  while(protect_reentry)
    {
    KERR("ERROR REENTRY ovs_popen %s %d", fct, line);
    usleep(10000);
    }
  protect_reentry = 1;
  if (ovs_popen(ovs_dir, argv))
    {
    if (quiet == 0)
      KERR("ERROR  %s %d %s", fct, line, make_cmd_str(argv));
    else if (quiet == 2)
      KERR("ERROR OVS COMMAND %s %d %s", fct, line, make_cmd_str(argv));
    result = -1;
    }
  protect_reentry = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void init_environ(char *net, char *ovs_bin, char *ovs_dir)
{
  setenv("OVS_BINDIR",  ovs_bin, 1);
  setenv("OVS_RUNDIR",  ovs_dir, 1);
  setenv("OVS_LOGDIR",  ovs_dir, 1);
  setenv("OVS_DBDIR",   ovs_dir, 1);
  setenv("CLOONIX_NET", net, 1);
}
/*---------------------------------------------------------------------------*/


