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
#define SCHEMA_TEMPLATE   "share/openvswitch/vswitch.ovsschema"

#define OVSDB_SERVER_SOCK "ovsdb_server.sock"
#define OVSDB_SERVER_CONF "ovsdb_server_conf"
#define OVSDB_SERVER_PID  "ovsdb_server.pid"
#define OVSDB_SERVER_CTL  "ovsdb_server.ctl"
#define OVS_VSWITCHD_PID  "ovs-vswitchd.pid"
#define OVS_VSWITCHD_LOG  "ovs-vswitchd.log"
#define OVS_VSWITCHD_CTL  "ovs-vswitchd.ctl"


#define MAX_ARG_LEN 400
#define MAX_ENV_LEN 100
#define NB_ENV 5
#define NB_ARG 30


/*****************************************************************************/
int my_popen(char *dpdk_db_dir, char *argv[], char *env[], int *child_pid);
/*---------------------------------------------------------------------------*/

static char *g_environ[NB_ENV];

/*****************************************************************************/
static void wait_for_server_sock(char *dpdk_db_dir)
{
  int count = 0;
  char server_sock[MAX_ARG_LEN];
  memset(server_sock, 0, MAX_ARG_LEN);
  snprintf(server_sock, MAX_ARG_LEN-1, "%s/%s",dpdk_db_dir,OVSDB_SERVER_SOCK);
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
static int wait_read_pid_file(char *dpdk_db_dir, char *name)
{
  FILE *fhd;
  char path[MAX_ARG_LEN];
  int pid, result = 0, count = 0;
  memset(path, 0, MAX_ARG_LEN);
  snprintf(path, MAX_ARG_LEN-1, "%s/%s", dpdk_db_dir, name);
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
static void init_environ(char *dpdk_db_dir)
{
  int i;
  static char env[NB_ENV][MAX_ENV_LEN];
  memset(env, 0, NB_ENV * MAX_ENV_LEN * sizeof(char)); 
  memset(g_environ, 0, NB_ENV * sizeof(char *));
  snprintf(env[0], MAX_ENV_LEN-1, "OVS_BINDIR=%s", dpdk_db_dir);
  snprintf(env[1], MAX_ENV_LEN-1, "OVS_RUNDIR=%s", dpdk_db_dir);
  snprintf(env[2], MAX_ENV_LEN-1, "OVS_LOGDIR=%s", dpdk_db_dir);
  snprintf(env[3], MAX_ENV_LEN-1, "OVS_DBDIR=%s", dpdk_db_dir);
  for (i=0; i<NB_ENV-1; i++)
    g_environ[i] = env[i];
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
static char *make_cmd_str(char **argv)
{
  int i;
  static char result[10*MAX_ARG_LEN];
  memset(result, 0, 10*MAX_ARG_LEN);
  for (i=0;  (argv[i] != NULL); i++)
    {
    strcat(result, argv[i]);
    if (strlen(result) >= 8*MAX_ARG_LEN)
      {
      KERR("NOT POSSIBLE");
      break;
      }
    strcat(result, " ");
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int call_my_popen(char *dpdk_db_dir, int nb,
                         char arg[NB_ARG][MAX_ARG_LEN])
{
  int i, child_pid, result = 0;
  char *argv[NB_ARG];
  memset(argv, 0, NB_ARG * sizeof(char *));
  for (i=0; i<nb; i++)
    argv[i] = arg[i];
  if (my_popen(dpdk_db_dir, argv, g_environ, &child_pid))
    {
    KERR("%s %d", make_cmd_str(argv), child_pid);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_vsctl(t_all_ctx *all_ctx, char *ovsx_bin,
                     char *dpdk_db_dir, char *multi_arg_cmd)
{
  int i, cmd_argc;
  char arg[NB_ARG][MAX_ARG_LEN];
  char cmd_argv[NB_ARG][MAX_ARG_LEN];
  memset(arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  memset(cmd_argv, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  cmd_argc = make_cmd_argv(cmd_argv, multi_arg_cmd);
  snprintf(arg[0],MAX_ARG_LEN-1,"%s/bin/ovs-vsctl", ovsx_bin);
  snprintf(arg[1],MAX_ARG_LEN-1,"--db=unix:%s/%s",
                                dpdk_db_dir, OVSDB_SERVER_SOCK);
  for (i=0; i<cmd_argc; i++)
    {
    strncpy(arg[2+i], cmd_argv[i], MAX_ARG_LEN-1);
    }
  return (call_my_popen(dpdk_db_dir, cmd_argc+2, arg));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void unlink_files(char *dpdk_db_dir)
{ 
  char pth[MAX_ARG_LEN];
  if (!access(dpdk_db_dir, F_OK))
    {
    memset(pth, 0, MAX_ARG_LEN);
    snprintf(pth, MAX_ARG_LEN-1,"/bin/rm -rf %s", dpdk_db_dir);
    system(pth);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void create_dpdk_dir(char *dpdk_db_dir)
{
  char pth[MAX_ARG_LEN];
  memset(pth, 0, MAX_ARG_LEN);
  snprintf(pth, MAX_ARG_LEN-1,"/bin/mkdir -vp %s", dpdk_db_dir);
  system(pth);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void create_ovsdb_server_conf(t_all_ctx *all_ctx, char *ovsx_bin,
                                     char *dpdk_db_dir)
{
  char arg[NB_ARG][MAX_ARG_LEN];
  memset(arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char)); 
  snprintf(arg[0],MAX_ARG_LEN-1,"%s/%s", ovsx_bin, OVSDB_TOOL_BIN);
  snprintf(arg[1],MAX_ARG_LEN-1,"-v");
  snprintf(arg[2],MAX_ARG_LEN-1,"create");
  snprintf(arg[3],MAX_ARG_LEN-1,"%s/%s", dpdk_db_dir, OVSDB_SERVER_CONF);
  snprintf(arg[4],MAX_ARG_LEN-1,"%s/%s", ovsx_bin, SCHEMA_TEMPLATE);
  call_my_popen(dpdk_db_dir, 5, arg);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int launch_ovs_server(t_all_ctx *all_ctx, char *ovsx_bin,
                             char *dpdk_db_dir)
{
  char arg[NB_ARG][MAX_ARG_LEN];
  memset(arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  snprintf(arg[0],MAX_ARG_LEN-1,"%s/%s", ovsx_bin, OVSDB_SERVER_BIN);
  snprintf(arg[1],MAX_ARG_LEN-1,"%s/%s",
                                dpdk_db_dir, OVSDB_SERVER_CONF);
  snprintf(arg[2],MAX_ARG_LEN-1,"--pidfile=%s/%s",
                                dpdk_db_dir, OVSDB_SERVER_PID);
  snprintf(arg[3],MAX_ARG_LEN-1,"--remote=punix:%s/%s",
                                dpdk_db_dir, OVSDB_SERVER_SOCK);
  snprintf(arg[4], MAX_ARG_LEN-1,
           "--remote=db:Open_vSwitch,Open_vSwitch,manager_options");
  snprintf(arg[5], MAX_ARG_LEN-1,"--unixctl=%s/%s",
                                 dpdk_db_dir, OVSDB_SERVER_CTL);
  snprintf(arg[6], MAX_ARG_LEN-1, "--detach");
  call_my_popen(dpdk_db_dir, 7, arg);
  return (wait_read_pid_file(dpdk_db_dir, OVSDB_SERVER_PID));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int launch_ovs_vswitchd(t_all_ctx *all_ctx, char *ovsx_bin,
                               char *dpdk_db_dir)
{
  char arg[NB_ARG][MAX_ARG_LEN];
  memset(arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char)); 
  snprintf(arg[0],MAX_ARG_LEN-1,"%s/%s", ovsx_bin, OVS_VSWITCHD_BIN);
  snprintf(arg[1],MAX_ARG_LEN-1,"unix:%s/%s", dpdk_db_dir, OVSDB_SERVER_SOCK);
  snprintf(arg[2],MAX_ARG_LEN-1,"--pidfile=%s/%s",
                                dpdk_db_dir, OVS_VSWITCHD_PID);
  snprintf(arg[3],MAX_ARG_LEN-1,"--log-file=%s/%s",
                                dpdk_db_dir, OVS_VSWITCHD_LOG);
  snprintf(arg[4],MAX_ARG_LEN-1,"--unixctl=%s/%s",
                                dpdk_db_dir, OVS_VSWITCHD_CTL);
  snprintf(arg[5], MAX_ARG_LEN-1, "--detach");
  call_my_popen(dpdk_db_dir, 6, arg);
  return (wait_read_pid_file(dpdk_db_dir, OVS_VSWITCHD_PID));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_lan(t_all_ctx *all_ctx, char *ovsx_bin,
                      char *dpdk_db_dir, char *lan_name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
           "-- add-br brlan_%s "
           "-- set bridge brlan_%s datapath_type=netdev", 
           lan_name, lan_name);
  if (ovs_vsctl(all_ctx, ovsx_bin, dpdk_db_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_lan(t_all_ctx *all_ctx, char *ovsx_bin,
                      char *dpdk_db_dir, char *lan_name)
{
  int result = 0;
  char cmd[MAX_ARG_LEN]; 
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br brlan_%s", lan_name);
  if (ovs_vsctl(all_ctx, ovsx_bin, dpdk_db_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_lan_eth(t_all_ctx *all_ctx, char *ovsx_bin,
                          char *dpdk_db_dir, char *lan_name,
                          char *vm_name, int num)
{
  int result = 0;
  char cmd[MAX_ARG_LEN]; 
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
  "-- add-port brlan_%s patch_%s_%s_%d "
  "-- set interface patch_%s_%s_%d type=patch options:peer=patch_%s_%d_%s "
  "-- add-port br_%s_%d patch_%s_%d_%s "
  "-- set interface patch_%s_%d_%s type=patch options:peer=patch_%s_%s_%d",
  lan_name, lan_name, vm_name, num,
  lan_name, vm_name, num, vm_name, num, lan_name,
  vm_name, num, vm_name, num, lan_name,
  vm_name, num, lan_name, lan_name, vm_name, num);
  if (ovs_vsctl(all_ctx, ovsx_bin, dpdk_db_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_lan_eth(t_all_ctx *all_ctx, char *ovsx_bin,
                          char *dpdk_db_dir, char *lan_name,
                          char *vm_name, int num)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
                "-- del-port brlan_%s patch_%s_%s_%d "
                "-- del-port br_%s_%d patch_%s_%d_%s",
                lan_name, lan_name, vm_name, num,
                vm_name, num, vm_name, num, lan_name);
  if (ovs_vsctl(all_ctx, ovsx_bin, dpdk_db_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_eth(t_all_ctx *all_ctx, char *ovsx_bin,
                      char *dpdk_db_dir, char *name, int num)
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
             name, i, name, i, name, i, name, i,
             name, i, dpdk_db_dir, name, i);
    if (ovs_vsctl(all_ctx, ovsx_bin, dpdk_db_dir, cmd)) 
      result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_eth(t_all_ctx *all_ctx, char *ovsx_bin,
                      char *dpdk_db_dir, char *name, int num)
{
  char cmd[MAX_ARG_LEN];
  int i, result = 0;
  for (i=0; (result==0)&&(i<num); i++)
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1, "del-br br_%s_%d", name, i);
    if (ovs_vsctl(all_ctx, ovsx_bin, dpdk_db_dir, cmd))
      result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_daemon(t_all_ctx *all_ctx, char *ovsx_bin, char *dpdk_db_dir)
{
  int pid;
  init_environ(dpdk_db_dir);
  if (!strcmp(all_ctx->g_name, "ovs"))
    {
    umask (0000);
    seteuid(0);
    setegid(0);
    wait_for_server_sock(dpdk_db_dir);
    pid = launch_ovs_vswitchd(all_ctx, ovsx_bin, dpdk_db_dir);
    }
  else
    {
    unlink_files(dpdk_db_dir);
    create_dpdk_dir(dpdk_db_dir);
    create_ovsdb_server_conf(all_ctx, ovsx_bin, dpdk_db_dir);
    pid = launch_ovs_server(all_ctx, ovsx_bin, dpdk_db_dir);
    ovs_vsctl(all_ctx, ovsx_bin, dpdk_db_dir, 
    "--verbose --no-wait init");
    ovs_vsctl(all_ctx, ovsx_bin, dpdk_db_dir,
    "--verbose --no-wait set Open_vSwitch . other_config:pmd-cpu-mask=0x02");
    ovs_vsctl(all_ctx, ovsx_bin, dpdk_db_dir,
    "--verbose --no-wait set Open_vSwitch . other_config:dpdk-lcore-mask=0x01");
    ovs_vsctl(all_ctx, ovsx_bin, dpdk_db_dir,
    "--verbose --no-wait set Open_vSwitch . other_config:dpdk-socket-mem=512");
    ovs_vsctl(all_ctx, ovsx_bin, dpdk_db_dir,
    "--verbose --no-wait set Open_vSwitch . other_config:dpdk-init=true");
    }
  return pid;
}
/*---------------------------------------------------------------------------*/
