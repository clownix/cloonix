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


#include "ioc.h"
#include "packet_arp_mangle.h"
#include "tun_tap.h"
#include "sock_fd.h"

/*--------------------------------------------------------------------------*/
static int glob_ifindex;
static char glob_wif_hwaddr[6];
static int g_llid_wif;
static int g_fd_wif;
static int g_count_reopen_wif_tries;
static char g_wif_name[MAX_NAME_LEN];
static struct sockaddr_ll wif_sockaddr;
static socklen_t wif_socklen;
/*--------------------------------------------------------------------------*/
static int rx_from_wif(void *ptr, int llid, int fd);
static void err_wif (void *ptr, int llid, int err, int from);
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_glob_wif_hwaddr(t_all_ctx *all_ctx)
{
  (void) all_ctx;
  return glob_wif_hwaddr;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void init_wif_sockaddr(int is_tx)
{
  memset(&wif_sockaddr, 0, sizeof(struct sockaddr_ll));
  wif_sockaddr.sll_family = AF_PACKET;
  wif_sockaddr.sll_protocol = htons(ETH_P_ALL);
  wif_sockaddr.sll_ifindex = glob_ifindex;
  wif_socklen = sizeof(struct sockaddr_ll);
  if (is_tx)
    wif_sockaddr.sll_pkttype = PACKET_OUTGOING;
  else
    wif_sockaddr.sll_pkttype = PACKET_HOST;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int bind_raw_sock(int fd)
{
  int result;
  init_wif_sockaddr(0);
  result = bind(fd, (struct sockaddr*) &wif_sockaddr, wif_socklen);
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int get_intf_hwaddr(t_all_ctx *all_ctx, char *name)
{
  int result = -1, s, io;
  struct ifreq ifr;
  s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s <= 0)
    KERR("Error %s line %d errno %d\n",__FUNCTION__,__LINE__,errno);
  else
    {
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    io = ioctl (s, SIOCGIFHWADDR, &ifr);
    if(io != 0)
      KERR("Error %s line %d errno %d\n",__FUNCTION__,__LINE__,errno);
    else
      {
      if (set_intf_flags_iff_up(name))
        KERR("Error %s line %d err %d\n",__FUNCTION__,__LINE__,errno);
      else
        {
        memcpy(glob_wif_hwaddr, ((char *)&(ifr.ifr_hwaddr.sa_data)), 6);
        result = 0;
        }
      }
    close(s);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_intf_ifindex(t_all_ctx *all_ctx, char *name)
{
  int result = -1, s, io;
  struct ifreq ifr;
  s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s <= 0)
    KERR("Error %s line %d errno %d\n",__FUNCTION__,__LINE__,errno);
  else
    {
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, name, IFNAMSIZ);
    io = ioctl (s, SIOCGIFINDEX, &ifr);
    if(io != 0)
      KERR("Error %s line %d errno %d\n",__FUNCTION__,__LINE__,errno);
    else
      {
      glob_ifindex = ifr.ifr_ifindex;
      result = 0;
      }
    close(s);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int wif_raw_socket_open(t_all_ctx *all_ctx)
{
  int result = 0;
  if (g_llid_wif)
    KOUT(" ");
  result = get_intf_ifindex(all_ctx, g_wif_name);
  if (!result)
    {
    result = get_intf_hwaddr(all_ctx, g_wif_name);
    if (!result)
      {
      g_fd_wif = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
      if ((g_fd_wif < 0) || (g_fd_wif >= MAX_SELECT_CHANNELS-1))
        KOUT("%d", g_fd_wif);
      if (bind_raw_sock(g_fd_wif))
        KOUT("%d %d", g_fd_wif, errno);
      g_llid_wif = msg_watch_fd(all_ctx,g_fd_wif,rx_from_wif,err_wif);
      nonblock_fd(g_fd_wif);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_reopen_wif(t_all_ctx *all_ctx, void *data)
{
  if (wif_raw_socket_open(all_ctx))
    {
    g_count_reopen_wif_tries += 1;
    clownix_timeout_add(all_ctx, 100, timer_reopen_wif, NULL, NULL, NULL);
    }
  if (g_count_reopen_wif_tries > 20)
    KERR(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void try_to_reopen_wif_raw_socket(t_all_ctx *all_ctx)
{
  g_count_reopen_wif_tries = 0;
  clownix_timeout_add(all_ctx, 100, timer_reopen_wif, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void wif_fd_tx(t_all_ctx *all_ctx, t_blkd *blkd)
{
  int len, fd;
  fd = get_fd_with_llid(all_ctx, g_llid_wif);
  if (fd < 0)
   KOUT(" ");
  else if (g_fd_wif != fd)
    KOUT("%d %d", g_fd_wif, fd);
  else
    {
    init_wif_sockaddr(1);
    if (!packet_arp_mangle(all_ctx, 1, blkd->payload_len, blkd->payload_blkd))
      {
      len = sendto(fd, blkd->payload_blkd, blkd->payload_len, 0,
                      (struct sockaddr *)&(wif_sockaddr),
                      wif_socklen);
      if(blkd->payload_len != len)
        KERR("%d %d", blkd->payload_len, len);
      }
    else 
      KERR("%d", blkd->payload_len);
    }
  blkd_free((void *)all_ctx, blkd);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void err_wif (void *ptr, int llid, int err, int from)
{
  int is_blkd;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
  if (llid == g_llid_wif)
    {
    KERR(" ");
    g_llid_wif = 0;
    try_to_reopen_wif_raw_socket(all_ctx);
    }
  else
    KOUT("%d %d", llid, g_llid_wif);
}
/*-------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_from_wif(void *ptr, int llid, int fd)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  t_blkd *bd;
  char *data;
  int len;
  init_wif_sockaddr(0);
  bd = blkd_create_tx_empty(0,0,0);
  data = bd->payload_blkd;
  len = recvfrom(fd, data, PAYLOAD_BLKD_SIZE, 0, 
                 (struct sockaddr *)&(wif_sockaddr), &wif_socklen);
  if (len == 0)
    KOUT(" ");
  if (len < 0)
    {
    if ((errno == EAGAIN) || (errno ==EINTR))
      len = 0;
    else
      KOUT("%d ", errno);
    blkd_free(ptr, bd);
    }
  else 
    {
    if (((wif_sockaddr.sll_pkttype == PACKET_HOST) ||
         (wif_sockaddr.sll_pkttype == PACKET_BROADCAST) ||
         (wif_sockaddr.sll_pkttype == PACKET_MULTICAST) ||
         (wif_sockaddr.sll_pkttype == PACKET_OTHERHOST)) &&
        (wif_sockaddr.sll_ifindex == glob_ifindex))
      {
      if (!packet_arp_mangle(all_ctx, 0, len, data))
        {
        if (llid != g_llid_wif)
          KOUT(" ");
        bd->payload_len = len;
        sock_fd_tx(all_ctx, bd);
        }
      else
        blkd_free(ptr, bd);
      }
    else
      {
      blkd_free(ptr, bd);
      }
    }
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  wif_fd_open(t_all_ctx *all_ctx, char *name)
{ 
  int result = -1;
  strncpy(g_wif_name, name, MAX_NAME_LEN-1);
  if (!wif_raw_socket_open(all_ctx))
    result = 0;
  else
    KERR("%s", name);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void wif_fd_init(t_all_ctx *all_ctx)
{
  init_packet_arp_mangle(all_ctx);
  glob_ifindex = 0;
  g_llid_wif = 0;
  memset(glob_wif_hwaddr, 0, 6 * sizeof(char));
  memset(g_wif_name, 0, MAX_NAME_LEN);
  memset(&wif_sockaddr, 0, sizeof(struct sockaddr_ll));
  wif_socklen = 0;
  g_fd_wif = -1;
}
/*---------------------------------------------------------------------------*/
