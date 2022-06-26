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
int ovs_cmd_vhost_up(char *ovs_bin, char *ovs_dir,
                     char *name, int num, char *vhost)
{
  int result;
  result = ifdev_set_intf_flags_iff_up_down(vhost, 1);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_snf_lan(char *ovs_bin, char *ovs_dir, char *name, int num,
                        char *vhost, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);

  snprintf(cmd, MAX_ARG_LEN-1,
  "-- add-port %s%s s%s "
  "-- --id=@m create mirror name=mir%s "
  "-- add bridge %s%s mirrors @m "
  "-- --id=@%s get port %s "
  "-- set mirror mir%s select_src_port=@%s select_dst_port=@%s "
  "-- --id=@s%s get port s%s -- set mirror mir%s output-port=@s%s",
                OVS_BRIDGE, lan, vhost, vhost,
                OVS_BRIDGE, lan, vhost, vhost,
                vhost, vhost, vhost, 
                vhost, vhost, vhost, vhost); 
  if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
    {
    KERR("ERROR OVSCMD: SNF PART2 %s", vhost);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_snf_lan(char *ovs_bin, char *ovs_dir, char *name, int num,
                        char *vhost, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
           "-- --id=@m get Mirror mir%s -- remove Bridge %s%s mirrors @m "
           "-- del-port %s%s s%s",
           vhost, OVS_BRIDGE, lan, OVS_BRIDGE, lan, vhost);
  if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
    {
    KERR("ERROR OVSCMD: SNF DEL %s", vhost);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_lan(char *ovs_bin, char *ovs_dir, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- add-br %s%s", OVS_BRIDGE, lan);
  if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
    {
    KERR("ERROR OVSCMD: ADD LAN %s", lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_lan(char *ovs_bin, char *ovs_dir, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN]; 
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br %s%s", OVS_BRIDGE, lan);
  if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
    {
    KERR("ERROR OVSCMD: DEL LAN %s", lan);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_add_lan_endp(char *ovs_bin, char *ovs_dir, char *lan, char *vhost)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- add-port %s%s %s", OVS_BRIDGE, lan, vhost);
  if (ovs_vsctl(ovs_bin, ovs_dir, cmd))
    {
    KERR("ERROR OVSCMD: ADD LAN ETH %s %s", lan, vhost);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_cmd_del_lan_endp(char *ovs_bin, char *ovs_dir, char *lan, char *vhost)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-port %s%s %s", OVS_BRIDGE, lan, vhost);
  ovs_vsctl_quiet(ovs_bin, ovs_dir, cmd);
  return result;
}
/*---------------------------------------------------------------------------*/

