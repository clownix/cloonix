/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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
#include <sys/socket.h>
#include <sched.h>
#include <sys/mount.h>
#include <dirent.h>
#include <time.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "crun.h"
#include "console.h"
#include "net_phy.h"
#include "crun_utils.h"
#include "utils.h"
#include "pthexec.h"
#include "vwifi.h"
#include "guard.h"

/* The only root process of a cloonix net. */
static int  g_cloonix_rank;
static int  g_llid;
static uid_t g_caller_uid;
static gid_t g_caller_gid;
static int  g_watchdog_ok;
static char g_network_name[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_root_work_kvm[MAX_PATH_LEN];
static char g_mnt_loop_dir[MAX_PATH_LEN];
static char g_bin_dir[MAX_PATH_LEN];
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

char *get_root_path(void)
{
  return g_root_path;
}

char *get_net_name(void)
{
  return g_network_name;
}

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

static t_vmon *alloc_vmon(char *name, int vm_id)
{
  t_vmon *cur = NULL;
  if (find_vmon_by_vm_id(vm_id))
    KERR("ERROR %s: vm_id %d is still monitored for %s", name, vm_id,
         find_vmon_by_vm_id(vm_id)->name);
  else if (find_vmon_by_name(name))
    KERR("ERROR %s: already monitored as vm_id %d", name,
         find_vmon_by_name(name)->vm_id);
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

static char *get_work_vm_path(int vm_id)
{
  static char path[MAX_PATH_LEN];
  char *root = g_root_work_kvm;
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path,MAX_PATH_LEN-1, "%s/vm%d", root, vm_id);
  return(path);
}

static int check_pid_is_clownix( int pid, int vm_id)
{
  int result = 0;
  int i, len, fd;
  char path[MAX_PATH_LEN];
  char ndpath[MAX_PATH_LEN];
  static char cmd[MAX_BIG_BUF];
  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/cmdline", pid);
  fd = open(path, O_RDONLY);
  if (fd >= 0)
    {
    memset(cmd, 0, MAX_BIG_BUF);
    len = read(fd, cmd, MAX_BIG_BUF-2);
    if (len > 0)
      {
      for (i=0; i<len; i++)
        if (cmd[i] == 0)
          cmd[i] = ' ';
      snprintf(ndpath, MAX_PATH_LEN-1, "%s", get_work_vm_path(vm_id));
      if (strstr(cmd, ndpath))
        result = 1;
      }
    if (close(fd))
      KOUT("%d", errno);
    }
  return result;
}

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

uid_t get_caller_uid(void)
{
  return g_caller_uid;
}

gid_t get_caller_gid(void)
{
  return g_caller_gid;
}

static int check_and_set_uid(void)
{
  int result = -1;
  uid_t uid;
  if ((getuid() == 0) && getgid() == 0)
    {
    result = 0;
    }
  else
    {
    if (seteuid(0))
      KERR("ERROR seteuid %d", errno);
    if (setegid(0))
      KERR("ERROR setegid %d", errno);
    uid = geteuid();
    if (uid == 0)
      {
      result = 0;
      umask(0000);
      if (setuid(0))
        KOUT(" ");
      if (setgid(0))
        KOUT(" ");
      }
    else
      KERR("ERROR uid %d", uid);
    }
  return result;
}

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
          kill(cur->pid, SIGKILL);
        }
      else if (pid != cur->pid)
        KERR("ERROR %s: pid file now says %d, monitored pid is %d",
             cur->name, pid, cur->pid);
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

static void heartbeat (int delta)
{
  static int count_ticks_blkd = 0;
  count_ticks_blkd += 1;
  if (count_ticks_blkd == 10)
    {
    count_ticks_blkd = 0;
    automate_pid_monitor();
    crun_beat(g_llid);
    }
}

static void req_kill_clean_all(void)
{
  char root_work_crun[MAX_PATH_LEN];
  char *root = g_root_path;
  int pid;
  t_vmon *next, *cur = g_head_vmon;

  crun_kill_all();
  while(cur)
    {
    next = cur->next;
    pid = read_umid_pid(cur->vm_id);

    if (pid > 0)
      {
      kill(pid, SIGKILL);
      free_vmon(cur);
      }
    cur = next;
    }
  snprintf(root_work_crun, MAX_PATH_LEN-1,"%s/%s", root, CNT_DIR);
  crun_utils_unlink_sub_dir_files(root_work_crun);
  crun_utils_unlink_sub_dir_files(g_mnt_loop_dir);
}

void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  if (llid != g_llid)
    KERR("%s %d %d", g_network_name, llid, g_llid);
  if (tid != type_hop_suid_power)
    KERR("%s %d %d", g_network_name, tid, type_hop_suid_power);
  rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
  g_watchdog_ok = 1;
}

void rpct_recv_kil_req(int llid, int tid)
{
  req_kill_clean_all();
  exit(0);
}

static void qemu_launch(char *name, int vm_id, int argc,
                        char *line, char *resp, int llid, int tid)
{
  int result;
  char *ptr_start, *ptr;
  t_vmon *cur = alloc_vmon(name, vm_id);
  if (cur == NULL)
    {
    snprintf(resp, MAX_PATH_LEN-1,
             "cloonsuid_resp_launch_vm_ko name=%s", name);
    rpct_send_sigdiag_msg(llid, tid, resp);
    return;
    }
  ptr_start = strchr(line, ':');
  if (!ptr_start)
    KOUT("ERROR %s", line);
  ptr = strchr(ptr_start, '"');
  while(ptr)
    {
    *ptr = ' ';
    ptr = strchr(ptr, '"');
    } 
  /* Runs as root: pinned dtach, bash and script; files the caller can open. */
  if (!guard_launch_line(ptr_start+1))
    result = -1;
  else
    result = execute_cmd(ptr_start+1, 1, 0);
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
}

static char *cloonix_resp_phy(int nb_phy, t_topo_info_phy *phy)
{
  int i, ln, len = MAX_PATH_LEN + (nb_phy * MAX_NAME_LEN * 4);
  char *buf = (char *) malloc(len);
  memset(buf, 0, len);
  ln = sprintf(buf, "cloonsuid_resp_phy nb_phys=%d", nb_phy);
  for (i=0; i<nb_phy; i++)
    ln += sprintf(buf+ln, " phy_name:%s phy_mac:%s", phy[i].name,
                                                     phy[i].phy_mac);
  return buf;
}

static int peer_uid_is_owner(int llid)
{
  struct ucred cred;
  socklen_t len = sizeof(cred);
  int fd = get_fd_with_llid(llid);
  if (fd < 0)
    return 0;
  if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len))
    return 0;
  if ((cred.uid == g_caller_uid) || (cred.uid == 0))
    return 1;
  KERR("ERROR suid_power refused connection from uid %d", (int)cred.uid);
  return 0;
}

static int spawn_token_safe(char *t)
{
  int i, len;
  if ((!t) || (!t[0]))
    return 0;
  len = strlen(t);
  if (len >= MAX_PATH_LEN)
    return 0;
  if (t[0] == '-')
    return 0;
  if (strstr(t, ".."))
    return 0;
  for (i=0; i<len; i++)
    {
    char c = t[i];
    if ((c>='a')&&(c<='z')) continue;
    if ((c>='A')&&(c<='Z')) continue;
    if ((c>='0')&&(c<='9')) continue;
    if ((c=='_')||(c=='-')||(c=='.')||(c=='/')) continue;
    return 0;
    }
  return 1;
}

static int spawn_bin_allowed(char *bin)
{
  static const char * const names[5] = {
  "cloonix-ovs-drv", "cloonix-ovs-snf", "cloonix-ovs-a2b",
  "cloonix-ovs-c2c", "cloonix-ovs-nat"};
  char expected[MAX_PATH_LEN];
  int i;
  if (g_bin_dir[0] != '/')
    return 0;
  for (i=0; i<5; i++)
    {
    snprintf(expected, MAX_PATH_LEN-1, "%s/cloonfs/bin/%s", g_bin_dir, names[i]);
    if (!strcmp(bin, expected))
      return 1;
    }
  return 0;
}

/* The read only ovs passthrough, run here because ovsdb_server.sock and ovs-
   vswitchd.ctl belong to root */
static int ovs_word_allowed(char *tool, char *args)
{
  static char *vsctl = "list show find get list-br list-ports list-ifaces";
  char first[MAX_NAME_LEN];
  char *allowed, *ptr;
  int len;
  if (!strcmp(tool, "ovs-vsctl"))
    allowed = vsctl;
  else
    return 0;
  memset(first, 0, MAX_NAME_LEN);
  while ((*args == ' ') || (*args == '\t'))
    args += 1;
  /* No option anywhere: --log-file or --db would let the tool write or read
     as root wherever the caller says. */
  if ((*args == '-') || (strstr(args, " -")))
    return 0;
  ptr = strchr(args, ' ');
  len = ptr ? (int) (ptr - args) : (int) strlen(args);
  if ((len <= 0) || (len >= MAX_NAME_LEN - 2))
    return 0;
  memcpy(first, args, len);
  ptr = strstr(allowed, first);
  while (ptr)
    {
    if (((ptr == allowed) || (*(ptr-1) == ' ')) &&
        ((ptr[len] == 0) || (ptr[len] == ' ')))
      return 1;
    ptr = strstr(ptr + 1, first);
    }
  return 0;
}

static int ovs_shell_safe(char *args)
{
  char *bad = "\"';|&`$<>\n\\";
  int i;
  for (i = 0; args[i]; i++)
    if (strchr(bad, args[i]))
      return 0;
  return 1;
}

static void req_ovs(int llid, int tid, char *line)
{
  char tool[MAX_NAME_LEN];
  char cmd[2*MAX_PATH_LEN];
  char resp[MAX_PRINT_LEN];
  char rd[MAX_PATH_LEN];
  FILE *fp;
  char *args, *ptr;
  int child_pid, status = -1;
  memset(tool, 0, MAX_NAME_LEN);
  if (sscanf(line, "cloonsuid_req_ovs tool=%63s args:", tool) != 1)
    {
    KERR("ERROR bad ovs request %s", line);
    return;
    }
  args = strstr(line, "args:");
  if (!args)
    {
    KERR("ERROR bad ovs request %s", line);
    return;
    }
  args += strlen("args:");
  memset(cmd, 0, 2*MAX_PATH_LEN);
  if ((!ovs_shell_safe(args)) || (!ovs_word_allowed(tool, args)))
    {
    KERR("ERROR REFUSED ovs %s %s", tool, args);
    snprintf(resp, MAX_PRINT_LEN-1,
             "cloonsuid_resp_ovs_line refused by suid_power: %s", args);
    rpct_send_sigdiag_msg(llid, tid, resp);
    }
  else
    {
    snprintf(cmd, 2*MAX_PATH_LEN-1,
             "%s --no-syslog --db=unix:%s/ovsdb_server.sock %s",
             pthexec_cloonfs_ovs_vsctl(), g_root_path, args);
    fp = cmd_lio_popen(cmd, &child_pid, 0);
    if (fp == NULL)
      {
      KERR("ERROR could not run %s", cmd);
      snprintf(resp, MAX_PRINT_LEN-1,
               "cloonsuid_resp_ovs_line could not run the ovs tool");
      rpct_send_sigdiag_msg(llid, tid, resp);
      }
    else
      {
      while (fgets(rd, MAX_PATH_LEN-1, fp))
        {
        ptr = strchr(rd, '\n');
        if (ptr)
          *ptr = 0;
        snprintf(resp, MAX_PRINT_LEN-1, "cloonsuid_resp_ovs_line %s", rd);
        rpct_send_sigdiag_msg(llid, tid, resp);
        }
      fclose(fp);
      if (force_waitpid(child_pid, &status))
        KERR("ERROR waiting for %s", tool);
      }
    }
  snprintf(resp, MAX_PRINT_LEN-1, "cloonsuid_resp_ovs_end status=%d", status);
  rpct_send_sigdiag_msg(llid, tid, resp);
}

#define NS_MAX_PIDS 512

typedef struct t_ns_pid
{
  char name[MAX_NAME_LEN];
  int pid;
  unsigned long ino[7];
} t_ns_pid;

static t_ns_pid g_ns_pids[NS_MAX_PIDS];
static int g_nb_ns_pids;

static const char *g_ns_kind[7] =
  {"net", "mnt", "pid", "uts", "ipc", "user", "cgroup"};
static const char *g_ns_flag[7] =
  {"-n", "-m", "-p", "-u", "-i", "-U", "-C"};

static unsigned long ns_inode(int pid, const char *kind)
{
  char path[MAX_PATH_LEN], link[MAX_PATH_LEN];
  char *p;
  ssize_t len;
  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/ns/%s", pid, kind);
  len = readlink(path, link, MAX_PATH_LEN-2);
  if (len <= 0)
    return 0;
  link[len] = 0;
  p = strchr(link, '[');
  if (!p)
    return 0;
  return strtoul(p+1, NULL, 10);
}

static void ns_line(int llid, int tid, const char *txt)
{
  char resp[MAX_PRINT_LEN];
  snprintf(resp, MAX_PRINT_LEN-1, "cloonsuid_resp_ovs_line %s", txt);
  rpct_send_sigdiag_msg(llid, tid, resp);
}

static void req_ns_pid(char *line)
{
  char name[MAX_NAME_LEN];
  int pid, k;
  if (sscanf(line, "cloonsuid_req_ns_pid name=%63s pid=%d", name, &pid) != 2)
    {
    KERR("ERROR bad ns request %s", line);
    return;
    }
  if (g_nb_ns_pids >= NS_MAX_PIDS)
    return;
  memset(&(g_ns_pids[g_nb_ns_pids]), 0, sizeof(t_ns_pid));
  strncpy(g_ns_pids[g_nb_ns_pids].name, name, MAX_NAME_LEN-1);
  g_ns_pids[g_nb_ns_pids].pid = pid;
  for (k=0; k<7; k++)
    g_ns_pids[g_nb_ns_pids].ino[k] = ns_inode(pid, g_ns_kind[k]);
  g_nb_ns_pids += 1;
}

static void req_ns_go(int llid, int tid)
{
  char txt[MAX_PRINT_LEN], resp[MAX_PRINT_LEN];
  unsigned long mine[7], done[NS_MAX_PIDS];
  int k, i, j, nb_done, len, first_pid, nb_ns = 0;

  for (k=0; k<7; k++)
    mine[k] = ns_inode(getpid(), g_ns_kind[k]);

  ns_line(llid, tid, "kind    id           members  (from /proc/<pid>/ns, "
                     "one line per namespace)");
  for (k=0; k<7; k++)
    {
    nb_done = 0;
    for (i=0; i<g_nb_ns_pids; i++)
      {
      unsigned long ino = g_ns_pids[i].ino[k];
      if (ino == 0)
        continue;
      for (j=0; j<nb_done; j++)
        if (done[j] == ino)
          break;
      if (j < nb_done)
        continue;
      done[nb_done++] = ino;
      nb_ns += 1;
      first_pid = g_ns_pids[i].pid;
      len = snprintf(txt, MAX_PRINT_LEN-1, "%-7s %-12lu", g_ns_kind[k], ino);
      for (j=i; (j<g_nb_ns_pids) && (len < MAX_PRINT_LEN-100); j++)
        if (g_ns_pids[j].ino[k] == ino)
          len += snprintf(txt+len, MAX_PRINT_LEN-1-len, " %s",
                          g_ns_pids[j].name);
      if (ino == mine[k])
        len += snprintf(txt+len, MAX_PRINT_LEN-1-len,
                        "   <- where cloonix was started (the host)");
      ns_line(llid, tid, txt);
      if (ino != mine[k])
        {
        snprintf(txt, MAX_PRINT_LEN-1,
                 "        enter:  sudo nsenter -t %d %s %s", first_pid,
                 g_ns_flag[k],
                 (k == 0) ? "ip addr" :
                 (k == 2) ? "-m ps aux" :
                 (k == 1) ? "ls /" : "bash");
        ns_line(llid, tid, txt);
        }
      }
    }
  ns_line(llid, tid, "");
  ns_line(llid, tid, "nsenter (util-linux) runs a command inside the "
                     "namespaces of a target process -t <pid>:");
  ns_line(llid, tid, "  -n network   ip addr, ip route, ss -tunap, tcpdump");
  ns_line(llid, tid, "  -p -m pid    ps needs the pid AND the mount namespace, "
                     "its /proc is there: nsenter -t <pid> -p -m ps aux");
  ns_line(llid, tid, "  -m mount     ls /, cat /etc/hostname");
  ns_line(llid, tid, "  -u uts       hostname;  -i ipc;  -U user;  -C cgroup");
  ns_line(llid, tid, "  -a           all of them, then a shell: "
                     "sudo nsenter -t <pid> -a bash");
  ns_line(llid, tid, "  a kvm guest is not a namespace: its network is the "
                     "vhost of the ovs namespace, the guest itself is reached "
                     "with cloonix cli <net> ssh");
  snprintf(resp, MAX_PRINT_LEN-1, "%d namespaces over %d processes",
           nb_ns, g_nb_ns_pids);
  ns_line(llid, tid, resp);
  g_nb_ns_pids = 0;
  snprintf(resp, MAX_PRINT_LEN-1, "cloonsuid_resp_ovs_end status=0");
  rpct_send_sigdiag_msg(llid, tid, resp);
}

static void req_spawn(char *line)
{
  char copy[MAX_PRINT_LEN];
  char cmd[MAX_PRINT_LEN];
  char *args, *tok, *sav, *first = NULL;
  int nb = 0;
  args = line + strlen("cloonsuid_req_spawn");
  while (*args == ' ')
    args++;
  if (strlen(args) >= MAX_PRINT_LEN - 100)
    {
    KERR("ERROR spawn line too long");
    return;
    }
  strncpy(copy, args, MAX_PRINT_LEN-1);
  copy[MAX_PRINT_LEN-1] = 0;
  tok = strtok_r(copy, " ", &sav);
  while (tok)
    {
    if (!spawn_token_safe(tok))
      {
      KERR("ERROR spawn refused token");
      return;
      }
    if (!first)
      first = tok;
    nb++;
    tok = strtok_r(NULL, " ", &sav);
    }
  if ((nb == 0) || (!spawn_bin_allowed(first)))
    {
    KERR("ERROR spawn refused binary");
    return;
    }
  snprintf(cmd, MAX_PRINT_LEN-1,
           "CLOONIX_CALLER_UID=%d CLOONIX_CALLER_GID=%d %s",
           (int)g_caller_uid, (int)g_caller_gid, args);
  execute_cmd(cmd, 1, 0);
}

static int erase_tree(char *dir)
{
  int result = 0;
  char pth[MAX_PATH_LEN+256+2];
  DIR *dirptr;
  struct dirent *ent;
  struct stat st;
  dirptr = opendir(dir);
  if (dirptr)
    {
    while ((result == 0) && ((ent = readdir(dirptr)) != NULL))
      {
      if ((!strcmp(ent->d_name, ".")) || (!strcmp(ent->d_name, "..")))
        continue;
      snprintf(pth, sizeof(pth), "%s/%s", dir, ent->d_name);
      pth[sizeof(pth)-1] = 0;
      if (lstat(pth, &st))
        result = -1;
      else if (S_ISDIR(st.st_mode))
        result = erase_tree(pth);
      else if (unlink(pth))
        result = -1;
      }
    if (closedir(dirptr))
      result = -1;
    if (rmdir(dir))
      result = -1;
    }
  return result;
}

static int erase_workdir(char *net_name)
{
  char path[MAX_PATH_LEN];
  struct stat stat_path;
  if (!name_is_sane(net_name))
    {
    printf("ERROR, refused cloonix name: %s\n", net_name);
    return -1;
    }
  if (check_and_set_uid())
    {
    printf("ERROR, cloonix-suid-power is not suid root\n");
    return -1;
    }
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/var/lib/cloonix/%s", net_name);
  if (stat(path, &stat_path) == 0)
    {
    if ((g_caller_uid != 0) && (stat_path.st_uid != g_caller_uid))
      {
      printf("ERROR, %s belongs to uid %d, not to you\n",
             path, (int)stat_path.st_uid);
      return -1;
      }
    if (erase_tree(path))
      {
      printf("Path: %s already exists please remove and restart\n", path);
      return -1;
      }
    printf("Path: %s already exists, removing it\n", path);
    }
  return 0;
}

void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  t_topo_info_phy *net_phy;
  int nb_phy;
  char *str_net_phy;
  if (llid != g_llid)
    KERR("%s %s", g_network_name, line);
  else if (!strncmp(line,
  "cloonsuid_crun", strlen("cloonsuid_crun")))
    {
    crun_recv_poldiag_msg(llid, tid, line);
    }
  else if (!strncmp(line,
  "cloonsuid_req_phy", strlen("cloonsuid_req_phy")))
    {
    net_phy = net_phy_get(&nb_phy);
    str_net_phy = cloonix_resp_phy(nb_phy, net_phy);
    rpct_send_poldiag_msg(llid, tid, str_net_phy);
    free(str_net_phy);
    }
  else
    KERR("%s %s", g_network_name, line);
}

void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  char resp[MAX_PATH_LEN];
  char dtach[MAX_PATH_LEN];
  int  argc, vm_id, pid;
  t_vmon *cur;

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
      {
      vwifi_after_suidroot_init();
      rpct_send_sigdiag_msg(llid, tid, "cloonsuid_resp_suidroot_ok");
      }
    }
  else if (sscanf(line, 
  "cloonsuid_req_launch name=%63s vm_id=%d dtach=%299s argc=%d:",
         name, &vm_id, dtach, &argc) == 4)
    {
    qemu_launch(name, vm_id, argc, line, resp, llid, tid);
    if (!guard_path_in_root(dtach))
      KERR("ERROR REFUSED chown outside the net dir %s", dtach);
    else if (chown(dtach, g_caller_uid, g_caller_gid))
      KERR("ERROR chown %s %d", dtach, errno);
    else if (chmod(dtach, S_IRUSR | S_IWUSR))
      KERR("ERROR chmod %s %d", dtach, errno);
    }
  else if (sscanf(line,
  "cloonsuid_req_vm_kill vm_id: %d", &vm_id) == 1)
    {
    cur = find_vmon_by_vm_id(vm_id);
    if (!cur)
      KTRC("vm_kill for vm_id %d: no record, its qemu had already gone",
           vm_id);
    else
      {
      pid = cur->pid;
      if (pid <= 0)
        pid = read_umid_pid(vm_id);
      if ((pid > 0) && (!kill(pid, SIGKILL)))
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
      KERR("ERROR kill asked for bad pid %d", pid);
    else if (!guard_pid_is_callers(pid))
      KERR("ERROR REFUSED kill of pid %d, not a process of uid %d",
           pid, (int)g_caller_uid);
    else if ((kill(pid, SIGKILL)) && (errno != ESRCH))
      KERR("ERROR could not kill pid %d: %s", pid, strerror(errno));
    }
  else if (!strncmp(line,
  "cloonsuid_req_ovs", strlen("cloonsuid_req_ovs")))
    {
    req_ovs(llid, tid, line);
    }
  else if (!strncmp(line,
  "cloonsuid_req_ns_pid", strlen("cloonsuid_req_ns_pid")))
    {
    req_ns_pid(line);
    }
  else if (!strcmp(line, "cloonsuid_req_ns_go"))
    {
    req_ns_go(llid, tid);
    }
  else if (!strncmp(line,
  "cloonsuid_req_spawn", strlen("cloonsuid_req_spawn")))
    {
    req_spawn(line);
    }
  else if (!strncmp(line,
  "cloonsuid_crun", strlen("cloonsuid_crun")))
    {
    crun_recv_sigdiag_msg(llid, tid, line);
    }
  else if (!strncmp(line,
  "cloonsuid_air_", strlen("cloonsuid_air_")))
    {
    vwifi_air_msg(llid, tid, line);
    }
  else
    KERR("ERROR %s %s", g_network_name, line);
}

static void err_ctrl_cb (int llid, int err, int from)
{
  KERR("ERROR SUIDPOWER RECEIVED DISCONNECT %d %d", err, from);
  req_kill_clean_all();
  exit(0);
}

static void rx_ctrl_cb (int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    KOUT("%s %s", g_network_name, buf);
}

static void connect_from_ctrl_client(int llid, int llid_new)
{
  if (!peer_uid_is_owner(llid_new))
    {
    if (msg_exist_channel(llid_new))
      msg_delete_channel(llid_new);
    return;
    }
  msg_mngt_set_callbacks (llid_new, err_ctrl_cb, rx_ctrl_cb);
  g_llid = llid_new;
  g_watchdog_ok = 1;
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}

static void timer_heartbeat(void *data)
{
  static int count = 0;
  count += 1;
  if (g_watchdog_ok == 1)
    count = 1;
  if (count == 8)
    {
    count = 0;
    if (g_watchdog_ok == 0)
      {
      KERR("ERROR WATCHDOG SUID_POWER SELF DESTRUCT");
      req_kill_clean_all();
      }
    g_watchdog_ok = 0;
    }
  clownix_timeout_add(200, timer_heartbeat, NULL, NULL, NULL);
}

int get_cloonix_rank(void)
{
  return (g_cloonix_rank);
}

/* 0 while the switch is not connected: nothing can be sent to it then. */
int get_cloonix_llid(void)
{
  return (g_llid);
}

void run_dir_create(void)
{
  char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/var/lib/cloonix");
  check_and_set_uid();
  /* Sticky: any user may create a net dir, only its owner may remove or
     rename it. */
  mkdir(path, 01777);
  chmod(path, 01777);
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/var/lib/cloonix/bulk");
  mkdir(path, 0777);
  chmod(path, 0777);
}

int main (int argc, char *argv[])
{
  unsigned seed = 0;
  int fd;
  struct timespec ts;
  char ctrl_path[MAX_PATH_LEN];
  char *root;
  g_head_vmon = NULL;
  g_llid = 0;
  g_watchdog_ok = 0;
  g_caller_uid = getuid();
  g_caller_gid = getgid();
  if (!guard_group_allows(g_caller_uid))
    {
    printf("ERROR, uid %d is not a member of group %s\n",
           (int)g_caller_uid, CLOONIX_GROUP);
    KOUT("ERROR uid %d is not a member of group %s",
         (int)g_caller_uid, CLOONIX_GROUP);
    }
  if (argc == 1)
    {
    run_dir_create();
    return 0;
    }
  if ((argc == 3) && (!strcmp(argv[2], "erase_workdir")))
    return erase_workdir(argv[1]);
  umask(0000);
  fd = open("/dev/urandom", O_RDONLY);
  if (fd >= 0)
    {
    if (read(fd, &seed, sizeof(seed)) != sizeof(seed))
      KERR("ERROR /dev/urandom");
    close(fd);
    }
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  seed += ts.tv_nsec;
  srand(seed);
  /* 7: the process of the wifi medium, started by vwifi_init */
  if ((argc != 5) && (argc != 7))
    KOUT(" ");
  msg_mngt_init("suid_power", IO_MAX_BUF_LEN);
  memset(g_network_name, 0, MAX_NAME_LEN);
  memset(ctrl_path, 0, MAX_PATH_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_root_work_kvm, 0, MAX_PATH_LEN);
  memset(g_mnt_loop_dir, 0, MAX_PATH_LEN);
  strncpy(g_network_name, argv[1], MAX_NAME_LEN-1);
  set_cloonix_name(g_network_name);
  strncpy(g_root_path, argv[2], MAX_PATH_LEN-1);
  snprintf(g_mnt_loop_dir, MAX_PATH_LEN-1,"%s/mnt", argv[2]);
  /* The bin dir is the installed one; argv[3] only has to agree. */
  memset(g_bin_dir, 0, MAX_PATH_LEN);
  strncpy(g_bin_dir, pthexec_usr_libexec_cloonix(), MAX_PATH_LEN-1);
  if (strcmp(argv[3], g_bin_dir))
    KOUT("ERROR bin dir %s is not %s", argv[3], g_bin_dir);
  /* Net name and root path are checked, not taken from the caller as is. */
  if (!name_is_sane(argv[1]))
    KOUT("ERROR bad net name %s", argv[1]);
  snprintf(ctrl_path, MAX_PATH_LEN-1, "/var/lib/cloonix/%s", argv[1]);
  if (strcmp(argv[2], ctrl_path))
    KOUT("ERROR root path %s is not %s", argv[2], ctrl_path);
  guard_init(g_caller_uid, g_caller_gid, g_network_name, g_root_path);
  if (sscanf(argv[4], "%d", &g_cloonix_rank) != 1)
    KOUT("ERROR %s", argv[4]);
  pthexec_set_rank(g_cloonix_rank);
  if (argc == 7)
    {
    medium_main(atoi(argv[5]), atoi(argv[6]));
    return 0;
    }
  root = g_root_path;
  crun_init(root);
  snprintf(g_root_work_kvm, MAX_PATH_LEN-1, "%s/%s", root, VM_WORKDIR);
  snprintf(ctrl_path,MAX_PATH_LEN-1,"%s/%s", root, SUID_POWER_SOCK_DIR);
  unlink(ctrl_path);
  msg_mngt_heartbeat_init(heartbeat);
  string_server_unix(ctrl_path, connect_from_ctrl_client, "ctrl");
  if (chown(ctrl_path, g_caller_uid, g_caller_gid))
    KERR("ERROR chown %s %d", ctrl_path, errno);
  if (chmod(ctrl_path, S_IRUSR | S_IWUSR))
    KERR("ERROR chmod %s %d", ctrl_path, errno);
  console_init(root);
  if (daemon(0,0))
    KOUT("ERROR daemon %d", errno);
  net_phy_init();
  vwifi_init();
  if (seteuid(getuid()))
    KOUT("ERROR seteuid %d", errno);
  cloonix_set_pid(getpid());
  clownix_timeout_add(200, timer_heartbeat, NULL, NULL, NULL);
  msg_mngt_loop();
  return 0;
}

