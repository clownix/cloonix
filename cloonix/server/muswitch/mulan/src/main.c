/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <signal.h>
#include <sys/types.h>
#include <libgen.h>
#include <stdint.h>

#include "ioc.h"
#include "lib_mulan.h"


/*****************************************************************************/
void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line)
{
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void timeout_rpct_heartbeat(t_all_ctx *all_ctx, void *data)
{
  rpct_heartbeat((void *) all_ctx);
  clownix_timeout_add(all_ctx, 100, timeout_rpct_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_blkd_heartbeat(t_all_ctx *all_ctx, void *data)
{
  blkd_heartbeat((void *) all_ctx);
  clownix_timeout_add(all_ctx, 1, timeout_blkd_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int main (int argc, char *argv[])
{
  char *dirc, *basec, *bname, *dname;
  char spy_path[MAX_PATH_LEN];
  t_all_ctx *all_ctx;
  if (argc != 2)
    KOUT(" ");
  dirc = strdup(argv[1]);
  basec = strdup(argv[1]);
  dname = dirname(dirc);
  bname = basename(basec);
  cloonix_set_pid(getpid());
  all_ctx = msg_mngt_init(bname, 0, IO_MAX_BUF_LEN);
  blkd_set_our_mutype((void *) all_ctx, mulan_type);
  strncpy(all_ctx->g_name, bname, MAX_NAME_LEN-1);  
  strncpy(all_ctx->g_path, argv[1], MAX_PATH_LEN-1);  
  if (strlen(dname) + strlen("spy_") + strlen(bname) + 3 >= MAX_PATH_LEN)
    KOUT("%s/spy_%s", dname, bname);
  memset(spy_path, 0, MAX_PATH_LEN);
  snprintf(spy_path, MAX_PATH_LEN-1, "%s/spy_%s", dname, bname);
  free(dirc);
  free(basec);
  if (string_server_unix(all_ctx, all_ctx->g_path, client_connect) == 0)
    KOUT(" ");
  init_lib_mulan(all_ctx);
  blkd_server_listen((void *) all_ctx, spy_path, spy_traf_connection);
  clownix_timeout_add(all_ctx, 100, timeout_rpct_heartbeat, NULL, NULL, NULL);
  clownix_timeout_add(all_ctx, 100, timeout_blkd_heartbeat, NULL, NULL, NULL);
  msg_mngt_loop(all_ctx);
  return 0;
}
/*--------------------------------------------------------------------------*/



