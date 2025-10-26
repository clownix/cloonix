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
#include <errno.h>
#include <signal.h>
#include <linux/prctl.h>
#include <sys/prctl.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "utils_cmd_line_maker.h"
#include "uml_clownix_switch.h"

void lio_clean_all_llid(void);

static int  g_wif_pipe[2];
static int g_pid;

/****************************************************************************/
int *get_wif_pipe(void)
{
  int *result = NULL;
  if (g_wif_pipe[0] != -1)
    KERR("ERROR");
  else if (g_wif_pipe[1] != -1)
    KERR("ERROR");
  else if (pipe(g_wif_pipe))
    KERR("ERROR");
  else if (g_wif_pipe[1] == STDOUT_FILENO)
    KERR("ERROR");
  else if (g_wif_pipe[1] == STDERR_FILENO)
    KERR("ERROR");
  else
    result = g_wif_pipe;
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int vwifi_server_launch(void)
{
  char *argv[5];
  char *cmd = pthexec_cloonfs_vwifi_server();
  char line[MAX_PATH_LEN];
  int *pdes = get_wif_pipe();
  FILE *iop = NULL;
  int pid, pidwif = 0;
  int llid, nb_chan, i; 

  if (access(cmd, X_OK))
    KERR("ERROR %s", cmd);
  else if (pdes == NULL)
    KERR("ERROR %s", cmd);
  else if ((pidwif = fork()) < 0)
    KERR("ERROR %s", cmd);
  else
    {
    argv[0] = cmd;
    argv[1] = cfg_get_root_work();
    argv[2] = NULL;
    if (pidwif == 0)
      {
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
      if ((pid = fork()) < 0)
        KOUT("ERROR %s", cmd);
      (void)dup2(pdes[1], STDOUT_FILENO);
      (void)dup2(pdes[1], STDERR_FILENO);
      lio_clean_all_llid();
      if (pid == 0)
        {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        iop = fdopen(pdes[0], "r");
        if (iop == NULL)
          KOUT("ERROR");
        while( 1 )
          {
          if (fgets(line, MAX_PATH_LEN-1, iop) != NULL)
            KERR("INFO VWIFI: %s", line);
          }
        }
      execv(argv[0], argv);
      KOUT("ERROR FORK %s %s", strerror(errno), cmd);
      }
    }
  return pidwif;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void vwifi_end(void)
{
  if (g_wif_pipe[0] != -1)
    close(g_wif_pipe[0]);
  if (g_wif_pipe[1] != -1)
    close(g_wif_pipe[1]);
  g_wif_pipe[0] = -1;
  g_wif_pipe[1] = -1;
  if (g_pid)
    kill(g_pid, SIGKILL);
  g_pid = 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void vwifi_init(void)
{
  g_wif_pipe[0] = -1;
  g_wif_pipe[1] = -1;
  g_pid = vwifi_server_launch();
}
/*--------------------------------------------------------------------------*/

