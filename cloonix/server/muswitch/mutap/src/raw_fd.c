/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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


struct vlan_tag {
        uint16_t        vlan_tpid;              /* ETH_P_8021Q */
        uint16_t        vlan_tci;               /* VLAN TCI */
};

# define VLAN_TPID(hdr, hv)     (((hv)->tp_vlan_tpid || ((hdr)->tp_status & TP_STATUS_VLAN_TPID_VALID)) ? (hv)->tp_vlan_tpid : ETH_P_8021Q)

#define VLAN_VALID(hdr, hv)   ((hv)->tp_vlan_tci != 0 || ((hdr)->tp_status & TP_STATUS_VLAN_VALID))
#define VLAN_TAG_LEN	4
#define VLAN_OFFSET     (2 * ETH_ALEN)



#include "ioc.h"
#include "tun_tap.h"
#include "sock_fd.h"

/*--------------------------------------------------------------------------*/
static int g_raw_fd_open;
static int g_ifindex;
static int g_llid_raw;
static int g_fd_raw;
static int g_fd_raw_tx;
static char g_raw_name[MAX_NAME_LEN];
static struct sockaddr_ll g_raw_sockaddr_rx;
static struct sockaddr_ll g_raw_sockaddr_tx;
static socklen_t g_raw_socklen_rx;
static socklen_t g_raw_socklen_tx;
static int g_inhibit_kerr;
/*--------------------------------------------------------------------------*/
static int rx_from_raw(void *ptr, int llid, int fd);
static void err_raw (void *ptr, int llid, int err, int from);
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int bind_raw_sock(int fd)
{
  int result;
  memset(&g_raw_sockaddr_rx, 0, sizeof(struct sockaddr_ll));
  g_raw_sockaddr_rx.sll_family = AF_PACKET;
  g_raw_sockaddr_rx.sll_protocol = htons(ETH_P_ALL);
  g_raw_sockaddr_rx.sll_ifindex = g_ifindex;
  g_raw_sockaddr_rx.sll_pkttype = PACKET_HOST;
  g_raw_socklen_rx = sizeof(struct sockaddr_ll);
  result = bind(fd, (struct sockaddr*) &g_raw_sockaddr_rx, g_raw_socklen_rx); 
  return result;
}
/*---------------------------------------------------------------------------*/

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
      g_ifindex = ifr.ifr_ifindex;
      io = ioctl (s, SIOCGIFFLAGS, &ifr);
      if(io != 0)
        KERR("Error %s line %d errno %d\n",__FUNCTION__,__LINE__,errno);
      else
        {
        ifr.ifr_flags |= IFF_PROMISC;
        ifr.ifr_flags |= IFF_UP;
        io = ioctl(s, SIOCSIFFLAGS, &ifr);
        if(io != 0)
          KERR("Error %s line %d errno %d\n",__FUNCTION__,__LINE__,errno);
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
static int raw_socket_open(t_all_ctx *all_ctx)
{
  int val = 1, result = 0;
  if (g_llid_raw)
    KOUT(" ");
  result = get_intf_ifindex(all_ctx, g_raw_name);
  if (!result)
    {
    g_fd_raw_tx = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    g_fd_raw = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (g_fd_raw_tx < 0)
      KOUT("%d %d", g_fd_raw_tx, errno);
    if ((g_fd_raw < 0) || (g_fd_raw >= MAX_SELECT_CHANNELS-1))
      KOUT("%d %d", g_fd_raw, errno);
    if (bind_raw_sock(g_fd_raw))
      KOUT("%d %d", g_fd_raw, errno);
    if (setsockopt(g_fd_raw, SOL_PACKET, PACKET_AUXDATA, &val, sizeof(val)))
      KOUT("%d %d", g_fd_raw, errno);
    g_llid_raw = msg_watch_fd(all_ctx, g_fd_raw, rx_from_raw, err_raw);
    nonblock_fd(g_fd_raw);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void raw_fd_tx(t_all_ctx *all_ctx, t_blkd *blkd)
{
  int len;
  memset(&g_raw_sockaddr_tx, 0, sizeof(struct sockaddr_ll));
  g_raw_sockaddr_tx.sll_family = AF_PACKET;
  g_raw_sockaddr_tx.sll_protocol = htons(ETH_P_ALL);
  g_raw_sockaddr_tx.sll_ifindex = g_ifindex;
  g_raw_sockaddr_tx.sll_pkttype = PACKET_OUTGOING;
  g_raw_socklen_tx = sizeof(struct sockaddr_ll);
  len = sendto(g_fd_raw_tx, blkd->payload_blkd, blkd->payload_len, 0,
              (struct sockaddr *)&(g_raw_sockaddr_tx), g_raw_socklen_tx);
  if(blkd->payload_len != len)
    KERR("%d %d", blkd->payload_len, len);
  blkd_free((void *)all_ctx, blkd);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void err_raw (void *ptr, int llid, int err, int from)
{
  int is_blkd;
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  if (msg_exist_channel(all_ctx, llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(all_ctx, llid);
  if (llid == g_llid_raw)
    {
    close(g_fd_raw_tx);
    close(g_fd_raw);
    KERR("%d %d", err, from);
    g_ifindex = 0;
    g_llid_raw = 0;
    g_fd_raw = -1;
    g_fd_raw_tx = -1;
    }
  else
    KOUT("%d %d", llid, g_llid_raw);
}
/*-------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_inhibit_kerr(t_all_ctx *all_ctx, void *data)
{
  g_inhibit_kerr = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void purge_raw_sock(void *ptr, int fd)
{
  t_blkd *bd;
  char *data;
  int len;
  bd = blkd_create_tx_empty(0,0,0);
  data = bd->payload_blkd;
  len = recvfrom(fd, data, PAYLOAD_BLKD_SIZE, 0,
                 (struct sockaddr *)&(g_raw_sockaddr_rx),
                 &g_raw_socklen_rx);
  while (len > 0)
    {
    len = recvfrom(fd, data, PAYLOAD_BLKD_SIZE, 0,
                   (struct sockaddr *)&(g_raw_sockaddr_rx),
                   &g_raw_socklen_rx);
    }
  blkd_free(ptr, bd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static struct tpacket_auxdata *look_for_vlan(struct msghdr *msg)
{
  struct cmsghdr *cmsg;
  struct tpacket_auxdata *aux = NULL;
  for (cmsg = CMSG_FIRSTHDR(msg); cmsg; cmsg = CMSG_NXTHDR(msg, cmsg))
    {
    if (cmsg->cmsg_len < CMSG_LEN(sizeof(struct tpacket_auxdata)) ||
                         cmsg->cmsg_level != SOL_PACKET ||
                         cmsg->cmsg_type != PACKET_AUXDATA)
      {
      KERR(" ");
      aux = NULL;
      break;
      }
    else
      {
      aux = (struct tpacket_auxdata *)CMSG_DATA(cmsg);
      if (!VLAN_VALID(aux, aux))
        {
        aux = NULL;
        break;
        }  
      }
    }
  return aux;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_from_raw(void *ptr, int llid, int fd)
{
  t_all_ctx *all_ctx = (t_all_ctx *) ptr;
  t_blkd *bd;
  char *data;
  struct vlan_tag *tag;
  struct tpacket_auxdata *aux;
  int len, queue_size;
  struct iovec            iov;
  struct msghdr           msg;
  union {
        struct cmsghdr  cmsg;
        char   buf[CMSG_SPACE(sizeof(struct tpacket_auxdata))];
        } cmsg_buf;

  if (ioctl(fd, SIOCINQ, &queue_size))
    {
    KERR("DROP");
    return 0;
    }
  if (queue_size > PAYLOAD_BLKD_SIZE - VLAN_TAG_LEN)
    {
    if (g_inhibit_kerr == 0)
      {
      g_inhibit_kerr = 1;
      KERR("DROP  Offloading? Do \"ethtool -K ethx gro/gso off\"");
      clownix_timeout_add(all_ctx, 500, timer_inhibit_kerr, NULL, NULL, NULL);
      }
    purge_raw_sock(ptr, fd);
    return 0;
    }

  bd = blkd_create_tx_empty(0,0,0);
  data = bd->payload_blkd;

  msg.msg_name            = &g_raw_sockaddr_rx;
  msg.msg_namelen         = sizeof(g_raw_sockaddr_rx);
  msg.msg_iov             = &iov;
  msg.msg_iovlen          = 1;
  msg.msg_control         = &cmsg_buf;
  msg.msg_controllen      = sizeof(cmsg_buf);
  msg.msg_flags           = 0;
  iov.iov_len             = PAYLOAD_BLKD_SIZE - VLAN_TAG_LEN;
  iov.iov_base            = data;

  len = recvmsg(fd, &msg, 0);

  if (len == 0)
    KOUT(" ");
  if (len < 0)
    {
    if ((errno != EAGAIN) && (errno != EINTR))
      KOUT("%d ", errno);
    blkd_free(ptr, bd);
    KERR(" ");
    return 0;
    }
  if (len != queue_size)
    {
    blkd_free(ptr, bd);
    KERR("%d %d", len, queue_size);
    return 0;
    }
  if (((g_raw_sockaddr_rx.sll_pkttype != PACKET_HOST)      &&
       (g_raw_sockaddr_rx.sll_pkttype != PACKET_BROADCAST) &&
       (g_raw_sockaddr_rx.sll_pkttype != PACKET_MULTICAST) &&
       (g_raw_sockaddr_rx.sll_pkttype != PACKET_OTHERHOST)) ||
      (g_raw_sockaddr_rx.sll_ifindex != g_ifindex))
    {
    blkd_free(ptr, bd);
    }
  else
    {
    aux = look_for_vlan(&msg);
    if (aux)
      {
      if (len <= VLAN_OFFSET)
        KOUT("%d", len); 
      memmove(data + VLAN_OFFSET + VLAN_TAG_LEN, data + VLAN_OFFSET, len - VLAN_OFFSET);
      tag = (struct vlan_tag *)(data + VLAN_OFFSET);
      tag->vlan_tpid = htons(VLAN_TPID(aux, aux));
      tag->vlan_tci = htons(aux->tp_vlan_tci);
      bd->payload_len = len + VLAN_TAG_LEN;
      }
    else
      bd->payload_len = len;
    sock_fd_tx(all_ctx, bd);
    }
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(t_all_ctx *all_ctx, void *data)
{
  int queue_size;
  if (g_llid_raw)
    {
    if (g_fd_raw == -1)
      KOUT(" ");
    if (ioctl(g_fd_raw, SIOCINQ, &queue_size))
      KERR("IOCTL");
    }
  else if (g_raw_fd_open == 1)
    {
    if (!raw_socket_open(all_ctx))
      {
      KERR(" ");
      g_raw_fd_open = 0;
      }
    }
  clownix_timeout_add(all_ctx, 100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  raw_fd_open(t_all_ctx *all_ctx, char *name)
{ 
  int result = -1;
  strncpy(g_raw_name, name, MAX_NAME_LEN-1);
  if (g_raw_fd_open != 0)
    KERR("%s", name);
  else if (!raw_socket_open(all_ctx))
    {
    result = 0;
    g_raw_fd_open = 1;
    }
  else
    KERR("%s", name);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void raw_fd_init(t_all_ctx *all_ctx)
{
  g_raw_fd_open = 0;
  g_ifindex = 0;
  g_llid_raw = 0;
  memset(g_raw_name, 0, MAX_NAME_LEN);
  g_fd_raw = -1;
  g_fd_raw_tx = -1;
  clownix_timeout_add(all_ctx, 100, timer_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/
