/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <string.h>
#include <errno.h>

#include "io_clownix.h"
#include "net_phy.h"

static t_topo_info_phy g_topo_phy[MAX_PHY];
static int g_nb_phy;

/****************************************************************************/
static void get_pci_for_virtio_net(char *name, char *pci)
{
  FILE *fh;
  char cmd[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char *ptr_start, *ptr_end, *start="/sys/devices/pci0000:00/";
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "find /sys | grep ifindex |grep %s", name); 
  fh = popen(cmd, "r");
  if (fh)
    {
    fgets(line, MAX_PATH_LEN-1, fh);
    if (!strncmp(line, start, strlen(start)))
      {
      ptr_start = line + strlen(start);
      ptr_end = strstr(ptr_start, "/virtio");
      if (ptr_end)
        {
        *ptr_end = 0;
        strncpy(pci, ptr_start, MAX_NAME_LEN-1);
        }
      else
        KERR("%s", line);
      }
    else
      KERR("%s", line);
    fclose(fh);
    }
  else
    KERR("%s", cmd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void be_sure_to_close(int socketfd)
{
  int res;
  do
    res = close(socketfd);
  while (res == -1 && errno == EINTR);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_interface_by_index(int index, t_topo_info_phy *info)
{
  int socketfd, result = -1;
  struct ifreq  ifr;
  socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (socketfd == -1)
    KOUT(" ");
  ifr.ifr_ifindex = index;
  if (ioctl(socketfd, SIOCGIFNAME, &ifr) == -1)
    {
    be_sure_to_close(socketfd);
    }
  else
    {
    info->index = index;
    strncpy(info->name, ifr.ifr_name, IF_NAMESIZE-1);
    info->name[IF_NAMESIZE-1] = '\0';
    if (ioctl(socketfd, SIOCGIFFLAGS, &ifr) != -1)
      {
      info->flags = ifr.ifr_flags;
      result = 0;
      }
    be_sure_to_close(socketfd);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

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

/****************************************************************************/
static int get_driver_mac_pci(t_topo_info_phy *phy)
{
  int result = -1;
  char path[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char ident[MAX_NAME_LEN];
  char devtype[MAX_NAME_LEN];
  FILE *fh;
  sprintf(path, "/sys/class/net/%s/device/uevent", phy->name);
  fh = fopen(path, "r");
  memset(devtype, 0, MAX_NAME_LEN);
  if (fh)
    {
    while (fgets(line, MAX_PATH_LEN-1, fh) != NULL)
      {
      if (sscanf(line, "DRIVER=%s", ident) == 1)
        strncpy(phy->drv, ident, MAX_NAME_LEN-1);
      if (sscanf(line, "PCI_SLOT_NAME=%s", ident) == 1)
        strncpy(phy->pci, ident, MAX_NAME_LEN-1);
      if (sscanf(line, "DEVTYPE=%s", ident) == 1)
        strncpy(devtype, ident, MAX_NAME_LEN-1);
      }
    fclose(fh);

    sprintf(path, "/sys/class/net/%s/device/vendor", phy->name);
    fh = fopen(path, "r");
    if (fh)
      {
      if (fgets(phy->vendor, MAX_NAME_LEN-1, fh) == NULL)
        KERR("%s", phy->name);
      phy->vendor[6] = 0;
      fclose(fh);
      }

    sprintf(path, "/sys/class/net/%s/device/device", phy->name);
    fh = fopen(path, "r");
    if (fh)
      {
      if (fgets(phy->device, MAX_NAME_LEN-1, fh) == NULL)
        KERR("%s", phy->name);
      phy->device[6] = 0;
      fclose(fh);
      }

   sprintf(path, "/sys/class/net/%s/address", phy->name);
    fh = fopen(path, "r");
    if (fh)
      {
      if (fgets(phy->mac, MAX_NAME_LEN-1, fh) == NULL)
        KERR("%s", phy->name);
      fclose(fh);
      }
    else
      KERR("%s", path);

    if ((strlen(phy->pci) == 0) && (strlen(devtype) != 0))
      strncpy(phy->pci, devtype, MAX_NAME_LEN-1);
    else if (!strncmp(phy->drv, "virtio_net", strlen("virtio_net")))
      {
      phy->drv[strlen("virtio_net")] = 0;
      get_pci_for_virtio_net(phy->name, phy->pci);
      }

    if (strlen(phy->vendor) == 0)
      strcpy(phy->vendor, "none");
    if (strlen(phy->device) == 0)
      strcpy(phy->device, "none");

    if ((strlen(phy->pci) != 0) &&
        (strlen(phy->drv) != 0) &&
        (strlen(phy->mac) != 0))
      result = 0;
    }
  else
    {
    sprintf(path, "/sys/class/net/%s/uevent", phy->name);
    fh = fopen(path, "r");
    if (fh)
      {
      while (fgets(line, MAX_PATH_LEN-1, fh) != NULL)
        {
        if (sscanf(line, "DEVTYPE=%s", ident) == 1)
          strncpy(devtype, ident, MAX_NAME_LEN-1);
        }
      fclose(fh);
      }
    if (strlen(devtype))
      {
      strcpy(phy->vendor, devtype);
      strcpy(phy->device, devtype);
      strcpy(phy->pci, devtype);
      strcpy(phy->drv, devtype);
      sprintf(path, "/sys/class/net/%s/address", phy->name);
      fh = fopen(path, "r");
      if (fh)
        {
        if (fgets(phy->mac, MAX_NAME_LEN-1, fh) == NULL)
          KERR("%s", phy->name);
        else
          result = 0;
        fclose(fh);
        }
      else
        KERR("%s", path);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int net_phy_flags_iff_up_down(char *intf, int up)
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
      ifr.ifr_flags = flags | IFF_UP;
    else
      ifr.ifr_flags = flags & ~IFF_UP;
    io = ioctl (s, SIOCSIFFLAGS, &ifr);
    if(!io)
      result = 0;
    else
      KERR(" ");
    close(s);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_info_phy *net_phy_get(int *nb)
{
  int i;
  t_topo_info_phy phy;
  g_nb_phy = 0;
  for (i = 1; i<32; i++)
    {
    memset(&phy, 0, sizeof(t_topo_info_phy));
    if (get_interface_by_index(i, &phy) == 0)
      {
      if (!get_driver_mac_pci(&phy))
        {
        if (g_nb_phy >= MAX_PHY-2)
          KERR("ERROR, NOT ENOUGH SPACE MAX_PHY %d", g_nb_phy);
        else
          {
          memcpy(&(g_topo_phy[g_nb_phy]), &phy, sizeof(t_topo_info_phy));
          g_nb_phy += 1;
          }
        }
      }
    }
  *nb = g_nb_phy;
  return g_topo_phy;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void net_phy_init(void)
{
  memset(g_topo_phy, 0, MAX_PHY * sizeof(t_topo_info_phy));
  g_nb_phy = 0;
}
/*--------------------------------------------------------------------------*/
