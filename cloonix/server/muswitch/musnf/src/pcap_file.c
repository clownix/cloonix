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

#include "ioc.h"
#include "inotify_open.h"



static int g_fd;
static char g_path_file[MAX_PATH_LEN];


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

void send_config_modif(t_all_ctx *all_ctx);


/*****************************************************************************/
static void pcap_file_start_begin(t_all_ctx *all_ctx)
{
  g_fd = -1;
  unlink(g_path_file);
  if (mkfifo(g_path_file, 0666) == -1)
    KOUT("%s", strerror(errno));
  inotify_upon_open(all_ctx, g_path_file);
  send_config_modif(all_ctx);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_close_and_reinit(t_all_ctx *all_ctx)
{
  if (g_fd != -1)
    close(g_fd);
  pcap_file_start_begin(all_ctx);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int pcap_file_is_recording(t_all_ctx *all_ctx)
{
  int result = 0;
  if (g_fd != -1)
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_file_rx_packet(t_all_ctx *all_ctx, long long usec, int len, char *buf)
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
void pcap_file_start_end(t_all_ctx *all_ctx, int fd)
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
    send_config_modif(all_ctx);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int pcap_file_set_path(t_all_ctx *all_ctx, char *path)
{
  int result = -1;
  if (g_path_file[0] != 0)
    KERR("PATH_FILE ALREADY INITIALIZED to %s", g_path_file);
  else if (g_fd != -1)
    KERR("PROBLEM %s", g_path_file);
  else
    {
    strcpy(g_path_file, path);
    pcap_file_start_begin(all_ctx);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *pcap_file_get_path(t_all_ctx *all_ctx)
{
  return(g_path_file);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_file_init(t_all_ctx *all_ctx, char *net_name, char *name)
{
  g_fd = -1;
  memset(g_path_file, 0, MAX_PATH_LEN);
}
/*--------------------------------------------------------------------------*/
