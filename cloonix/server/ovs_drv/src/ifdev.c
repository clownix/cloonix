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
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <time.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <arpa/inet.h>



#include "io_clownix.h"
#include "ovs_execv.h"


/*****************************************************************************/
static int get_intf_flags_iff(char *intf, int *flags)
{
  int result = -1, s, io;
  struct ifreq ifr;
  s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s == -1)
    KOUT(" "); 
  memset(&ifr, 0, sizeof(struct ifreq));
  memcpy(ifr.ifr_name, intf, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0;
  io = ioctl (s, SIOCGIFFLAGS, &ifr);
  if(io == 0)
    {
    *flags = ifr.ifr_flags;
    result = 0;
    }
  close(s);
  return result;
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
int ifdev_get_intf_hwaddr(char *intf, char *mac)
{
  int result = -1, s, io;
  struct ifreq ifr;
  unsigned char *mc;
  memset(mac, 0, MAX_NAME_LEN);
  s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s == -1)
    KOUT(" ");
  memset(&ifr, 0, sizeof(struct ifreq));
  memcpy(ifr.ifr_name, intf, IFNAMSIZ);
  ifr.ifr_name[IFNAMSIZ-1] = 0;
  io = ioctl(s, SIOCGIFHWADDR, &ifr);
  if(io)
    KERR("ERROR %s", strerror(errno));
  else
    {
    if (ifr.ifr_hwaddr.sa_family!=ARPHRD_ETHER)
      KERR("ERROR %s", strerror(errno));
    else
      {
      mc = (unsigned char*)ifr.ifr_hwaddr.sa_data;
      snprintf(mac, MAX_NAME_LEN-1, "%02X:%02X:%02X:%02X:%02X:%02X",
               mc[0],mc[1],mc[2],mc[3],mc[4],mc[5]);
      }
    result = 0;
    }
  close(s);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ifdev_set_intf_flags_iff_up_promisc(char *intf)
{
  int result = -1, s, io;
  struct ifreq ifr;
  int flags;
  if (!get_intf_flags_iff(intf, &flags))
    {
    s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (s == -1)
      KOUT(" ");
    memset(&ifr, 0, sizeof(struct ifreq));
    memcpy(ifr.ifr_name, intf, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ-1] = 0;
    flags = flags | IFF_PROMISC | IFF_UP;
    io = ioctl (s, SIOCSIFFLAGS, &ifr);
    if(!io)
      result = 0;
    else
      KERR("ERROR %s", strerror(errno));
    close(s);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ifdev_set_intf_flags_iff_up_down(char *intf, int up)
{
  int result = -1, s, io;
  struct ifreq ifr;
  int flags;
  if (!get_intf_flags_iff(intf, &flags))
    {
    s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (s == -1)
      KOUT(" ");
    memset(&ifr, 0, sizeof(struct ifreq));
    memcpy(ifr.ifr_name, intf, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ-1] = 0;
    if (up)
      ifr.ifr_flags = flags | IFF_UP ;
    else
      ifr.ifr_flags = flags & ~IFF_UP;
    io = ioctl (s, SIOCSIFFLAGS, &ifr);
    if(!io)
      result = 0;
    else
      KERR("ERROR %s", strerror(errno));
    close(s);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void SetEthtoolValue(char *intf, int cmd, uint32_t value)
{
  struct ifreq ifr;
  int fd;
  struct ethtool_value ethv;
  memset(&ifr, 0, sizeof(ifr));
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == -1)
    KOUT(" ");
  strncpy(ifr.ifr_name, intf, sizeof(ifr.ifr_name)-1);
  ethv.cmd = cmd;
  ethv.data = value; 
  ifr.ifr_data = (void *) &ethv;
  ioctl(fd, SIOCETHTOOL, (char *)&ifr);
  close(fd);
 }
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void ifdev_disable_offloading(char *intf)
{
#ifdef ETHTOOL_SRXCSUM
  SetEthtoolValue(intf, ETHTOOL_SRXCSUM, 0);
#endif
#ifdef ETHTOOL_STXCSUM
  SetEthtoolValue(intf, ETHTOOL_STXCSUM, 0);
#endif
#ifdef ETHTOOL_SGRO
  SetEthtoolValue(intf, ETHTOOL_SGRO, 0);
#endif
#ifdef ETHTOOL_STSO
  SetEthtoolValue(intf, ETHTOOL_STSO, 0);
#endif
#ifdef ETHTOOL_SGSO
  SetEthtoolValue(intf, ETHTOOL_SGSO, 0);
#endif
}
/*---------------------------------------------------------------------------*/

