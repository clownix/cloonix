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


#define MAX_PACKETS_BEFORE_AUTO_CLOSE 20000000

static int g_fd;
static int g_stop_req;
static int g_count_packets;
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
static void close_and_reinit(t_all_ctx *all_ctx)
{
  g_stop_req = 1;
  if (g_fd)
    close(g_fd);
  g_fd = 0;
  g_count_packets = 0;
  send_config_modif(all_ctx);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int pcap_file_is_recording(t_all_ctx *all_ctx)
{
  int result = 0;
  if (g_fd)
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_file_rx_packet(t_all_ctx *all_ctx, long long usec, int len, char *buf)
{
  int hlen = sizeof(pcaprec_hdr_s);
  pcaprec_hdr_s rec_hdr_s;
  DOUT(all_ctx, FLAG_HOP_DIAG, "%s %d",  __FUNCTION__, len);
  if ((g_stop_req==0) && g_fd)
    {
    g_count_packets += 1;
    memset(&rec_hdr_s, 0, sizeof(pcaprec_hdr_s));
    rec_hdr_s.ts_sec = usec/1000000;
    rec_hdr_s.ts_usec = usec%1000000;
    rec_hdr_s.incl_len = len;
    rec_hdr_s.orig_len = len;
    if (write(g_fd, (void *)&rec_hdr_s,hlen) != hlen)
      {
      close_and_reinit(all_ctx);
      KERR("%d", errno);
      DOUT(all_ctx, FLAG_HOP_DIAG, "%s ERROR WRITE: %d", __FUNCTION__, errno);
      }
    else
      {
      if (write(g_fd, (void *) buf, len) != len)
        {
        close_and_reinit(all_ctx);
        KERR("%d", errno);
        DOUT(all_ctx, FLAG_HOP_DIAG, "%s SHORT WRITE: %d", __FUNCTION__, errno);
        }
      else if (g_count_packets > MAX_PACKETS_BEFORE_AUTO_CLOSE)
        {
        close_and_reinit(all_ctx);
        KERR("%d", g_count_packets);
        DOUT(all_ctx, FLAG_HOP_DIAG, "%s FILE FULL: %d", __FUNCTION__, g_count_packets);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int pcap_file_start(t_all_ctx *all_ctx)
{
  int result = -1;
  pcap_hdr_s hdr_s;
  DOUT(all_ctx, FLAG_HOP_DIAG, "%s",  __FUNCTION__);
  if (g_path_file[0] == 0)
    KERR("%s", "PATH_FILE NOT INITIALIZED");
  else if (g_fd == 0)
    {
    g_stop_req = 0;
    g_fd = open(g_path_file, O_CREAT|O_TRUNC|O_WRONLY, 0666);
    if (g_fd < 0)
      {
      KERR("%d", errno);
      g_fd = 0;
      }
    else
      {
      memset(&hdr_s, 0, sizeof(pcap_hdr_s));
      hdr_s.magic_number   = 0xa1b2c3d4;
      hdr_s.version_major  = 2;
      hdr_s.version_minor  = 4;
      hdr_s.thiszone       = 0;
      hdr_s.sigfigs        = 0;
      hdr_s.snaplen        = 65535;
      hdr_s.network        = 1;
      if (write(g_fd,(void *)&hdr_s,sizeof(pcap_hdr_s))!= sizeof(pcap_hdr_s))
        {
        close_and_reinit(all_ctx);
        KERR("%d", errno);
        }
      else
        result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int pcap_file_end(t_all_ctx *all_ctx)
{
  int result = -1;
  DOUT(all_ctx, FLAG_HOP_DIAG, "%s",  __FUNCTION__);
  if (g_fd)
    result = 0;
  close_and_reinit(all_ctx);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int pcap_file_set_path(t_all_ctx *all_ctx, char *path)
{
  int result = -1;
  DOUT(all_ctx, FLAG_HOP_DIAG, "%s",  __FUNCTION__);
  if (g_fd == 0)
    {
    strcpy(g_path_file, path);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *pcap_file_get_path(t_all_ctx *all_ctx)
{
  DOUT(all_ctx, FLAG_HOP_DIAG, "%s",  __FUNCTION__);
  return(g_path_file);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void pcap_file_init(t_all_ctx *all_ctx, char *net_name, char *name)
{
  g_fd = 0;
  g_count_packets = 0;
  memset(g_path_file, 0, MAX_PATH_LEN);
}
/*--------------------------------------------------------------------------*/
