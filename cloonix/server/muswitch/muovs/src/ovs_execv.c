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
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ioc.h"
#include "ovs_execv.h"

#define OVSDB_SERVER_BIN  "sbin/ovsdb-server"
#define OVS_VSWITCHD_BIN  "sbin/ovs-vswitchd"
#define OVSDB_TOOL_BIN    "bin/ovsdb-tool"
#define OVSDB_APPCTL_BIN  "bin/ovs-appctl"
#define SCHEMA_TEMPLATE   "share/openvswitch/vswitch.ovsschema"

#define OVSDB_SERVER_SOCK "ovsdb_server.sock"
#define OVSDB_SERVER_CONF "ovsdb_server_conf"
#define OVSDB_SERVER_PID  "ovsdb_server.pid"
#define OVSDB_SERVER_CTL  "ovsdb_server.ctl"
#define OVS_VSWITCHD_PID  "ovs-vswitchd.pid"
#define OVS_VSWITCHD_LOG  "ovs-vswitchd.log"
#define OVS_VSWITCHD_CTL  "ovs-vswitchd.ctl"




/*****************************************************************************/
int call_my_popen(char *dpdk_dir, int nb, char arg[NB_ARG][MAX_ARG_LEN]);
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void wait_for_server_sock(char *dpdk_dir)
{
  int count = 0;
  char server_sock[MAX_ARG_LEN];
  memset(server_sock, 0, MAX_ARG_LEN);
  snprintf(server_sock, MAX_ARG_LEN-1, "%s/%s",dpdk_dir,OVSDB_SERVER_SOCK);
  while (access(server_sock, F_OK))
    {
    usleep(10000);
    count++;
    if (count == 100)
      KOUT(" ");
    }
  usleep(10000);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int wait_read_pid_file(char *dpdk_dir, char *name)
{
  FILE *fhd;
  char path[MAX_ARG_LEN];
  int pid, result = 0, count = 0;
  memset(path, 0, MAX_ARG_LEN);
  snprintf(path, MAX_ARG_LEN-1, "%s/%s", dpdk_dir, name);
  while(result == 0)
    {
    count += 1;
    if (count > 100)
      KOUT(" ");
    usleep(10000);
    fhd = fopen(path, "r");
    if (fhd)
      {
      if (fscanf(fhd, "%d", &pid) == 1)
        result = pid;
      if (fclose(fhd))
        KOUT("%s", strerror(errno));
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int make_cmd_argv(char cmd_argv[NB_ARG][MAX_ARG_LEN], char *multi_arg_cmd)
{
  int cmd_argc;
  char *ptr, *cur=multi_arg_cmd;
  for (cmd_argc=0;  (cur != NULL); cmd_argc++)
    {
    strncpy(cmd_argv[cmd_argc], cur, MAX_ARG_LEN-1);
    ptr = strchr(cmd_argv[cmd_argc], ' ');
    if (ptr)
      *ptr = 0;
    if (cmd_argc == NB_ARG-3)
      KOUT("%d", cmd_argc);
    ptr = strchr(cur, ' ');
    if (ptr)
      {
      cur = ptr+1;
      }
    else
      {
      cur = NULL;
      }
    }
  return cmd_argc;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_vsctl(t_all_ctx *all_ctx, char *ovs_bin,
                     char *dpdk_dir, char *multi_arg_cmd)
{
  int i, cmd_argc;
  char arg[NB_ARG][MAX_ARG_LEN];
  char cmd_argv[NB_ARG][MAX_ARG_LEN];
  memset(arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  memset(cmd_argv, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  cmd_argc = make_cmd_argv(cmd_argv, multi_arg_cmd);
  snprintf(arg[0],MAX_ARG_LEN-1,"%s/bin/ovs-vsctl", ovs_bin);
  snprintf(arg[1],MAX_ARG_LEN-1,"--db=unix:%s/%s",
                                dpdk_dir, OVSDB_SERVER_SOCK);
  for (i=0; i<cmd_argc; i++)
    {
    strncpy(arg[2+i], cmd_argv[i], MAX_ARG_LEN-1);
    }
  return (call_my_popen(dpdk_dir, cmd_argc+2, arg));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int create_ovsdb_server_conf(t_all_ctx *all_ctx, char *ovs_bin,
                                    char *dpdk_dir)
{
  char arg[NB_ARG][MAX_ARG_LEN];
  memset(arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char)); 
  snprintf(arg[0],MAX_ARG_LEN-1,"%s/%s", ovs_bin, OVSDB_TOOL_BIN);
  snprintf(arg[1],MAX_ARG_LEN-1,"-v");
  snprintf(arg[2],MAX_ARG_LEN-1,"create");
  snprintf(arg[3],MAX_ARG_LEN-1,"%s/%s", dpdk_dir, OVSDB_SERVER_CONF);
  snprintf(arg[4],MAX_ARG_LEN-1,"%s/%s", ovs_bin, SCHEMA_TEMPLATE);
  return (call_my_popen(dpdk_dir, 5, arg));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int launch_ovs_server(t_all_ctx *all_ctx, char *ovs_bin,
                             char *dpdk_dir)
{
  char arg[NB_ARG][MAX_ARG_LEN];
  int result;
  memset(arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  snprintf(arg[0],MAX_ARG_LEN-1,"%s/%s", ovs_bin, OVSDB_SERVER_BIN);
  snprintf(arg[1],MAX_ARG_LEN-1,"%s/%s",
                                dpdk_dir, OVSDB_SERVER_CONF);
  snprintf(arg[2],MAX_ARG_LEN-1,"--pidfile=%s/%s",
                                dpdk_dir, OVSDB_SERVER_PID);
  snprintf(arg[3],MAX_ARG_LEN-1,"--remote=punix:%s/%s",
                                dpdk_dir, OVSDB_SERVER_SOCK);
  snprintf(arg[4], MAX_ARG_LEN-1,
           "--remote=db:Open_vSwitch,Open_vSwitch,manager_options");
  snprintf(arg[5], MAX_ARG_LEN-1,"--unixctl=%s/%s",
                                 dpdk_dir, OVSDB_SERVER_CTL);
  snprintf(arg[6], MAX_ARG_LEN-1, "--detach");
  if (call_my_popen(dpdk_dir, 7, arg))
    {
    result = -1;
    KERR(" ");
    } 
  else
    {
    result = wait_read_pid_file(dpdk_dir, OVSDB_SERVER_PID);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void appctl_debug_fix(char *bin, char *db, char *debug_cmd)
{
  char arg[NB_ARG][MAX_ARG_LEN];
  memset(arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  snprintf(arg[0],MAX_ARG_LEN-1, "%s/%s", bin, OVSDB_APPCTL_BIN);
  snprintf(arg[1],MAX_ARG_LEN-1, "--target=%s/%s", db, OVS_VSWITCHD_CTL);
  snprintf(arg[2],MAX_ARG_LEN-1, "vlog/set");
  snprintf(arg[3],MAX_ARG_LEN-1, "%s", debug_cmd);
  if (call_my_popen(db, 4, arg))
    KERR("%s", debug_cmd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int launch_ovs_vswitchd(t_all_ctx *all_ctx, char *bin, char *db)
{
  char arg[NB_ARG][MAX_ARG_LEN];
  int result;
  memset(arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char)); 
  snprintf(arg[0],MAX_ARG_LEN-1,"%s/%s", bin, OVS_VSWITCHD_BIN);
  snprintf(arg[1],MAX_ARG_LEN-1,"unix:%s/%s", db, OVSDB_SERVER_SOCK);
  snprintf(arg[2],MAX_ARG_LEN-1,"--pidfile=%s/%s", db, OVS_VSWITCHD_PID);
  snprintf(arg[3],MAX_ARG_LEN-1,"--log-file=%s/%s", db, OVS_VSWITCHD_LOG);
  snprintf(arg[4],MAX_ARG_LEN-1,"--unixctl=%s/%s", db, OVS_VSWITCHD_CTL);
  snprintf(arg[5], MAX_ARG_LEN-1, "--detach");
  if (call_my_popen(db, 6, arg))
    {
    result = -1;
    KERR(" ");
    } 
  else
    {
    result = wait_read_pid_file(db, OVS_VSWITCHD_PID);
    sleep(2);
    appctl_debug_fix(bin, db, "ANY:syslog:err");
    appctl_debug_fix(bin, db, "netdev_dpdk:syslog:warn");
    appctl_debug_fix(bin, db, "netdev_dpdk:file:dbg");
    appctl_debug_fix(bin, db, "dpdk:syslog:warn");
    appctl_debug_fix(bin, db, "dpdk:file:dbg");
    appctl_debug_fix(bin, db, "memory:syslog:warn");
    appctl_debug_fix(bin, db, "memory:file:dbg");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *get_cmd_mirror(int spy, char *br)
{
  static char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
        "-- add-port %s dpdkr%u "
        "-- set Interface dpdkr%u type=dpdkr "
        "-- --id=@p get port dpdkr%u "
        "-- --id=@m create mirror name=mir_%s "
        "-- set mirror mir_%s select-all=1 "
        "-- set mirror mir_%s output-port=@p "
        "-- set bridge %s mirrors=@m",
        br, spy, spy, spy, br, br, br, br);
  return cmd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_lan(t_all_ctx *all_ctx, char *ovs_bin,
                      char *dpdk_dir, char *lan_name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
           "-- add-br %s "
           "-- set bridge %s datapath_type=netdev", 
           lan_name, lan_name);
  if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_lan(t_all_ctx *all_ctx, char *ovs_bin,
                      char *dpdk_dir, char *lan_name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN]; 
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br %s", lan_name);
  if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_lan_eth(t_all_ctx *all_ctx, char *ovs_bin,
                          char *dpdk_dir, char *lan_name,
                          char *vm_name, int num)
{
  int result = 0;
  char cmd[MAX_ARG_LEN]; 
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
  "-- add-port %s patch_%s_%s_%d "
  "-- set interface patch_%s_%s_%d type=patch options:peer=patch_%s_%d_%s "
  "-- add-port br_%s_%d patch_%s_%d_%s "
  "-- set interface patch_%s_%d_%s type=patch options:peer=patch_%s_%s_%d",
  lan_name, lan_name, vm_name, num,
  lan_name, vm_name, num, vm_name, num, lan_name,
  vm_name, num, vm_name, num, lan_name,
  vm_name, num, lan_name, lan_name, vm_name, num);
  if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_lan_eth(t_all_ctx *all_ctx, char *ovs_bin,
                          char *dpdk_dir, char *lan_name,
                          char *vm_name, int num)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
                "-- del-port %s patch_%s_%s_%d "
                "-- del-port br_%s_%d patch_%s_%d_%s",
                lan_name, lan_name, vm_name, num,
                vm_name, num, vm_name, num, lan_name);
  if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_eth(t_all_ctx *all_ctx, char *ovs_bin,
                      char *dpdk_dir, char *name, int num)
{
  char cmd[MAX_ARG_LEN];
  int i, result = 0;
  
  for (i=0; i<num; i++)
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1,
             "-- add-br br_%s_%d "
             "-- set bridge br_%s_%d datapath_type=netdev "
             "-- add-port br_%s_%d %s_%d "
             "-- set Interface %s_%d type=dpdkvhostuserclient "
             "options:vhost-server-path=%s_qemu/%s_%d",
             name, i, name, i,
             name, i, name, i, name, i,
             dpdk_dir, name, i);
    if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir, cmd)) 
      result = -1;
    }

  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_spy(t_all_ctx *all_ctx,
                      char *ovs_bin, char *dpdk_dir,
                      char *lan, int spy)
{
  char *cmd;
  int result = 0;
  cmd = get_cmd_mirror(spy, lan);
  if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_spy_eth(t_all_ctx *all_ctx,
                          char *ovs_bin, char *dpdk_dir,
                          char *lan, char *name, int num)
{
  char cmd[MAX_ARG_LEN];
  int result = 0;
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
        "-- --id=@p get port patch_%s_%s_%d "
        "-- add mirror mir_%s select_dst_port @p "
        "-- add mirror mir_%s select_src_port @p",
        lan, name, num, lan, lan);
  if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int ovs_execv_del_eth(t_all_ctx *all_ctx, char *ovs_bin,
                      char *dpdk_dir, char *name, int num)
{
  char cmd[MAX_ARG_LEN];
  int i, result = 0;
  for (i=0; (result==0)&&(i<num); i++)
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1, "del-br br_%s_%d", name, i);
    if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir, cmd))
      result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_daemon(t_all_ctx *all_ctx, int is_switch,
                     char *ovs_bin, char *dpdk_dir)
{
  int pid, result = -1;
  
  init_environ(ovs_bin, dpdk_dir);
  if (is_switch)
    {
    wait_for_server_sock(dpdk_dir);
    pid = launch_ovs_vswitchd(all_ctx, ovs_bin, dpdk_dir);
    if (pid <= 0)
      KERR(" ");
    else
      result = pid;
    }
  else
    {
    if (!create_ovsdb_server_conf(all_ctx, ovs_bin, dpdk_dir))
      {
      pid = launch_ovs_server(all_ctx, ovs_bin, dpdk_dir);
      if (pid <= 0)
        KERR(" ");
      else
        {
        result = pid;
        if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir, 
        "--verbose --no-wait init"))
          result = -1;
        else if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir,
        "--verbose --no-wait set Open_vSwitch . "
        "other_config:pmd-cpu-mask=0x02"))
          result = -1;
        else if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir,
        "--verbose --no-wait set Open_vSwitch . "
        "other_config:dpdk-lcore-mask=0x01"))
          result = -1;
        else if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir,
        "--verbose --no-wait set Open_vSwitch . "
        "other_config:dpdk-socket-mem=2048"))
          result = -1;
        else if (ovs_vsctl(all_ctx, ovs_bin, dpdk_dir,
        "--verbose --no-wait set Open_vSwitch . "
        "other_config:dpdk-init=true"))
          result = -1;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/
