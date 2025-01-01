/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/if_tun.h>
#include <linux/if_arp.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <time.h>
#include <sched.h>
#include <sys/mount.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "tun_tap.h"


static char g_netns_namespace[MAX_NAME_LEN];
static char g_tap_name[MAX_NAME_LEN];
static uint8_t g_buf_rx[TOT_MSG_BUF_LEN];
static uint8_t g_buf_tx[TOT_MSG_BUF_LEN];
static uint8_t g_head_msg[HEADER_TAP_MSG];
static uint8_t g_mac_tap[6];
static int g_fd_tx_to_parent;
static int g_fd_rx_from_parent;
char *get_net_name(void);

/****************************************************************************/
static void tx_tap(int fd)
{
  int len, tx, rx;
  len = read(g_fd_rx_from_parent, g_head_msg, HEADER_TAP_MSG);
  if (len != HEADER_TAP_MSG)
    KOUT("ERROR %d %d", len, errno);
  else
    {
    if ((g_head_msg[0] != 0xCA) || (g_head_msg[1] != 0xFE)) 
      KOUT("ERROR %d %d %hhx %hhx",len,errno,g_head_msg[0],g_head_msg[1]);
    len = ((g_head_msg[2] & 0xFF) << 8) + (g_head_msg[3] & 0xFF);
    if ((len == 0) || (len > MAX_TAP_BUF_LEN))
      KOUT("ERROR %d %hhx %hhx", len, g_head_msg[2], g_head_msg[3]);
    rx = read(g_fd_rx_from_parent, g_buf_rx, len);
    if (len != rx)
      KOUT("ERROR TAP %s %d %d %d", g_tap_name, len, rx, errno);
    tx = write(fd, g_buf_rx, len);
    if (tx != len)
      KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
    }
}
/*-------------------------------------------------------------------------*/


/****************************************************************************/
static void rx_tap(int fd)
{
  int len, tx;
  len = read(fd, g_buf_tx + HEADER_TAP_MSG, MAX_TAP_BUF_LEN);
  if (len <= 0)
    KOUT("ERROR READ TAP %s %d %d", g_tap_name, len, errno);
  else
    {
    g_buf_tx[0] = 0xCA;
    g_buf_tx[1] = 0xFE;
    g_buf_tx[2] = (len & 0xFF00) >> 8;
    g_buf_tx[3] =  len & 0xFF;
    tx = write(g_fd_tx_to_parent, g_buf_tx, len + HEADER_TAP_MSG);
    if (tx != len + HEADER_TAP_MSG)
      KOUT("ERROR WRITE %s %d %d %d", g_tap_name, tx, len, errno);
    }
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_intf_flags_iff(char *intf, int *flags)
{
  int result = -1, s, io;
  struct ifreq ifr;
  s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s > 0)
    {
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, intf, IFNAMSIZ);
    io = ioctl (s, SIOCGIFFLAGS, &ifr);
    if(io == 0)
      {
      *flags = ifr.ifr_flags;
      result = 0;
      }
    close(s);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int set_intf_flags_iff_up_and_promisc(char *intf)
{
  int result = -1, s, io;
  struct ifreq ifr;
  int flags;
  if (!get_intf_flags_iff(intf, &flags))
    {
    s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (s > 0)
      {
      memset(&ifr, 0, sizeof(struct ifreq));
      strncpy(ifr.ifr_name, intf, IFNAMSIZ);
      ifr.ifr_flags = flags | IFF_UP | IFF_RUNNING | IFF_PROMISC;
      io = ioctl (s, SIOCSIFFLAGS, &ifr);
      if(!io)
        result = 0;
      close(s);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int open_tap(char *tap, char *err)
{
  int result = -1;
  struct ifreq ifr;
  int i, flags, fd;
  uid_t owner = getuid();
  if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
    {
    sprintf(err, "\nError open \"/dev/net/tun\", %d\n", errno);
    }
  else if (!get_intf_flags_iff(tap, &flags))
    {
    sprintf(err, "\nError system already has %s\n\n", tap);
    }
  else
    {
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, tap, sizeof(ifr.ifr_name) - 1);
    if(ioctl(fd, TUNSETIFF, (void *) &ifr) < 0)
      sprintf(err, "\nError ioctl TUNSETIFF \"%s\", %d", tap,  errno);
    else if(ioctl(fd, TUNSETOWNER, owner) < 0)
      sprintf(err, "\nError ioctl TUNSETOWNER \"%s\", %d", tap,  errno);
    else if(ioctl(fd, TUNSETGROUP, owner) < 0)
      sprintf(err, "\nError ioctl TUNSETGROUP \"%s\", %d", tap,  errno);
    else
      {
      memset(&ifr, 0, sizeof(struct ifreq));
      strcpy(ifr.ifr_name, tap);
      ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
      if (ioctl(fd, SIOCGIFHWADDR, &ifr) != -1)
        {
        for (i=0; i<MAC_ADDR_LEN; i++)
          g_mac_tap[i] = ifr.ifr_hwaddr.sa_data[i];
        result = 0;
        result = fd;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void forked_tx_to_tap(int fd_rx_from_parent, int fd_to_tap)
{
  int fd_max, result;
  fd_set infd;

  fd_max = fd_rx_from_parent;

  while(1)
    {
    FD_ZERO(&infd);
    FD_SET(fd_rx_from_parent, &infd);
    result = select(fd_max + 1, &infd, NULL, NULL, NULL);
    if (result < 0)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);
    if (FD_ISSET(fd_rx_from_parent, &infd))
      tx_tap(fd_to_tap);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void forked_rx_from_tap(int fd_rx_from_parent)
{
  int pid, fd, fd_max, result; 
  fd_set infd;
  char err[MAX_PATH_LEN];

  fd = open(g_netns_namespace, O_RDONLY|O_CLOEXEC);
  if (fd < 0)
    KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);
  if (setns(fd, CLONE_NEWNET) != 0)
    KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);
  close(fd);

  fd = open_tap(g_tap_name, err);
  if (fd < 0)
    KOUT("ERROR %s %s %s", g_netns_namespace, g_tap_name, err);
  if (set_intf_flags_iff_up_and_promisc(g_tap_name))
    KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);


  if ((pid = fork()) < 0)
    KOUT("ERROR %s", g_tap_name);

  if (pid == 0)
    {
    forked_tx_to_tap(fd_rx_from_parent, fd);
    }

  fd_max = fd;

  while(1)
    {
    FD_ZERO(&infd);
    FD_SET(fd, &infd);
    result = select(fd_max + 1, &infd, NULL, NULL, NULL);
    if (result < 0)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);
    if (FD_ISSET(fd, &infd))
      rx_tap(fd);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int tun_tap_open(char *net_namespace, char *name, int *fd_rx, int *fd_tx)
{
  int pid;
  int fd1[2];
  int fd2[2];
  memset(g_tap_name, 0, MAX_NAME_LEN);
  memset(g_netns_namespace, 0, MAX_NAME_LEN);
  strncpy(g_tap_name, name, MAX_NAME_LEN-1);
  strncpy(g_netns_namespace, net_namespace, MAX_NAME_LEN-1);

  if (pipe(fd1) == -1)
    KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);
  if (pipe(fd2) == -1)
    KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);
  if ((pid = fork()) < 0)
    KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);
  if (pid == 0)
    {
    close(fd1[0]);
    close(fd2[1]);
    g_fd_tx_to_parent = fd1[1];
    g_fd_rx_from_parent = fd2[0];
    forked_rx_from_tap(fd2[0]);
    }
  else
    {
    close(fd1[1]);
    close(fd2[0]);
    *fd_rx = fd1[0];
    *fd_tx = fd2[1]; 
    }
  return pid;
}
/*---------------------------------------------------------------------------*/

