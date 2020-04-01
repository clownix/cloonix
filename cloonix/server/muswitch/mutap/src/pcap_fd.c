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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <signal.h>
#include <linux/sockios.h>
#include <pcap/pcap.h>


#define PRINT_BYTES_PER_LINE 16


#include "ioc.h"
#include "tun_tap.h"
#include "sock_fd.h"

/*--------------------------------------------------------------------------*/
static int g_ifindex;
static int g_llid_pcap;
static char g_raw_name[MAX_NAME_LEN];
static int g_req_to_restart; 
/*--------------------------------------------------------------------------*/
static int rx_from_pcap(void *ptr, int llid, int fd);
static void err_pcap(void *ptr, int llid, int err, int from);
/*--------------------------------------------------------------------------*/
static pcap_t* g_pcap;
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int get_intf_ifindex(t_all_ctx *all_ctx, char *name)
{
  int result = -1, s, io;
  struct ifreq ifr;
  s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s <= 0)
    KERR("Error %s line %d %s\n", __FUNCTION__, __LINE__, strerror(errno));
  else
    {
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    io = ioctl (s, SIOCGIFINDEX, &ifr);
    if(io != 0)
      KERR("Error %s line %d %s\n", __FUNCTION__, __LINE__, strerror(errno));
    else
      {
      g_ifindex = ifr.ifr_ifindex;
      io = ioctl (s, SIOCGIFFLAGS, &ifr);
      if(io != 0)
        KERR("Error %s line %d %s\n",__FUNCTION__,__LINE__,strerror(errno));
      else
        {
        ifr.ifr_flags |= IFF_PROMISC;
        ifr.ifr_flags |= IFF_UP;
        io = ioctl(s, SIOCSIFFLAGS, &ifr);
        if(io != 0)
          KERR("Error %s line %d %s\n",__FUNCTION__,__LINE__,strerror(errno));
        else
          result = 0;
        }
      }
    close(s);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void pcap_open_llid(t_all_ctx *all_ctx)
{
  struct bpf_program filterprog;
  int fd;
  char errbuf[PCAP_ERRBUF_SIZE];
  if (g_llid_pcap)
    KOUT(" ");
  if (get_intf_ifindex(all_ctx, g_raw_name))
    KOUT("%s", g_raw_name);
  if (g_pcap)
    KOUT("%s", g_raw_name);
  g_pcap = pcap_open_live(g_raw_name, 65535, 1, 0, errbuf);
  if (!g_pcap)
    KOUT("%s", errbuf);
  if(pcap_compile(g_pcap, &filterprog, "inbound", 0, PCAP_NETMASK_UNKNOWN))
    KOUT("%s", g_raw_name);
  if (pcap_setfilter(g_pcap, &filterprog))
    KOUT("%s", g_raw_name);
  fd = pcap_get_selectable_fd(g_pcap);
  if ((fd < 0) || (fd >= MAX_SELECT_CHANNELS-1))
    KOUT("%d %s", fd, strerror(errno));
  g_llid_pcap = msg_watch_fd(all_ctx, fd, rx_from_pcap, err_pcap);
  nonblock_fd(fd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void raw_fd_tx(t_all_ctx *all_ctx, t_blkd *blkd)
{
  if (pcap_sendpacket(g_pcap, (const void *)  blkd->payload_blkd,
                               (size_t) blkd->payload_len))
    KOUT(" ");
  blkd_free((void *)all_ctx, blkd);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void err_pcap(void *ptr, int llid, int err, int from)
{
  int is_blkd;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
  if (llid == g_llid_pcap)
    {
    if (!strlen(g_raw_name))
      KOUT(" ");
    KERR("Something wrong on raw %s", g_raw_name);
    g_req_to_restart = 1; 
    g_llid_pcap = 0;
    }
  else
    KOUT("%d %d", llid, g_llid_pcap);
}
/*-------------------------------------------------------------------------*/

/****************************************************************************/
static char *get_rand_name(void)
{
  static char name[100];
  sprintf(name, "/tmp/debug%04X", rand());
  return name;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void print_data_hex(const uint8_t* data, int size)
{
  FILE *logfile;
  int i, j, offset = 0, nlines = size / PRINT_BYTES_PER_LINE;

  if (nlines * PRINT_BYTES_PER_LINE < size)
      nlines++;

  logfile = fopen(get_rand_name(), "w");
  if (logfile)
    { 
    for (i = 0; i < nlines; i++)
      {
      fprintf(logfile, "%04X    ", offset);
      for (j = 0; j < PRINT_BYTES_PER_LINE; j++)
        {
        if (offset + j >= size)
           fprintf(logfile, "   ");
         else
           fprintf(logfile, "%02X ", data[offset + j]);
        }
      fprintf(logfile, "   ");
      for (j = 0; j < PRINT_BYTES_PER_LINE; j++)
        {
        if (offset + j >= size)
          fprintf(logfile, " ");
        else if(data[offset + j] > 31 && data[offset + j] < 127)
          fprintf(logfile, "%c", data[offset + j]);
        else
          fprintf(logfile, ".");
        }
      offset += PRINT_BYTES_PER_LINE;
      fprintf(logfile, "\n");
      }
    fclose(logfile);
    }
}


/****************************************************************************/
static void fct_pcap_handler(u_char *ptr,
                         const struct pcap_pkthdr *hdr,
                         const u_char *bytes) 
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  t_blkd *bd;
  if (hdr->len != hdr->caplen)
    {
    KERR("DROP len: %d caplen %d", hdr->len, hdr->caplen);
    }
  else if (hdr->len >= PAYLOAD_BLKD_SIZE)
    {
    KERR("DROP len: %d bigger than %d", hdr->len, PAYLOAD_BLKD_SIZE);
    print_data_hex(bytes, hdr->len);
    }
  else
    {
    bd = blkd_create_tx_empty(0,0,0);
    memcpy(bd->payload_blkd, bytes, hdr->len);
    bd->payload_len = hdr->len;
    sock_fd_tx(all_ctx, bd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_from_pcap(void *ptr, int llid, int fd)
{
  if (pcap_dispatch(g_pcap, 1, fct_pcap_handler, (u_char *)ptr) < 0)
    KOUT(" ");
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(t_all_ctx *all_ctx, void *data)
{
   if (g_req_to_restart == 1)
    {
    KERR("Restarting raw sockets on %s", g_raw_name);
    g_req_to_restart = 0; 
    if (g_pcap)
      pcap_close(g_pcap);
    g_pcap = NULL;
    pcap_open_llid(all_ctx);
    }
  clownix_timeout_add(all_ctx, 100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  raw_fd_open(t_all_ctx *all_ctx, char *name)
{ 
  strncpy(g_raw_name, name, MAX_NAME_LEN-1);
  pcap_open_llid(all_ctx);
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void raw_fd_init(t_all_ctx *all_ctx)
{
  g_ifindex = 0;
  g_llid_pcap = 0;
  g_req_to_restart = 0; 
  g_pcap = NULL;
  memset(g_raw_name, 0, MAX_NAME_LEN);
  clownix_timeout_add(all_ctx, 100, timer_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/
