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
#include "cirspy.h"


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



typedef struct t_wpcap
{
  char path[MAX_PATH_LEN];
  int fd;
  int wd;
} t_wpcap;

static t_wpcap *g_wpcap[CIRC_MAX_TAB + 1];
static int g_wd2idx[MAX_SELECT_CHANNELS];
static char g_dpdk_dir[MAX_PATH_LEN];

static int g_fdnotify;
static int g_llid_notify;


/*****************************************************************************/
static void wpcap_start_begin(int idx)
{
  int fd;
  t_wpcap *cur = g_wpcap[idx];
  if (cur == NULL)
    KERR("%d", idx);
  else if (strlen(cur->path) == 0)
    KERR("%d", idx);
  else
    {
    unlink(cur->path);
    if (mkfifo(cur->path, 0666) == -1)
      KERR("%s", strerror(errno));
    else if (cur->wd != -1)
      KERR("%d %d", idx, cur->wd);
    else
      {
      fd = inotify_add_watch(g_fdnotify, cur->path, IN_OPEN | IN_CLOSE);
      if (fd < 0)
        KERR("%s", strerror(errno));
      else if (fd >= MAX_SELECT_CHANNELS)
        {
        KERR("%d", fd);
        inotify_rm_watch(g_fdnotify, fd);
        close(fd);
        }
      else if (g_wd2idx[fd] != 0)
        {
        KERR("%d", fd);
        inotify_rm_watch(g_fdnotify, fd);
        close(fd);
        }
      else
        {
        cur->wd = fd;
        g_wd2idx[fd] = idx;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void wpcap_start_end(int idx)
{
  int fd;
  pcap_hdr_s hdr_s;
  t_wpcap *cur = g_wpcap[idx];
  if (cur == NULL)
    KERR("%d", idx);
  else if (cur->fd != -1)
    KERR("%d %d", idx, cur->fd);
  else if (strlen(cur->path) == 0)
    KERR("%d", idx);
  else
    {
    fd = open(cur->path, O_WRONLY);
    if (fd < 0)
      KERR("%s", strerror(errno));
    else
      {
      cur->fd = fd;
      memset(&hdr_s, 0, sizeof(pcap_hdr_s));
      hdr_s.magic_number   = 0xa1b2c3d4;
      hdr_s.version_major  = 2;
      hdr_s.version_minor  = 4;
      hdr_s.thiszone       = 0;
      hdr_s.sigfigs        = 0;
      hdr_s.snaplen        = 65535;
      hdr_s.network        = 1;
      if (write(fd,(void *)&hdr_s,sizeof(pcap_hdr_s))!= sizeof(pcap_hdr_s))
        KERR("%s", strerror(errno));
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void helper_wpcap_close(t_wpcap *cur)
{
  if (cur->fd != -1)
    close(cur->fd);
  if (cur->wd == -1)
    KERR(" ");
  else
    {
    g_wd2idx[cur->wd] = 0;
    inotify_rm_watch(g_fdnotify, cur->wd);
    close(cur->wd);
    }
  cur->fd = -1;
  cur->wd = -1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_inot(void *ptr, int llid, int fdnotify)
{
  char buffer[2048];
  struct inotify_event *event = NULL;
  int len, idx;
  t_wpcap *cur;
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
      idx = g_wd2idx[event->wd];
      if ((idx <= 0) || (idx > CIRC_MAX_TAB))
        KERR("%d", idx);
      else
        {
        cur = g_wpcap[idx];
        if (cur == NULL)
          KERR("%d", idx);
        else if(cur->wd != event->wd)
          KERR("%d %d %d", idx, event->wd, cur->wd);
        else
          {
          if (event->mask & IN_OPEN)
            {
            wpcap_start_end(idx);
            }
          else if (event->mask & IN_CLOSE)
            {
            helper_wpcap_close(cur);
            wpcap_start_begin(idx);
            }
          else
            KERR("Unknown Mask 0x%.8x\n", event->mask);
          }
        }
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
void pcap_fifo_rx_packet(int idx, long long usec, int len, char *buf)
{
  int ln, hlen = sizeof(pcaprec_hdr_s);
  pcaprec_hdr_s rec_hdr_s;
  t_wpcap *cur;
  if ((idx <= 0) || (idx > CIRC_MAX_TAB))
    KOUT("%d", idx);
  cur = g_wpcap[idx];
  if (cur == NULL)
    KERR("%d %d", idx, len);
  else
    {
    if (cur->fd != -1)
      {
      memset(&rec_hdr_s, 0, sizeof(pcaprec_hdr_s));
      rec_hdr_s.ts_sec = usec/1000000;
      rec_hdr_s.ts_usec = usec%1000000;
      rec_hdr_s.incl_len = len;
      rec_hdr_s.orig_len = len;
      ln = write(cur->fd, (void *)&rec_hdr_s, hlen);
      if (ln != hlen)
        {
        KERR("%d %d %s", ln, hlen, strerror(errno));
        helper_wpcap_close(cur);
        wpcap_start_begin(idx);
        }
      else
        {
        if (write(cur->fd, (void *) buf, len) != len)
          {
          KERR("%s", strerror(errno));
          helper_wpcap_close(cur);
          wpcap_start_begin(idx);
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_fifo_open(int idx, char *lan)
{
  t_wpcap *cur;
  char pth[2*MAX_PATH_LEN];
  if ((idx <= 0) || (idx > CIRC_MAX_TAB))
    KOUT("%d", idx);
  cur = g_wpcap[idx];
  if (cur != NULL)
    KERR("%d %s", idx, lan);
  else
    {
    cur = (t_wpcap *) malloc(sizeof(t_wpcap));
    memset(cur, 0, sizeof(t_wpcap));
    memset(pth, 0, 2*MAX_PATH_LEN);
    snprintf(pth, 2*MAX_PATH_LEN-1, "%s_snf/%s", g_dpdk_dir, lan);
    memcpy(cur->path, pth, MAX_PATH_LEN-1);
    cur->fd = -1;
    cur->wd = -1;
    g_wpcap[idx] = cur;
    wpcap_start_begin(idx);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_fifo_close(int idx)
{
  t_wpcap *cur;
  if ((idx <= 0) || (idx > CIRC_MAX_TAB))
    KOUT("%d", idx);
  cur = g_wpcap[idx];
  if (cur == NULL)
    KERR("%d", idx);
  else
    {
    helper_wpcap_close(cur);
    free(cur);
    g_wpcap[idx] = NULL;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_fifo_init(t_all_ctx *all_ctx, char *dpdk_dir)
{
  memset(g_wpcap, 0, (CIRC_MAX_TAB + 1) * sizeof(t_wpcap *));
  memset(g_wd2idx, 0, MAX_SELECT_CHANNELS * sizeof(int));
  memset(g_dpdk_dir, 0, MAX_PATH_LEN);
  memcpy(g_dpdk_dir, dpdk_dir, MAX_PATH_LEN-1);
  g_fdnotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (g_fdnotify == -1)
    KOUT("%s", strerror(errno));
  g_llid_notify = msg_watch_fd(all_ctx, g_fdnotify, rx_inot, err_inot);
  if ((g_llid_notify <= 0) || (g_llid_notify >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", g_llid_notify);
}
/*--------------------------------------------------------------------------*/
