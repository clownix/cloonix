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
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>


#include "mdl.h"
#include "wrap.h"
#include "io_clownix.h"
#include "doorways_sock.h"
#include "epoll_hooks.h"


#include "cli_lib.h"
#include "scp.h"
#include "getsock.h"
#include "low_write.h"
#include "debug.h"
#include "xdoors.h"
#include "x11_cli.h"


/****************************************************************************/
static void usage(char *name)
{
  printf("\n%s <ip> <port>", name);
  printf("\n%s <ip> <port> -cmd <params>", name);
  printf("\n%s <ip> <port> -dae <params>", name);
  printf("\n%s <ip> <port> -get <distant-file> <local-dir>", name);
  printf("\n%s <ip> <port> -put <local-file> <distant-dir>", name);
  printf("\n");
  printf("\n -dae daemonizes the command, -cmd has a controlling term");
  printf("\n\n");
  exit(1);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int main(int argc, char **argv)
{
  int action, ip, port;
  char *cmd, *src, *dst;

  DEBUG_INIT(0);
  if (argc < 2)
    usage(argv[0]);
  if (get_input_params(argc, argv, &action, &ip, &port, &src, &dst, &cmd))
    usage(argv[0]);

  if (action == action_cmd)
    printf("\n-cmd %s\n", cmd);
  else if (action == action_crun)
    printf("\n-crun %s\n", cmd);
  else if (action == action_dae)
    printf("\n-dae %s\n", cmd);
  else if (action == action_get)
    printf("\n-get %s %s\n", src, dst);
  else if (action == action_put)
    printf("\n-put %s %s\n", src, dst);

  doorways_sock_init();

  xdoors_connect_init(ip, port, "password", action, cmd, src, dst);

  msg_mngt_loop();

  return 0;
}
/*--------------------------------------------------------------------------*/
