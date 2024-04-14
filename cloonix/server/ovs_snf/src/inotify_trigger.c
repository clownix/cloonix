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
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
 
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "pcap_record.h"
#include "inotify_trigger.h"

static int g_wd, g_fdnotify, g_llid;
static int g_state;


/****************************************************************************/
void close_pcap_record(void)
{
  if (g_llid)
    {
    if (msg_exist_channel(g_llid))
      msg_delete_channel(g_llid);
    g_llid = 0;
    }
  if ((g_fdnotify != -1) && (g_wd != -1))
    inotify_rm_watch(g_fdnotify, g_wd); 
  if (g_wd != -1)
    {
    close(g_wd);
    g_wd = -1;
    }
  if (g_fdnotify != -1)
    {
    close(g_fdnotify); 
    g_fdnotify = -1;
    }
  pcap_record_close();
  g_state = 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_inot(int llid, int fd)
{
  char buffer[2048];
  struct inotify_event *event = NULL;
  int len;
  if (fd != g_fdnotify)
    KERR("ERROR %d %d", fd, g_fdnotify);
  if (llid != g_llid)
    KERR("ERROR %d %d", llid, g_llid);
  len = read(fd, buffer, sizeof(buffer));
  if (len < 0)
    KERR("ERROR %s", strerror(errno));
  else
    {
    event = (struct inotify_event *) buffer;
    while(event != NULL)
      {
      if (event->mask & IN_OPEN)
        {
        if (g_state == 0)
          {
          pcap_record_start_phase2();
          g_state = 1;
          }
        else if (g_state == 1)
          {
          g_state = 2;
          }
        else
          {
          KERR("ERROR INOTIFY IN_OPEN");
          }
        }
      else if (event->mask & IN_CLOSE)
        {
        close_pcap_record();
        }
      else if (event->mask & IN_IGNORED)
        {
        KERR("WARNING INOTIFY IN_IGNORE");
        }
      else
        KERR("ERROR INOTIFY Unknown Mask 0x%.8x\n", event->mask);
      len -= sizeof(*event) + event->len;
      if (len > 0)
        event = ((void *) event) + sizeof(event) + event->len;
      else
        event = NULL;
      }
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void err_inot(int llid, int err, int from)
{
  KERR("ERROR %d", llid);
  inotify_trigger_end();
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
void inotify_trigger_end(void)
{
  if (msg_exist_channel(g_llid))
    msg_delete_channel(g_llid);
  if ((g_fdnotify != -1) && (g_wd != -1))
    inotify_rm_watch(g_fdnotify, g_wd); 
  if (g_wd != -1)
    close(g_wd);
  if (g_fdnotify != -1)
    close(g_fdnotify);
  g_llid = -1;
  g_wd = -1;
  g_fdnotify = -1;
  pcap_record_close();
  g_state = 0;
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
int inotify_get_state(void)
{
  return g_state;
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
void inotify_trigger_init(char *path_file)
{
  g_state = 0;
  g_fdnotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (g_fdnotify == -1)
    {
    KERR("ERROR %s", strerror(errno));
    KERR("ERROR SPY will not work for %s", path_file);
    KERR("ERROR You can increase fs.inotify.max_user_instances on your system");
    }
  else
    {
    g_wd = inotify_add_watch(g_fdnotify, path_file, IN_OPEN | IN_CLOSE);
    if (g_wd == -1)
      KERR("ERROR %s", strerror(errno));
    else
      g_llid = msg_watch_fd(g_fdnotify, rx_inot, err_inot, "inotify_trigger");
    }
}
/*---------------------------------------------------------------------------*/
