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
#include <sys/ioctl.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if_arp.h>
#include <net/if.h>
#include <string.h>
#include <errno.h>
#include <ifaddrs.h>
#include <limits.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/syscall.h>
#include <linux/if_packet.h>

#include "ioc.h"
#include "iface.h"
#include "main.h"
#include "blkd_addr.h"

#define ETH_P_PREAUTH 0x88C7 

typedef struct t_iface
{
  char name[IFNAMSIZ + 1];
  u_short ifindex;
  short flags;
  uint8_t mac[ETH_ALEN];
} t_iface;

static unsigned int g_num_if, g_num_if_alloc;
static t_iface *g_iface_list;


/****************************************************************************/
static t_iface *iface_get(char *ifname)
{
  unsigned int i;
  t_iface *result = NULL;
  t_iface *iface;
  if (!ifname)
    KOUT(" ");
  for (i = 0; i < g_num_if; i++)
    {
    iface = &g_iface_list[i];
    if (!strcmp(ifname, iface->name))
      {
      result = iface;
      break;
      }
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
  strncpy(ifr.ifr_name, intf, IFNAMSIZ);
  io = ioctl (s, SIOCGIFFLAGS, &ifr);
  if(io == 0)
    {
    *flags = ifr.ifr_flags;
    result = 0;
    }
  else
    KERR(" ");
  close(s);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int set_intf_flags_iff_up_down(char *intf, int up)
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
    strncpy(ifr.ifr_name, intf, IFNAMSIZ);
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
static int fill_mac_field(char *ifname, uint8_t *mac)
{
  int result = -1;
  struct ifreq ifr;
  int i, s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s == -1)
    KOUT(" ");
  memset(&ifr, 0, sizeof(struct ifreq));
  strcpy(ifr.ifr_name, ifname);
  ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
  if (ioctl(s, SIOCGIFHWADDR, &ifr) != -1)
    {
    for (i=0; i<ETH_ALEN; i++)
      mac[i] = ifr.ifr_hwaddr.sa_data[i];
    result = 0; 
    }
  close(s);
  return result;
}
/*--------------------------------------------------------------------------*/




/****************************************************************************/
static int set_new_mac_addr(uint8_t *mac_rand, char *if_name, int idx)
{ 
  int i, s, result = -1;
  struct ifreq ifr;
  s = socket(AF_INET, SOCK_DGRAM, 0);
  if (s == -1)
    KOUT(" ");
  memset(&ifr, 0, sizeof(struct ifreq));
  strcpy(ifr.ifr_name, if_name);
  for (i=0; i<ETH_ALEN; i++)
    ifr.ifr_hwaddr.sa_data[i] = mac_rand[i];
  ifr.ifr_hwaddr.sa_data[5] = 0x42+idx;
  ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
  if (ioctl(s, SIOCSIFHWADDR, &ifr) != -1)
    result = 0;
  else
    KERR(" ");
  close(s);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void change_wlan_mac(int idx)
{
  t_iface *ifc;
  char if_name[IFNAMSIZ + 1];
  uint8_t mac_rand[ETH_ALEN];
  memset(if_name, 0, IFNAMSIZ + 1);
  snprintf(if_name, IFNAMSIZ, "wlan%d", idx);
  ifc = iface_get(if_name);
  mac_rand[0] = 0x2;
  mac_rand[1] = 0xFF & rand();
  mac_rand[2] = 0xFF & rand();
  mac_rand[3] = 0xFF & rand();
  mac_rand[4] = 0xFF & rand();
  mac_rand[5] = idx;
  if (!ifc)
    KOUT("%s interface not found", if_name);
  if (set_intf_flags_iff_up_down(if_name, 0))
    KOUT("%s interface down error", if_name);
  if (set_new_mac_addr(mac_rand, if_name, idx))
    KOUT("%s interface set mac error", if_name);
  if (set_intf_flags_iff_up_down(if_name, 1))
    KOUT("%s interface up error", if_name);
  if (fill_mac_field(ifc->name, ifc->mac))
    KOUT("%s fill mac error", if_name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
uint8_t *iface_get_mac(char *ifname)
{
  t_iface *iface = iface_get(ifname);
  uint8_t *mac = NULL;
  if (iface)
    mac = iface->mac;
  return mac;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void iface_init(int nb_idx)
{
  int i;
  t_iface *iface;
  struct ifaddrs *ifaddr, *ifa;
  if (g_iface_list)
    free(g_iface_list);
  g_num_if = 0;
  g_num_if_alloc = 1;
  g_iface_list = calloc(g_num_if_alloc, sizeof(t_iface));
  if (!g_iface_list)
    KOUT(" ");
  if (getifaddrs(&ifaddr) == -1)
    KOUT(" ");
  for (ifa = ifaddr; ifa; ifa = ifa->ifa_next)
    {
    if (!iface_get(ifa->ifa_name))
      {
      if (g_num_if == g_num_if_alloc)
        {
        g_num_if_alloc *= 2;
        g_iface_list = realloc(g_iface_list, g_num_if_alloc * sizeof(t_iface));
        if (!g_iface_list)
          KOUT(" ");
        memset(&g_iface_list[g_num_if], 0, g_num_if * sizeof(t_iface));
        }
      iface = &g_iface_list[g_num_if++];
      strncpy(iface->name, ifa->ifa_name, IFNAMSIZ);
      iface->name[IFNAMSIZ] = 0;
      iface->flags = ifa->ifa_flags;
      iface->ifindex = if_nametoindex(iface->name);
      if (fill_mac_field(iface->name, iface->mac))
        KOUT("%s fill mac error", iface->name);
      }
    }
  freeifaddrs(ifaddr);
  for (i=0; i<nb_idx; i++)
    change_wlan_mac(i);
}
/*--------------------------------------------------------------------------*/
