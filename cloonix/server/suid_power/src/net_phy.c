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
static int get_driver_mac_pci(t_topo_info_phy *phy)
{
  int result = -1;
  char path[MAX_PATH_LEN];
  FILE *fh;
  sprintf(path, "/sys/class/net/%s/device/uevent", phy->name);
  fh = fopen(path, "r");
  if (fh)
    {
    fclose(fh);
    sprintf(path, "/sys/class/net/%s/address", phy->name);
    fh = fopen(path, "r");
    if (fh)
      {
      if (fgets(phy->phy_mac, MAX_NAME_LEN-1, fh) == NULL)
        KERR("%s", phy->name);
      else
        result = 0;
      fclose(fh);
      }
    else
      KERR("%s", path);
    }
  return result;
}
/*---------------------------------------------------------------------------*/


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
    strncpy(info->name, ifr.ifr_name, IF_NAMESIZE-1);
    info->name[IF_NAMESIZE-1] = '\0';
    if (ioctl(socketfd, SIOCGIFFLAGS, &ifr) != -1)
      result = 0;
    be_sure_to_close(socketfd);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

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
