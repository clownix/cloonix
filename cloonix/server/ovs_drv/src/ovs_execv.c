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

#define RANDOM_APPEND_SIZE 6
#define OVSDB_SERVER_BIN  "bin/ovsdb-server"
#define OVS_VSWITCHD_BIN  "bin/ovs-vswitchd"
#define OVSDB_TOOL_BIN    "bin/ovsdb-tool"
#define SCHEMA_TEMPLATE   "vswitch.ovsschema"

#define OVSDB_SERVER_SOCK "ovsdb_server.sock"
#define OVSDB_SERVER_CONF "ovsdb_server_conf"
#define OVSDB_SERVER_PID  "ovsdb_server.pid"
#define OVSDB_SERVER_CTL  "ovsdb_server.ctl"
#define OVS_VSWITCHD_PID  "ovs-vswitchd.pid"
#define OVS_VSWITCHD_LOG  "ovs-vswitchd.log"
#define OVS_VSWITCHD_CTL  "ovs-vswitchd.ctl"

static char g_arg[NB_ARG][MAX_ARG_LEN];
static char g_ovsdb_server_conf[MAX_NAME_LEN];

static int g_with_dpdk;

/*****************************************************************************/
int call_my_popen(char *dpdk_dir, int nb, char arg[NB_ARG][MAX_ARG_LEN]);
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *random_str(void)
{
  static char rd[RANDOM_APPEND_SIZE+1];
  int i;
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  srand(ts.tv_nsec);
  memset (rd, 0 , RANDOM_APPEND_SIZE+1);
  for (i=0; i<RANDOM_APPEND_SIZE; i++)
    rd[i] = 'A' + (rand() % 26);
  return rd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int file_exists(char *path)
{
  int err, result = 0;
  err = access(path, F_OK);
  if (!err)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int huge_page_is_ok(void)
{
  int nb = 0, found = 0;
  char line[100];
  FILE *fh = fopen("/proc/meminfo", "r");
  if (fh == NULL)
    KERR("ERR fopen /proc/meminfo");
  else
    {
    while (fgets(line, 99, fh) != NULL)
      {
      if (sscanf(line, "HugePages_Total: %d", &nb) == 1)
        {
        found = 1;
        break;
        }
      }
    fclose(fh);
    }
  if (!found)
    KERR("ERR read /proc/meminfo");
  else if (nb == 0)
    KERR("ERR %s", line);
  return nb;
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
      {
      KERR("NO PID %s", name);
      result = -1;
      }
    usleep(100000);
    fhd = fopen(path, "r");
    if (fhd)
      {
      if (fscanf(fhd, "%d", &pid) == 1)
        result = pid;
      if (fclose(fhd))
        KERR("%s", strerror(errno));
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int make_cmd_argv(char cmd_argv[NB_ARG][MAX_ARG_LEN],char *multi_arg_cmd)
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
int ovs_vsctl(char *ovs_bin, char *dpdk_dir, char *multi_arg_cmd)
{
  int i, cmd_argc;
  char cmd_argv[NB_ARG][MAX_ARG_LEN];
  memset(g_arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  memset(cmd_argv, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  cmd_argc = make_cmd_argv(cmd_argv, multi_arg_cmd);
  snprintf(g_arg[0],MAX_ARG_LEN-1,"%s/bin/ovs-vsctl", ovs_bin);
  snprintf(g_arg[1],MAX_ARG_LEN-1,"--no-syslog");
  snprintf(g_arg[2],MAX_ARG_LEN-1,"--db=unix:%s/%s",
                                dpdk_dir, OVSDB_SERVER_SOCK);
  for (i=0; i<cmd_argc; i++)
    {
    strncpy(g_arg[3+i], cmd_argv[i], MAX_ARG_LEN-1);
    }
  return (call_my_popen(dpdk_dir, cmd_argc+3, g_arg));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_appctl(char *ovs_bin, char *dpdk_dir, char *multi_arg_cmd)
{
  int i, cmd_argc;
  char cmd_argv[NB_ARG][MAX_ARG_LEN];
  memset(g_arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  memset(cmd_argv, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  cmd_argc = make_cmd_argv(cmd_argv, multi_arg_cmd);
  snprintf(g_arg[0],MAX_ARG_LEN-1,"%s/bin/ovs-appctl", ovs_bin);
  snprintf(g_arg[1],MAX_ARG_LEN-1,"--target=%s/%s",
                                dpdk_dir, OVS_VSWITCHD_CTL);
  for (i=0; i<cmd_argc; i++)
    {
    strncpy(g_arg[2+i], cmd_argv[i], MAX_ARG_LEN-1);
    }
  return (call_my_popen(dpdk_dir, cmd_argc+2, g_arg));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int launch_ovsdb_server(char *ovs_bin, char *dpdk_dir)
{
  int result = -1;
  char *pidfile = get_pidfile_ovsdb(dpdk_dir);
  if (file_exists(pidfile))
    unlink(get_pidfile_ovs(dpdk_dir));
  memset(g_arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  snprintf(g_arg[0],MAX_ARG_LEN-1,"%s/%s", ovs_bin, OVSDB_SERVER_BIN);
  if (!file_exists(g_arg[0]))
    KOUT("MISSING %s", g_arg[0]);
  snprintf(g_arg[1],MAX_ARG_LEN-1,"%s/%s", dpdk_dir, g_ovsdb_server_conf);
  snprintf(g_arg[2],MAX_ARG_LEN-1,"--pidfile=%s/%s",
                                  dpdk_dir, OVSDB_SERVER_PID);
  snprintf(g_arg[3],MAX_ARG_LEN-1,"--remote=punix:%s/%s",
                                  dpdk_dir, OVSDB_SERVER_SOCK);
  snprintf(g_arg[4], MAX_ARG_LEN-1,
           "--remote=db:Open_vSwitch,Open_vSwitch,manager_options");
  snprintf(g_arg[5], MAX_ARG_LEN-1,"--unixctl=%s/%s",
                                   dpdk_dir, OVSDB_SERVER_CTL);
  snprintf(g_arg[6], MAX_ARG_LEN-1, "--verbose=err");
  snprintf(g_arg[7], MAX_ARG_LEN-1, "--detach");
  result = call_my_popen(dpdk_dir, 8, g_arg);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void appctl_debug_fix(char *ovs_bin, char *dpdk_dir, char *debug_cmd)
{
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "vlog/set %s", debug_cmd);
  if (ovs_appctl(ovs_bin, dpdk_dir, cmd))
    KERR("%s", debug_cmd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int launch_ovs_vswitchd(char *ovs_bin, char *dpdk_dir)
{
  int result = -1;
  char *pidfile = get_pidfile_ovs(dpdk_dir);
  if (file_exists(pidfile))
    {
    KERR("PIDFILE: %s EXISTS! ERROR", pidfile);
    unlink(get_pidfile_ovs(dpdk_dir));
    }
  memset(g_arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char)); 
  snprintf(g_arg[0],MAX_ARG_LEN-1,"%s/%s", ovs_bin, OVS_VSWITCHD_BIN);
  if (!file_exists(g_arg[0]))
    KOUT("MISSING %s", g_arg[0]);
  snprintf(g_arg[1],MAX_ARG_LEN-1,"unix:%s/%s", dpdk_dir, OVSDB_SERVER_SOCK);
  snprintf(g_arg[2],MAX_ARG_LEN-1,"--pidfile=%s/%s",dpdk_dir,OVS_VSWITCHD_PID);
  snprintf(g_arg[3],MAX_ARG_LEN-1,"--log-file=%s/%s",dpdk_dir,OVS_VSWITCHD_LOG);
  snprintf(g_arg[4],MAX_ARG_LEN-1,"--unixctl=%s/%s",dpdk_dir,OVS_VSWITCHD_CTL);
  snprintf(g_arg[5],MAX_ARG_LEN-1,"--verbose=err");
  snprintf(g_arg[6],MAX_ARG_LEN-1,"--detach");
  result = call_my_popen(dpdk_dir, 7, g_arg);
  if (!result)
    {
    appctl_debug_fix(ovs_bin, dpdk_dir, "ANY:syslog:err");
    appctl_debug_fix(ovs_bin, dpdk_dir, "ANY:file:info");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_is_usable(void)
{
  return g_with_dpdk;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int create_ovsdb_server_conf(char *ovs_bin, char *dpdk_dir)
{
  memset(g_ovsdb_server_conf, 0, MAX_NAME_LEN);
  snprintf(g_ovsdb_server_conf, MAX_NAME_LEN-1,
           "%s%s", OVSDB_SERVER_CONF, random_str());
  memset(g_arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  snprintf(g_arg[0],MAX_ARG_LEN-1,"%s/%s", ovs_bin, OVSDB_TOOL_BIN);
  if (!file_exists(g_arg[0]))
    KOUT("MISSING %s", g_arg[0]);
  snprintf(g_arg[1],MAX_ARG_LEN-1,"create");
  snprintf(g_arg[2],MAX_ARG_LEN-1,"%s/%s", dpdk_dir, g_ovsdb_server_conf);
  snprintf(g_arg[3],MAX_ARG_LEN-1,"%s/%s", ovs_bin, SCHEMA_TEMPLATE);
  return (call_my_popen(dpdk_dir, 4, g_arg));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int send_config_ovs( char *net, char *ovs_bin, char *dpdk_dir,
                            uint32_t lcore_mask, uint32_t socket_mem,
                            uint32_t cpu_mask)
{
  int result = 0;
  char other_config_extra[MAX_ARG_LEN];
  /*---------------------------------------------------------*/
  if (result != -1)
    {
    memset(other_config_extra, 0, MAX_ARG_LEN);
    snprintf(other_config_extra, MAX_ARG_LEN-1,
             "--no-wait set Open_vSwitch . "
             "other_config:dpdk-extra=\"--iova-mode=va\"");
    if (ovs_vsctl(ovs_bin, dpdk_dir, other_config_extra))
      {
      KERR("Fail launch ovsbd server ");
      result = -1;
      }
    }
  /*---------------------------------------------------------*/
  if (result != -1)
    {
    memset(other_config_extra, 0, MAX_ARG_LEN);
    snprintf(other_config_extra, MAX_ARG_LEN-1,
             "--no-wait set Open_vSwitch . "
             "other_config:dpdk-extra=\"--proc-type=primary\"");
    if (ovs_vsctl(ovs_bin, dpdk_dir, other_config_extra))
      {
      KERR("Fail launch ovsbd server ");
      result = -1;
      }
    }
  /*---------------------------------------------------------*/
  if (result != -1)
    {
    memset(other_config_extra, 0, MAX_ARG_LEN);
    snprintf(other_config_extra, MAX_ARG_LEN-1,
             "--no-wait set Open_vSwitch . "
             "other_config:dpdk-extra=\"--file-prefix=cloonix%s\"", net);
    if (ovs_vsctl(ovs_bin, dpdk_dir, other_config_extra))
      { 
      KERR("Fail launch ovsbd server ");
      result = -1;
      }
    } 
  /*---------------------------------------------------------*/
  if (result != -1)
    {  
    if (ovs_vsctl(ovs_bin, dpdk_dir,
                  "--no-wait set Open_vSwitch . "
                  "other_config:vhost-sock-dir=."))
      {
      KERR("Fail launch ovsbd server ");
      result = -1;
      }
    }
  /*---------------------------------------------------------*/
  if (result != -1)
    {
    if (ovs_vsctl(ovs_bin, dpdk_dir,
                     "--no-wait set Open_vSwitch . "
                     "other_config:tso-support=true"))
      {
      KERR("Fail launch ovsbd server ");
      result = -1;
      }
    }

  /*---------------------------------------------------------*/
  if (result != -1)
    {
    if (ovs_vsctl(ovs_bin, dpdk_dir,
                     "--no-wait set Open_vSwitch . "
                     "other_config:bond-rebalance-interval=0"))
      {
      KERR("Fail launch ovsbd server ");
      result = -1;
      }
    }
  /*---------------------------------------------------------*/
  if (result != -1)
    {
    if (ovs_vsctl(ovs_bin, dpdk_dir,
                     "--no-wait set Open_vSwitch . "
                     "other_config:userspace-tso-enable=true"))
      {
      KERR("Fail launch ovsbd server ");
      result = -1;
      }
    }
  /*---------------------------------------------------------*/
  if (result != -1)
    {
    memset(other_config_extra, 0, MAX_ARG_LEN);
    snprintf(other_config_extra, MAX_ARG_LEN-1,
             "--no-wait set Open_vSwitch . "
             "other_config:dpdk-lcore-mask=0x%x", lcore_mask);
    if (ovs_vsctl(ovs_bin, dpdk_dir, other_config_extra))
      {
      KERR("Fail launch ovsbd server ");
      result = -1;
      }
    }
  /*---------------------------------------------------------*/
  if (result != -1)
    {
    memset(other_config_extra, 0, MAX_ARG_LEN);
    snprintf(other_config_extra, MAX_ARG_LEN-1,
             "--no-wait set Open_vSwitch . "
             "other_config:dpdk-socket-mem=%d", socket_mem);
    if (ovs_vsctl(ovs_bin, dpdk_dir, other_config_extra))
      {
      KERR("Fail launch ovsbd server ");
      result = -1;
      }
    }
  /*---------------------------------------------------------*/
  if (result != -1)
    {
    memset(other_config_extra, 0, MAX_ARG_LEN);
    snprintf(other_config_extra, MAX_ARG_LEN-1,
             "--no-wait set Open_vSwitch . "
             "other_config:pmd-cpu-mask=0x%x", cpu_mask);
    if (ovs_vsctl(ovs_bin, dpdk_dir, other_config_extra))
      {
      KERR("Fail launch ovsbd server ");
      result = -1;
      }
    }
  /*---------------------------------------------------------*/
  if (result != -1)
    {
    if (ovs_vsctl(ovs_bin, dpdk_dir,
                  "--no-wait set Open_vSwitch . "
                  "other_config:dpdk-init=true"))
      {
      KERR("Fail launch ovsbd server ");
      result = -1;
      }
    }
  /*---------------------------------------------------------*/
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void clean_and_wait(char *dpdk_dir, int pid)
{
  KERR("ERROR, CLEAN OVS AND OVSDB");
  kill(pid, SIGKILL);
  while (!kill(pid, 0))
    usleep(5000);
  unlink(get_pidfile_ovs(dpdk_dir));
  usleep(10000);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_ovs_vswitchd(char *net, char *ovs_bin, char *dpdk_dir,
                           uint32_t lcore_mask, uint32_t socket_mem,
                           uint32_t cpu_mask)
{
  int pid, result = -1;
  init_environ(net, ovs_bin, dpdk_dir);
  if (launch_ovs_vswitchd(ovs_bin, dpdk_dir))
    {
    KERR("ERROR Fail launch vswitchd server ");
    pid = wait_read_pid_file(dpdk_dir, OVS_VSWITCHD_PID);
    if (pid > 0)
      {
      clean_and_wait(dpdk_dir, pid);
      if (launch_ovs_vswitchd(ovs_bin, dpdk_dir))
        {
        KERR("ERROR SECOND Fail launch vswitchd server ");
        pid = wait_read_pid_file(dpdk_dir, OVS_VSWITCHD_PID);
        }
      }
    else
      KERR("ERROR Bad pid vswitchd server ");
    }
  else
    {
    pid = wait_read_pid_file(dpdk_dir, OVS_VSWITCHD_PID);
    }
  if (pid <= 0)
    {
    KERR("ERROR Fail launch vswitchd server ");
    }
  else
    {
    result = pid;
    if (send_config_ovs(net, ovs_bin, dpdk_dir, lcore_mask,
                        socket_mem, cpu_mask))
      KERR("ERROR Fail Configure ovsbd server ");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_ovsdb_server(char *net, char *ovs_bin, char *dpdk_dir,
                           uint32_t lcore_mask, uint32_t socket_mem,
                           uint32_t cpu_mask)
{
  int pid, result = -1;
  init_environ(net, ovs_bin, dpdk_dir);
  if (launch_ovsdb_server(ovs_bin, dpdk_dir))
    KERR("ERROR Fail launch ovsbd server ");
  else
    {
    pid = wait_read_pid_file(dpdk_dir, OVSDB_SERVER_PID);
    if (pid <= 0)
      {
      KERR("ERROR Fail launch ovsbd server ");
      }
    else if (ovs_vsctl(ovs_bin, dpdk_dir, "--no-wait init"))
      {
      KERR("ERROR Fail launch init ovsbd server ");
      result = -1;
      }
    else
      {
      result = pid;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_pidfile_ovsdb(char *dpdk_dir)
{
  static char pidfile[MAX_PATH_LEN];
  memset(pidfile, 0, MAX_PATH_LEN);
  snprintf(pidfile, MAX_PATH_LEN-1, "%s/%s", dpdk_dir, OVSDB_SERVER_PID);
  return pidfile;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_pidfile_ovs(char *dpdk_dir)
{
  static char pidfile[MAX_PATH_LEN];
  memset(pidfile, 0, MAX_PATH_LEN);
  snprintf(pidfile, MAX_PATH_LEN-1, "%s/%s", dpdk_dir, OVS_VSWITCHD_PID);
  return pidfile;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void ovs_execv_init(void)
{
  g_with_dpdk = 0;
  if (huge_page_is_ok())
    g_with_dpdk = 1;
}
/*---------------------------------------------------------------------------*/
