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
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <net/if.h>
#include <errno.h>
#include <linux/if_tun.h>
#include <netinet/in.h>

#include "ioc.h"
#include "tun_tap.h"

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
int set_intf_flags_iff_up(char *intf)
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
      ifr.ifr_flags = flags | IFF_UP;
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
  int flags, fd;
  uid_t owner = getuid();
  if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
    sprintf(err, "\nError open \"/dev/net/tun\", %d\n", errno);
  else if (!get_intf_flags_iff(tap, &flags))
    sprintf(err, "\nError system already has %s\n\n", tap);
  else
    {
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, tap, sizeof(ifr.ifr_name) - 1);
    if(ioctl(fd, TUNSETIFF, (void *) &ifr) < 0)
      sprintf(err, "\nError ioctl TUNSETIFF \"%s\", %d", tap,  errno);
    else
      {
      if(ioctl(fd, TUNSETOWNER, owner) < 0)
        sprintf(err, "\nError ioctl TUNSETOWNER \"%s\", %d", tap,  errno);
      else
        {
        if(ioctl(fd, TUNSETGROUP, owner) < 0)
          sprintf(err, "\nError ioctl TUNSETGROUP \"%s\", %d", tap,  errno);
        else
          {
          nonblock_fd(fd);
          result = fd;
          }
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int tun_tap_open(t_all_ctx *all_ctx, char *name, int *fd)
{
  char err[MAX_ERR_LEN];
  int result = -1;
  memset(err, 0, MAX_ERR_LEN);
  *fd = open_tap(name, err);
  if (*fd > 0)
    {
    if (set_intf_flags_iff_up(name))
      {
      KERR("Not able to set up flag for tap: %s", name);
      close(*fd);
      }
    else
      {
      result = 0;
      }
    }
  else
    KERR("failure open tap: %s", err); 
  return result;
}
/*---------------------------------------------------------------------------*/


