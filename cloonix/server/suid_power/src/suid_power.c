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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "net_phy.h"
#include "ovs_get.h"
#include "cnt.h"
#include "launcher.h"

/*--------------------------------------------------------------------------*/
static int  g_cloonix_rank;
static int  g_llid;
static int  g_watchdog_ok;
static char g_network_name[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_root_work_kvm[MAX_PATH_LEN];
static char g_bin_dir[MAX_PATH_LEN];
/*--------------------------------------------------------------------------*/
enum {
  state_idle = 1,
  state_running,
  state_lost_pid,
};

typedef struct t_vmon
{
  int  state;
  char name[MAX_NAME_LEN];
  int  vm_id;
  int  pid;
  int  count;
  struct t_vmon *prev;
  struct t_vmon *next;
} t_vmon;

static t_vmon *g_head_vmon;

char *get_net_name(void)
{
  return g_network_name;
}

/****************************************************************************/
static t_vmon *find_vmon_by_name(char *name)
{
  t_vmon *cur = g_head_vmon;
  while(cur)
    {
    if (!strcmp(name, cur->name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_vmon *find_vmon_by_vm_id(int vm_id)
{
  t_vmon *cur = g_head_vmon;
  while(cur)
    {
    if (vm_id == cur->vm_id)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_vmon *alloc_vmon(char *name, int vm_id)
{
  t_vmon *cur;
  if (find_vmon_by_vm_id(vm_id))
    KERR("%s %d", name, vm_id);
  else if (find_vmon_by_name(name))
    KERR("%s %d", name, vm_id);
  else
    {
    cur = (t_vmon *) malloc(sizeof(t_vmon));
    memset(cur, 0, sizeof(t_vmon));
    cur->state = state_idle;
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    cur->vm_id = vm_id;
    if (g_head_vmon)
      g_head_vmon->prev = cur;
    cur->next = g_head_vmon;
    g_head_vmon = cur;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_vmon(t_vmon *cur)
{
  if (cur->prev) 
    cur->prev->next = cur->next;
  if (cur->next) 
    cur->next->prev = cur->prev;
  if (cur == g_head_vmon)
    g_head_vmon = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *get_work_vm_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  char *root = g_root_work_kvm;
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path,MAX_PATH_LEN-1, "%s/vm%d", root, vm_id);
  return(path);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_pid_is_clownix( int pid, int vm_id)
{
  int result = 0;
  int len, fd;
  char path[MAX_PATH_LEN];
  char ndpath[MAX_PATH_LEN];
  char *ptr;
  static char cmd[MAX_BIG_BUF];
  sprintf(path, "/proc/%d/cmdline", pid);
  fd = open(path, O_RDONLY);
  if (fd > 0)
    {
    memset(cmd, 0, MAX_BIG_BUF);
    len = read(fd, cmd, MAX_BIG_BUF-2);
    ptr = cmd;
    if (len >= 0)
      {
      while ((ptr = memchr(cmd, 0, len)) != NULL)
        *ptr = ' ';
      sprintf(ndpath,"%s", get_work_vm_path(vm_id));
      if (strstr(cmd, ndpath))
        result = 1;
      }
    if (close(fd))
      KOUT("%d", errno);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int read_umid_pid(int vm_id)
{
  FILE *fhd;
  char path[MAX_PATH_LEN];
  int pid;
  int result = 0;
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path,MAX_PATH_LEN-1, "%s/%s/pid",get_work_vm_path(vm_id),DIR_UMID);
  fhd = fopen(path, "r");
  if (fhd)
    {
    if (fscanf(fhd, "%d", &pid) == 1)
      {
      if (check_pid_is_clownix(pid, vm_id))
        result = pid;
      }
    if (fclose(fhd))
      KOUT("%d", errno);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char **unlinearize(int nb, char *line)
{
  char **argv; 
  int i;
  char *ptr_end, *ptr_start = line;
  argv = (char **) malloc((nb+1)*sizeof(char **));
  memset(argv, 0, (nb+1)*sizeof(char **));
  for (i=0; i<nb; i++)
    {
    if (*ptr_start != '"')
      KERR("%s", line);
    else
      {
      ptr_start += 1;
      ptr_end = strchr(ptr_start, '"');
      if (!ptr_end)
        KERR("%s", line);
      else
        {
        *ptr_end = 0;
        argv[i] = ptr_start;
        ptr_start = ptr_end + 1; 
        }
      }
    }
  return argv;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_and_set_uid(void)
{
  int result = -1;
  uid_t uid;
  seteuid(0);
  setegid(0);
  uid = geteuid();
  if (uid == 0)
    {
    result = 0;
    umask (0000);
    if (setuid(0))
      KOUT(" ");
    if (setgid(0))
      KOUT(" ");
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void automate_pid_monitor(void)
{
  char resp[MAX_PATH_LEN];
  int pid;
  t_vmon *next, *cur = g_head_vmon;
  while(cur)
    {
    next = cur->next;
    if (cur->state == state_idle)
      {
      pid = read_umid_pid(cur->vm_id);
      if (pid > 0)
        {
        memset(resp, 0, MAX_PATH_LEN);
        snprintf(resp, MAX_PATH_LEN-1,
                 "cloonsuid_resp_pid_ok vm_id=%d pid=%d", cur->vm_id, pid);
        rpct_send_sigdiag_msg(g_llid, type_hop_suid_power, resp);
        cur->pid = pid;
        cur->state = state_running;
        }
      }
    else if (cur->state == state_running)
      {
      pid = read_umid_pid(cur->vm_id);
      if (pid == 0)
        {
        memset(resp, 0, MAX_PATH_LEN);
        snprintf(resp, MAX_PATH_LEN-1,
                 "cloonsuid_resp_pid_ko vm_id=%d", cur->vm_id);
        rpct_send_sigdiag_msg(g_llid, type_hop_suid_power, resp);
        cur->state = state_lost_pid;
        if (cur->pid > 0)
          kill(cur->pid, SIGTERM);
        }
      else if (pid != cur->pid)
        KERR("%d %d", pid, cur->pid);
      }
    else if (cur->state == state_lost_pid)
      {
      cur->count += 1;
      if (cur->count > 10)
        free_vmon(cur);
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void heartbeat (int delta)
{
  static int count_ticks_blkd = 0;
  count_ticks_blkd += 1;
  if (count_ticks_blkd == 10)
    {
    count_ticks_blkd = 0;
    automate_pid_monitor();
    cnt_beat(g_llid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*
static char *cloonix_resp_phy(int nb_phy, t_topo_info_phy *phy)
{
  int i, ln, len = MAX_PATH_LEN + (nb_phy * MAX_NAME_LEN * 4);
  char *buf = (char *) malloc(len);
  memset(buf, 0, len);
  ln = sprintf(buf, "cloonsuid_resp_phy nb_phys=%d", nb_phy); 
  for (i=0; i<nb_phy; i++)
    {
    ln += sprintf(buf+ln,
          " phy:%s idx:%d flags:%X drv:%s pci:%s mac:%s vendor:%s device:%s",
          phy[i].name, phy[i].index, phy[i].flags, phy[i].drv,
          phy[i].pci, phy[i].mac, phy[i].vendor, phy[i].device); 
    }
  return buf;
}
*/
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void req_kill_clean_all(void)
{
  char root_work_cnt[MAX_PATH_LEN];
  char *root = g_root_path;
  int pid;
  t_vmon *next, *cur = g_head_vmon;
  cnt_kill_all();
  while(cur)
    {
    next = cur->next;
    pid = read_umid_pid(cur->vm_id);

    if (pid > 0)
      {
      kill(cur->pid, SIGTERM);
      free_vmon(cur);
      }
    cur = next;
    }
  snprintf(root_work_cnt, MAX_PATH_LEN-1,"%s/%s", root, CNT_DIR);
  unlink(root_work_cnt);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  if (llid != g_llid)
    KERR("%s %d %d", g_network_name, llid, g_llid);
  if (tid != type_hop_suid_power)
    KERR("%s %d %d", g_network_name, tid, type_hop_suid_power);
  rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
  g_watchdog_ok = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_kil_req(int llid, int tid)
{
  req_kill_clean_all();
  exit(0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qemu_launch(char *name, int vm_id, int argc,
                        char *line, char *resp, int llid, int tid)
{
  int result;
  char **argv;
  char *ptr_arg;
  t_vmon *cur = alloc_vmon(name, vm_id);
  ptr_arg = strchr(line, ':');
  argv = unlinearize(argc, ptr_arg+1);
  result = launcher(argv);
  if (result)
    {
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_resp_launch_vm_ko name=%s", name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    free_vmon(cur);
    }
  else
    {
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_resp_launch_vm_ok name=%s",name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  free(argv);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
//  char *str_net_phy, *str_net_ovs;
//  int nb_phy;
//  t_topo_info_phy *net_phy;
  DOUT(FLAG_HOP_POLDIAG, "SUID %s", line);
  if (!strcmp(line,
  "cloonsuid_req_phy"))
    {
    KERR("ERROR cloonsuid_req_phy");
/*
    net_phy = net_phy_get(&nb_phy);
    str_net_phy = cloonix_resp_phy(nb_phy, net_phy);
    rpct_send_poldiag_msg(llid, tid, str_net_phy);
    free(str_net_phy);
*/
    }
  else if (!strcmp(line,
  "cloonsuid_req_ovs"))
    {
    KERR("ERROR cloonsuid_req_ovs");
/*
    str_net_ovs = ovs_get_topo(g_bin_dir, g_root_path);
    rpct_send_poldiag_msg(llid, tid, str_net_ovs);
    free(str_net_ovs);
*/
    }
  else if (!strncmp(line,
  "cloonsuid_cnt", strlen("cloonsuid_cnt")))
    {
    cnt_recv_poldiag_msg(llid, tid, line);
    }
  else
    KERR("%s %s", g_network_name, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  char dtach[MAX_PATH_LEN];
  int argc, vm_id, pid;
  t_vmon *cur;

  DOUT(FLAG_HOP_SIGDIAG, "SUID %s", line);
  memset(resp, 0, MAX_PATH_LEN);
  if (llid != g_llid)
    KERR("%s %d %d", g_network_name, llid, g_llid);
  if (tid != type_hop_suid_power)
    KERR("%s %d %d", g_network_name, tid, type_hop_suid_power);
  if (!strncmp(line,
  "cloonsuid_req_suidroot", strlen("cloonsuid_req_suidroot")))
    {
    if (check_and_set_uid())
      rpct_send_sigdiag_msg(llid, tid, "cloonsuid_resp_suidroot_ko");
    else
      rpct_send_sigdiag_msg(llid, tid, "cloonsuid_resp_suidroot_ok");
    }
  else if (sscanf(line, 
  "cloonsuid_req_launch name=%s vm_id=%d dtach=%s argc=%d:",
         name, &vm_id, dtach, &argc) == 4)
    {
    qemu_launch(name, vm_id, argc, line, resp, llid, tid);
    if (chmod(dtach, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))
      KERR("%s", dtach);
    }
  else if (sscanf(line,
  "cloonsuid_req_ifup phy: %s", name) == 1)
    {
    if (net_phy_flags_iff_up_down(name, 1))
      KERR("ERROR Bad ifup %s", name);
    }
  else if (sscanf(line,
  "cloonsuid_req_ifdown phy: %s", name) == 1)
    {
    if (net_phy_flags_iff_up_down(name, 0))
      KERR("ERROR Bad ifdown %s", name);
    }
  else if (sscanf(line,
  "cloonsuid_req_vm_kill vm_id: %d", &vm_id) == 1)
    {
    cur = find_vmon_by_vm_id(vm_id);
    if (!cur)
      KERR("Not found: vm_id = %d", vm_id);
    else if (cur->pid > 0)
      {
      if (kill(cur->pid, SIGTERM))
        KERR("ERROR Bad kill %d", vm_id);
      else
        {
        memset(resp, 0, MAX_PATH_LEN);
        snprintf(resp, MAX_PATH_LEN-1,
                 "cloonsuid_resp_pid_ko vm_id=%d", vm_id);
        rpct_send_sigdiag_msg(g_llid, type_hop_suid_power, resp);
        }
      free_vmon(cur);
      }
    }
  else if (sscanf(line,
  "cloonsuid_req_pid_kill pid: %d", &pid) == 1)
    {
    if (pid <= 0)
      KERR("ERROR BECAUSE BAD pid %d", pid);
    else if (!kill(pid, SIGTERM))
      KERR("ERROR BECAUSE GOOD KILL %d", pid);
    }
  else if (!strncmp(line,
  "cloonsuid_cnt", strlen("cloonsuid_cnt")))
    cnt_recv_sigdiag_msg(llid, tid, line);
  else
    KERR("ERROR %s %s", g_network_name, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_ctrl_cb (int llid, int err, int from)
{
  req_kill_clean_all();
  KERR("SUIDPOWER RECEIVED DISCONNECT %d %d", err, from);
  exit(0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_ctrl_cb (int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    KOUT("%s %s", g_network_name, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_ctrl_client(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, err_ctrl_cb, rx_ctrl_cb);
  g_llid = llid_new;
  g_watchdog_ok = 1;
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fct_timeout_self_destruct(void *data)
{
  if (g_watchdog_ok == 0)
    KOUT("SUID_POWER SELF DESTRUCT");
  g_watchdog_ok = 0;
  clownix_timeout_add(1500, fct_timeout_self_destruct, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_cloonix_rank(void)
{
  return (g_cloonix_rank);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main (int argc, char *argv[])
{
  char ctrl_path[MAX_PATH_LEN];
  char *root;
  net_phy_init();
  ovs_get_init();
  g_head_vmon = NULL;
  g_llid = 0;
  g_watchdog_ok = 0;
  if (argc != 8)
    KOUT(" ");
  cnt_init();
  set_saved_environ(argv[5], argv[6], argv[7]);
  msg_mngt_init("suid_power", IO_MAX_BUF_LEN);
  memset(g_network_name, 0, MAX_NAME_LEN);
  memset(ctrl_path, 0, MAX_PATH_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_root_work_kvm, 0, MAX_PATH_LEN);
  memset(g_bin_dir, 0, MAX_PATH_LEN);
  strncpy(g_network_name, argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  strncpy(g_bin_dir, argv[3], MAX_PATH_LEN-1);
  if (sscanf(argv[4], "%d", &g_cloonix_rank) != 1)
    KOUT("ERROR %s", argv[4]);
  root = g_root_path;
  snprintf(g_root_work_kvm,MAX_PATH_LEN-1,"%s/%s", root, CLOONIX_VM_WORKDIR);
  snprintf(ctrl_path,MAX_PATH_LEN-1,"%s/%s", root, SUID_POWER_SOCK_DIR);
  unlink(ctrl_path);
  msg_mngt_heartbeat_init(heartbeat);
  string_server_unix(ctrl_path, connect_from_ctrl_client, "ctrl");
  daemon(0,0);
  seteuid(getuid());
  cloonix_set_pid(getpid());
  clownix_timeout_add(1000, fct_timeout_self_destruct, NULL, NULL, NULL);
  msg_mngt_loop();
  return 0;
}
/*--------------------------------------------------------------------------*/

