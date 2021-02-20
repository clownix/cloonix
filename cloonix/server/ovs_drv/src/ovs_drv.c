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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "ovs_execv.h"
#include "action.h"

#define CLOONIX_DIAG_LOG  "cloonix_diag.log"

static char g_net_name[MAX_NAME_LEN];
static char g_ovs_bin[MAX_PATH_LEN];
static char g_dpdk_dir[MAX_PATH_LEN];
static int g_ovsdb_launched;
static int g_ovs_launched;
static int g_cloonix_listen_fd;
static int g_cloonix_fd;
static int g_ovsdb_pid;
static int g_ovs_pid;
static char g_arg[NB_ARG][MAX_ARG_LEN];
static char g_name[MAX_NAME_LEN];
static char g_path[MAX_PATH_LEN];

/*****************************************************************************/
int get_ovs_launched(void)
{
  return g_ovs_launched;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_ovsdb_launched(void)
{
  return g_ovsdb_launched;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void set_ovs_launched(int val)
{
  g_ovs_launched = val;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void set_ovsdb_launched(int val)
{
  g_ovsdb_launched = val;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_net_name(void)
{
  return g_net_name;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_ovs_bin(void)
{
  return g_ovs_bin;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *get_dpdk_dir(void)
{
  return g_dpdk_dir;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_ovsdb_pid(void)
{
  return g_ovsdb_pid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void set_ovsdb_pid(int pid)
{
  g_ovsdb_pid = pid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_ovs_pid(void)
{
  return (g_ovs_pid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void set_ovs_pid(int pid)
{
  g_ovs_pid = pid;
}
/*---------------------------------------------------------------------------*/

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
void unlink_files(char *dpdk_dir)
{
  if (!access(dpdk_dir, F_OK))
    {
    memset(g_arg, 0, MAX_ARG_LEN * NB_ARG);
    snprintf(g_arg[0], MAX_PATH_LEN-1,"/bin/rm");
    snprintf(g_arg[1], MAX_PATH_LEN-1,"-fdR");
    snprintf(g_arg[2], MAX_ARG_LEN-1,"%s/*", dpdk_dir);
    if (call_my_popen(dpdk_dir, 3, g_arg)) 
      KERR(" ");
    sync();
    usleep(5000);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void unlink_dir(char *dpdk_dir)
{
  memset(g_arg, 0, MAX_ARG_LEN * NB_ARG);
  snprintf(g_arg[0], MAX_PATH_LEN-1,"/bin/rm");
  snprintf(g_arg[1], MAX_PATH_LEN-1,"-fdR");
  snprintf(g_arg[2], MAX_ARG_LEN-1,"%s/dpdk", dpdk_dir);
  if (call_my_popen(dpdk_dir, 3, g_arg)) 
    KERR(" ");
  while (!access(g_arg[2], F_OK))
    {
    if (call_my_popen(dpdk_dir, 3, g_arg)) 
      KERR(" ");
    usleep(10000);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_rpct_heartbeat(void *data)
{
  rpct_heartbeat(NULL);
  if (g_ovsdb_launched && g_ovs_launched)
    {
    if (g_ovs_pid > 0)
      {
      if (kill(g_ovs_pid, 0))
        {
        KERR("ovs pid %d diseapeared, killing ovsdb", g_ovs_pid);
        if (g_ovsdb_pid > 0)
          kill(g_ovsdb_pid, SIGKILL);
        kill(g_ovs_pid, SIGKILL);
        KOUT(" ");
        }
      }
    if (g_ovsdb_pid > 0)
      {
      if (kill(g_ovsdb_pid, 0))
        {
        KERR("ovsdb pid %d diseapeared, killing ovs", g_ovsdb_pid);
        if (g_ovs_pid > 0)
          kill(g_ovs_pid, SIGKILL);
        kill(g_ovsdb_pid, SIGKILL);
        KOUT(" ");
        }
      }
    if ((g_ovsdb_pid <= 0) && (g_ovs_pid <= 0))
      KOUT(" ");
    }
  clownix_timeout_add(100, timeout_rpct_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_blkd_heartbeat(void *data)
{
  blkd_heartbeat(NULL);
  clownix_timeout_add(1, timeout_blkd_heartbeat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int mycmp(char *req, char *targ)
{
  int result = strncmp(req, targ, strlen(targ));
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void log_write_req_resp(char *line, char *respb)
{
  FILE *fp_log;
  char pth[MAX_PATH_LEN];
  memset(pth, 0, MAX_PATH_LEN);
  snprintf(pth, MAX_PATH_LEN, "%s", g_dpdk_dir);
  if ((strlen(pth) + MAX_NAME_LEN) >= MAX_PATH_LEN)
    KOUT("%d", (int) strlen(pth));
  strcat(pth, "/");
  strcat(pth, CLOONIX_DIAG_LOG);
  fp_log = fopen(pth, "a+");
  if (fp_log)
    {
    fprintf(fp_log, "%s\n", line);
    fprintf(fp_log, "%s\n\n", respb);
    fflush(fp_log);
    fclose(fp_log);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_report(void *ptr, int llid, t_blkd_item *item)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_resp(void *ptr, int llid, int tid, char *name, int num,
                        int toppid, int pid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_msg(void *ptr, int llid, int tid, int flags_hop, char *txt)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_sub(void *ptr, int llid, int tid, int flags_hop)
{
  DOUT(NULL, FLAG_HOP_DIAG, "Hello from ovs_drv");
  rpct_hop_print_add_sub(NULL, llid, tid, flags_hop);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_unsub(void *ptr, int llid, int tid)
{
  rpct_hop_print_del_sub(ptr, llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_kil_req(void *ptr, int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num)
{
  if ((g_ovs_pid == 0) && (g_ovsdb_pid == 0)) 
    rpct_send_pid_resp(ptr, llid, tid, name, num, 0, getpid());
  else
    rpct_send_pid_resp(ptr, llid, tid, name, num, g_ovs_pid, g_ovsdb_pid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_resp(void *ptr, int llid, int tid,
                     int cli_llid, int cli_tid, char *line)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clean_all_upon_error(void)
{
  KERR("ERROR, CLEAN OVS AND OVSDB");
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
  unlink(get_pidfile_ovsdb(g_dpdk_dir));
  unlink(get_pidfile_ovs(g_dpdk_dir));
  usleep(10000);
  g_ovsdb_pid = -1;
  g_ovs_pid = -1;
  g_ovs_launched = 0;
  g_ovsdb_launched = 0;
  unlink_dir(g_dpdk_dir);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line)
{
  uint32_t lcore_mask, cpu_mask, socket_mem;
  char *bin = g_ovs_bin;
  char *db = g_dpdk_dir;
  int num, cloonix_llid;
  char respb[MAX_PATH_LEN];
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char mac[MAC_ADDR_LEN];
  if (!file_exists(g_dpdk_dir))
    KOUT("%s", g_dpdk_dir);
  if (!is_directory_writable(g_dpdk_dir))
    KOUT("%s", g_dpdk_dir);
  memset(respb, 0, MAX_PATH_LEN); 
  if (!mycmp(line, "cloonixovs_req_suidroot"))
    action_req_suidroot(respb);
  else if (sscanf(line,
           "cloonixovs_req_ovsdb lcore_mask=0x%x socket_mem=%d cpu_mask=0x%x",
                                 &lcore_mask, &socket_mem, &cpu_mask) == 3)
    action_req_ovsdb(bin, db, respb, lcore_mask, socket_mem, cpu_mask);
  else if (sscanf(line,
           "cloonixovs_req_ovs lcore_mask=0x%x socket_mem=%d cpu_mask=0x%x",
                                 &lcore_mask, &socket_mem, &cpu_mask) == 3)
    action_req_ovs_switch(bin, db, respb, lcore_mask, socket_mem, cpu_mask);

  else if (sscanf(line,
           "cloonixovs_add_lan_pci_dpdk lan=%s pci=%s", lan, name) == 2)
    action_add_lan_pci_dpdk(respb, lan, name);
  else if (sscanf(line,
           "cloonixovs_del_lan_pci_dpdk lan=%s pci=%s", lan, name) == 2)
    action_del_lan_pci_dpdk(respb, lan, name);

  else if (sscanf(line,
           "cloonixovs_add_lan_phy_port lan=%s phy=%s", lan, name) == 2)
    action_add_lan_phy_port(respb, lan, name);
  else if (sscanf(line,
           "cloonixovs_del_lan_phy_port lan=%s phy=%s", lan, name) == 2)
    action_del_lan_phy_port(respb, lan, name);

  else if (action_add_eth_req(line, name, &num, mac))
    action_add_eth_br(respb, name, num, mac);
  else if (sscanf(line,"cloonixovs_del_eth name=%s num=%d", name, &num) == 2)
    action_del_eth_br(respb, name, num);
  else if (sscanf(line,"cloonixovs_add_lan_br lan=%s", name) == 1)
    action_add_lan_br(respb, name);
  else if (sscanf(line,"cloonixovs_del_lan_br lan=%s", name) == 1)
    action_del_lan_br(respb, name);

  else if (sscanf(line,"cloonixovs_add_lan_snf lan=%s name=%s",lan,name) == 2)
    action_add_lan_snf_br(respb, lan, name);
  else if (sscanf(line,"cloonixovs_del_lan_snf lan=%s name=%s",lan,name) == 2)
    action_del_lan_snf_br(respb, lan, name);
  else if (sscanf(line,"cloonixovs_add_lan_nat lan=%s name=%s",lan,name) == 2)
    action_add_lan_nat_br(respb, lan, name);
  else if (sscanf(line,"cloonixovs_del_lan_nat lan=%s name=%s",lan,name) == 2)
    action_del_lan_nat_br(respb, lan, name);

  else if (sscanf(line,"cloonixovs_add_lan_d2d lan=%s name=%s",lan,name) == 2)
    action_add_lan_d2d_br(respb, lan, name);
  else if (sscanf(line,"cloonixovs_del_lan_d2d lan=%s name=%s",lan,name) == 2)
    action_del_lan_d2d_br(respb, lan, name);

  else if (sscanf(line,"cloonixovs_add_lan_a2b lan=%s name=%s",lan,name) == 2)
    action_add_lan_a2b_br(respb, lan, name);
  else if (sscanf(line,"cloonixovs_del_lan_a2b lan=%s name=%s",lan,name) == 2)
    action_del_lan_a2b_br(respb, lan, name);

  else if (sscanf(line,"cloonixovs_add_lan_b2a lan=%s name=%s",lan,name) == 2)
    action_add_lan_b2a_br(respb, lan, name);
  else if (sscanf(line,"cloonixovs_del_lan_b2a lan=%s name=%s",lan,name) == 2)
    action_del_lan_b2a_br(respb, lan, name);

  else if (sscanf(line,"cloonixovs_add_lan_eth lan=%s name=%s num=%d",
                       lan, name, &num) == 3)
    action_add_lan_eth_br(respb, lan, name, num);
  else if (sscanf(line, "cloonixovs_del_lan_eth lan=%s name=%s num=%d",
                        lan, name, &num) == 3)
    action_del_lan_eth_br(respb, lan, name, num);
  else if (sscanf(line, "cloonixovs_add_tap name=%s", name))
    action_add_tap_br(respb, name);
  else if (sscanf(line, "cloonixovs_del_tap name=%s", name))
    action_del_tap_br(respb, name);
  else if(sscanf(line,"cloonixovs_add_lan_tap lan=%s name=%s", lan, name))
    action_add_lan_tap_br(respb, lan, name);
  else if(sscanf(line,"cloonixovs_del_lan_tap lan=%s name=%s", lan, name))
    action_del_lan_tap_br(respb, lan, name);
  else if (!strcmp(line, "cloonixovs_req_destroy"))
    action_req_destroy();
  else
    KOUT("%s", line);
  cloonix_llid = blkd_get_cloonix_llid(NULL);
  if (!cloonix_llid)
    KOUT(" ");
  if (cloonix_llid != llid)
    KOUT("%d %d", cloonix_llid, llid);

  log_write_req_resp(line, respb);

  rpct_send_diag_msg(NULL, llid, tid, respb);

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line)
{
  KERR("%s", line);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line)
{
  KERR("%s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void rx_cloonix_cb(int llid, int len, char *buf)
{
  if (rpct_decoder(NULL, llid, len, buf))
    {
    KOUT("%s", buf);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cloonix_err_cb(void *ptr, int llid, int err, int from)
{
  KERR(" ");
  action_req_destroy();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cloonix_connect(void *ptr, int llid, int llid_new)
{
  int cloonix_llid = blkd_get_cloonix_llid(NULL);
  if (!cloonix_llid)
    blkd_set_cloonix_llid(ptr, llid_new);
  msg_mngt_set_callbacks (llid_new, cloonix_err_cb, rx_cloonix_cb);
  g_cloonix_fd = get_fd_with_llid(llid_new);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cmd_interrupt(int signo)
{
  if (g_ovs_pid > 0)
    {
    if (kill(g_ovs_pid, SIGKILL))
      KERR("Received SIGKILL no kill %d", g_ovs_pid);
    }
  if (g_ovsdb_pid > 0)
    {
    if (kill(g_ovsdb_pid, SIGKILL))
      KERR("Received SIGKILL no kill %d", g_ovsdb_pid);
    }
  KOUT("Received SIGKILL");
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
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clean_before_exit(void *ptr)
{
  KERR("OVS");
  action_req_destroy();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int main (int argc, char *argv[])
{
  char net[2*MAX_NAME_LEN];
  char name[2*MAX_NAME_LEN];
  char sock[2*MAX_PATH_LEN];
  char *ctl_argv[4] = {net, name, sock, NULL};
  g_ovsdb_launched = 0;
  g_ovsdb_pid = 0;
  g_ovs_launched = 0;
  g_ovs_pid = 0;
  seteuid(0);
  setegid(0);
  umask(0000);
  if (argc != 6)
    KOUT("wrong params nb: net,name,sock,ovs_bin,dpdk_dir as params");
  memset(net, 0, MAX_NAME_LEN);
  memset(name, 0, MAX_NAME_LEN);
  memset(sock, 0, MAX_PATH_LEN);
  memset(g_ovs_bin, 0, MAX_PATH_LEN);
  memset(g_dpdk_dir, 0, MAX_PATH_LEN);
  memcpy(net,  argv[1], MAX_NAME_LEN-1);
  memcpy(name, argv[2], MAX_NAME_LEN-1);
  memcpy(sock, argv[3], MAX_PATH_LEN-1);
  memcpy(g_ovs_bin, argv[4], MAX_PATH_LEN-1);
  memcpy(g_dpdk_dir, argv[5], MAX_PATH_LEN-1);


  if ((!strncmp("/bin", g_dpdk_dir, strlen("/bin"))) ||
      (!strncmp("/etc", g_dpdk_dir, strlen("/etc"))) ||
      (!strncmp("/run", g_dpdk_dir, strlen("/run"))) ||
      (!strncmp("/sys", g_dpdk_dir, strlen("/sys"))) ||
      (!strncmp("/var", g_dpdk_dir, strlen("/var"))) ||
      (!strncmp("/boot", g_dpdk_dir, strlen("/boot"))) ||
      (!strncmp("/lib", g_dpdk_dir, strlen("/lib"))) ||
      (!strncmp("/media", g_dpdk_dir, strlen("/media"))) ||
      (!strncmp("/proc", g_dpdk_dir, strlen("/proc"))) ||
      (!strncmp("/dev", g_dpdk_dir, strlen("/dev"))) ||
      (!strncmp("/usr", g_dpdk_dir, strlen("/usr"))) ||
      (!strncmp("/sbin", g_dpdk_dir, strlen("/sbin"))))
    KOUT("Too risky to erase %s/dpdk ERROR FAIL", g_dpdk_dir);
  unlink_dir(g_dpdk_dir);
  signal(SIGINT, cmd_interrupt);
  cloonix_part_init(ctl_argv);
  clownix_timeout_add(100, timeout_rpct_heartbeat, NULL, NULL, NULL);
  clownix_timeout_add(100, timeout_blkd_heartbeat, NULL, NULL, NULL);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/

