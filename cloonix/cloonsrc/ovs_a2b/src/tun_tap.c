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
#include "utils.h"


static char g_netns_namespace[MAX_NAME_LEN];
static char g_tap_name0[MAX_NAME_LEN];
static char g_tap_name1[MAX_NAME_LEN];
static uint8_t g_buf_rx0[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_buf_rx1[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_buf_tx0[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_buf_tx1[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_mac_tap0[6];
static uint8_t g_mac_tap1[6];
static int g_fd_tx_to_parent0;
static int g_fd_rx_from_parent0;
static int g_fd_tx_to_parent1;
static int g_fd_rx_from_parent1;

/****************************************************************************/
static void tx_tapif0(int fd)
{
  static uint16_t seqtap0=0;
  uint16_t seq;
  int len, buf_len, tx, result;
  uint8_t *buf;
  len = read(g_fd_rx_from_parent0, g_buf_rx0, HEADER_TAP_MSG);
  if (len != HEADER_TAP_MSG)
    KOUT("ERROR READ %d %d", len, errno);
  result = fct_seqtap_rx(g_fd_rx_from_parent0, g_buf_rx0, &seq, &buf_len, &buf);
  if (result != kind_seqtap_data)
    KOUT("ERROR %d", result);
  if (seq != ((seqtap0+1)&0xFFFF))
    KERR("WARNING %d %d", seq, seqtap0);
  seqtap0 = seq;
  tx = write(fd, buf, buf_len);
  if (tx != buf_len)
    KOUT("ERROR WRITE TAP %d %d %d", tx, buf_len, errno);
}
/*-------------------------------------------------------------------------*/

/****************************************************************************/
static void tx_tapif1(int fd)
{
  static uint16_t seqtap1=0;
  uint16_t seq;
  int len, buf_len, tx, result;
  uint8_t *buf;
  len = read(g_fd_rx_from_parent1, g_buf_rx1, HEADER_TAP_MSG);
  if (len != HEADER_TAP_MSG)
    KOUT("ERROR READ %d %d", len, errno);
  result = fct_seqtap_rx(g_fd_rx_from_parent1, g_buf_rx1, &seq, &buf_len, &buf);
  if (result != kind_seqtap_data)
    KOUT("ERROR %d", result);
  if (seq != ((seqtap1+1)&0xFFFF)) 
    KERR("WARNING %d %d", seq, seqtap1);
  seqtap1 = seq;
  tx = write(fd, buf, buf_len);
  if (tx != buf_len)
    KOUT("ERROR WRITE TAP %d %d %d", tx, buf_len, errno);
}
/*-------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_tapif0(int fd)
{
  static uint16_t seqtap0 = 0;
  char *name;
  int len, tx;
  seqtap0 += 1;
  name = g_tap_name0;
  len = read(fd, g_buf_tx0 + HEADER_TAP_MSG, TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN);
  if ((len <= 0) || (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR READ %d", len);
  fct_seqtap_tx(kind_seqtap_data, g_buf_tx0, seqtap0, len, NULL);
  tx = write(g_fd_tx_to_parent0, g_buf_tx0, len + HEADER_TAP_MSG);
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE %s %d %d %d", name, tx, len, errno);
}
/*-------------------------------------------------------------------------*/
      
/****************************************************************************/
static void rx_tapif1(int fd)
{     
  static uint16_t seqtap1 = 0;
  char *name;
  int len, tx;
  seqtap1 += 1;
  name = g_tap_name1; 
  len = read(fd, g_buf_tx1 + HEADER_TAP_MSG, TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN);
  if ((len <= 0) || (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR READ %d", len);
  fct_seqtap_tx(kind_seqtap_data, g_buf_tx1, seqtap1, len, NULL);
  tx = write(g_fd_tx_to_parent1, g_buf_tx1, len + HEADER_TAP_MSG);
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE %s %d %d %d", name, tx, len, errno);
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
static int open_tap(int side, char *tap, char *err)
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
          {
          if (side)
            g_mac_tap1[i] = ifr.ifr_hwaddr.sa_data[i];
          else
            g_mac_tap0[i] = ifr.ifr_hwaddr.sa_data[i];
          }
        result = 0;
        result = fd;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void forked_process_tap(int side, int fd_rx_from_parent)
{
  int fd, fd_max, result; 
  fd_set infd;
  char err[MAX_PATH_LEN];
  char *name;

  if (side)
    name = g_tap_name1;
  else
    name = g_tap_name0;

  fd = open(g_netns_namespace, O_RDONLY|O_CLOEXEC);
  if (fd < 0)
    KOUT("ERROR %s %s", g_netns_namespace, name);
  if (setns(fd, CLONE_NEWNET) != 0)
    KOUT("ERROR %s %s", g_netns_namespace, name);
  close(fd);

  fd = open_tap(side, name, err);
  if (fd < 0)
    KOUT("ERROR %s %s %s", g_netns_namespace, name, err);
  if (set_intf_flags_iff_up_and_promisc(name))
    KOUT("ERROR %s %s", g_netns_namespace, name);
  fd_max = fd;
  if (fd_rx_from_parent > fd_max)
    fd_max = fd_rx_from_parent;

  while(1)
    {
    FD_ZERO(&infd);
    FD_SET(fd, &infd);
    FD_SET(fd_rx_from_parent, &infd);
    result = select(fd_max + 1, &infd, NULL, NULL, NULL);
    if (result < 0)
      KOUT("ERROR %s %s", g_netns_namespace, name);
    if (FD_ISSET(fd, &infd))
      {
      if (side)
        rx_tapif1(fd);
      else
        rx_tapif0(fd);
      }
    if (FD_ISSET(fd_rx_from_parent, &infd))
      {
      if (side)
        tx_tapif1(fd);
      else
        tx_tapif0(fd);
      }
    }

}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int tun_tap_open(int side, char *net_namespace, char *name, int *fd_rx, int *fd_tx)
{
  int pid;
  int fd1[2];
  int fd2[2];
  memset(g_netns_namespace, 0, MAX_NAME_LEN);
  strncpy(g_netns_namespace, net_namespace, MAX_NAME_LEN-1);
  if (side)
    {
    memset(g_tap_name1, 0, MAX_NAME_LEN);
    strncpy(g_tap_name1, name, MAX_NAME_LEN-1);
    if (pipe(fd1) == -1)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name1);
    if (pipe(fd2) == -1)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name1);
    if ((pid = fork()) < 0)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name1);
    if (pid == 0)
      {
      close(fd1[0]);
      close(fd2[1]);
      g_fd_tx_to_parent1 = fd1[1];
      g_fd_rx_from_parent1 = fd2[0];
      forked_process_tap(side, fd2[0]);
      }
    else
      {
      close(fd1[1]);
      close(fd2[0]);
      *fd_rx = fd1[0];
      *fd_tx = fd2[1]; 
      }
    }
  else
    {
    memset(g_tap_name0, 0, MAX_NAME_LEN);
    strncpy(g_tap_name0, name, MAX_NAME_LEN-1);
    if (pipe(fd1) == -1)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name0);
    if (pipe(fd2) == -1)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name0);
    if ((pid = fork()) < 0)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name0);
    if (pid == 0)
      {
      close(fd1[0]);
      close(fd2[1]);
      g_fd_tx_to_parent0 = fd1[1];
      g_fd_rx_from_parent0 = fd2[0];
      forked_process_tap(side, fd2[0]);
      }
    else
      {
      close(fd1[1]);
      close(fd2[0]);
      *fd_rx = fd1[0];
      *fd_tx = fd2[1];
      }
    }
  return pid;
}
/*---------------------------------------------------------------------------*/

