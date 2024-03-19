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
#include <sys/un.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include "io_clownix.h"
#include "util_sock.h"
#include "llid_xwy.h"
#include "dispach.h"
#include "doorways_sock.h"

#define MAX_XWY_LEN 50000

static char g_buf[MAX_XWY_LEN];
static char g_xwy_path[MAX_PATH_LEN];
static int g_xwy_path_ok;


/*****************************************************************************/
static int xwy_rx_cb(int llid, int fd)
{
  int len;
  t_transfert *ilt;
  ilt = dispach_get_inside_transfert(llid);
  if (!ilt)
    {
    KERR("%d", llid);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
  else
    {
    if (llid != ilt->inside_llid)
      KOUT(" ");
    len = util_read(g_buf, MAX_XWY_LEN, fd);
    if (len == 0)
      KERR(" ");
    else if (len < 0)
      {
      doorways_tx(ilt->dido_llid, 0, ilt->type, doors_val_link_ko, 3, "KO");
      dispach_free_transfert(ilt->dido_llid, ilt->inside_llid);
KERR("free_transfert %d %d", ilt->dido_llid, ilt->inside_llid);
      }
    else
      {
      if (msg_exist_channel(ilt->dido_llid))
        {
        if (doorways_tx(ilt->dido_llid, ilt->inside_llid, ilt->type,
                        doors_val_xwy, len, g_buf))
          {
          doorways_tx(ilt->dido_llid, 0, ilt->type, doors_val_link_ko, 3, "KO");
          dispach_free_transfert(ilt->dido_llid, ilt->inside_llid);
KERR("free_transfert %d %d", ilt->dido_llid, ilt->inside_llid);
          }
        }
      else
        {
        doorways_tx(ilt->dido_llid, 0, ilt->type, doors_val_link_ko, 3, "KO");
        dispach_free_transfert(ilt->dido_llid, ilt->inside_llid);
KERR("free_transfert %d %d", ilt->dido_llid, ilt->inside_llid);
        }
      }
    }
  return len;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int llid_xwy_ctrl(void)
{
  int fd, llid = 0;
  if (g_xwy_path_ok == 1)
    {
    if (!util_nonblock_client_socket_unix(g_xwy_path, &fd))
      {
      if (fd <= 0)
        KOUT(" ");
      llid = msg_watch_fd(fd, xwy_rx_cb, in_err_gene, "xwy");
      if (llid == 0)
        KOUT(" ");
      }
    else
      KERR("COULD NOT CONNECT %s", g_xwy_path);
    }
  else
    KERR("COULD NOT CONNECT NO PATH");
  return llid;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void llid_xwy_connect_info(char *path)
{
  g_xwy_path_ok = 1;
  strncpy(g_xwy_path, path, MAX_PATH_LEN);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void llid_xwy_init(void)
{
  g_xwy_path_ok = 0;
  memset(g_xwy_path, 0, MAX_PATH_LEN);
}
/*--------------------------------------------------------------------------*/


