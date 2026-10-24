/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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

static int g_wd = -1, g_fdnotify = -1, g_llid;
static int g_state;

void inotify_trigger_end(void)
{
  if ((g_llid) && (msg_exist_channel(g_llid)))
    msg_delete_channel(g_llid);
  else if (g_fdnotify != -1)
    close(g_fdnotify);
  g_llid = 0;
  g_wd = -1;
  g_fdnotify = -1;
  pcap_record_close();
  g_state = 0;
}

void close_pcap_record(void)
{
  inotify_trigger_end();
}

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
        /* 0: no reader; 1: a reader opened, this end opens (its own open
           is the next IN_OPEN); 2: writing. */
        if ((g_state == 0) || ((g_state == 2) && (!pcap_record_is_writing())))
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
          KTRC("another reader opened the capture fifo, which is already "
               "writing");
          }
        }
      else if (event->mask & IN_CLOSE_NOWRITE)
        {
        /* Reader gone: close so the next gets a fresh header; it may have opened already, probe. */
        pcap_record_close_writer();
        g_state = 0;
        if (pcap_record_probe_reader())
          g_state = 1;
        }
      else if (event->mask & IN_IGNORED)
        {
        KERR("WARNING INOTIFY IN_IGNORE");
        }
      else
        KERR("ERROR INOTIFY Unknown Mask 0x%.8x", event->mask);
      len -= sizeof(*event) + event->len;
      if (len > 0)
        event = (struct inotify_event *)
                ((char *) event + sizeof(*event) + event->len);
      else
        event = NULL;
      }
    }
  return 0;
}

static void err_inot(int llid, int err, int from)
{
  KERR("ERROR %d", llid);
  inotify_trigger_end();
}

int inotify_get_state(void)
{
  return g_state;
}

void inotify_trigger_init(char *path_file)
{
  if ((g_fdnotify != -1) || (g_llid))
    {
    KERR("WARNING inotify re-armed for %s while an instance was still "
         "there, releasing it first", path_file);
    inotify_trigger_end();
    }
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
    g_wd = inotify_add_watch(g_fdnotify, path_file,
                             IN_OPEN | IN_CLOSE_NOWRITE);
    if (g_wd == -1)
      {
      KERR("ERROR inotify watch on %s: %s", path_file, strerror(errno));
      close(g_fdnotify);
      g_fdnotify = -1;
      }
    else
      {
      g_llid = msg_watch_fd(g_fdnotify, rx_inot, err_inot, "inotify_trigger");
      if (!g_llid)
        {
        KERR("ERROR inotify fd for %s could not be watched", path_file);
        inotify_rm_watch(g_fdnotify, g_wd);
        close(g_fdnotify);
        g_fdnotify = -1;
        g_wd = -1;
        }
      }
    }
}
