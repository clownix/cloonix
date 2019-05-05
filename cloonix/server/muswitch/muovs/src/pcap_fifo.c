/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <linux/types.h>
#include <sys/inotify.h>

#include "ioc.h"
#include "pcap_fifo.h"

static int g_wd, g_fdnotify, g_llid_notify;
static int g_fd;
static char g_path_file[2*MAX_PATH_LEN];

typedef struct pcap_hdr_s
{
  __u32 magic_number;
  __u16 version_major;
  __u16 version_minor;
  __u32 thiszone;
  __u32 sigfigs;
  __u32 snaplen;
  __u32 network;
} pcap_hdr_s;

typedef struct pcaprec_hdr_s
{
  __u32 ts_sec;
  __u32 ts_usec;
  __u32 incl_len;
  __u32 orig_len;
} pcaprec_hdr_s; 

static void pcap_close_and_reinit(t_all_ctx *all_ctx);
static void pcap_fifo_start_end(t_all_ctx *all_ctx);

/****************************************************************************/
static void reset_all_fd(void)
{
  g_fd = -1;
  g_wd = -1;
  g_fdnotify = -1;
  g_llid_notify = 0;
}
/*---------------------------------------------------------------------------*/

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
        pcap_fifo_start_end(all_ctx);
        }
      else if (event->mask & IN_CLOSE)
        {
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
static void inotify_upon_open(t_all_ctx *all_ctx, char *path_file)
{
  g_fdnotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (g_fdnotify == -1)
    KOUT("%s", strerror(errno));
  g_wd = inotify_add_watch(g_fdnotify, path_file, IN_OPEN | IN_CLOSE);
  if (g_wd == -1)
    KOUT("%s", strerror(errno));
  g_llid_notify = msg_watch_fd(all_ctx, g_fdnotify, rx_inot, err_inot);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void pcap_fifo_start_begin(t_all_ctx *all_ctx)
{
  g_fd = -1;
  unlink(g_path_file);
  if (mkfifo(g_path_file, 0666) == -1)
    KOUT("%s", strerror(errno));
  inotify_upon_open(all_ctx, g_path_file);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void pcap_fifo_start_end(t_all_ctx *all_ctx)
{
  pcap_hdr_s hdr_s;
  if (g_fd == -1)
    {
    g_fd = open(g_path_file, O_WRONLY);
    if (g_fd < 0)
      KOUT("%s", strerror(errno));
    memset(&hdr_s, 0, sizeof(pcap_hdr_s));
    hdr_s.magic_number   = 0xa1b2c3d4;
    hdr_s.version_major  = 2;
    hdr_s.version_minor  = 4;
    hdr_s.thiszone       = 0;
    hdr_s.sigfigs        = 0;
    hdr_s.snaplen        = 65535;
    hdr_s.network        = 1;
    if (write(g_fd,(void *)&hdr_s,sizeof(pcap_hdr_s))!= sizeof(pcap_hdr_s))
      KOUT("%s", strerror(errno));
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void pcap_close_and_reinit(t_all_ctx *all_ctx)
{
  if (g_fd != -1)
    close(g_fd);
  if (g_wd != -1)
    inotify_rm_watch(g_fdnotify, g_wd);
  if (g_llid_notify != 0)
    msg_delete_channel(all_ctx, g_llid_notify);
  if (g_wd != -1)
    close(g_wd);
  if (g_fdnotify != -1)
    close(g_fdnotify);
  reset_all_fd();
  pcap_fifo_start_begin(all_ctx);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_fifo_rx_packet(t_all_ctx *all_ctx, long long usec, int len, char *buf)
{
  int ln, hlen = sizeof(pcaprec_hdr_s);
  pcaprec_hdr_s rec_hdr_s;
  if (g_fd != -1)
    {
    memset(&rec_hdr_s, 0, sizeof(pcaprec_hdr_s));
    rec_hdr_s.ts_sec = usec/1000000;
    rec_hdr_s.ts_usec = usec%1000000;
    rec_hdr_s.incl_len = len;
    rec_hdr_s.orig_len = len;
    ln = write(g_fd, (void *)&rec_hdr_s, hlen);
    if (ln != hlen)
      {
      pcap_close_and_reinit(all_ctx);
      KERR("%d %d %s", ln, hlen, strerror(errno));
      }
    else
      {
      if (write(g_fd, (void *) buf, len) != len)
        {
        pcap_close_and_reinit(all_ctx);
        KERR("%s", strerror(errno));
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_fifo_init(t_all_ctx *all_ctx, char *dpdk_dir, char *name)
{
  reset_all_fd();
  memset(g_path_file, 0, 2*MAX_PATH_LEN);
  snprintf(g_path_file, 2*MAX_PATH_LEN-1, "%s_snf/%s", dpdk_dir, name);
  pcap_fifo_start_begin(all_ctx);
}
/*--------------------------------------------------------------------------*/
