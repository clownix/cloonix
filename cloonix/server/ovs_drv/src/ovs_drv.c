/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include <errno.h>
#include <linux/if_tun.h>
#include <linux/if_arp.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <time.h>
#include <sched.h>
#include <sys/mount.h>
#include <libgen.h>
#include <dirent.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "ovs_execv.h"
#include "action.h"


static char g_net_name[MAX_NAME_LEN];
static char g_netns_namespace[MAX_NAME_LEN];
static char g_ovs_bin[MAX_PATH_LEN];
static char g_ovs_dir[MAX_PATH_LEN];
static int g_ovsdb_launched;
static int g_ovs_launched;
static int g_cloonix_listen_fd;
static int g_cloonix_llid;
static int g_cloonix_fd;
static int g_netns_pid;
static int g_ovsdb_pid;
static int g_ovs_pid;
static char g_name[MAX_NAME_LEN];
static char g_path[MAX_PATH_LEN];
static int g_fd_tx_to_netns;
static uint8_t g_buf_rx[TOT_NETNS_BUF_LEN];
static uint8_t g_buf_tx[TOT_NETNS_BUF_LEN];
static uint8_t g_head_msg[HEADER_NETNS_MSG];

int netns_open(char *net_namespace, int *fd_rx, int *fd_tx,
                char *ovs_bin, char *ovs_dir);


/*****************************************************************************/
char *get_ns(void)
{
  return g_netns_namespace;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_net_name(void)
{
  return g_net_name;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void change_mac_macvlan(char *ovs_dir, char *name, char *mac)
{
  char *argv[NB_ARG];
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "-netns";
  argv[2] = get_ns();
  argv[3] = "link";
  argv[4] = "set";
  argv[5] = "dev";
  argv[6] = name;
  argv[7] = "addr";
  argv[8] = mac;
  if (call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 3))
    KERR("ERROR change_mac_macvlan %s %s", name, mac);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int put_in_namespace(char *ovs_dir, char *name)
{
  char *argv[NB_ARG];
  int result;
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "link";
  argv[2] = "set";
  argv[3] = name;
  argv[4] = "netns";
  argv[5] = get_ns();
  result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 3);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int set_dev_up_in_namespace(char *ovs_dir, char *name)
{
  char *argv[NB_ARG];
  int result;
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "-netns";
  argv[2] = get_ns();
  argv[3] = "link";
  argv[4] = "set";
  argv[5] = "dev";
  argv[6] = name;
  argv[7] = "up";
  result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 3);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int set_dev_down_in_namespace(char *ovs_dir, char *name)
{
  char *argv[NB_ARG];
  int result;
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "-netns";
  argv[2] = get_ns();
  argv[3] = "link";
  argv[4] = "set";
  argv[5] = "dev";
  argv[6] = name;
  argv[7] = "down";
  result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 3);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int set_dev_up_link(char *ovs_dir, char *name)
{
  char *argv[NB_ARG];
  int result;
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "link";
  argv[2] = "set";
  argv[3] = "dev";
  argv[4] = name;
  argv[5] = "up";
  result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 4);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int change_name_within_namespace(char *ovs_dir, char *onm, char *nnm)
{
  char *argv[NB_ARG];
  int result;
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "-netns";
  argv[2] = get_ns();
  argv[3] = "link";
  argv[4] = "set";
  argv[5] = onm;
  argv[6] = "name";
  argv[7] = nnm;
  result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 3);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int put_back_in_main_namespace(char *ovs_dir, char *name)
{ 
  char *argv[NB_ARG];
  int result;
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "-netns";
  argv[2] = get_ns();
  argv[3] = "link";
  argv[4] = "set";
  argv[5] = name;
  argv[6] = "netns";
  argv[7] = "1";
  result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 3);
  return result;
} 
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int change_mac_address(char *ovs_dir, char *name, char *mac)
{
  char *argv[NB_ARG];
  int result;
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "link";
  argv[2] = "set";
  argv[3] = "dev";
  argv[4] = name;
  argv[5] = "address";
  argv[6] = mac;
  result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 3);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_cmd_add_tap(char *ovs_bin, char *ovs_dir, char *name,
                           char *vhost, char *mac)
{
  int result = 0;
  char *argv[NB_ARG];
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "link";
  argv[2] = "add";
  argv[3] = "name";
  argv[4] = name;
  argv[5] = "address";
  argv[6] = mac;
  argv[7] = "type";
  argv[8] = "veth";
  argv[9] = "peer";
  argv[10]= "name";
  argv[11]= vhost;
  result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 1);
  if (result)
    KERR("ERROR %s %s", name, mac);
  else
    {
    result = put_in_namespace(ovs_dir, vhost);
    if (result)
      KERR("ERROR %s %s", name, mac);
    else
      {
      result = set_dev_up_in_namespace(ovs_dir, vhost);
      if (result)
        KERR("ERROR %s %s", name, mac);
      else
        {
        result = set_dev_up_link(ovs_dir, name);
        if (result)
          KERR("ERROR %s %s", name, mac);
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_cmd_del_tap(char *ovs_bin, char *ovs_dir, char *name, char *vhost)
{
  int result;
  char *argv[NB_ARG];
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "link";
  argv[2] = "del";
  argv[3] = "name";
  argv[4] = name;
  result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 1);
  if (result)
    KERR("ERROR %s %s", name, vhost);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_cmd_add_aphy(char *ovs_bin, char *ovs_dir, char *name,
                            char *vhost, char *mac)
{
  int result;

  if (change_mac_address(ovs_dir, name, mac))
    KERR("ERROR %s %s %s", name, vhost, mac);
  else if (put_in_namespace(ovs_dir, name))
    KERR("ERROR %s %s %s", name, vhost, mac);
  else if (change_name_within_namespace(ovs_dir, name, vhost))
    KERR("ERROR %s %s %s", name, vhost, mac);
  else
    {
    result = set_dev_up_in_namespace(ovs_dir, vhost);
    if (result)
      KERR("ERROR %s %s", name, mac);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_cmd_del_aphy(char *ovs_bin, char *ovs_dir,
                            char *name, char *vhost)
{
  int result;
  if (set_dev_down_in_namespace(ovs_dir, vhost))
    KERR("ERROR %s %s", name, vhost);
  else if (change_name_within_namespace(ovs_dir, vhost, name))
    KERR("ERROR %s %s", name, vhost);
  else
    {
    result = put_back_in_main_namespace(ovs_dir, name);
    if (result)
      KERR("ERROR %s %s", name, vhost);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_cmd_add_mphy(char *ovs_bin, char *ovs_dir, char *name,
                            char *vhost, char *mac)
{
  static int tmpnum=0;
  int result = 0;
  char *argv[NB_ARG];
  char macvlan[MAX_NAME_LEN];

  tmpnum += 1;
  if (tmpnum == 100)
    tmpnum = 1;
  memset(macvlan, 0, MAX_NAME_LEN);
  snprintf(macvlan, MAX_NAME_LEN-1, "mcvl%d", tmpnum);
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "link";
  argv[2] = "add";
  argv[3] = macvlan;
  argv[4] = "link";
  argv[5] = name;
  argv[6] = "type";
  argv[7] = "macvlan";
  argv[8] = "mode";
  argv[9] = "bridge";
  result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 2);
  if (result)
    KERR("ERROR %s %s", name, mac);
  else
    {
    result = change_mac_address(ovs_dir, macvlan, mac);
    if (result)
      KERR("ERROR %s %s", name, mac);
    else
      {
      result = put_in_namespace(ovs_dir, macvlan);
      if (result)
        KERR("ERROR %s %s", name, mac);
      else
        {
        result = change_name_within_namespace(ovs_dir, macvlan, vhost);
        if (result)
          KERR("ERROR %s %s", name, mac);
        else
          {
          result = set_dev_up_in_namespace(ovs_dir, vhost);
          if (result)
             KERR("ERROR %s %s", name, mac);
          else
            {
            result = set_dev_up_link(ovs_dir, name);
            if (result)
              KERR("ERROR %s %s", name, mac);
            }
          }
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int ovs_cmd_del_mphy(char *ovs_bin, char *ovs_dir,
                            char *name, char *vhost)
{
  int result = 0;
  char *argv[NB_ARG];
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "-netns";
  argv[2] = get_ns();
  argv[3] = "link";
  argv[4] = "set";
  argv[5] = "dev";
  argv[6] = vhost;
  argv[7] = "down";
  result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 3);
  if (result)
    KERR("ERROR %s %s", name, vhost);
  else
    {
    memset(argv, 0, NB_ARG * sizeof(char *));
    argv[0] = IP_BIN;
    argv[1] = "-netns";
    argv[2] = get_ns();
    argv[3] = "link";
    argv[4] = "del";
    argv[5] = vhost;
    result = call_ovs_popen(ovs_dir, argv, 2, __FUNCTION__, 3);
    if (result)
      KERR("ERROR %s %s", name, vhost);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void action_add_tap(char *bin, char *db, char *respb,
                           char *name, char *vhost, char *mac)
{
  if (ovs_cmd_add_tap(bin, db, name, vhost, mac))
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "KO ovs_add_tap name=%s vhost=%s mac=%s", name, vhost, mac);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "OK ovs_add_tap name=%s vhost=%s mac=%s", name, vhost, mac);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void action_del_tap(char *bin, char *db, char *respb,
                           char *name, char *vhost)
{
  if (ovs_cmd_del_tap(bin, db, name, vhost))
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "KO ovs_del_tap name=%s vhost=%s", name, vhost);
    }
  else
    {
    snprintf(respb, MAX_PATH_LEN-1,
    "OK ovs_del_tap name=%s vhost=%s", name, vhost);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void action_add_phy(char *bin, char *db, char *respb,
                           char *name, char *vhost, char *mac,
                           int num_macvlan)
{
  if (num_macvlan == 0)
    {
    if (ovs_cmd_add_aphy(bin, db, name, vhost, mac))
      {
      snprintf(respb, MAX_PATH_LEN-1,
      "KO ovs_add_phy name=%s vhost=%s mac=%s num_macvlan=%d",
      name, vhost, mac, num_macvlan);
      }
    else
      {
      snprintf(respb, MAX_PATH_LEN-1,
      "OK ovs_add_phy name=%s vhost=%s mac=%s num_macvlan=%d",
      name, vhost, mac, num_macvlan);
      }
    }
  else
    {
    if (ovs_cmd_add_mphy(bin, db, name, vhost, mac))
      {
      snprintf(respb, MAX_PATH_LEN-1,
      "KO ovs_add_phy name=%s vhost=%s mac=%s num_macvlan=%d",
      name, vhost, mac, num_macvlan);
      }
    else
      {
      snprintf(respb, MAX_PATH_LEN-1,
      "OK ovs_add_phy name=%s vhost=%s mac=%s num_macvlan=%d",
      name, vhost, mac, num_macvlan);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void action_del_phy(char *bin, char *db, char *respb,
                           char *name, char *vhost, int num_macvlan)
{
  if (num_macvlan == 0)
    {
    if (ovs_cmd_del_aphy(bin, db, name, vhost))
      {
      snprintf(respb, MAX_PATH_LEN-1,
      "KO ovs_del_phy name=%s vhost=%s num_macvlan=%d",
      name, vhost, num_macvlan);
      }
    else
      {
      snprintf(respb, MAX_PATH_LEN-1,
      "OK ovs_del_phy name=%s vhost=%s num_macvlan=%d",
      name, vhost, num_macvlan);
      }
    }
  else
    {
    if (ovs_cmd_del_mphy(bin, db, name, vhost))
      {
      snprintf(respb, MAX_PATH_LEN-1,
      "KO ovs_del_phy name=%s vhost=%s num_macvlan=%d",
      name, vhost, num_macvlan);
      }
    else
      {
      snprintf(respb, MAX_PATH_LEN-1,
      "OK ovs_del_phy name=%s vhost=%s num_macvlan=%d",
      name, vhost, num_macvlan);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int unlink_sub_dir_files(char *dir, int erase_dir)
{
  int result = 0;
  char pth[MAX_PATH_LEN+MAX_NAME_LEN];
  DIR *dirptr;
  struct dirent *ent;
  dirptr = opendir(dir);
  if (dirptr)
    {
    while ((result == 0) && ((ent = readdir(dirptr)) != NULL))
      {
      if (!strcmp(ent->d_name, "."))
        continue;
      if (!strcmp(ent->d_name, ".."))
        continue;
      snprintf(pth, MAX_PATH_LEN+MAX_NAME_LEN, "%s/%s", dir, ent->d_name);
      pth[MAX_PATH_LEN+MAX_NAME_LEN-1] = 0;
      if(ent->d_type == DT_DIR)
        result = unlink_sub_dir_files(pth, 1);
      else if (unlink(pth))
        {
        KERR("ERROR File: %s could not be deleted\n", pth);
        result = -1;
        }
      }
    if (closedir(dirptr))
      KOUT("%d", errno);
    if (erase_dir)
      {
      if (rmdir(dir))
        {
        KERR("ERROR Dir: %s could not be deleted\n", dir);
        result = -1;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void common_end_of_all_destroy(void)
{
  char *db = g_ovs_dir;
  char *var_dir = "/var/lib/cloonix";
  char *argv[NB_ARG];
  close(g_fd_tx_to_netns);
  if (g_netns_pid > 0)
    kill(g_netns_pid, SIGKILL);
  if (g_ovs_pid > 0)
    {
    kill(g_ovs_pid, SIGKILL);
    while (!kill(g_ovs_pid, 0))
      usleep(5000);
    }
  if (g_ovsdb_pid > 0)
    {
    kill(g_ovsdb_pid, SIGKILL);
    while (!kill(g_ovsdb_pid, 0))
      usleep(5000);
    }
  unlink(get_pidfile_ovsdb(g_ovs_dir));
  unlink(get_pidfile_ovs(g_ovs_dir));
  g_ovsdb_pid = -1;
  g_ovs_pid = -1;
  g_ovs_launched = 0;
  g_ovsdb_launched = 0;
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "netns";
  argv[2] = "del";
  argv[3] = get_ns();
  call_ovs_popen(db, argv, 0, __FUNCTION__, 1);
  if (!access(db, F_OK))
    {
    if ((!strncmp(var_dir, g_ovs_dir, strlen(var_dir))) &&
        (strstr(db, g_net_name)))
      {
      if (unlink_sub_dir_files(db, 1))
        KERR("ERROR ERASE %s", db);
      sync();
      }
    else
      KERR("ERROR %s", db);
    }
  sync();
  exit(0);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void log_write_req(int tid, char *line)
{
  FILE *fp_log;
  char pth[MAX_PATH_LEN];
  memset(pth, 0, MAX_PATH_LEN);
  snprintf(pth, MAX_PATH_LEN, "%s", g_ovs_dir);
  if ((strlen(pth) + MAX_NAME_LEN) >= MAX_PATH_LEN)
    KOUT("%d", (int) strlen(pth));
  strcat(pth, "/log/");
  strcat(pth, DEBUG_LOG_DIALOG);
  fp_log = fopen(pth, "a+");
  if (fp_log)
    {
    fprintf(fp_log, "%04X:%s\n", tid, line);
    fflush(fp_log);
    fclose(fp_log);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void log_write_resp(int tid, char *respb)
{
  FILE *fp_log;
  char pth[MAX_PATH_LEN];
  memset(pth, 0, MAX_PATH_LEN);
  snprintf(pth, MAX_PATH_LEN, "%s", g_ovs_dir);
  if ((strlen(pth) + MAX_NAME_LEN) >= MAX_PATH_LEN)
    KOUT("%d", (int) strlen(pth));
  strcat(pth, "/log/");
  strcat(pth, DEBUG_LOG_DIALOG);
  fp_log = fopen(pth, "a+");
  if (fp_log)
    {
    fprintf(fp_log, "%04X:%s\n\n", tid, respb);
    fflush(fp_log);
    fclose(fp_log);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void tx_to_netns(int tid, char *line)
{
  int tx, len = strlen(line);
  if ((len <= 0) || (len >= MAX_PATH_LEN))
    KOUT("ERROR %d", len);
  len += 1;
  g_buf_tx[0] =  0xCA;
  g_buf_tx[1] =  0xFE;
  g_buf_tx[2] =  (len & 0xFF00) >> 8;
  g_buf_tx[3] =  len & 0xFF;
  g_buf_tx[4] =  (tid & 0xFF00) >> 8;
  g_buf_tx[5] =  tid & 0xFF;
  memcpy(g_buf_tx + HEADER_NETNS_MSG, line, strlen(line) + 1);
  tx = write(g_fd_tx_to_netns, g_buf_tx, len + HEADER_NETNS_MSG);
  if (tx != len + HEADER_NETNS_MSG)
    KOUT("ERROR WRITE NETNS %d %d %d", tx, len, errno);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_netns_cb(int llid, int fd)
{
  int pid, tid, len, rx;
  char *line; 
  len = read(fd, g_head_msg, HEADER_NETNS_MSG);
  if (len != HEADER_NETNS_MSG)
    KOUT("ERROR %d %d", len, errno);
  else
    {
    if ((g_head_msg[0] != 0xCA) || (g_head_msg[1] != 0xFE))
      KOUT("ERROR %d %d %hhx %hhx", len, errno, g_head_msg[0], g_head_msg[1]);
    len = ((g_head_msg[2] & 0xFF) << 8) + (g_head_msg[3] & 0xFF);
    tid = ((g_head_msg[4] & 0xFF) << 8) + (g_head_msg[5] & 0xFF);
    if ((len == 0) || (len > MAX_NETNS_BUF_LEN))
      KOUT("ERROR %d %hhx %hhx", len, g_head_msg[2], g_head_msg[3]);
    rx = read(fd, g_buf_rx, len);
    if (len != rx)
      KOUT("ERROR %d %d %d", len, rx, errno);
    line = (char *) g_buf_rx;
    if (strlen(line) == 0)
      KOUT("ERROR %d %d", len, rx);
    if (strlen(line) >= MAX_PATH_LEN)
      KOUT("ERROR %d %d %d", len, rx, (int)strlen(line));
    else
      {
      if (sscanf(line, "ovs_resp_ovsdb_ok pid=%d", &pid) == 1)
        g_ovsdb_pid = pid;
      if (sscanf(line, "ovs_resp_ovs_ok pid=%d", &pid) == 1)
        g_ovs_pid = pid;
      if (!strcmp(line, "ovs_resp_ovsdb_ko"))
        common_end_of_all_destroy();
      if (!strcmp(line, "ovs_resp_ovs_ko"))
        common_end_of_all_destroy();
      log_write_resp(tid, line);
      rpct_send_sigdiag_msg(g_cloonix_llid, tid, line);
      }
    }
  return (len+4);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_netns_err(int llid, int err, int from)
{
  KOUT("ERROR %d %d %d", llid, err, from);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void linker_helper1_fct(void)
{
  printf("useless");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_cloonix_listen_fd(void)
{
  return g_cloonix_listen_fd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_cloonix_fd(void)
{
  return g_cloonix_fd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static char *mydirname(char *path)
{
  static char tmp[MAX_PATH_LEN];
  char *pdir;
  memset(tmp, 0, MAX_PATH_LEN);
  memcpy(tmp, path, MAX_PATH_LEN-1);
  pdir = dirname(tmp);
  return (pdir);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int is_directory_writable(char *path)
{
  int result = 0;
  struct stat stat_file;
  if (!stat(path, &stat_file))
    {
    if ((stat_file.st_mode & S_IFMT) == S_IFDIR)
      if (!access(path, W_OK))
        result = 1;
    }
  return result;
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
static void timeout_heartbeat(void *data)
{
  if (g_ovs_pid > 0)
    {
    if (kill(g_ovs_pid, 0))
      {
      KERR("ERROR HEARTBEAT ovs %d ovsdb %d", g_ovs_pid, g_ovsdb_pid);
      common_end_of_all_destroy();
      KOUT("ERROR HEARTBEAT ovs %d ovsdb %d", g_ovs_pid, g_ovsdb_pid);
      }
    }
  if (g_ovsdb_pid > 0)
    {
    if (kill(g_ovsdb_pid, 0))
      {
      KERR("ERROR HEARTBEAT ovs %d ovsdb %d", g_ovs_pid, g_ovsdb_pid);
      common_end_of_all_destroy();
      KOUT("ERROR HEARTBEAT ovs %d ovsdb %d", g_ovs_pid, g_ovsdb_pid);
      }
    }
  clownix_timeout_add(50, timeout_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_heartbeat_start(void *data)
{
  if ((g_ovsdb_pid <= 0) || (g_ovs_pid <= 0))
    KOUT("ERROR HEARTBEAT ovs %d ovsdb %d", g_ovs_pid, g_ovsdb_pid);
  clownix_timeout_add(50, timeout_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_resp(int llid, int tid, char *name, int num,
                        int toppid, int pid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_msg(int llid, int tid, int flags_hop, char *txt)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_sub(int llid, int tid, int flags_hop)
{
  DOUT(FLAG_HOP_SIGDIAG, "Hello from ovs_drv");
  rpct_hop_print_add_sub(llid, tid, flags_hop);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_unsub(int llid, int tid)
{
  rpct_hop_print_del_sub(llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_kil_req(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  if ((g_ovs_pid == 0) && (g_ovsdb_pid == 0)) 
    rpct_send_pid_resp(llid, tid, name, num, 0, getpid());
  else
    rpct_send_pid_resp(llid, tid, name, num, g_ovs_pid, g_ovsdb_pid);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  KOUT("ERROR %s", line);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char respb[MAX_PATH_LEN];
  char *bin = g_ovs_bin;
  char *db = g_ovs_dir;
  char mac[MAX_NAME_LEN];
  char macv[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];
  int  type, num_macvlan;
  memset(respb, 0, MAX_PATH_LEN);

  if (strlen(line) == 0)
    KERR("ERROR %s", line);
  else if (strlen(line) >= MAX_PATH_LEN)
    KERR("ERROR %s", line);
  else if (!strcmp(line, 
"ovs_req_destroy"))
    {
    common_end_of_all_destroy();
    }
  else if ((!strcmp(line, 
"ovs_req_ovsdb")) &&
           (g_ovsdb_pid != 0))
    {
    tx_to_netns(tid, "ovs_resp_ovsdb_ok");
    }
  else if ((!strcmp(line,
"ovs_req_ovs")) &&
           (g_ovs_pid != 0))
    {
    tx_to_netns(tid, "ovs_resp_ovs_ok");
    }

  else if (sscanf(line,
"ovs_add_tap name=%s vhost=%s mac=%s", name, vhost, mac) == 3)
    {
    action_add_tap(bin, db, respb, name, vhost, mac);
    log_write_resp(tid, respb);
    rpct_send_sigdiag_msg(g_cloonix_llid, tid, respb);
    }
  else if (sscanf(line,
"ovs_del_tap name=%s vhost=%s", name, vhost) == 2)
    {
    action_del_tap(bin, db, respb, name, vhost);
    log_write_resp(tid, respb);
    rpct_send_sigdiag_msg(g_cloonix_llid, tid, respb);
    }

  else if (sscanf(line,
"ovs_add_phy name=%s vhost=%s mac=%s num_macvlan=%d", name, vhost, mac, &num_macvlan) == 4)
    {
    action_add_phy(bin, db, respb, name, vhost, mac, num_macvlan);
    log_write_resp(tid, respb);
    rpct_send_sigdiag_msg(g_cloonix_llid, tid, respb);
    }
  else if (sscanf(line,
"ovs_del_phy name=%s vhost=%s num_macvlan=%d", name, vhost, &num_macvlan) == 3)
    {
    action_del_phy(bin, db, respb, name, vhost, num_macvlan);
    log_write_resp(tid, respb);
    rpct_send_sigdiag_msg(g_cloonix_llid, tid, respb);
    }
  else if (sscanf(line,
"ovs_change_macvlan_mac type=%d macv=%s nmac=%s", &type, macv, mac) == 3)
    {
    if (type == 1)
      change_mac_macvlan(db, macv, mac);
    }
  else
    {
    if (!strcmp(line,
"ovs_req_ovsdb"))
      {
      }
    log_write_req(tid, line);
    tx_to_netns(tid, line);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_cloonix_cb(int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    {
    KOUT("%s", buf);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cloonix_err_cb(int llid, int err, int from)
{
  KERR("ERROR DECONNECT CLOONIX");
  common_end_of_all_destroy();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cloonix_connect(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, cloonix_err_cb, rx_cloonix_cb);
  g_cloonix_fd = get_fd_with_llid(llid_new);
  g_cloonix_llid = llid_new;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cmd_interrupt(int signo)
{
  KERR("ERROR INTERRUPT");
  common_end_of_all_destroy();
  KOUT("ERROR Received SIGKILL");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void cloonix_part_init(char **argv)
{
  int llid;
  char *dir_sock;
  cloonix_set_pid(getpid());
  msg_mngt_init((char *) argv[1],IO_MAX_BUF_LEN);
  memset(g_net_name, 0, MAX_NAME_LEN);
  memset(g_netns_namespace, 0, MAX_NAME_LEN);
  memset(g_name, 0, MAX_NAME_LEN);
  memset(g_path, 0, MAX_PATH_LEN);
  memcpy(g_net_name, argv[0], MAX_NAME_LEN-1);
  memcpy(g_name, argv[1], MAX_NAME_LEN-1);
  memcpy(g_path, argv[2], MAX_PATH_LEN-1);
  if (file_exists(g_path))
    KOUT("PROBLEM WITH: %s EXISTS!", g_path);
  dir_sock = mydirname(g_path);
  if (!file_exists(dir_sock))
    KOUT("PROBLEM WITH: %s DOES NOT EXIST!", dir_sock);
  if (!is_directory_writable(dir_sock))
    KOUT("PROBLEM WITH: %s NOT WRITABLE!", dir_sock);
  llid = string_server_unix(g_path, cloonix_connect, "ovs_drv");
  if (llid == 0)
    KOUT("PROBLEM WITH: %s", g_path);
  g_cloonix_listen_fd = get_fd_with_llid(llid);
  snprintf(g_netns_namespace, MAX_NAME_LEN-1, "%s_%s",
           BASE_NAMESPACE, get_net_name());
}
/*---------------------------------------------------------------------------*/
 
/*****************************************************************************/
int main (int argc, char *iargv[])
{
  int llid, fd_rx_from_netns;
  char net[2*MAX_NAME_LEN];
  char name[2*MAX_NAME_LEN];
  char sock[2*MAX_PATH_LEN];
  char *ctl_argv[4] = {net, name, sock, NULL};
  char *argv[NB_ARG];
  g_ovsdb_launched = 0;
  g_ovsdb_pid = 0;
  g_ovs_launched = 0;
  g_ovs_pid = 0;
  seteuid(0);
  setegid(0);
  umask(0000);
  if (argc != 6)
    KOUT("wrong params nb: net,name,sock,ovs_bin,ovs_dir as params");
  memset(net, 0, MAX_NAME_LEN);
  memset(name, 0, MAX_NAME_LEN);
  memset(sock, 0, MAX_PATH_LEN);
  memset(g_ovs_bin, 0, MAX_PATH_LEN);
  memset(g_ovs_dir, 0, MAX_PATH_LEN);
  strncpy(net,  iargv[1], MAX_NAME_LEN-1);
  strncpy(name, iargv[2], MAX_NAME_LEN-1);
  strncpy(sock, iargv[3], MAX_PATH_LEN-1);
  strncpy(g_ovs_bin, iargv[4], MAX_PATH_LEN-1);
  strncpy(g_ovs_dir, iargv[5], MAX_PATH_LEN-1);
  init_environ(net, g_ovs_bin, g_ovs_dir);
  signal(SIGINT, cmd_interrupt);
  cloonix_part_init(ctl_argv);
  clownix_timeout_add(2000, timeout_heartbeat_start, NULL, NULL, NULL);
  daemon(0,0);
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "netns";
  argv[2] = "del";
  argv[3] = get_ns();
  call_ovs_popen(g_ovs_dir, argv, 1, __FUNCTION__, 1);
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[0] = IP_BIN;
  argv[1] = "netns";
  argv[2] = "add";
  argv[3] = get_ns();
  call_ovs_popen(g_ovs_dir, argv, 0, __FUNCTION__, 2);
  g_netns_pid = netns_open(g_net_name, &fd_rx_from_netns, &g_fd_tx_to_netns,
                           g_ovs_bin, g_ovs_dir);
  llid = msg_watch_fd(fd_rx_from_netns, rx_netns_cb, rx_netns_err, "ovs");
  if (llid == 0)
    KOUT("ERROR OVS_DRV");
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/

