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
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "io_clownix.h"


static void close_all(void)
{
  int i, llid, nb_chan;
  job_for_select_close_fd();
  clownix_timeout_del_all();
  nb_chan = get_clownix_channels_nb();
  for (i=1; i<=nb_chan; i++)
    {
    llid = channel_get_llid_with_cidx(i);
    if (llid)
      {
      if (msg_exist_channel(llid))
        msg_delete_channel(llid);
      }
    }
  for (i=3; i<1024; i++)
    close(i);
}



int clownix_system (char * commande)
{
  pid_t pid;
  int   status;
  char *environ[] = { NULL };
  char * argv [4];
  if (commande == NULL)
    return (1);
  if ((pid = fork ()) < 0)
    return (-1);
  if (pid == 0) 
    {
    close_all();
    argv[0] = "/bin/bash";
    argv[1] = "-c";
    argv[2] = commande;
    argv[3] = NULL;
    execve ("/bin/bash", argv, environ);
    exit (127);
    }
  while (1) 
    {
    if (waitpid (pid, &status, 0) == -1) 
      return (-1);
    else 
      return (status);
    }
}









