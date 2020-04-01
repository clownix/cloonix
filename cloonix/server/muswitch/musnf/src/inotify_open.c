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
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>

#include "ioc.h"
#include "pcap_file.h"

static int g_wd, g_fdnotify;

/****************************************************************************/
static void inotify_read_event(t_all_ctx *all_ctx, int llid, int fdnotify)
{
  char buffer[2048];
  struct inotify_event *event = NULL;
  int len;
  if (fdnotify != g_fdnotify)
    KERR("%d %d", fdnotify, g_fdnotify);
  len = read(fdnotify, buffer, sizeof(buffer));
  if (len < 0)
    KERR("%s", strerror(errno));
  else
    {
    event = (struct inotify_event *) buffer;
    while(event != NULL)
      {
      if (event->mask & IN_OPEN)
        {
        pcap_file_start_end(all_ctx);
        }
      else if (event->mask & IN_CLOSE)
        {
        msg_delete_channel(all_ctx, llid);
        inotify_rm_watch(g_fdnotify, g_wd); 
        close(g_wd);
        close(g_fdnotify);
        pcap_close_and_reinit(all_ctx);
        }
      else
        KERR("Unknown Mask 0x%.8x\n", event->mask);
      len -= sizeof(*event) + event->len;
      if (len > 0)
        event = ((void *) event) + sizeof(event) + event->len;
      else
        event = NULL;
      }
    }
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
static int rx_inot(void *ptr, int llid, int fd)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  inotify_read_event(all_ctx, llid, fd);
  return 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void err_inot(void *ptr, int llid, int err, int from)
{
  int is_blkd;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
  KOUT("%d", llid);
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
void inotify_upon_open(t_all_ctx *all_ctx, char *path_file)
{
  g_fdnotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (g_fdnotify == -1)
    KOUT("%s", strerror(errno));
  g_wd = inotify_add_watch(g_fdnotify, path_file, IN_OPEN | IN_CLOSE);
  if (g_wd == -1)
    KOUT("%s", strerror(errno));
  msg_watch_fd(all_ctx, g_fdnotify, rx_inot, err_inot);
}
/*---------------------------------------------------------------------------*/
