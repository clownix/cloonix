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

/*--------------------------------------------------------------------------*/
static int  g_llid;
static int  g_watchdog_ok;
static char g_network_name[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_root_work[MAX_PATH_LEN];
static char *g_saved_environ[4];
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
  int  nb_vhostif;
  char vhostif[MAX_VHOST_VM][IFNAMSIZ];
  struct t_vmon *prev;
  struct t_vmon *next;
} t_vmon;

static t_vmon *g_head_vmon;

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
static char **get_saved_environ(void)
{
  return (g_saved_environ);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void clean_all_llid(void)
{
  int llid, nb_chan, i;
  clownix_timeout_del_all();
  nb_chan = get_clownix_channels_nb();
  for (i=0; i<=nb_chan; i++)
    {
    llid = channel_get_llid_with_cidx(i);
    if (llid)
      {
      if (msg_exist_channel(llid))
        msg_delete_channel(llid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int ifup_all_vsockif(t_vmon *vmon)
{
  int i, result = 0;
  
  for (i=0; i<vmon->nb_vhostif; i++)
    {
    if (net_phy_flags_iff_up_down(vmon->vhostif[i], 1))
      result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int launcher(char *argv[])
{
  char **env = get_saved_environ();
  int exited_pid, timeout_pid, worker_pid, chld_state, pid, status=77;
  pid_t rc_pid;
  if ((pid = fork()) < 0)
    KOUT(" ");
  if (pid == 0)
    {
    clean_all_llid();
    worker_pid = fork();
    if (worker_pid == 0)
      {
      execve(argv[0], argv, env);
      KOUT("FORK error %s", strerror(errno));
      }
    timeout_pid = fork();
    if (timeout_pid == 0)
      {
      sleep(3);
      exit(1);
      }
    exited_pid = wait(&chld_state);
    if (exited_pid == worker_pid)
      {
      if (WIFEXITED(chld_state))
        status = WEXITSTATUS(chld_state);
      if (WIFSIGNALED(chld_state))
        KERR("Child exited via signal %d\n",WTERMSIG(chld_state));
      kill(timeout_pid, SIGKILL);
      }
    else
      {
      kill(worker_pid, SIGKILL);
      }
    wait(NULL);
    exit(status);
    }
  rc_pid = waitpid(pid, &chld_state, 0);
  if (rc_pid > 0)
    {
    if (WIFEXITED(chld_state))
      status = WEXITSTATUS(chld_state);
    if (WIFSIGNALED(chld_state))
      KERR("Child exited via signal %d\n",WTERMSIG(chld_state));
    } 
  else
    {
    if (errno != ECHILD)
      KERR("Unexpected error %s", strerror(errno));
    } 
  return status;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *get_work_vm_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  char *root = g_root_work;
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
static char *get_ifnames(t_vmon *vmon, int nb, char *msg)
{
  int i;
  char *ptr_end = msg, *ptr = msg;
  if (nb > MAX_VHOST_VM)
    KOUT("%d %d", nb, MAX_VHOST_VM);
  for (i=0; i<nb; i++)
    {
    ptr_end = strchr(ptr, ':');
    if (!ptr_end)
      KOUT(" "); 
    (*ptr_end) = 0;
    strncpy(vmon->vhostif[i], ptr, IFNAMSIZ-1);
    ptr = ptr_end + 1;
    }
  return ptr_end;
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
                 "cloonixsuid_resp_pid_ok vm_id=%d pid=%d", cur->vm_id, pid);
        rpct_send_diag_msg(NULL, g_llid, type_hop_suid_power, resp);
        cur->pid = pid;
        if (!ifup_all_vsockif(cur))
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
                 "cloonixsuid_resp_pid_ko vm_id=%d", cur->vm_id);
        rpct_send_diag_msg(NULL, g_llid, type_hop_suid_power, resp);
        cur->state = state_lost_pid;
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
  static int count_ticks_rpct = 0;
  count_ticks_blkd += 1;
  count_ticks_rpct += 1;
  if (count_ticks_blkd == 5)
    {
    blkd_heartbeat(NULL);
    count_ticks_blkd = 0;
    automate_pid_monitor();
    }
  if (count_ticks_rpct == 100)
    {
    rpct_heartbeat(NULL);
    count_ticks_rpct = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *cloonix_resp_phy(int nb_phy, t_topo_phy *phy)
{
  int i, ln, len = MAX_PATH_LEN + (nb_phy * MAX_NAME_LEN);
  char *buf = (char *) malloc(len);
  memset(buf, 0, len);
  ln = sprintf(buf, "cloonixsuid_resp_phy nb_phys=%d", nb_phy); 
  for (i=0; i<nb_phy; i++)
    {
    ln += sprintf(buf+ln,
          " phy:%s idx:%d flags:%X drv:%s pci:%s mac:%s vendor:%s device:%s",
          phy[i].name, phy[i].index, phy[i].flags, phy[i].drv,
          phy[i].pci, phy[i].mac, phy[i].vendor, phy[i].device); 
    }
  return buf;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *cloonix_resp_pci(int nb_pci, t_topo_pci *pci)
{
  int i, ln, len = MAX_PATH_LEN + (nb_pci * MAX_NAME_LEN);
  char *buf = (char *) malloc(len);
  memset(buf, 0, len);
  ln = sprintf(buf, "cloonixsuid_resp_pci nb_pcis=%d", nb_pci);
  for (i=0; i<nb_pci; i++)
    {
    ln += sprintf(buf+ln, " pci:%s drv:%s unused:%s",
                  pci[i].pci, pci[i].drv, pci[i].unused);
    }
  return buf;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void req_kill_clean_all(void)
{
  int pid;
  t_vmon *next, *cur = g_head_vmon;
  while(cur)
    {
    next = cur->next;
    pid = read_umid_pid(cur->vm_id);
    if (pid > 0)
      {
      if (kill(cur->pid, SIGTERM))
        KERR("ERROR Bad kill %d", cur->vm_id);
      else
        KERR("ERROR GOOD kill %d", cur->vm_id);
      free_vmon(cur);
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num)
{
  if (llid != g_llid)
    KERR("%s %d %d", g_network_name, llid, g_llid);
  if (tid != type_hop_suid_power)
    KERR("%s %d %d", g_network_name, tid, type_hop_suid_power);
  rpct_send_pid_resp(ptr, llid, tid, name, num, cloonix_get_pid(), getpid());
  g_watchdog_ok = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_kil_req(void *ptr, int llid, int tid)
{
  req_kill_clean_all();
  exit(0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qemu_launch(char *name, int vm_id, int nb_vhost, int argc,
                        char *line, char *resp, int llid, int tid)
{
  int result;
  char **argv;
  char *ptr_arg;
  t_vmon *cur = alloc_vmon(name, vm_id);
  ptr_arg = strchr(line, ':');
  if (nb_vhost > 0)
    ptr_arg = get_ifnames(cur, nb_vhost, ptr_arg+1);
  cur->nb_vhostif = nb_vhost;
  argv = unlinearize(argc, ptr_arg+1);
  result = launcher(argv);
  if (result)
    {
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonixsuid_resp_launch_vm_ko name=%s", name);
    rpct_send_diag_msg(NULL, llid, tid, resp);
    free_vmon(cur);
    }
  else
    {
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonixsuid_resp_launch_vm_ok name=%s",name);
    rpct_send_diag_msg(NULL, llid, tid, resp);
    }
  free(argv);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  char dtach[MAX_PATH_LEN];
  char old_name[MAX_NAME_LEN];
  char new_name[MAX_NAME_LEN];
  char *str_net_phy, *str_net_pci;
  int num, argc, vm_id, nb_vhost, nb_phy, nb_pci;
  t_topo_pci *net_pci;
  t_topo_phy *net_phy;
  t_vmon *cur;

  memset(resp, 0, MAX_PATH_LEN);
  if (llid != g_llid)
    KERR("%s %d %d", g_network_name, llid, g_llid);
  if (tid != type_hop_suid_power)
    KERR("%s %d %d", g_network_name, tid, type_hop_suid_power);
  if (!strncmp(line, "cloonixsuid_req_suidroot",
               strlen("cloonixsuid_req_suidroot")))
    {
    if (check_and_set_uid())
      rpct_send_diag_msg(NULL, llid, tid, "cloonixsuid_resp_suidroot_ko");
    else
      rpct_send_diag_msg(NULL, llid, tid, "cloonixsuid_resp_suidroot_ok");
    }
  else if (sscanf(line, 
        "cloonixsuid_req_launch name=%s vm_id=%d dtach=%s nb_eth=%d argc=%d:",
         name, &vm_id, dtach, &nb_vhost, &argc) == 5)
    {
    qemu_launch(name, vm_id, nb_vhost, argc, line, resp, llid, tid);
    if (chmod(dtach, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))
      KERR("%s", dtach);
    }
  else if (sscanf(line,
                  "cloonixsuid_req_ifname_change %s %d old:%s new:%s",
                  name, &num, old_name, new_name) == 4)
    {
    if (!net_phy_ifname_change(old_name, new_name))
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonixsuid_resp_ifname_change_ok %s %d old:%s new:%s",
               name, num, old_name, new_name);
    else
      snprintf(resp, MAX_PATH_LEN-1,
               "cloonixsuid_resp_ifname_change_ko %s %d old:%s new:%s",
               name, num, old_name, new_name);
    rpct_send_diag_msg(NULL, llid, tid, resp);
    }
  else if (!strcmp(line, "cloonixsuid_req_phy")) 
    {
    net_phy = net_phy_get(&nb_phy);
    str_net_phy = cloonix_resp_phy(nb_phy, net_phy);
    rpct_send_diag_msg(NULL, llid, tid, str_net_phy);
    free(str_net_phy);
    }
  else if (!strcmp(line, "cloonixsuid_req_pci"))
    {
    net_pci = net_pci_get(&nb_pci);
    str_net_pci = cloonix_resp_pci(nb_pci, net_pci);
    rpct_send_diag_msg(NULL, llid, tid, str_net_pci);
    free(str_net_pci);
    }
  else if (sscanf(line, "cloonixsuid_req_ifup phy: %s", name) == 1)
    {
    if (net_phy_flags_iff_up_down(name, 1))
      KERR("ERROR Bad ifup %s", name);
    }
  else if (sscanf(line, "cloonixsuid_req_ifdown phy: %s", name) == 1)
    {
    if (net_phy_flags_iff_up_down(name, 0))
      KERR("ERROR Bad ifdown %s", name);
    }
  else if (sscanf(line, "cloonixsuid_req_vfio_attach: %s", name) == 1)
    {
    if (!net_phy_vfio_attach(name))
      snprintf(resp,MAX_PATH_LEN-1,"cloonixsuid_resp_vfio_attach_ok: %s",name);
    else
      snprintf(resp,MAX_PATH_LEN-1,"cloonixsuid_resp_vfio_attach_ko: %s",name);
    rpct_send_diag_msg(NULL, llid, tid, resp);
    }
  else if (sscanf(line, "cloonixsuid_req_kill vm_id: %d", &vm_id) == 1)
    {
    cur = find_vmon_by_vm_id(vm_id);
    if (!cur)
      KERR("Not found: vm_id = %d", vm_id);
    else if (cur->pid > 0)
      {
      if (kill(cur->pid, SIGTERM))
        KERR("ERROR Bad kill %d", vm_id);
      free_vmon(cur);
      }
    }
  else if (!strcmp(line, "cloonixsuid_req_kill_all"))
    {
    req_kill_clean_all();
    }
  else
    KERR("%s %s", g_network_name, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_ctrl_cb (void *ptr, int llid, int err, int from)
{
  req_kill_clean_all();
  exit(0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_ctrl_cb (int llid, int len, char *buf)
{
  if (rpct_decoder(NULL, llid, len, buf))
    KOUT("%s %s", g_network_name, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_ctrl_client(void *ptr, int llid, int llid_new)
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
  clownix_timeout_add(500, fct_timeout_self_destruct, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main (int argc, char *argv[])
{
  char ctrl_path[MAX_PATH_LEN];
  char *root;
  net_phy_init();
  g_head_vmon = NULL;
  g_llid = 0;
  g_watchdog_ok = 0;
  if (argc != 6)
    KOUT(" ");
  g_saved_environ[0] = argv[3]; //username
  g_saved_environ[1] = argv[4]; //home
  g_saved_environ[2] = argv[5]; //spice_env
  g_saved_environ[3] = NULL;
  msg_mngt_init("suid_power", IO_MAX_BUF_LEN);
  memset(g_network_name, 0, MAX_NAME_LEN);
  memset(ctrl_path, 0, MAX_PATH_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_root_work, 0, MAX_PATH_LEN);
  strncpy(g_network_name, argv[1], MAX_NAME_LEN-1);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  root = g_root_path;
  snprintf(g_root_work,MAX_PATH_LEN-1,"%s/%s", root, CLOONIX_VM_WORKDIR);
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

