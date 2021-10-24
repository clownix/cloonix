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

#include "io_clownix.h"
#include "ovs_execv.h"
#include "ovs_cmd.h"
#include "ifdev.h"


/*****************************************************************************/
int ovs_cmd_add_lan(char *ovsb, char *dpdkd, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- add-br _b_%s", lan);
  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: ADD LAN %s", lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_lan(char *ovsb, char *dpdkd, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN]; 
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br _b_%s", lan);
  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL LAN %s", lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_lan_eths(char *ovsb, char *dpdkd, char *lan, char *nm, int num)
{
  int result = 0;
  char name[MAX_NAME_LEN];
  char cmd[MAX_ARG_LEN]; 

  memset(name, 0, MAX_NAME_LEN);
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(name, MAX_NAME_LEN-1, "%s_%d", nm, num);

  snprintf(cmd, MAX_ARG_LEN-1,
                "-- set bridge _b_%s datapath_type=netdev "
                "-- add-port _b_%s _p_1%s "
                "-- set Interface _p_1%s type=dpdk "
                "options:dpdk-devargs=net_virtio_user__p_1%s,"
                "mrg_rxbuf=0,path=%s/x2c_%s",
                lan, lan, name, name, name,
                dpdkd, name);

  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: ADD LAN ETH %s %d %s", name, num, lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_lan_eths(char *ovsb, char *dpdkd, char *lan, char *nm, int num)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  char name[MAX_NAME_LEN];

  memset(name, 0, MAX_NAME_LEN);
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(name, MAX_NAME_LEN-1, "%s_%d", nm, num);

  snprintf(cmd, MAX_ARG_LEN-1, "-- del-port _b_%s _p_1%s", lan, name);

  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL LAN ETH %s %d %s", name, num, lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int ovs_cmd_add_lan_ethd(char *ovsb, char *dpdkd, char *lan, char *nm, int num)
{
  int result = 0;
  char name[MAX_NAME_LEN];
  char cmd[MAX_ARG_LEN];

  memset(name, 0, MAX_NAME_LEN);
  snprintf(name, MAX_NAME_LEN-1, "%s_%d", nm, num);

  if (result == 0)
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1,
           "-- set bridge _b_%s datapath_type=netdev "
           "-- add-port _b_%s _p_%s_%s "
           "-- set interface _p_%s_%s type=patch options:peer=_p_%s_%s "
           "-- add-port _b_%s _p_%s_%s "
           "-- set interface _p_%s_%s type=patch options:peer=_p_%s_%s",
           lan, lan, lan, name,
           lan, name, name, lan,
           name, name, lan,
           name, lan, lan, name);
    }

  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: ADD LAN ETH %s %d %s", name, num, lan);
    result = -1;
    }

  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_lan_ethd(char *ovsb, char *dpdkd, char *lan, char *nm, int num)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  char name[MAX_NAME_LEN];

  memset(name, 0, MAX_NAME_LEN);
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(name, MAX_NAME_LEN-1, "%s_%d", nm, num);

  snprintf(cmd, MAX_ARG_LEN-1, "-- del-port _b_%s _p_1%s", lan, name);

  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL LAN ETH %s %d %s", name, num, lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_ethds(char *ovsb, char *dpdkd, char *nm, int num)
{
  char cmd[MAX_ARG_LEN];
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char *wname = name;
  int result = 0;

  memset(name, 0, MAX_NAME_LEN);
  memset(lan, 0, MAX_NAME_LEN);
  snprintf(name, MAX_NAME_LEN-1, "%s_%d", nm, num);
  snprintf(lan, MAX_NAME_LEN-1, "_p_%s", wname);

  if (result == 0)
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1,
           "-- add-br _b_%s "
           "-- set bridge _b_%s datapath_type=netdev "
           "-- add-port _b_%s _p_%s "
           "-- set Interface _p_%s type=dpdkvhostuserclient"
           " options:vhost-server-path=%s_qemu/%s",
           name, name, name, name, name, dpdkd, name);
    if (ovs_vsctl(ovsb, dpdkd, cmd))
      {
      KERR("ERROR OVSCMD: ADD LAN ETH %s %d %s", name, num, lan);
      result = -1;
      }
    }

  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_eths2(char *ovsb, char *dpdkd, char *nm, int num)
{
  char cmd[MAX_ARG_LEN];
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char *wname = name;
  int result = 0;
  
  memset(name, 0, MAX_NAME_LEN);
  memset(lan, 0, MAX_NAME_LEN);
  snprintf(name, MAX_NAME_LEN-1, "%s_%d", nm, num);
  snprintf(lan, MAX_NAME_LEN-1, "_p_%s", wname);

  if (result == 0)
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1,
           "-- add-br _b_%s "
           "-- set bridge _b_%s datapath_type=netdev "
           "-- add-port _b_%s _p_%s_%s "
           "-- set interface _p_%s_%s type=patch options:peer=_p_%s_%s "
           "-- add-port _b_%s _p_%s_%s "
           "-- set interface _p_%s_%s type=patch options:peer=_p_%s_%s",
           lan, lan, lan, lan, name,
           lan, name, name, lan,
           name, name, lan,
           name, lan, lan, name);
    if (ovs_vsctl(ovsb, dpdkd, cmd))
      {
      KERR("ERROR OVSCMD: ADD LAN ETH %s %d %s", name, num, lan);
      result = -1;
      }
    }

   if (result == 0)
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1,
                "-- add-port _b_%s _p_0%s "
                "-- set Interface _p_0%s type=dpdk "
                "options:dpdk-devargs=net_virtio_user__p_0%s,"
                "mrg_rxbuf=0,path=%s/c2x_%s",
                lan, name, name, name,
                dpdkd, name);
    if (ovs_vsctl(ovsb, dpdkd, cmd))
      {
      KERR("ERROR OVSCMD: ADD TAP %s", name);
      result = -1;
      }
    }

  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_ethds(char *ovsb, char *dpdkd, char *nm, int num)
{
  char cmd[MAX_ARG_LEN];
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char *wname = name;
  int result = 0;

  memset(name, 0, MAX_NAME_LEN);
  memset(lan, 0, MAX_NAME_LEN);
  snprintf(name, MAX_NAME_LEN-1, "%s_%d", nm, num);
  snprintf(lan, MAX_NAME_LEN-1, "_p_%s", wname);

  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br _b_%s", name);
  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL ETH %s %d", name, num);
    result = -1;
    }

  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br _b_%s", lan);
  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL ETH %s %d", name, num);
    result = -1;
    }

  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_lan_nat(char *ovsb, char *dpdkd, char *lan, char *name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];

  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
                "-- set bridge _b_%s datapath_type=netdev "
                "-- add-port _b_%s _p_%s "
                "-- set Interface _p_%s type=dpdk"
                " options:dpdk-devargs=net_virtio_user__p_%s,"
                "mrg_rxbuf=0,path=%s/nat_%s",
                lan, lan, name, name, name,
                dpdkd, name);

  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: ADD LAN NAT %s %s", name, lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_lan_nat(char *ovsb, char *dpdkd, char *lan, char *name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];

  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-port _b_%s _p_%s", lan, name);
  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL LAN NAT %s %s", name, lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_lan_d2d(char *ovsb, char *dpdkd, char *lan, char *name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];

  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
                "-- set bridge _b_%s datapath_type=netdev "
                "-- add-port _b_%s _p_%s "
                "-- set Interface _p_%s type=dpdk "
                "options:dpdk-devargs=net_virtio_user__p_%s,"
                "mrg_rxbuf=0,path=%s/d2d_%s",
                lan, lan, name, name, name,
                dpdkd, name);

  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: ADD LAN D2D %s %s", name, lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_lan_d2d(char *ovsb, char *dpdkd, char *lan, char *name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];

  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-port _b_%s _p_%s", lan, name);
  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL LAN D2D %s %s", name, lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_phy(char *ovsb, char *dpdkd, char *name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  char lan[MAX_NAME_LEN];

  memset(lan, 0, MAX_NAME_LEN);
  snprintf(lan, MAX_NAME_LEN-1, "_p_%s", name);

  if (result == 0)
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1,
                "-- add-br _b_%s "
                "-- set bridge _b_%s datapath_type=netdev "
                "-- add-port _b_%s %s "
                "-- add-port _b_%s _p_0%s "
                "-- set Interface _p_0%s type=dpdk "
                "options:dpdk-devargs=net_virtio_user__p_0%s,"
                "mrg_rxbuf=0,path=%s/c2x_%s",
                lan, lan, lan, name, lan, name, name, name,
                dpdkd, name);
    if (ovs_vsctl(ovsb, dpdkd, cmd))
      {
      KERR("ERROR OVSCMD: ADD PHY %s", name);
      result = -1;
      }
    }

  if (result == 0)
    {
    if (ifdev_set_intf_flags_iff_up_down(name, 1))
      KERR("ERROR OVSCMD: ADD PHY %s", name);
    ifdev_disable_offloading(name);
    }

  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_phy(char *ovsb, char *dpdkd, char *name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  char lan[MAX_NAME_LEN];

  memset(lan, 0, MAX_NAME_LEN);
  snprintf(lan, MAX_NAME_LEN-1, "_p_%s", name);
  ifdev_set_intf_flags_iff_up_down(name, 0);
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br _b_%s", lan);
  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL PHY %s", name);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_lan_phy(char *ovsb, char *dpdkd, char *lan, char *name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];

  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
                "-- set bridge _b_%s datapath_type=netdev "
                "-- add-port _b_%s _p_1%s "
                "-- set Interface _p_1%s type=dpdk "
                "options:dpdk-devargs=net_virtio_user__p_1%s,"
                "mrg_rxbuf=0,path=%s/x2c_%s",
                lan, lan, name, name, name,
                dpdkd, name);

  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: ADD LAN %s %s", name, lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_lan_phy(char *ovsb, char *dpdkd, char *lan, char *name)
{
  char cmd[MAX_ARG_LEN];
  int result = 0;

  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-port _b_%s _p_1%s", lan, name);
  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL LAN %s %s", name, lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_tap(char *ovsb, char *dpdkd, char *name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  char lan[MAX_NAME_LEN];

  memset(lan, 0, MAX_NAME_LEN);
  snprintf(lan, MAX_NAME_LEN-1, "_p_%s", name);

  if (result == 0)
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1,
           "-- add-br %s "
           "-- add-br _b_%s "
           "-- set bridge _b_%s datapath_type=netdev "
           "-- set bridge %s datapath_type=netdev "
           "-- add-port _b_%s _p_%s_%s "
           "-- set interface _p_%s_%s type=patch options:peer=_p_%s_%s "
           "-- add-port %s _p_%s_%s "
           "-- set interface _p_%s_%s type=patch options:peer=_p_%s_%s",
           name, lan, lan, name, lan,lan,name, lan,name,name,lan,
           name,name,lan, name,lan,lan,name);
    if (ovs_vsctl(ovsb, dpdkd, cmd))
      {
      KERR("ERROR OVSCMD: ADD TAP %s", name);
      result = -1;
      }
    }

  
  if (result == 0)
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1,
                "-- add-port _b_%s _p_0%s "
                "-- set Interface _p_0%s type=dpdk "
                "options:dpdk-devargs=net_virtio_user__p_0%s,"
                "mrg_rxbuf=0,path=%s/c2x_%s",
                lan, name, name, name,
                dpdkd, name);
    if (ovs_vsctl(ovsb, dpdkd, cmd))
      {
      KERR("ERROR OVSCMD: ADD TAP %s", name);
      result = -1;
      }
    }

  if (result == 0)
    {
    if (ifdev_set_intf_flags_iff_up_down(name, 1))
      KERR("ERROR OVSCMD: ADD TAP %s", name);
    ifdev_disable_offloading(name);
    }

  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_tap(char *ovsb, char *dpdkd, char *name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  char lan[MAX_NAME_LEN];

  memset(lan, 0, MAX_NAME_LEN);
  snprintf(lan, MAX_NAME_LEN-1, "_p_%s", name);
  if (ifdev_set_intf_flags_iff_up_down(name, 0))
    KERR("ERROR OVSCMD: DEL TAP %s", name);
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br _b_%s", lan);
  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL TAP %s", name);
    result = -1;
    }
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br %s", name);
  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL TAP %s", name);
    result = -1;
    }

  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_lan_tap(char *ovsb, char *dpdkd, char *lan, char *name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];

  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
                "-- set bridge _b_%s datapath_type=netdev "
                "-- add-port _b_%s _p_1%s "
                "-- set Interface _p_1%s type=dpdk "
                "options:dpdk-devargs=net_virtio_user__p_1%s,"
                "mrg_rxbuf=0,path=%s/x2c_%s",
                lan, lan, name, name, name,
                dpdkd, name);

  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: ADD LAN %s %s", name, lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_lan_tap(char *ovsb, char *dpdkd, char *lan, char *name)
{
  char cmd[MAX_ARG_LEN];
  int result = 0;

  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-port _b_%s _p_1%s", lan, name);
  if (ovs_vsctl(ovsb, dpdkd, cmd))
    {
    KERR("ERROR OVSCMD: DEL LAN %s %s", name, lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_lan_a2b(char *ovsb, char *dpdkd, char *lan, char *name, int num)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];

  memset(cmd, 0, MAX_ARG_LEN);
  if (num == 0)
    {
    snprintf(cmd, MAX_ARG_LEN-1,
                "-- set bridge _b_%s datapath_type=netdev "
                "-- add-port _b_%s _p_0%s "
                "-- set Interface _p_0%s type=dpdk "
                "options:dpdk-devargs=net_virtio_user__p_0%s,"
                "mrg_rxbuf=0,path=%s/a2b_%s",
                lan, lan, name, name, name, 
                dpdkd, name);
    }
  else if (num == 1)
    {
    snprintf(cmd, MAX_ARG_LEN-1,
                "-- set bridge _b_%s datapath_type=netdev "
                "-- add-port _b_%s _p_1%s "
                "-- set Interface _p_1%s type=dpdk "
                "options:dpdk-devargs=net_virtio_user__p_1%s,"
                "mrg_rxbuf=0,path=%s/b2a_%s",
                lan, lan, name, name, name,
                dpdkd, name);
    }
  else
    {
    KERR("ERROR OVSCMD: ADD LAN %s %s %d", name, lan, num);
    result = -1;
    }
  if (result == 0)
    {
    if (ovs_vsctl(ovsb, dpdkd, cmd))
      {
      KERR("ERROR OVSCMD: ADD LAN %s %s %d", name, lan, num);
      result = -1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_lan_a2b(char *ovsb, char *dpdkd, char *lan, char *name, int num)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];

  memset(cmd, 0, MAX_ARG_LEN);
  if (num == 0)
    snprintf(cmd, MAX_ARG_LEN-1, "-- del-port _b_%s _p_0%s", lan, name);
  else if (num == 1)
    snprintf(cmd, MAX_ARG_LEN-1, "-- del-port _b_%s _p_1%s", lan, name);
  else
    {
    KERR("ERROR OVSCMD: DEL LAN %s %s %d", name, lan, num);
    result = -1;
    }
  if (result == 0)
    {
    if (ovs_vsctl(ovsb, dpdkd, cmd))
      {
      KERR("ERROR OVSCMD: DEL LAN %s %s", name, lan);
      result = -1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

