/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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


#include "ioc.h"
#include "ovs_execv.h"

#define OVSDB_SERVER_BIN  "bin/ovsdb-server"
#define OVS_VSWITCHD_BIN  "bin/ovs-vswitchd"
#define OVSDB_TOOL_BIN    "bin/ovsdb-tool"
#define SCHEMA_TEMPLATE   "share/openvswitch/vswitch.ovsschema"

#define OVSDB_SERVER_SOCK "ovsdb_server.sock"
#define OVSDB_SERVER_1CONF "ovsdb_server_1conf"
#define OVSDB_SERVER_2CONF "ovsdb_server_2conf"
#define OVSDB_SERVER_PID  "ovsdb_server.pid"
#define OVSDB_SERVER_CTL  "ovsdb_server.ctl"
#define OVS_VSWITCHD_PID  "ovs-vswitchd.pid"
#define OVS_VSWITCHD_LOG  "ovs-vswitchd.log"
#define OVS_VSWITCHD_CTL  "ovs-vswitchd.ctl"

int init_module(void *module_image, unsigned long len,
                       const char *param_values);

static char g_arg[NB_ARG][MAX_ARG_LEN];

static int g_with_dpdk;

/*****************************************************************************/
int call_my_popen(char *dpdk_dir, int nb, char arg[NB_ARG][MAX_ARG_LEN]);
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
static int ovs_vsctl(char *ovs_bin, char *dpdk_dir, char *multi_arg_cmd)
{
  int i, cmd_argc;
  char cmd_argv[NB_ARG][MAX_ARG_LEN];
  memset(g_arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  memset(cmd_argv, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  cmd_argc = make_cmd_argv(cmd_argv, multi_arg_cmd);
  snprintf(g_arg[0],MAX_ARG_LEN-1,"%s/bin/ovs-vsctl", ovs_bin);
  snprintf(g_arg[1],MAX_ARG_LEN-1,"--db=unix:%s/%s",
                                dpdk_dir, OVSDB_SERVER_SOCK);
  for (i=0; i<cmd_argc; i++)
    {
    strncpy(g_arg[2+i], cmd_argv[i], MAX_ARG_LEN-1);
    }
  return (call_my_popen(dpdk_dir, cmd_argc+2, g_arg));
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
static int launch_ovs_server(char *ovs_bin, char *dpdk_dir)
{
  int result;
  memset(g_arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  snprintf(g_arg[0],MAX_ARG_LEN-1,"%s/%s", ovs_bin, OVSDB_SERVER_BIN);
  if (g_with_dpdk)
    snprintf(g_arg[1],MAX_ARG_LEN-1,"%s/%s", dpdk_dir, OVSDB_SERVER_1CONF);
  else
    snprintf(g_arg[1],MAX_ARG_LEN-1,"%s/%s", dpdk_dir, OVSDB_SERVER_2CONF);
  snprintf(g_arg[2],MAX_ARG_LEN-1,"--pidfile=%s/%s",
                                  dpdk_dir, OVSDB_SERVER_PID);
  snprintf(g_arg[3],MAX_ARG_LEN-1,"--remote=punix:%s/%s",
                                  dpdk_dir, OVSDB_SERVER_SOCK);
  snprintf(g_arg[4], MAX_ARG_LEN-1,
           "--remote=db:Open_vSwitch,Open_vSwitch,manager_options");
  snprintf(g_arg[5], MAX_ARG_LEN-1,"--unixctl=%s/%s",
                                   dpdk_dir, OVSDB_SERVER_CTL);
  snprintf(g_arg[6], MAX_ARG_LEN-1, "--detach");
  if (call_my_popen(dpdk_dir, 7, g_arg))
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
  int result;
  memset(g_arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char)); 
  snprintf(g_arg[0],MAX_ARG_LEN-1,"%s/%s", ovs_bin, OVS_VSWITCHD_BIN);
  snprintf(g_arg[1],MAX_ARG_LEN-1,"unix:%s/%s", dpdk_dir, OVSDB_SERVER_SOCK);
  snprintf(g_arg[2],MAX_ARG_LEN-1,"--pidfile=%s/%s", dpdk_dir, OVS_VSWITCHD_PID);
  snprintf(g_arg[3],MAX_ARG_LEN-1,"--log-file=%s/%s", dpdk_dir, OVS_VSWITCHD_LOG);
  snprintf(g_arg[4],MAX_ARG_LEN-1,"--unixctl=%s/%s", dpdk_dir, OVS_VSWITCHD_CTL);
  snprintf(g_arg[5], MAX_ARG_LEN-1, "--detach");
  if (call_my_popen(dpdk_dir, 6, g_arg))
    {
    result = -1;
    } 
  else
    {
    result = wait_read_pid_file(dpdk_dir, OVS_VSWITCHD_PID);
    appctl_debug_fix(ovs_bin, dpdk_dir, "ANY:syslog:warn");
    appctl_debug_fix(ovs_bin, dpdk_dir, "ANY:file:info");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int module_is_inserted(char *name)
{
  FILE* fp;
  size_t bytes_read;
  char *file_data;
  int len = 50000, result = 1;
  fp = fopen ("/proc/modules", "r");
  if (fp == NULL)
    KERR("ERROR FOPEN: %s", "/proc/modules");
  else
    {
    file_data = malloc(len);
    bytes_read = fread(file_data, 1, len, fp);
    fclose (fp);
    if ((bytes_read == 0) || (bytes_read == len)) 
      KERR("ERROR FREAD: %d %d", (int) bytes_read, len);
    else if (strstr(file_data, name) == NULL)
      result = 0;
    free(file_data);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_module_path(char *ovs_bin, char path[MAX_ARG_LEN], char *name)
{
  struct utsname buf;
  char kern[MAX_NAME_LEN+2];
  int result = -1;
  if (uname(&buf))
    KERR("No kernel version, %s will not be inserted", name);
  else
    { 
    memset(path, 0, MAX_ARG_LEN);
    memset(kern, 0, MAX_NAME_LEN+2);
    strncpy(kern, buf.release, MAX_NAME_LEN+1);
    if (!strcmp(name, "vfio"))
      snprintf(path, MAX_ARG_LEN-1,
               "/lib/modules/%s/kernel/drivers/vfio/vfio.ko", kern); 
    else if (!strcmp(name, "vfio_iommu_type1"))
      snprintf(path, MAX_ARG_LEN-1,
               "/lib/modules/%s/kernel/drivers/vfio/vfio_iommu_type1.ko", kern);
    else if (!strcmp(name, "vfio_virqfd"))
      snprintf(path, MAX_ARG_LEN-1,
               "/lib/modules/%s/kernel/drivers/vfio/vfio_virqfd.ko", kern);
    else if (!strcmp(name, "vfio_pci"))
      snprintf(path, MAX_ARG_LEN-1,
               "/lib/modules/%s/kernel/drivers/vfio/pci/vfio-pci.ko", kern); 
    else
      KERR("MODULE NOT KNOWN %s", name);
    if (strlen(path))
      {
      if (access(path, F_OK))
        KERR("DOES NOT EXISTS: %s", path);
      else
        result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void insert_module(char *ovs_bin, char *mod_name)
{
  char mod_path[MAX_ARG_LEN];
  int fd;
  size_t image_size;
  struct stat st;
  void *image;
  if (!module_is_inserted(mod_name))
    {
    if (get_module_path(ovs_bin, mod_path, mod_name))
      KERR("Error getting path for %s", mod_name);
    else
      {
      fd = open(mod_path, O_RDONLY);
      if (fd < 0)
        KERR("ERROR OPEN: %s", mod_path);
      else
        {
        if (fstat(fd, &st))
          KERR("ERROR FSTAT: %s", mod_path);
        else
          {
          image_size = st.st_size;
          image = malloc(image_size);
          read(fd, image, image_size);
          close(fd);
          if (init_module(image, image_size, ""))
            KERR("ERROR INSMOD: %s", mod_path);
          free(image);
          }
        }
      }
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int dpdk_is_usable(void)
{
  return g_with_dpdk;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void force_dpdk_off(void)
{
  g_with_dpdk = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_pci_dpdk(char *ovs, char *dpdk, char *lan, char *pci)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, 
           "-- add-port %s %s "
           "-- set Interface %s type=dpdk options:dpdk-devargs=%s",
           lan, pci, pci, pci);
  if (ovs_vsctl(ovs, dpdk, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_pci_dpdk(char *ovs, char *dpdk, char *lan, char *pci)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-port %s %s", lan, pci);
  if (ovs_vsctl(ovs, dpdk, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int create_ovsdb_server_conf(char *ovs_bin, char *dpdk_dir)
{
  memset(g_arg, 0, NB_ARG * MAX_ARG_LEN * sizeof(char));
  snprintf(g_arg[0],MAX_ARG_LEN-1,"%s/%s", ovs_bin, OVSDB_TOOL_BIN);
  snprintf(g_arg[1],MAX_ARG_LEN-1,"create");
  if (g_with_dpdk)
    snprintf(g_arg[2],MAX_ARG_LEN-1,"%s/%s", dpdk_dir, OVSDB_SERVER_1CONF);
  else
    snprintf(g_arg[2],MAX_ARG_LEN-1,"%s/%s", dpdk_dir, OVSDB_SERVER_2CONF);
  snprintf(g_arg[3],MAX_ARG_LEN-1,"%s/%s", ovs_bin, SCHEMA_TEMPLATE);
  return (call_my_popen(dpdk_dir, 4, g_arg));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_get_tap_mac(char *ifname, char *mac)
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
    for (i=0; i<MAC_ADDR_LEN; i++)
      mac[i] = ifr.ifr_hwaddr.sa_data[i];
    if (set_intf_flags_iff_up_down(ifname, 1))
      KERR("%s", ifname);
    else
      result = 0;
    }
  close(s);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_vhost_br(char *ovs, char *dpdk, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- add-br br_vhost_%s", lan);
  if (ovs_vsctl(ovs, dpdk, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_vhost_br(char *ovs, char *dpdk, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br br_vhost_%s", lan);
  if (ovs_vsctl(ovs, dpdk, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_vhost_br_port(char *ovs, char *dpdk, char *lan, char *vhost)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- add-port br_vhost_%s %s", lan, vhost);
  if (ovs_vsctl(ovs, dpdk, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_vhost_br_port(char *ovs, char *dpdk, char *lan, char *vhost)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-port br_vhost_%s %s", lan, vhost);
  if (ovs_vsctl(ovs, dpdk, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_lan(char *ovs_bin, char *dpdk_dir, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
           "-- add-br %s "
           "-- set bridge %s datapath_type=netdev", 
           lan, lan);
  if (ovs_vsctl(ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_lan(char *ovs_bin,
                      char *dpdk_dir, char *lan)
{
  int result = 0;
  char cmd[MAX_ARG_LEN]; 
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "-- del-br %s", lan);
  if (ovs_vsctl(ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_lan_eth(char *ovs_bin, char *dpdk_dir, char *lan,
                          char *name, int num)
{
  int result = 0;
  char cmd[MAX_ARG_LEN]; 
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
  "-- add-port %s patch_%s_%s_%d "
  "-- set interface patch_%s_%s_%d type=patch options:peer=patch_%s_%d_%s "
  "-- add-port br_%s_%d patch_%s_%d_%s "
  "-- set interface patch_%s_%d_%s type=patch options:peer=patch_%s_%s_%d",
  lan, lan, name, num,
  lan, name, num, name, num, lan,
  name, num, name, num, lan,
  name, num, lan, lan, name, num);
  if (ovs_vsctl(ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_lan_eth(char *ovs_bin, char *dpdk_dir, char *lan,
                          char *name, int num)
{
  int result = 0;
  char cmd[MAX_ARG_LEN];
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
                "-- del-port %s patch_%s_%s_%d "
                "-- del-port br_%s_%d patch_%s_%d_%s",
                lan, lan, name, num,
                name, num, name, num, lan);
  if (ovs_vsctl(ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_eth(char *ovs_bin, char *dpdk_dir, char *name, int num)
{
  char cmd[MAX_ARG_LEN];
  int result = 0;
  
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
           "-- add-br br_%s_%d "
           "-- set bridge br_%s_%d datapath_type=netdev "
           "-- add-port br_%s_%d %s_%d "
           "-- set Interface %s_%d type=dpdkvhostuserclient"
           " options:vhost-server-path=%s_qemu/%s_%d"
           " options:n_rxq=%d",
           name, num, name, num,
           name, num, name, num, name, num,
           dpdk_dir, name, num, MQ_QUEUES);
  if (ovs_vsctl(ovs_bin, dpdk_dir, cmd)) 
    result = -1;

  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_eth(char *ovs_bin, char *dpdk_dir, char *name, int num)
{
  char cmd[MAX_ARG_LEN];
  int result = 0;
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1, "del-br br_%s_%d", name, num);
  if (ovs_vsctl(ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_tap(char *ovs_bin, char *dpdk_dir, char *name)
{
  char cmd[MAX_ARG_LEN];
  int result = 0;
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
           "-- add-br br_%s "
           "-- set bridge br_%s datapath_type=netdev "
           "-- add-port br_%s %s "
           "-- set Interface %s type=dpdk"
           " options:dpdk-devargs=net_tap_%s,iface=%s"
           " options:n_rxq=%d",
           name, name, name, name, name, name, name, MQ_QUEUES);
  if (ovs_vsctl(ovs_bin, dpdk_dir, cmd))
    result = -1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_tap(char *ovs_bin, char *dpdk_dir, char *name)
{
  char cmd[MAX_ARG_LEN];
  int result = -1;
  if (!set_intf_flags_iff_up_down(name, 0))
    {
    memset(cmd, 0, MAX_ARG_LEN);
    snprintf(cmd, MAX_ARG_LEN-1, "del-br br_%s", name);
    if (!ovs_vsctl(ovs_bin, dpdk_dir, cmd))
      result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_add_lan_tap(char *ovs_bin, char *dpdk_dir, char *lan, char *name)
{
  char cmd[MAX_ARG_LEN];
  int result = -1;
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
           "-- add-port %s patch_%s_%s "
           "-- set interface patch_%s_%s type=patch options:peer=patch_%s_%s "
           "-- add-port br_%s patch_%s_%s "
           "-- set interface patch_%s_%s type=patch options:peer=patch_%s_%s",
           lan,lan,name, lan,name,name,lan, name,name,lan, name,lan,lan,name);
  if (!ovs_vsctl(ovs_bin, dpdk_dir, cmd))
    result = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_del_lan_tap(char *ovs_bin, char *dpdk_dir, char *lan, char *name)
{
  char cmd[MAX_ARG_LEN];
  int result = -1;
  memset(cmd, 0, MAX_ARG_LEN);
  snprintf(cmd, MAX_ARG_LEN-1,
           "-- del-port %s patch_%s_%s "
           "-- del-port br_%s patch_%s_%s",
           lan,lan,name, name,name,lan);
  if (!ovs_vsctl(ovs_bin, dpdk_dir, cmd))
    result = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int ovs_execv_daemon(int is_switch, char *net, char *ovs_bin, char *dpdk_dir)
{
  int pid, result = -1;
  char other_config_extra[MAX_ARG_LEN];
  
  init_environ(ovs_bin, dpdk_dir);
  if (is_switch)
    {
    wait_for_server_sock(dpdk_dir);
    pid = launch_ovs_vswitchd(ovs_bin, dpdk_dir);
    if (pid <= 0)
      KERR("Fail launch vswitchd server ");
    else
      result = pid;
    }
  else
    {
    pid = launch_ovs_server(ovs_bin, dpdk_dir);
    usleep(10000);
    if (pid <= 0)
      KERR("Fail launch ovsbd server ");
    else
      {
      result = pid;
      if (g_with_dpdk)
        {
        insert_module(ovs_bin, "vfio");
        insert_module(ovs_bin, "vfio_iommu_type1");
        insert_module(ovs_bin, "vfio_virqfd");
        insert_module(ovs_bin, "vfio_pci");
        memset(other_config_extra, 0, MAX_ARG_LEN);
        snprintf(other_config_extra, MAX_ARG_LEN-1,
                   "--no-wait set Open_vSwitch . "
                   "other_config:dpdk-extra=\"--file-prefix=cloonix%s\"", net);
        if (ovs_vsctl(ovs_bin, dpdk_dir, "--no-wait init"))
          result = -1;
        else if (ovs_vsctl(ovs_bin, dpdk_dir, other_config_extra))
          result = -1;
        else if (ovs_vsctl(ovs_bin, dpdk_dir, "--no-wait set Open_vSwitch . "
                 "other_config:vhost-sock-dir=."))
          result = -1;
        else if (ovs_vsctl(ovs_bin, dpdk_dir, "--no-wait set Open_vSwitch . "
                 "other_config:vhost-iommu-support=true"))
          result = -1;
        else if (ovs_vsctl(ovs_bin, dpdk_dir, "--no-wait set Open_vSwitch . "
                 "other_config:vhost-postcopy-support=true"))
          result = -1;
        else if (ovs_vsctl(ovs_bin, dpdk_dir, "--no-wait set Open_vSwitch . "
                 "other_config:pmd-cpu-mask=0x03"))
          result = -1;
        else if (ovs_vsctl(ovs_bin, dpdk_dir, "--no-wait set Open_vSwitch . "
                 "other_config:dpdk-lcore-mask=0x03"))
          result = -1;
        else if (ovs_vsctl(ovs_bin, dpdk_dir, "--no-wait set Open_vSwitch . "
                 "other_config:dpdk-socket-mem=1024"))
          result = -1;
        else if (ovs_vsctl(ovs_bin, dpdk_dir, "--no-wait set Open_vSwitch . "
                 "other_config:dpdk-init=true"))
          result = -1;
        } 
      }
    }
  return result;
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
