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
#include <sys/wait.h>
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
static uint8_t g_buf_rx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_buf_tx[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_mac_tap[6];
static int g_fd_tx_to_parent;
static int g_fd_rx_from_parent;
char *get_net_name(void);

/****************************************************************************/
static void tx_tapif(int fd)
{
  static uint16_t seqtap=0;
  uint16_t seq; 
  int tx, len, buf_len, result;
  uint8_t *buf;
  int fdrx = g_fd_rx_from_parent;
  len = read(fdrx, g_buf_rx, HEADER_TAP_MSG);
  if (len == 0)
    exit(0);
  if (len != HEADER_TAP_MSG)
    KOUT("ERROR READ %d %d", len, errno);
  result = fct_seqtap_rx(fdrx, g_buf_rx, &seq, &buf_len, &buf);
  if (result != kind_seqtap_data)
    KOUT("ERROR %d", result);
  if (seq != ((seqtap+1)&0xFFFF)) 
    KERR("WARNING %d %d", seq, seqtap);
  seqtap = seq;
  tx = write(fd, buf, buf_len);
  if (tx != buf_len)
    KOUT("ERROR WRITE TAP %d %d %d", tx, buf_len, errno);
}
/*-------------------------------------------------------------------------*/


/****************************************************************************/
static void rx_tapif(int fd)
{
  static uint16_t seqtap = 0;
  int len, tx;
  seqtap += 1;
  len = read(fd, g_buf_tx + HEADER_TAP_MSG, TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN);
  if ((len <= 0) || (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR READ %d", len);
  fct_seqtap_tx(kind_seqtap_data, g_buf_tx, seqtap, len, NULL);
  tx = write(g_fd_tx_to_parent, g_buf_tx, len + HEADER_TAP_MSG);
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE %d %d %d", tx, len, errno);
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
      tx_tapif(fd_to_tap);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void pid_check_child(int pid)
{ 
  int res;
  int options = WNOHANG | WEXITED | WSTOPPED | WCONTINUED;
  siginfo_t infop;
  memset(&infop, 0, sizeof(siginfo_t));
  res = waitid(P_PID, pid, &infop, options);
  if ((res==0) && (pid == infop.si_pid))
    KOUT("ERROR CHILD %s", g_tap_name);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void forked_rx_from_tap(int fd_rx_from_parent)
{
  int pid, fd, fd_max, result; 
  fd_set infd;
  char err[MAX_PATH_LEN];
  struct timeval timeout;

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
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    result = select(fd_max + 1, &infd, NULL, NULL, &timeout);
    if (result < 0)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);
    if (FD_ISSET(fd, &infd))
      rx_tapif(fd);
    pid_check_child(pid);
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

