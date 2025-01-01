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
/*---------------------------------------------------------------------------*/
#include "mdl.h"
#include "wrap.h"
#include "cli_lib.h"
#include "debug.h"
#include "io_clownix.h"
#include "doorways_sock.h"
#include "epoll_hooks.h"
#include "cloonix_conf_info.h"
#include "xdoors.h"
#include "getsock.h"

/*---------------------------------------------------------------------------*/
static t_cloonix_conf_info g_cloonix_conf_info;

//static int g_action;
//static char *g_src, *g_dst, *g_cmd;

/****************************************************************************/

/****************************************************************************/
static char *init_local_cloonix_bin_path(char *callbin)
{
  static char cloonix_root_tree[MAX_PATH_LEN];
  char curdir[MAX_PATH_LEN];
  char path[2*MAX_PATH_LEN];
  char *ptr;
  memset(cloonix_root_tree, 0, MAX_PATH_LEN);
  memset(path, 0, 2*MAX_PATH_LEN);
  memset(curdir, 0, MAX_PATH_LEN);
  if (!getcwd(curdir, MAX_PATH_LEN-1))
    KOUT(" ");
  if (callbin[0] == '/')
    snprintf(path, MAX_PATH_LEN, "%s", callbin);
  else if ((callbin[0] == '.') && (callbin[1] == '/'))
    snprintf(path, 2*MAX_PATH_LEN, "%s/%s", curdir, &(callbin[2]));
  else
    KOUT("%s", callbin);
  path[MAX_PATH_LEN-1] = 0;
  ptr = strrchr(path, '/');
  if (!ptr)
    KOUT("%s", path);
  *ptr = 0;
  ptr = strrchr(path, '/');
  if (!ptr)
    KOUT("%s", path);
  *ptr = 0;
  strncpy(cloonix_root_tree, path, MAX_PATH_LEN-1);
  if (strcmp(cloonix_root_tree, "/usr/libexec/cloonix"))
    KOUT("ERROR %s", cloonix_root_tree);
  if (access(XWYCLI_BIN, X_OK))
    KOUT("ERROR %s", path);
  return cloonix_root_tree;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void usage(void)
{
  printf("\ncloonix_xwy <cloonix_name>");
  printf("\ncloonix_xwy <cloonix_name> -cmd <params>");
  printf("\ncloonix_xwy <cloonix_name> -get <distant-file> <local-dir>");
  printf("\ncloonix_xwy <cloonix_name> -put <local-file> <distant-dir>");
  printf("\n\n");
  exit(1);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
int main (int argc, char *argv[])
{
  int i, action=action_bash;
  char *src=NULL, *dst=NULL, *cmd=NULL;
  int nb_cloon;
  t_cloonix_conf_info *cloonix_conf;
  t_cloonix_conf_info *cnf;
  if (argc < 3)
    KOUT("%d", argc);
  if (cloonix_conf_info_init(argv[1]))
    KOUT("%s", argv[1]);
  if (argc > 3)
    {
    if (get_input_params_cloon(argc-3, argv+3, &action, &src, &dst, &cmd))
      usage();
    }
  DEBUG_INIT(0);
  init_local_cloonix_bin_path(argv[0]);
  doorways_sock_init(1);
  msg_mngt_init("ctrl", IO_MAX_BUF_LEN);
  cloonix_conf_info_get_all(&nb_cloon, &cloonix_conf);
  for (i=0; i<nb_cloon; i++)
    {
    if (!strcmp(argv[2], cloonix_conf[i].name))
      {
      memcpy(&g_cloonix_conf_info, &(cloonix_conf[i]), 
             sizeof(t_cloonix_conf_info));
      g_cloonix_conf_info.doors_llid = 0;
      cnf = &g_cloonix_conf_info;
      xdoors_connect_init(cnf->name, cnf->ip, cnf->port,
                          cnf->passwd, action, cmd, src, dst);
      break;
      }
    }
  if (i == nb_cloon)
    {
    printf("\nCloon name %s not found in cloonix_config\n", argv[2]);
    usage();
    }
  msg_mngt_loop();
  return 0;
}
/*---------------------------------------------------------------------------*/

