/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
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

#include "io_clownix.h"
#include "stall_detect.h"
#include "file_read_write.h"
#include "rpc_clownix.h"
#include "ovs_execv.h"
#include "action.h"
#include "netns.h"
#include "ovs_drv.h"

#if MAX_NAME_LEN != 64
#error "the sscanf %63s widths below assume MAX_NAME_LEN 64"
#endif

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
static int g_count_missed_pid_req;
#define HEARTBEAT_PERIOD_MS 500
#define HEARTBEAT_LIMIT_MS  20000
#define HEARTBEAT_MISSED_MAX (HEARTBEAT_LIMIT_MS / HEARTBEAT_PERIOD_MS)
static t_stall g_stall;
static char g_name[MAX_NAME_LEN];
static char g_path[MAX_PATH_LEN];
static int g_fd_tx_to_netns;
static uint8_t g_buf_rx[TOT_NETNS_BUF_LEN];
static uint8_t g_buf_tx[TOT_NETNS_BUF_LEN];
static uint8_t g_head_msg[HEADER_NETNS_MSG];

static char *get_ns(void)
{
  return g_netns_namespace;
}

static char *get_net_name(void)
{
  return g_net_name;
}

/* One ip command, argv from a NULL terminated list. ip_ns_cmd runs it in */
/* the cloonix net namespace. quiet is call_ovs_popen's. */
static int ip_argv_run(char *ovs_dir, int quiet, const char *fct, int line,
                       int in_ns, va_list ap)
{
  char *argv[NB_ARG];
  int i = 0;
  memset(argv, 0, NB_ARG * sizeof(char *));
  argv[i++] = pthexec_ip_bin();
  while ((argv[i] = va_arg(ap, char *)) != NULL)
    {
    i += 1;
    if (i >= NB_ARG - 1)
      KOUT("ERROR %s too many ip args", fct);
    }
  return call_ovs_popen_ns(ovs_dir, argv, quiet, fct, line,
                           in_ns ? get_ns() : NULL);
}

static int ip_cmd(char *ovs_dir, int quiet, const char *fct, int line, ...)
{
  va_list ap;
  int result;
  va_start(ap, line);
  result = ip_argv_run(ovs_dir, quiet, fct, line, 0, ap);
  va_end(ap);
  return result;
}

static int ip_ns_cmd(char *ovs_dir, int quiet, const char *fct, int line, ...)
{
  va_list ap;
  int result;
  va_start(ap, line);
  result = ip_argv_run(ovs_dir, quiet, fct, line, 1, ap);
  va_end(ap);
  return result;
}

static void change_mac_macvlan(char *ovs_dir, char *name, char *mac)
{
  if (ip_ns_cmd(ovs_dir, 2, __FUNCTION__, 3,
                "link", "set", "dev", name, "addr", mac, NULL))
    KERR("ERROR change_mac_macvlan %s %s", name, mac);
}

static int put_in_namespace(char *ovs_dir, char *name)
{
  return ip_cmd(ovs_dir, 2, __FUNCTION__, 3,
                "link", "set", name, "netns", get_ns(), NULL);
}

static int set_dev_up_in_namespace(char *ovs_dir, char *name)
{
  return ip_ns_cmd(ovs_dir, 2, __FUNCTION__, 3,
                   "link", "set", "dev", name, "up", NULL);
}

static int set_dev_down_in_namespace(char *ovs_dir, char *name)
{
  return ip_ns_cmd(ovs_dir, 2, __FUNCTION__, 3,
                   "link", "set", "dev", name, "down", NULL);
}

static int set_dev_up_link(char *ovs_dir, char *name)
{
  return ip_cmd(ovs_dir, 2, __FUNCTION__, 4,
                "link", "set", "dev", name, "up", NULL);
}

static int change_name_within_namespace(char *ovs_dir, char *onm, char *nnm)
{
  return ip_ns_cmd(ovs_dir, 2, __FUNCTION__, 3,
                   "link", "set", onm, "name", nnm, NULL);
}

static int put_back_in_main_namespace(char *ovs_dir, char *name)
{
  return ip_ns_cmd(ovs_dir, 2, __FUNCTION__, 3,
                   "link", "set", name, "netns", "1", NULL);
}

static int change_mac_address(char *ovs_dir, char *name, char *mac)
{
  return ip_cmd(ovs_dir, 2, __FUNCTION__, 3,
                "link", "set", "dev", name, "address", mac, NULL);
}

static void del_link_quiet(char *ovs_dir, char *name)
{
  ip_cmd(ovs_dir, 1, __FUNCTION__, 1, "link", "del", "name", name, NULL);
}

static int ovs_cmd_add_tap(char *ovs_bin, char *ovs_dir, char *name,
                           char *vhost, char *mac)
{
  int result;
  result = ip_cmd(ovs_dir, 2, __FUNCTION__, 1, "link", "add", "name", name,
                  "address", mac, "type", "veth", "peer", "name", vhost, NULL);
  if (result)
    KERR("ERROR tap %s: the veth pair %s/%s could not be made", name, name,
         vhost);
  else
    {
    result = put_in_namespace(ovs_dir, vhost);
    if (result)
      KERR("ERROR tap %s: %s could not enter the net namespace", name, vhost);
    else
      {
      result = set_dev_up_in_namespace(ovs_dir, vhost);
      if (result)
        KERR("ERROR tap %s: %s could not be set up", name, vhost);
      else
        {
        result = set_dev_up_link(ovs_dir, name);
        if (result)
          KERR("ERROR tap %s could not be set up", name);
        }
      }
    if (result)
      del_link_quiet(ovs_dir, name);
    }
  return result;
}

static int ovs_cmd_del_tap(char *ovs_bin, char *ovs_dir, char *name, char *vhost)
{
  int result;
  result = ip_cmd(ovs_dir, 2, __FUNCTION__, 1, "link", "del", "name", name,
                  NULL);
  if (result)
    KERR("ERROR tap %s: veth %s could not be deleted, peer %s", name, name,
         vhost);
  return result;
}

static int ovs_cmd_del_aphy(char *ovs_bin, char *ovs_dir,
                            char *name, char *vhost)
{
  int result = -1;
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

static int ovs_cmd_add_aphy(char *ovs_bin, char *ovs_dir, char *name,
                            char *vhost, char *mac)
{
  int result = -1;

  if (change_mac_address(ovs_dir, name, mac))
    KERR("ERROR phy %s: mac %s could not be set", name, mac);
  else if (put_in_namespace(ovs_dir, name))
    KERR("ERROR phy %s could not enter the net namespace", name);
  else if (change_name_within_namespace(ovs_dir, name, vhost))
    {
    KERR("ERROR phy %s could not be renamed %s in the net namespace, "
         "putting it back", name, vhost);
    put_back_in_main_namespace(ovs_dir, name);
    }
  else
    {
    result = set_dev_up_in_namespace(ovs_dir, vhost);
    if (result)
      {
      KERR("ERROR phy %s: %s could not be set up, putting it back", name,
           vhost);
      ovs_cmd_del_aphy(ovs_bin, ovs_dir, name, vhost);
      }
    }
  return result;
}

static int ovs_cmd_del_mphy(char *ovs_bin, char *ovs_dir,
                            char *name, char *vhost)
{
  int result;
  result = set_dev_down_in_namespace(ovs_dir, vhost);
  if (result)
    KERR("ERROR %s %s", name, vhost);
  else
    {
    result = ip_ns_cmd(ovs_dir, 2, __FUNCTION__, 3, "link", "del", vhost, NULL);
    if (result)
      KERR("ERROR %s %s", name, vhost);
    }
  return result;
}

static int ovs_cmd_add_mphy(char *ovs_bin, char *ovs_dir, char *name,
                            char *vhost, char *mac)
{
  static int tmpnum=0;
  int result;
  char macvlan[MAX_NAME_LEN];

  tmpnum += 1;
  if (tmpnum == 100)
    tmpnum = 1;
  memset(macvlan, 0, MAX_NAME_LEN);
  snprintf(macvlan, MAX_NAME_LEN-1, "mcvl%d", tmpnum);
  result = ip_cmd(ovs_dir, 2, __FUNCTION__, 2, "link", "add", macvlan,
                  "link", name, "type", "macvlan", "mode", "bridge", NULL);
  if (result)
    KERR("ERROR phy %s: macvlan %s could not be made", name, macvlan);
  else
    {
    result = change_mac_address(ovs_dir, macvlan, mac);
    if (result)
      KERR("ERROR phy %s: mac %s could not be set on %s", name, mac, macvlan);
    else
      {
      result = put_in_namespace(ovs_dir, macvlan);
      if (result)
        KERR("ERROR phy %s: %s could not enter the net namespace", name,
             macvlan);
      else
        {
        result = change_name_within_namespace(ovs_dir, macvlan, vhost);
        if (result)
          KERR("ERROR phy %s: %s could not be renamed %s", name, macvlan,
               vhost);
        else
          {
          result = set_dev_up_in_namespace(ovs_dir, vhost);
          if (result)
             KERR("ERROR phy %s: %s could not be set up", name, vhost);
          else
            {
            result = set_dev_up_link(ovs_dir, name);
            if (result)
              KERR("ERROR phy %s could not be set up", name);
            }
          }
        }
      }
    if (result)
      {
      if (ovs_cmd_del_mphy(ovs_bin, ovs_dir, name, vhost))
        del_link_quiet(ovs_dir, macvlan);
      }
    }
  return result;
}

static void action_add_tap(char *bin, char *db, char *respb,
                           char *name, char *vhost, char *mac)
{
  action_resp(respb, ovs_cmd_add_tap(bin, db, name, vhost, mac),
              "ovs_add_tap name=%s vhost=%s mac=%s", name, vhost, mac);
}

static void action_del_tap(char *bin, char *db, char *respb,
                           char *name, char *vhost)
{
  action_resp(respb, ovs_cmd_del_tap(bin, db, name, vhost),
              "ovs_del_tap name=%s vhost=%s", name, vhost);
}

static void action_add_phy(char *bin, char *db, char *respb,
                           char *name, char *vhost, char *mac,
                           int num_macvlan)
{
  int err;
  if (num_macvlan == 0)
    err = ovs_cmd_add_aphy(bin, db, name, vhost, mac);
  else
    err = ovs_cmd_add_mphy(bin, db, name, vhost, mac);
  action_resp(respb, err, "ovs_add_phy name=%s vhost=%s mac=%s num_macvlan=%d",
              name, vhost, mac, num_macvlan);
}

static void action_del_phy(char *bin, char *db, char *respb,
                           char *name, char *vhost, int num_macvlan)
{
  int err;
  if (num_macvlan == 0)
    err = ovs_cmd_del_aphy(bin, db, name, vhost);
  else
    err = ovs_cmd_del_mphy(bin, db, name, vhost);
  action_resp(respb, err, "ovs_del_phy name=%s vhost=%s num_macvlan=%d",
              name, vhost, num_macvlan);
}

static void common_end_of_all_destroy(void)
{
  char *db = g_ovs_dir;
  char err[MAX_PATH_LEN] = "";
  char *var_dir = "/var/lib/cloonix";
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
  ip_cmd(db, 0, __FUNCTION__, 1, "netns", "del", get_ns(), NULL);
  if (!access(db, R_OK))
    {
    if ((!strncmp(var_dir, g_ovs_dir, strlen(var_dir))) &&
        (strstr(db, g_net_name)))
      {
      if (rm_tree(db, err))
        KERR("ERROR ERASE %s %s", db, err);
      }
    else
      KERR("ERROR %s", db);
    }
  exit(0);
}

static void log_write(int tid, char *line, int is_resp)
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
    fprintf(fp_log, "%04X:%s\n%s", tid, line, is_resp?"\n":"");
    fflush(fp_log);
    fclose(fp_log);
    }
}

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
        {
        KERR("ERROR OUT OF OVS %s", line);
        common_end_of_all_destroy();
        }
      if (!strcmp(line, "ovs_resp_ovs_ko"))
        {
        KERR("ERROR OUT OF OVS %s", line);
        common_end_of_all_destroy();
        }
      log_write(tid, line, 1);
      rpct_send_sigdiag_msg(g_cloonix_llid, tid, line);
      }
    }
  return (len + HEADER_NETNS_MSG);
}

static void rx_netns_err(int llid, int err, int from)
{
  KOUT("ERROR %d %d %d", llid, err, from);
}

int get_cloonix_listen_fd(void)
{
  return g_cloonix_listen_fd;
}

int get_cloonix_fd(void)
{
  return g_cloonix_fd;
}

/* one line of a /proc file of pid, the first starting with key, or the
   first line when key is empty; "?" when it cannot be read */
static void proc_line(int pid, char *file, char *key, char *out, int max)
{
  char path[MAX_PATH_LEN], line[MAX_PATH_LEN];
  FILE *fp;
  snprintf(out, max, "?");
  snprintf(path, sizeof(path), "/proc/%d/%s", pid, file);
  fp = fopen(path, "r");
  if (!fp)
    return;
  while (fgets(line, sizeof(line), fp))
    {
    if ((!key[0]) || (!strncmp(line, key, strlen(key))))
      {
      line[strcspn(line, "\n")] = 0;
      snprintf(out, max, "%s", line);
      break;
      }
    }
  fclose(fp);
}

/* what the switch is doing when it stops answering: gone, blocked in a
   call, or turning without coming back to its loop */
static void tell_switch_state(void)
{
  struct ucred cr;
  socklen_t len = sizeof(cr);
  char state[MAX_NAME_LEN], wchan[MAX_NAME_LEN], sysc[MAX_NAME_LEN];
  char stat[MAX_PATH_LEN];
  memset(&cr, 0, sizeof(cr));
  if ((g_cloonix_fd <= 0) ||
      getsockopt(g_cloonix_fd, SOL_SOCKET, SO_PEERCRED, &cr, &len) ||
      (cr.pid <= 0))
    KERR("ERROR the pid of the switch is not known on its socket %d: %s",
         g_cloonix_fd, strerror(errno));
  else if (kill(cr.pid, 0))
    KERR("ERROR the switch pid %d is gone: %s", cr.pid, strerror(errno));
  else
    {
    proc_line(cr.pid, "status", "State:", state, sizeof(state));
    proc_line(cr.pid, "wchan", "", wchan, sizeof(wchan));
    proc_line(cr.pid, "syscall", "", sysc, sizeof(sysc));
    proc_line(cr.pid, "stat", "", stat, sizeof(stat));
    KERR("ERROR the switch pid %d is there: %s, wchan %s, syscall %s, "
         "stat %.200s", cr.pid, state, wchan, sysc, stat);
    /* its own handler logs rip and backtrace, then it dies */
    if (kill(cr.pid, SIGQUIT))
      KERR("ERROR SIGQUIT to the switch pid %d: %s", cr.pid, strerror(errno));
    else
      usleep(500000);
    }
}

static void timeout_heartbeat(void *data)
{
  int gap = stall_beat(&g_stall);
  int credit_beats;

  /* THE THREE REASONS THIS FIRES ARE NOT THE SAME REASON. */
  if (g_ovs_pid > 0)
    {
    if (kill(g_ovs_pid, 0))
      {
      KERR("ERROR ovs %d is GONE (kill 0 failed), ovsdb %d, destroying",
           g_ovs_pid, g_ovsdb_pid);
      common_end_of_all_destroy();
      }
    }
  if (g_ovsdb_pid > 0)
    {
    if (kill(g_ovsdb_pid, 0))
      {
      KERR("ERROR ovsdb %d is GONE (kill 0 failed), ovs %d, destroying",
           g_ovsdb_pid, g_ovs_pid);
      common_end_of_all_destroy();
      }
    }
  if (g_netns_pid > 0)
    {
    int st;
    if (waitpid(g_netns_pid, &st, WNOHANG) == g_netns_pid)
      {
      if (WIFEXITED(st))
        KERR("ERROR the netns worker %d EXITED with %d, every ovs request "
             "since is unanswered, destroying", g_netns_pid, WEXITSTATUS(st));
      else
        KERR("ERROR the netns worker %d KILLED by signal %d, every ovs "
             "request since is unanswered, destroying", g_netns_pid,
             WIFSIGNALED(st) ? WTERMSIG(st) : 0);
      g_netns_pid = 0;
      common_end_of_all_destroy();
      }
    }
  /* The two above are evidence: the process is not there. */
  if (gap > 0)
    {
    KERR("WARNING this loop did not beat for %d ms (%s): %d beat(s) were "
         "never sent, so they are not counted against the switch", gap,
         stall_why(&g_stall), gap / HEARTBEAT_PERIOD_MS);
    }
  g_count_missed_pid_req += 1;
  credit_beats = stall_credit_ms(&g_stall) / HEARTBEAT_PERIOD_MS;
  if (g_count_missed_pid_req > HEARTBEAT_MISSED_MAX + credit_beats)
    {
    if (credit_beats)
      KERR("ERROR the switch has not answered for %d beats of being given "
           "the chance (%d more were forgiven as this process was not "
           "running), ovs %d ovsdb %d, destroying",
           HEARTBEAT_MISSED_MAX, credit_beats, g_ovs_pid, g_ovsdb_pid);
    else
      KERR("ERROR the switch has not answered %d beats and this process "
           "was running throughout, ovs %d ovsdb %d, destroying",
           g_count_missed_pid_req, g_ovs_pid, g_ovsdb_pid);
    tell_switch_state();
    common_end_of_all_destroy();
    }
  else if (stall_credit_exhausted(&g_stall))
    {
    KERR("WARNING the forgiveness for not being scheduled is used up. From "
         "here the switch is judged as if this machine were idle");
    }
  clownix_timeout_add(50, timeout_heartbeat, NULL, NULL, NULL);
}

static void timeout_heartbeat_start(void *data)
{
  if ((g_ovsdb_pid <= 0) || (g_ovs_pid <= 0))
    KOUT("ERROR at heartbeat start, ovs %d ovsdb %d, one is not launched",
         g_ovs_pid, g_ovsdb_pid);
  stall_init(&g_stall, HEARTBEAT_PERIOD_MS, HEARTBEAT_LIMIT_MS);
  clownix_timeout_add(50, timeout_heartbeat, NULL, NULL, NULL);
}

void rpct_recv_kil_req(int llid, int tid)
{
  KOUT(" ");
}

void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  if ((g_ovs_pid == 0) && (g_ovsdb_pid == 0)) 
    rpct_send_pid_resp(llid, tid, name, num, 0, getpid());
  else
    rpct_send_pid_resp(llid, tid, name, num, g_ovs_pid, g_ovsdb_pid);
  g_count_missed_pid_req = 0;
  stall_answered(&g_stall);
}

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
"ovs_add_tap name=%63s vhost=%63s mac=%63s", name, vhost, mac) == 3)
    {
    action_add_tap(bin, db, respb, name, vhost, mac);
    log_write(tid, respb, 1);
    rpct_send_sigdiag_msg(g_cloonix_llid, tid, respb);
    }
  else if (sscanf(line,
"ovs_del_tap name=%63s vhost=%63s", name, vhost) == 2)
    {
    action_del_tap(bin, db, respb, name, vhost);
    log_write(tid, respb, 1);
    rpct_send_sigdiag_msg(g_cloonix_llid, tid, respb);
    }

  else if (sscanf(line,
"ovs_add_phy name=%63s vhost=%63s mac=%63s num_macvlan=%d", name, vhost, mac, &num_macvlan) == 4)
    {
    action_add_phy(bin, db, respb, name, vhost, mac, num_macvlan);
    log_write(tid, respb, 1);
    rpct_send_sigdiag_msg(g_cloonix_llid, tid, respb);
    }
  else if (sscanf(line,
"ovs_del_phy name=%63s vhost=%63s num_macvlan=%d", name, vhost, &num_macvlan) == 3)
    {
    action_del_phy(bin, db, respb, name, vhost, num_macvlan);
    log_write(tid, respb, 1);
    rpct_send_sigdiag_msg(g_cloonix_llid, tid, respb);
    }
  else if (sscanf(line,
"ovs_change_macvlan_mac type=%d macv=%63s nmac=%63s", &type, macv, mac) == 3)
    {
    if (type == 1)
      change_mac_macvlan(db, macv, mac);
    }
  else
    {
    log_write(tid, line, 0);
    tx_to_netns(tid, line);
    }
}

static void rx_cloonix_cb(int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    {
    KOUT("%s", buf);
    }
}

static void cloonix_err_cb(int llid, int err, int from)
{
  KERR("ERROR DECONNECT CLOONIX");
  common_end_of_all_destroy();
}

static void cloonix_connect(int llid, int llid_new)
{
  msg_mngt_set_callbacks (llid_new, cloonix_err_cb, rx_cloonix_cb);
  g_cloonix_fd = get_fd_with_llid(llid_new);
  g_cloonix_llid = llid_new;
}

static void cmd_interrupt(int signo)
{
  KERR("ERROR INTERRUPT");
  common_end_of_all_destroy();
}

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
  set_cloonix_name(g_net_name);
  memcpy(g_path, argv[2], MAX_PATH_LEN-1);
  if (file_exists(g_path, R_OK))
    KOUT("PROBLEM WITH: %s EXISTS!", g_path);
  dir_sock = mydirname(g_path);
  if (!file_exists(dir_sock, R_OK))
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
 
int main (int argc, char *iargv[])
{
  int llid, fd_rx_from_netns;
  char net[2*MAX_NAME_LEN];
  char name[2*MAX_NAME_LEN];
  char sock[2*MAX_PATH_LEN];
  char *ctl_argv[4] = {net, name, sock, NULL};
  g_ovsdb_launched = 0;
  g_ovsdb_pid = 0;
  g_ovs_launched = 0;
  g_ovs_pid = 0;
  g_count_missed_pid_req = 0;
  if (seteuid(0))
    KERR("ERROR seteuid %s", strerror(errno));
  if (setegid(0))
    KERR("ERROR setegid %s", strerror(errno));
  umask(0000);
  if (argc != 6)
    KOUT("wrong params nb: net,name,sock,ovs_bin,ovs_dir as params");
  memset(net, 0, sizeof(net));
  memset(name, 0, sizeof(name));
  memset(sock, 0, sizeof(sock));
  memset(g_ovs_bin, 0, MAX_PATH_LEN);
  memset(g_ovs_dir, 0, MAX_PATH_LEN);
/* Setuid binary: its arguments are not trusted. */
  if (!name_is_sane(iargv[1]))
    KOUT("ERROR refused net name");
  if (!name_is_sane(iargv[2]))
    KOUT("ERROR refused drv name");
  if (!path_is_sane(iargv[3]))
    KOUT("ERROR refused ctrl socket path");
  if (!path_is_sane(iargv[4]))
    KOUT("ERROR refused ovs bin path");
  if (!path_is_sane(iargv[5]))
    KOUT("ERROR refused ovs dir path");
  strncpy(net,  iargv[1], MAX_NAME_LEN-1);
  strncpy(name, iargv[2], MAX_NAME_LEN-1);
  strncpy(sock, iargv[3], MAX_PATH_LEN-1);
  strncpy(g_ovs_bin, iargv[4], MAX_PATH_LEN-1);
  strncpy(g_ovs_dir, iargv[5], MAX_PATH_LEN-1);
  init_environ(net, g_ovs_bin, g_ovs_dir);
  signal(SIGINT, cmd_interrupt);
  cloonix_part_init(ctl_argv);
  clownix_timeout_add(6000, timeout_heartbeat_start, NULL, NULL, NULL);
  if (daemon(0,0))
    KOUT("ERROR daemon %s", strerror(errno));
  ip_cmd(g_ovs_dir, 1, __FUNCTION__, 1, "netns", "del", get_ns(), NULL);
  ip_cmd(g_ovs_dir, 0, __FUNCTION__, 2, "netns", "add", get_ns(), NULL);
  g_netns_pid = netns_open(g_net_name, &fd_rx_from_netns, &g_fd_tx_to_netns,
                           g_ovs_bin, g_ovs_dir);
  llid = msg_watch_fd(fd_rx_from_netns, rx_netns_cb, rx_netns_err, "ovs");
  if (llid == 0)
    KOUT("ERROR OVS_DRV");
  msg_mngt_loop();
  return 0;
}

