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
#include <sys/time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "launcher.h"

static char *g_saved_environ[4];

/****************************************************************************/
void clean_all_llid(void)
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

/****************************************************************************/
static char **get_saved_environ(void)
{
  return (g_saved_environ);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void set_saved_environ(char *username, char *home, char *spice_env)
{
  g_saved_environ[0] = username;
  g_saved_environ[1] = home;
  g_saved_environ[2] = spice_env;
  g_saved_environ[3] = NULL;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int launcher(char *argv[])
{
  char **env = get_saved_environ();
  int exited_pid, timeout_pid, worker_pid, chld_state, pid, status=77;
  pid_t rc_pid;
  if ((pid = fork()) < 0)
    KOUT(" ");
  if (pid == 0)
    {
    clean_all_llid();
    worker_pid = fork();
    if (worker_pid == 0)
      {
      execve(argv[0], argv, env);
      KOUT("FORK error %s", strerror(errno));
      }
    timeout_pid = fork();
    if (timeout_pid == 0)
      {
      sleep(3);
      KERR("EXIT MAIN SUIDPOWER");
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
      KERR("KILL MAIN SUIDPOWER");
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
    KERR("END WAIT MAIN SUIDPOWER");
    } 
  return status;
}
/*--------------------------------------------------------------------------*/

