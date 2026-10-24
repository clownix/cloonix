/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "pcap_relay.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "ovs_nat_main.h"
#include "ovs_a2b.h"
#include "ovs_tap.h"
#include "ovs_phy.h"
#include "ovs_c2c.h"
#include "ovs_snf.h"
#include "wifi_air.h"
#include "llid_trace.h"
#include "msg.h"
#include "cnt.h"
#include "kvm.h"
#include "suid_power.h"
#include "lan_to_name.h"
#include "mactopo.h"

typedef struct t_snf
{
  char tap[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int  num;
  int item_type;
  char vhost[MAX_NAME_LEN];
  char socket[MAX_PATH_LEN];
  char lan[MAX_NAME_LEN];
  char lan_wanted[MAX_NAME_LEN];
  int count;
  int llid;
  int pid;
  int closed_count;
  int suid_root_done;
  int watchdog_count;
  int no_mirror_count;
  int said_no_mirror;
  int ms;
  int pkt_tx;
  int pkt_rx;
  int byte_tx;
  int byte_rx;
  t_fct_cb callback_process_up;
  int cli_llid;
  int cli_tid;
  int cli_llid_count;
  int dumpcap_pid;
  int dumpcap_retried;
  /* Which launch of a dumpcap this record's pid belongs to. */
  int dumpcap_serial;
  int said_gone;
  unsigned int gen;
  char name_num[MAX_NAME_LEN];
  /* Set the moment a teardown of this snf is asked for, cleared never -- the
     record is freed shortly after. */
  int being_stopped;
  int nb_add_ok;
  int nb_add_ko;
  int nb_del_ko;
  int mirror_on;
  int sync_deferred;
  struct t_snf *prev;
  struct t_snf *next;
} t_snf;

static unsigned int g_snf_gen;
static int g_dumpcap_serial;
static char g_cloonix_net[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_bin_snf[MAX_PATH_LEN];
static int dumpcap_start(t_snf *cur, char *name_num);
static void timer_dumpcap_restart(void *data);
static void defer_snf_restart(char *name, int num, int item_type,
                              char *vhost, char *lan);

/* An snf is wanted on the eth of a machine there, not being deleted, the eth
   of the s kind. */
static int eth_wants_snf(char *name, int num)
{
  t_cnt *cnt = find_cnt(name);
  t_vm *vm;
  if (cnt)
    return ((!cnt->poweroff_done) && (num >= 0) &&
            (num < cnt->cnt.nb_tot_eth) &&
            (cnt->cnt.eth_table[num].endp_type == endp_type_eths));
  vm = cfg_get_vm(name);
  return ((vm) && (!vm->vm_to_be_killed) && (num >= 0) &&
          (num < vm->kvm.nb_tot_eth) &&
          (vm->kvm.eth_table[num].endp_type == endp_type_eths));
}
static t_snf *g_head_snf;
static int g_nb_snf;

#define NB_SNF_HISTORY 4096

typedef struct t_snf_event
{
  char tap[MAX_NAME_LEN];
  char what[128];
  unsigned long at_ms;
} t_snf_event;

static t_snf_event g_snf_history[NB_SNF_HISTORY];
static int g_snf_history_idx;

static void snf_hist(char *tap, char *fmt, ...)
{
  t_snf_event *ev = &(g_snf_history[g_snf_history_idx]);
  va_list ap;
  memset(ev, 0, sizeof(t_snf_event));
  strncpy(ev->tap, tap, MAX_NAME_LEN-1);
  va_start(ap, fmt);
  vsnprintf(ev->what, sizeof(ev->what)-1, fmt, ap);
  va_end(ap);
  ev->at_ms = cloonix_now_ms();
  g_snf_history_idx = (g_snf_history_idx + 1) % NB_SNF_HISTORY;
}

static void snf_kill_pid(int pid, char *tap, char *why)
{
  if (pid <= 0)
    return;
  if (!kill(pid, SIGKILL))
    {
    snf_hist(tap, "killed pid %d (%s)", pid, why);
    return;
    }
  if (errno == ESRCH)
    {
    snf_hist(tap, "pid %d had already gone (%s)", pid, why);
    return;
    }
  if (errno == EPERM)
    {
    snf_hist(tap, "pid %d is root, asking suid_power to kill it (%s)",
             pid, why);
    KERR("WARNING snf %s: pid %d is root and this switch is not, so the "
         "kill was refused. suid_power is asked to do it (%s)", tap, pid,
         why);
    suid_power_kill_pid(pid);
    return;
    }
  KERR("ERROR snf %s: killing pid %d failed: %s (%s)", tap, pid,
       strerror(errno), why);
}

static int snf_pid_is_this_tap(int pid, char *tap)
{
  char path[MAX_PATH_LEN];
  char buf[1024];
  int fd, len, i, ok = 0;
  if (pid <= 0)
    return 0;
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/cmdline", pid);
  fd = open(path, O_RDONLY);
  if (fd < 0)
    return 0;
  memset(buf, 0, sizeof(buf));
  len = read(fd, buf, sizeof(buf)-1);
  close(fd);
  if (len <= 0)
    return 0;
  for (i=0; i<len; i++)
    if (buf[i] == 0)
      buf[i] = ' ';
  if ((strstr(buf, "cloonix-ovs-snf")) && (strstr(buf, tap)))
    ok = 1;
  return ok;
}

static int snf_find_pid_of_dumpcap(char *name_num)
{
  DIR *d = opendir("/proc");
  struct dirent *e;
  char path[MAX_PATH_LEN];
  char buf[1024];
  int fd, len, i, pid, found = 0;
  if (!d)
    return 0;
  while ((e = readdir(d)) != NULL)
    {
    if ((e->d_name[0] < '0') || (e->d_name[0] > '9'))
      continue;
    pid = atoi(e->d_name);
    memset(path, 0, MAX_PATH_LEN);
    snprintf(path, MAX_PATH_LEN-1, "/proc/%d/cmdline", pid);
    fd = open(path, O_RDONLY);
    if (fd < 0)
      continue;
    memset(buf, 0, sizeof(buf));
    len = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (len <= 0)
      continue;
    for (i=0; i<len; i++)
      if (buf[i] == 0)
        buf[i] = ' ';
    if ((strstr(buf, "cloonix-dumpcap")) && (strstr(buf, name_num)))
      {
      found = pid;
      break;
      }
    }
  closedir(d);
  return found;
}

/* The pid of the process that owns this tap, by asking /proc. */
static int snf_find_pid_of_tap(char *tap)
{
  DIR *d = opendir("/proc");
  struct dirent *e;
  int pid, found = 0;
  if (!d)
    return 0;
  while ((e = readdir(d)) != NULL)
    {
    if ((e->d_name[0] < '0') || (e->d_name[0] > '9'))
      continue;
    pid = atoi(e->d_name);
    if (snf_pid_is_this_tap(pid, tap))
      {
      found = pid;
      break;
      }
    }
  closedir(d);
  return found;
}

static void snf_hist_forget(char *tap)
{
  int i;
  for (i=0; i<NB_SNF_HISTORY; i++)
    {
    if (!g_snf_history[i].tap[0])
      continue;
    if (!strcmp(g_snf_history[i].tap, tap))
      memset(&(g_snf_history[i]), 0, sizeof(t_snf_event));
    }
}

static int snf_hist_has(char *tap, char *needle)
{
  int i, idx;
  for (i=0; i<NB_SNF_HISTORY; i++)
    {
    idx = (g_snf_history_idx + i) % NB_SNF_HISTORY;
    if (!g_snf_history[idx].tap[0])
      continue;
    if (strcmp(g_snf_history[idx].tap, tap))
      continue;
    if (strstr(g_snf_history[idx].what, needle))
      return 1;
    }
  return 0;
}

/* One death of an snf is seen from several places within a second; the */
/* history goes out with the first and the others refer to it. */
#define NB_HIST_DUMPED 64
#define HIST_DUMP_AGAIN_MS 10000
typedef struct t_hist_dumped
{
  char tap[MAX_NAME_LEN];
  unsigned long at_ms;
} t_hist_dumped;
static t_hist_dumped g_hist_dumped[NB_HIST_DUMPED];
static int g_hist_dumped_idx;

static int hist_dumped_recently(char *tap, unsigned long now)
{
  int i;
  for (i=0; i<NB_HIST_DUMPED; i++)
    {
    if ((!strcmp(g_hist_dumped[i].tap, tap)) &&
        (now - g_hist_dumped[i].at_ms < HIST_DUMP_AGAIN_MS))
      return (int) (now - g_hist_dumped[i].at_ms);
    }
  strncpy(g_hist_dumped[g_hist_dumped_idx].tap, tap, MAX_NAME_LEN-1);
  g_hist_dumped[g_hist_dumped_idx].at_ms = now;
  g_hist_dumped_idx = (g_hist_dumped_idx + 1) % NB_HIST_DUMPED;
  return -1;
}

/* The last event of the tap before its record was freed: what ended it. */
static char *snf_hist_cause(char *tap)
{
  int i, idx;
  for (i=1; i<=NB_SNF_HISTORY; i++)
    {
    idx = (g_snf_history_idx - i + NB_SNF_HISTORY) % NB_SNF_HISTORY;
    if ((!g_snf_history[idx].tap[0]) || strcmp(g_snf_history[idx].tap, tap))
      continue;
    if (strncmp(g_snf_history[idx].what, "record freed", 12))
      return g_snf_history[idx].what;
    }
  return "no event recorded";
}

/* One block: its head is gated, never a line in the middle of it. */
static void snf_hist_dump(char *tap, char *why)
{
  unsigned long now = cloonix_now_ms();
  int i, idx, passed, nb = 0, ago = hist_dumped_recently(tap, now);
  if (ago >= 0)
    {
    KERR("WARNING snf %s history (%s): as shown %d ms ago", tap, why, ago);
    return;
    }
  KERR_HEAD(passed, "WARNING snf %s history (%s):", tap, why);
  for (i=0; i<NB_SNF_HISTORY; i++)
    {
    idx = (g_snf_history_idx + i) % NB_SNF_HISTORY;
    if (!g_snf_history[idx].tap[0])
      continue;
    if (strcmp(g_snf_history[idx].tap, tap))
      continue;
    nb += 1;
    KERR_MORE(passed, "WARNING snf %s history: %lu ms ago: %s", tap,
              now - g_snf_history[idx].at_ms, g_snf_history[idx].what);
    }
  if (!nb)
    KERR_MORE(passed, "WARNING snf %s history: nothing in the last %d snf "
              "events", tap, NB_SNF_HISTORY);
}

int ovs_snf_get_qty(void)
{
  return (g_nb_snf);
}

static t_snf *find_snf_with_name_num(char *name, int num)
{
  t_snf *cur = g_head_snf;
  while(cur)
    {
    if ((!strcmp(cur->name, name)) && (cur->num == num))
      break;
    cur = cur->next;
    }
  return cur;
}

int ovs_snf_state(char *name, int num)
{
  t_snf *cur = find_snf_with_name_num(name, num);
  if (!cur)
    return 0;
  if ((cur->pid) && (cur->llid) && (cur->suid_root_done))
    return 2;
  return 1;
}

/* err: the record as it stands, for the sniff= ack; the history goes to
   syslog once per stall, throttled by snf_hist_dump itself. */
void ovs_snf_state_explain(char *name, int num, char *err)
{
  t_snf *cur = find_snf_with_name_num(name, num);
  if (!cur)
    snprintf(err, MAX_PATH_LEN-1, "snf %s %d: no record", name, num);
  else
    {
    snprintf(err, MAX_PATH_LEN-1, "snf %s %d: pid=%d llid=%d suid=%d "
             "stopping=%d lan=%s mirror_on=%d dumpcap_pid=%d",
             name, num, cur->pid, cur->llid, cur->suid_root_done,
             cur->being_stopped, cur->lan[0] ? cur->lan : "none",
             cur->mirror_on, cur->dumpcap_pid);
    }
}

void ovs_snf_state_stalled(char *name, int num)
{
  t_snf *cur = find_snf_with_name_num(name, num);
  if (cur)
    snf_hist_dump(cur->tap, "cnf sniff= timed out on it");
}

static t_snf *find_snf(char *tap)
{
  t_snf *cur = g_head_snf;
  while(cur)
    {
    if (!strcmp(cur->tap, tap))
      break;
    cur = cur->next;
    }
  return cur;
}

/* The dumpcap's <name_num>_pcap_sock/_log go with the record; the log is
   kept when the death was not asked for. */
static void snf_pcap_files_unlink(char *name_num, int stopped)
{
  char path[MAX_PATH_LEN];
  char *dir = utils_get_snf_pcap_dir();
  snprintf(path, MAX_PATH_LEN-1, "%s/%s_pcap_sock", dir, name_num);
  unlink(path);
  if (stopped)
    {
    snprintf(path, MAX_PATH_LEN-1, "%s/%s_pcap_log", dir, name_num);
    unlink(path);
    }
}

static void free_snf(t_snf *cur)
{
  snf_hist(cur->tap, "record freed (llid=%d pid=%d suid=%d stopping=%d "
           "lan=%s mirror_on=%d)", cur->llid, cur->pid,
           cur->suid_root_done, cur->being_stopped,
           cur->lan[0] ? cur->lan : "none", cur->mirror_on);
  if (!cur->being_stopped)
    {
    if (eth_wants_snf(cur->name, cur->num))
      {
      /* Restarted at once: one line; the history is dumped if the next
         one fails too. */
      KERR("WARNING the capture of %s %d went on its own (%s) while its "
           "interface is still marked sniffable: starting another one%s%s",
           cur->name, cur->num, snf_hist_cause(cur->tap),
           cur->lan[0] ? " on lan " : "", cur->lan[0] ? cur->lan : "");
      defer_snf_restart(cur->name, cur->num, cur->item_type, cur->vhost,
                        cur->lan[0] ? cur->lan : NULL);
      }
    else
      {
      snf_hist_dump(cur->tap, "the capture is gone with its interface");
      KERR("WARNING the capture of %s %d went on its own, its eth gone, "
           "going or no longer sniffed: none started in its place",
           cur->name, cur->num);
      }
    }
  /* Take the mirror out of ovs, if this snf still owns one; a lan that no
     longer exists took it with its bridge. */
  if ((cur->mirror_on) && (cur->lan[0]) && (!lan_get_with_name(cur->lan)))
    {
    snf_hist(cur->tap, "record going, its lan %s no longer exists, the "
             "mirror went with the bridge", cur->lan);
    cur->mirror_on = 0;
    }
  if ((cur->mirror_on) && (cur->lan[0]))
    {
    snf_hist(cur->tap, "record going with a mirror on lan %s, asking ovs "
             "to remove it", cur->lan);
    cur->mirror_on = 0;
    msg_send_del_snf_lan(cur->name, cur->num, cur->vhost, cur->lan);
    }
  if (cur->dumpcap_pid > 0)
    {
    int pid = cur->dumpcap_pid;
    /* Cleared first: killing it calls dumpcap_death, and that must not report
       as a fault the death we asked for. */
    cur->dumpcap_pid = 0;
    snf_hist(cur->tap, "its dumpcap %d is stopped with it", pid);
    PLOG("stopping the dumpcap %d of %s %d with its snf",
         pid, cur->name, cur->num);
    pid_clone_kill_single(pid);
    }
  pcap_relay_snf_gone(cur->name, cur->num);
  snf_pcap_files_unlink(cur->name_num, cur->being_stopped);
  /* by llid, the lan may already be forgotten */
  if (cur->llid)
    mactopo_snf_del(cur->name, cur->num, cur->lan, cur->llid);
  if ((cur->llid) && (msg_exist_channel(cur->llid)))
    {
    rpct_send_kil_req(cur->llid, type_hop_snf);
    llid_trace_free(cur->llid, 0, __FUNCTION__);
    }
  else if (cur->pid > 0)
    {
    /* Killed directly, NOT through pid_clone_kill_single */
    if (snf_pid_is_this_tap(cur->pid, cur->tap))
      {
      KTRC("snf %s: no channel to stop it on, killing pid %d", cur->tap,
           cur->pid);
      snf_kill_pid(cur->pid, cur->tap, "the record is going and there is "
                   "no channel to ask on");
      }
    }
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_snf)
    g_head_snf = cur->next;
  g_nb_snf -= 1;
  free(cur);
}

static char *snf_socket_state(char *path)
{
  static char answer[MAX_PATH_LEN];
  struct stat st;
  memset(answer, 0, MAX_PATH_LEN);
  if (stat(path, &st))
    snprintf(answer, MAX_PATH_LEN-1,
             "the socket file %s does not exist (%s), so the snf process "
             "never got that far", path, strerror(errno));
  else if (!S_ISSOCK(st.st_mode))
    snprintf(answer, MAX_PATH_LEN-1,
             "%s exists but is not a socket (mode 0%o)", path,
             st.st_mode & 07777);
  else
    snprintf(answer, MAX_PATH_LEN-1,
             "the socket %s exists, so the process made it and nothing "
             "accepted on it", path);
  return answer;
}

#define SNF_START_CHECK 3000

typedef struct t_start_check
{
  char tap[MAX_NAME_LEN];
  unsigned int gen;
} t_start_check;

static void timer_start_check(void *data)
{
  t_start_check *sc = (t_start_check *) data;
  char *tap = sc->tap;
  t_snf *cur = find_snf(tap);
  if ((cur) && (cur->gen != sc->gen))
    {
    KTRC("snf %s: the start this checked is over, the tap has been reused",
         tap);
    clownix_free(data, __FUNCTION__);
    return;
    }
  if (!cur)
    {
    /* No record is NOT a fault on its own, nor after a stop asked for. */
    if ((!snf_hist_has(tap, "connected on llid")) &&
        (!snf_hist_has(tap, "stop asked for")))
      {
      snf_hist_dump(tap, "launched, never connected, and its record has "
                    "gone");
      KERR("ERROR snf %s: %d seconds after it was launched it never "
           "connected to the switch and its record has gone. Nothing "
           "captures the endpoint it was for", tap, SNF_START_CHECK/100);
      }
    else
      KTRC("snf %s came up and its record has since gone, which is the "
           "ordinary end of a capture", tap);
    }
  else if (!cur->llid)
    {
    /* Still here, still not connected: the same question the other branch
       asks, for a record that has not been freed. */
    snf_hist_dump(tap, "started and still has not connected");
    KERR("ERROR snf %s: %d seconds after it was launched it still has no "
         "connection (pid=%d, suid=%d). %s", tap, SNF_START_CHECK/100,
         cur->pid, cur->suid_root_done, snf_socket_state(cur->socket));
    }
  clownix_free(data, __FUNCTION__);
}

#define SNF_KIL_CHECK 400

typedef struct t_kil_check
{
  char tap[MAX_NAME_LEN];
  char name_num[MAX_NAME_LEN];
  int pid;
  unsigned int gen;
} t_kil_check;

static void timer_kil_check(void *data)
{
  t_kil_check *kc = (t_kil_check *) data;
  char *name = kc->tap;
  t_snf *cur = find_snf(name);
  int alive = 0;
  if ((cur) && (cur->gen != kc->gen))
    {
    KTRC("snf %s: the kil was obeyed and the tap has been taken by a new "
         "snf since", name);
    clownix_free(data, __FUNCTION__);
    return;
    }
  /* Current owner of the tap: the remembered pid if still this snf, else /proc. */
  if (snf_pid_is_this_tap(kc->pid, name))
    alive = kc->pid;
  else
    alive = snf_find_pid_of_tap(name);
  if (!alive)
    {
    KTRC("snf %s obeyed its kil and is gone", name);
    }
  else if (!cur)
    {
    snf_hist_dump(name, "record gone but the process lives on");
    KERR("ERROR snf %s: its record has gone and pid %d is STILL running "
         "after the kil (the record remembered %d). Killing it outright",
         name, alive, kc->pid);
    snf_kill_pid(alive, name, "it did not obey the kil and its record has "
                 "gone");
    /* Its record has gone, so free_snf will never run for it and nothing else
       stops its dumpcap. */
      {
      int dpid = snf_find_pid_of_dumpcap(kc->name_num);
      if (dpid)
        {
        KERR("WARNING snf %s: its orphaned dumpcap %d goes with it", name,
             dpid);
        snf_kill_pid(dpid, name, "orphaned dumpcap of a killed snf");
        }
      }
    }
  else
    {
    snf_hist_dump(name, "it did not obey the kil");
    KERR("ERROR snf %s: %d seconds after its kil, pid %d is still running "
         "(the record remembered %d). Killing it outright", name,
         SNF_KIL_CHECK/100, alive, cur->pid);
    snf_kill_pid(alive, name, "it did not obey the kil");
    /* Its dumpcap goes with it: free_snf is not reached when the process is
       killed rather than stopped. */
      {
      int dpid = snf_find_pid_of_dumpcap(kc->name_num);
      if ((dpid) && (dpid != cur->dumpcap_pid))
        snf_kill_pid(dpid, name, "dumpcap of an snf that ignored its kil");
      }
    }
  clownix_free(data, __FUNCTION__);
}

typedef struct t_kil_req
{
  char tap[MAX_NAME_LEN];
  unsigned int gen;
  int tries;
} t_kil_req;

/* A stop before the switch connects waits for the connection, within the
   process watchdog (CTRL_LINK_GRACE_MS). */
#define SNF_KIL_WAIT 10
#define SNF_KIL_TRIES ((CTRL_LINK_GRACE_MS / 10 - 100) / SNF_KIL_WAIT)

static void timer_kil_req(void *data)
{
  t_kil_req *kr = (t_kil_req *) data;
  char *name = kr->tap;
  t_snf *cur = find_snf(name);
  if ((cur) && (cur->gen != kr->gen))
    {
    KTRC("snf %s: the record this stop was for has gone, a new snf holds "
         "the tap now", name);
    clownix_free(data, __FUNCTION__);
    return;
    }
  if (!cur)
    {
    /* its process gone first (killed), the record went with it: nominal */
    if (snf_hist_has(name, "is gone"))
      KTRC("snf %s: the stop found its process already gone", name);
    else
      {
      snf_hist_dump(name, "the record went before the stop could be sent");
      KERR("ERROR snf %s: its record went before the stop could be sent, so "
           "nothing told the process to go", name);
      }
    }
  else if ((!cur->llid) && (kr->tries < SNF_KIL_TRIES))
    {
    kr->tries += 1;
    if (kr->tries == 1)
      snf_hist(name, "stop waits for the connection to the process");
    clownix_timeout_add(SNF_KIL_WAIT, timer_kil_req, data, NULL, NULL);
    return;
    }
  else if (!cur->llid)
    {
    int pid = snf_find_pid_of_tap(name);
    snf_hist_dump(name, "stopped while starting, never connected");
    KERR("WARNING snf %s: stopped while starting and not connected after %d "
         "ms, %s. %s", name, kr->tries * SNF_KIL_WAIT * 10,
         pid ? "killing its process" : "no process of it is running",
         snf_socket_state(cur->socket));
    cur->pid = pid;
    free_snf(cur);
    }
  else if (!msg_exist_channel(cur->llid))
    {
    if (snf_pid_is_this_tap(cur->pid, cur->tap))
      {
      snf_hist_dump(name, "no channel for the stop, killing by pid");
      KERR("WARNING snf %s: no channel to send the stop on (llid %d), "
           "killing pid %d instead", name, cur->llid, cur->pid);
      snf_kill_pid(cur->pid, cur->tap, "no channel for the stop");
      }
    else
      {
      snf_hist_dump(name, "its channel went and its pid is not running");
      KERR("WARNING snf %s: its channel (llid %d) went and pid %d does not "
           "run this tap: nothing left to stop", name, cur->llid, cur->pid);
      }
    free_snf(cur);
    }
  else
    {
    snf_hist(name, "kil sent on llid %d", cur->llid);
    rpct_send_kil_req(cur->llid, type_hop_snf);
      {
      t_kil_check *chk;
      chk = (t_kil_check *) clownix_malloc(sizeof(t_kil_check), 34);
      memset(chk, 0, sizeof(t_kil_check));
      strncpy(chk->tap, name, MAX_NAME_LEN-1);
      snprintf(chk->name_num, MAX_NAME_LEN-1, "%s_%d", cur->name, cur->num);
      chk->pid = cur->pid;
      chk->gen = cur->gen;
      clownix_timeout_add(SNF_KIL_CHECK, timer_kil_check, (void *) chk,
                          NULL, NULL);
      }
    }
  clownix_free(data, __FUNCTION__);
}

static void snf_start(char *tap, char *name_num)
{
  static char *argv[8];
  argv[0] = g_bin_snf;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = tap;
  argv[4] = name_num;
  argv[5] = NULL;
  suid_power_spawn(argv);
}

static void dumpcap_death(void *data, int status, char *tap)
{
  t_snf *cur = find_snf(tap);
  int serial = (int) (intptr_t) data;
  if ((cur) && (serial) && (serial != cur->dumpcap_serial))
    {
    KTRC("the dumpcap of a previous snf of %s died (status %d), the tap "
         "belongs to another one now", tap, status);
    return;
    }
  if (!cur)
    {
    if ((status == 9) || (status == 0))
      KTRC("the dumpcap of %s ended with its snf (status %d)", tap, status);
    else
      {
      snf_hist_dump(tap, "its dumpcap died and the snf record has gone");
      KERR("ERROR the dumpcap of %s died (status %d) and its snf is "
           "already gone", tap, status);
      }
    return;
    }
  if (cur->dumpcap_pid == 0)
    {
    /* Already zeroed, so free_snf killed it on purpose and said so. */
    return;
    }
  if (cur->being_stopped)
    {
    cur->dumpcap_pid = 0;
    snf_hist(cur->tap, "its dumpcap ended with the stop we asked for");
    pcap_relay_snf_gone(cur->name, cur->num);
    return;
    }
  snf_hist(cur->tap, "its dumpcap died with status %d", status);
  if ((!cur->suid_root_done) && (cur->dumpcap_retried < 5))
    {
    cur->dumpcap_retried += 1;
    KERR("WARNING the dumpcap of %s %d died (status %d) before its snf was "
         "ready, starting another one; its own account is in %s/%s_%d_pcap_log",
         cur->name, cur->num, status, utils_get_snf_pcap_dir(), cur->name,
         cur->num);
    snf_hist(cur->tap, "dumpcap died before the snf was ready, retrying");
    cur->dumpcap_pid = dumpcap_start(cur, cur->name_num);
    snf_hist(cur->tap, "its second dumpcap started as pid %d",
             cur->dumpcap_pid);
    return;
    }
  /* EOF on the fifo: the snf is gone; free_snf reports and restarts. */
  if ((status == 0) &&
      ((cur->pid == 0) || (!snf_pid_is_this_tap(cur->pid, cur->tap))))
    {
    cur->dumpcap_pid = 0;
    snf_hist(cur->tap, "its dumpcap ended: its snf is gone (pid %d)",
             cur->pid);
    pcap_relay_snf_gone(cur->name, cur->num);
    return;
    }
  snf_hist_dump(cur->tap, "its dumpcap died");
  KERR("WARNING the dumpcap of %s %d ended (status %d) -- its own account "
       "is in %s/%s_%d_pcap_log", cur->name, cur->num, status,
       utils_get_snf_pcap_dir(), cur->name, cur->num);
  cur->dumpcap_pid = 0;
  pcap_relay_snf_gone(cur->name, cur->num);
  if ((cur->dumpcap_retried < 5) &&
      (cur->llid) && (msg_exist_channel(cur->llid)))
    {
    /* The snf is alive and keeps its fifo: another dumpcap on it, after
       the snf has seen the reader go, else the new one reads an EOF. */
    char *tap = (char *) clownix_malloc(MAX_NAME_LEN, 9);
    memset(tap, 0, MAX_NAME_LEN);
    strncpy(tap, cur->tap, MAX_NAME_LEN-1);
    cur->dumpcap_retried += 1;
    snf_hist(cur->tap, "another dumpcap will start (%d so far)",
             cur->dumpcap_retried);
    clownix_timeout_add(30, timer_dumpcap_restart, (void *) tap, NULL, NULL);
    }
  else
    {
    KERR("WARNING nothing captures %s %d now%s", cur->name, cur->num,
         (cur->dumpcap_retried >= 5) ? ": its dumpcap died 5 times" : "");
    snf_hist(cur->tap, "its dumpcap died and nothing captures it now");
    }
}

static void timer_dumpcap_restart(void *data)
{
  char *tap = (char *) data;
  t_snf *cur = find_snf(tap);
  if ((cur) && (cur->dumpcap_pid == 0) && (!cur->being_stopped) &&
      (cur->llid) && (msg_exist_channel(cur->llid)))
    {
    cur->dumpcap_pid = dumpcap_start(cur, cur->name_num);
    snf_hist(cur->tap, "another dumpcap started as pid %d", cur->dumpcap_pid);
    KERR("WARNING another dumpcap started for %s %d, pid %d", cur->name,
         cur->num, cur->dumpcap_pid);
    }
  else
    KTRC("snf %s: no dumpcap restart, the snf changed meanwhile", tap);
  clownix_free(data, __FUNCTION__);
}

static int dumpcap_start(t_snf *cur, char *name_num)
{
  static char *argv[8];
  static char snf_file[MAX_PATH_LEN];
  char *bin = utils_get_dumpcap_bin_dir();
  int pid;
  if (access(bin, X_OK))
    {
    KERR("ERROR %s not found, no capture will be served for %s %d",
         bin, cur->name, cur->num);
    return 0;
    }
  snprintf(snf_file, MAX_PATH_LEN-1, "%s/%s",
           utils_get_snf_pcap_dir(), name_num);
  argv[0] = bin;
  argv[1] = "-i";
  argv[2] = snf_file;
  argv[3] = NULL;
  g_dumpcap_serial += 1;
  cur->dumpcap_serial = g_dumpcap_serial;
  /* Its own death callback, not process_demonized: this one does not
     daemonise, so its exit is news. */
  pid = pid_clone_launch(utils_execv, dumpcap_death, NULL,
                         (void *) argv, (void *) (intptr_t) g_dumpcap_serial,
                         NULL, cur->tap, -1, 1);
  if (pid <= 0)
    {
    KERR("ERROR could not start %s -i %s", bin, snf_file);
    return 0;
    }
  PLOG("dumpcap %d serving %s_pcap_sock", pid, snf_file);
  return pid;
}

static int try_connect(char *socket, char *tap)
{
  int llid = string_client_unix(socket, uml_clownix_switch_error_cb,
                                uml_clownix_switch_rx_cb, tap);
  if (llid)
    {
    llid_trace_alloc(llid, tap, 0, 0, type_llid_trace_endp_ovsdb);
    rpct_send_pid_req(llid, type_hop_snf, tap, 0);
    }
  return llid;
}

static void timer_heartbeat(void *data)
{
  t_snf *next, *cur = g_head_snf;
  int llid;
  char *msg = "cloonsnf_suidroot";
  while(cur)
    {
    next = cur->next;
    cur->watchdog_count += 1;
    if ((cur->llid) && (!msg_exist_channel(cur->llid)))
      {
      if (cur->being_stopped)
        KTRC("snf %s: its channel closed after the stop we asked for",
             cur->tap);
      else if (cur->suid_root_done)
        KTRC("snf %s: its channel (llid %d) went while it was running",
             cur->tap, cur->llid);
      else
        KERR("ERROR snf %s: its channel (llid %d) is gone, the process "
             "died before it was set up (%d tr(ies) so far)",
             cur->tap, cur->llid, cur->count);
      free_snf(cur);
      cur = next;
      continue;
      }
    if (cur->llid == 0)
      {
      llid = try_connect(cur->socket, cur->tap);
      if (llid)
        {
        cur->llid = llid;
        cur->count = 0;
        snf_hist(cur->tap, "connected on llid %d after %d tr(ies)", llid,
                 cur->count);
        }
      else
        {
        cur->count += 1;
        if (cur->count >= 50)
          {
          snf_hist_dump(cur->tap, "never answered on its socket");
          KERR("ERROR snf %s never answered after %d tries, its record is "
               "freed. %s", cur->tap, cur->count,
               snf_socket_state(cur->socket));
          free_snf(cur);
          }
        }
      }
    else if (cur->suid_root_done == 0)
      {
      cur->count += 1;
      if (cur->count >= 50)
        {
        snf_hist_dump(cur->tap, "never became root");
        KERR("ERROR snf %s never became root after 50 tries, its record is "
             "freed. socket=%s llid=%d channel=%s pid=%d watchdog_count=%d",
             cur->tap, cur->socket, cur->llid,
             msg_exist_channel(cur->llid) ? "open" : "GONE",
             cur->pid, cur->watchdog_count);
        free_snf(cur);
        }
      else
        {
        rpct_send_sigdiag_msg(cur->llid, type_hop_snf, msg);
        }
      }
    else if (cur->watchdog_count >= 150)
      {
      KERR("ERROR snf %s of %s went silent, its record is freed (socket %s)",
           cur->tap, cur->name, cur->socket);
      free_snf(cur);
      }
    else
      {
      cur->count += 1;
      if (cur->count == 2)
        {
        rpct_send_pid_req(cur->llid, type_hop_snf, cur->tap, 0);
        cur->count = 0;
        /* Two audits in a row: a killed snf is seen by its dumpcap a few
           ms before its channel closes. */
        if ((cur->pid > 0) && (!cur->being_stopped) &&
            (!snf_pid_is_this_tap(cur->pid, cur->tap)))
          {
          cur->said_gone += 1;
          if (cur->said_gone == 2)
            {
            snf_hist_dump(cur->tap, "the record is here and the process is "
                          "not");
            KERR("ERROR snf %s of %s %d: its record says pid %d on llid %d, "
                 "and no process of that pid owns this tap. Nothing captures "
                 "it and no channel has closed to say so",
                 cur->tap, cur->name, cur->num, cur->pid, cur->llid);
            }
          }
        else if (cur->said_gone == 1)
          cur->said_gone = 0;
        }
      if (cur->closed_count > 0)
        { 
        cur->closed_count -= 1;
        if (cur->closed_count == 0)
          {
          free_snf(cur);
          cur = next;
          continue;
          }
        }
      if (cur->cli_llid)
        {
        cur->cli_llid_count += 1;
        if (cur->cli_llid_count == 50)
          {
          KERR("ERROR snf %s of %s: the client waiting on it got no answer "
               "in 50 tries, the request is dropped (socket %s)",
               cur->tap, cur->name, cur->socket);
          cur->cli_llid = 0;
          cur->cli_tid = 0;
          cur->cli_llid_count = 0;
          }
        }
      if ((cur->pid) && ((cur->lan[0]) || (cur->lan_wanted[0])) &&
          (!cur->mirror_on) && (!cur->being_stopped))
        {
        cur->no_mirror_count += 1;
        if ((cur->no_mirror_count == 100) && (!cur->said_no_mirror))
          {
          cur->said_no_mirror = 1;
          snf_hist_dump(cur->tap, "cabled for five seconds with no mirror");
          KERR("ERROR snf %s of %s %d has been cabled to lan %s for five "
               "seconds with no mirror: it is capturing nothing. "
               "add_ok=%d add_ko=%d del_ko=%d pid=%d wanted=%s",
               cur->tap, cur->name, cur->num, cur->lan,
               cur->nb_add_ok, cur->nb_add_ko, cur->nb_del_ko, cur->pid,
               cur->lan_wanted);
          }
        }
      else
        {
        cur->no_mirror_count = 0;
        cur->said_no_mirror = 0;
        }
      }
    cur = next;
    }
  clownix_timeout_add(50, timer_heartbeat, NULL, NULL, NULL);
}

void ovs_snf_pid_resp(int llid, char *tap, int pid)
{
  char lan[MAX_NAME_LEN];
  t_snf *cur = find_snf(tap);
  if (!cur)
    KERR("%s %d", tap, pid);
  else
    {
    cur->watchdog_count = 0;
    if ((cur->pid == 0) && (cur->suid_root_done == 1))
      {
      cur->pid = pid;
      snf_hist(cur->tap, "reported pid %d, telling the caller it is up",
               pid);
      if (cur->callback_process_up != NULL)
        cur->callback_process_up(cur->name, cur->num, cur->vhost);
      if ((cur->lan_wanted[0]) && (!cur->lan[0]))
        {
        memset(lan, 0, MAX_NAME_LEN);
        strncpy(lan, cur->lan_wanted, MAX_NAME_LEN-1);
        memset(cur->lan_wanted, 0, MAX_NAME_LEN);
        snf_hist(cur->tap, "the deferred add to lan %s goes out now the "
                 "pid is here", lan);
        ovs_snf_send_add_snf_lan(cur->name, cur->num, cur->vhost, lan);
        }
      }
    else if (cur->pid && (cur->pid != pid))
      {
      KERR("ERROR snf %s reports pid %d but another was recorded: two "
             "processes answer for one tap", tap, pid);
      }
    }
}

int ovs_snf_diag_llid(int llid)
{
  t_snf *cur = g_head_snf;
  int result = 0;
  while(cur)
    {
    if (cur->llid == llid)
      {
      result = 1;
      break; 
      } 
    cur = cur->next;
    }
  return result;
}

int ovs_snf_collect_snf(t_eventfull_endp *eventfull)
{
  int result = 0;;
  t_snf *cur = g_head_snf;
  while(cur)
    {
    strncpy(eventfull[result].name, cur->name, MAX_NAME_LEN-1);
    eventfull[result].type = endp_type_eths;
    eventfull[result].num  = cur->num;
    eventfull[result].ms   = cur->ms;
    eventfull[result].ptx  = cur->pkt_tx;
    eventfull[result].prx  = cur->pkt_rx;
    eventfull[result].btx  = cur->byte_tx;
    eventfull[result].brx  = cur->byte_rx;
    cur->pkt_tx  = 0;
    cur->pkt_rx  = 0;
    cur->byte_tx = 0;
    cur->byte_rx = 0;
    result += 1;
    cur = cur->next;
    }
  return result;
}

void ovs_snf_poldiag_resp(int llid, int tid, char *line)
{
  t_snf *cur;
  char tap[MAX_NAME_LEN];
  int ms, ptx, btx, prx, brx;

  if (sscanf(line, "endp_eventfull_tx_rx %63s %d %d %d %d %d",
             tap, &ms, &ptx, &btx, &prx, &brx) != 6)
    KERR("ERROR an eventfull from an snf could not be read: %s", line);
  else
    { 
    cur = find_snf(tap);
    if (!cur)
      KERR("ERROR an eventfull message from an snf could not be read: %s",
           line);
    else
      {
      cur->ms      = ms;
      cur->pkt_tx  += ptx;
      cur->pkt_rx  += prx;
      cur->byte_tx += btx;
      cur->byte_rx += brx;
      }
    }
}

void ovs_snf_sigdiag_resp(int llid, int tid, char *line)
{
  char tap[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int num, status;
  t_snf *cur;
  
  if (sscanf(line,
  "cloonsnf_suidroot_ko %63s", tap) == 1)
    {
    cur = find_snf(tap);
    if (!cur)
      KERR("ERROR snf message for a tap with no record, net %s: %s",
           g_cloonix_net, line);
    else
      KERR("ERROR snf %s could not become root, net %s", tap,
           g_cloonix_net);
    }
  else if (sscanf(line,
  "cloonsnf_suidroot_ok %63s", tap) == 1)
    {
    cur = find_snf(tap);
    if (!cur)
      KERR("ERROR snf message for a tap with no record, net %s: %s",
           g_cloonix_net, line);
    else
      {
      cur->suid_root_done = 1;
      cur->count = 0;
      snf_hist(cur->tap, "became root");
      }
    }
  else if (sscanf(line,
    "cloonsnf_sync_wireshark_resp %63s %d %d", name, &num, &status))
    {
    cur = find_snf_with_name_num(name, num);
    if ((cur == NULL) || (cur->cli_llid == 0))
      KERR("ERROR snf message for a tap with no record, net %s: %s",
           g_cloonix_net, line);
    else
      {
      if (!msg_exist_channel(cur->llid))
        KERR("ERROR snf message for a tap with no record, net %s: %s",
           g_cloonix_net, line);
      else
        send_sync_wireshark_resp(cur->cli_llid,cur->cli_tid,name,num,status);
      cur->cli_llid = 0;
      cur->cli_tid = 0;
      }
    }
  else
    KERR("ERROR snf message for a tap with no record, net %s: %s",
           g_cloonix_net, line);
}

int ovs_snf_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_snf *cur = g_head_snf;
  int i, result = 0;
  /* The snf and its dumpcap: two processes per capture, both listed. */
  while(cur)
    {
    if (cur->pid)
      result++;
    if (cur->dumpcap_pid > 0)
      result++;
    cur = cur->next;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid), 34);
    memset(glob_lst, 0, result*sizeof(t_lst_pid));
    cur = g_head_snf;
    i = 0;
    while(cur)
      {
      if (cur->pid)
        {
        strncpy(glob_lst[i].name, cur->tap, MAX_NAME_LEN-1);
        glob_lst[i].pid = cur->pid;
        i++;
        }
      if (cur->dumpcap_pid > 0)
        {
        snprintf(glob_lst[i].name, MAX_NAME_LEN-1, "dumpcap_%s", cur->tap);
        glob_lst[i].pid = cur->dumpcap_pid;
        i++;
        }
      cur = cur->next;
      }
    if (i != result)
      KOUT("%d %d", i, result);
    }
  *lst_pid = glob_lst;
  return result;
}

void ovs_snf_llid_closed(int llid, int from_clone)
{
  t_snf *cur = g_head_snf;
  if (!from_clone)
    {
    while(cur)
      {
      if (llid && (cur->llid == llid))
        {
        cur->closed_count = 2;
        }
      cur = cur->next;
      }
    }
}

void ovs_snf_stop_all(void)
{
  t_snf *cur = g_head_snf;
  int nb = 0;
  while(cur)
    {
    if (!cur->being_stopped)
      {
      cur->being_stopped = 1;
      snf_hist(cur->tap, "stopped by the net shutdown, kil sent on llid %d",
               cur->llid);
      if (cur->llid)
        rpct_send_kil_req(cur->llid, type_hop_snf);
      nb += 1;
      }
    cur = cur->next;
    }
  if (nb)
    KTRC("shutdown: %d snf process(es) told to stop", nb);
}

#define SNF_DEFER_MAX 300

typedef struct t_snf_defer
{
  char name[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];   /* the lan the dead snf mirrored, to mirror again */
  int num;
  int item_type;
  int tries;
  t_fct_cb cb;
} t_snf_defer;

static void timer_snf_defer(void *data)
{
  t_snf_defer *df = (t_snf_defer *) data;
  char tap[MAX_NAME_LEN];
  int old_pid;
  utils_get_snf_tap_name(tap, df->vhost);
  if (!find_snf(tap))
    {
    char use[MAX_PATH_LEN];
    if (!cfg_name_is_in_use(0, df->name, use))
      {
      KTRC("%s %d: its tap %s is free now, but the endpoint is gone -- no "
           "snf is started", df->name, df->num, tap);
      clownix_free(df, __FUNCTION__);
      return;
      }
    old_pid = snf_find_pid_of_tap(tap);
    if (old_pid)
      {
      KERR("WARNING %s %d: the record for tap %s has gone but its process "
           "%d is still running. Killing it before starting the new one, "
           "or it would be orphaned", df->name, df->num, tap, old_pid);
      snf_hist(tap, "an older process %d still owned this tap, killed",
               old_pid);
      snf_kill_pid(old_pid, tap, "an older snf still owned this tap");
        {
        char name_num[MAX_NAME_LEN];
        int dpid;
        memset(name_num, 0, MAX_NAME_LEN);
        snprintf(name_num, MAX_NAME_LEN-1, "%s_%d", df->name, df->num);
        dpid = snf_find_pid_of_dumpcap(name_num);
        if (dpid)
          {
          KERR("WARNING %s %d: its orphaned dumpcap %d goes with it",
               df->name, df->num, dpid);
          snf_hist(tap, "its orphaned dumpcap %d killed too", dpid);
          snf_kill_pid(dpid, tap, "dumpcap of the older snf of this tap");
          }
        }
      }
    KTRC("%s %d: the tap %s is free now, starting its snf", df->name,
         df->num, tap);
    /* Started: the record exists with no pid, the lan is filed as wanted
       and the mirror is asked for when the pid arrives. */
    if ((!ovs_dyn_snf_start_process(df->name, df->num, df->item_type,
                                    df->vhost, df->cb)) && (df->lan[0]))
      ovs_snf_send_add_snf_lan(df->name, df->num, df->vhost, df->lan);
    clownix_free(df, __FUNCTION__);
    }
  else
    {
    df->tries += 1;
    if (df->tries > SNF_DEFER_MAX)
      {
      KERR("ERROR %s %d: the tap %s is still held after %d tries, the snf "
           "of this endpoint is abandoned and nothing will capture it",
           df->name, df->num, tap, df->tries);
      clownix_free(df, __FUNCTION__);
      }
    else
      clownix_timeout_add(20, timer_snf_defer, (void *) df, NULL, NULL);
    }
}

static void defer_snf_start(char *name, int num, int item_type, char *vhost,
                            t_fct_cb cb)
{
  t_snf_defer *df;
  df = (t_snf_defer *) clownix_malloc(sizeof(t_snf_defer), 34);
  memset(df, 0, sizeof(t_snf_defer));
  strncpy(df->name, name, MAX_NAME_LEN-1);
  strncpy(df->vhost, vhost, MAX_NAME_LEN-1);
  df->num = num;
  df->item_type = item_type;
  df->cb = cb;
  clownix_timeout_add(20, timer_snf_defer, (void *) df, NULL, NULL);
}

/* The same, for an snf that went on its own: its lan comes back with it. */
static void defer_snf_restart(char *name, int num, int item_type,
                              char *vhost, char *lan)
{
  t_snf_defer *df;
  df = (t_snf_defer *) clownix_malloc(sizeof(t_snf_defer), 34);
  memset(df, 0, sizeof(t_snf_defer));
  strncpy(df->name, name, MAX_NAME_LEN-1);
  strncpy(df->vhost, vhost, MAX_NAME_LEN-1);
  if (lan)
    strncpy(df->lan, lan, MAX_NAME_LEN-1);
  df->num = num;
  df->item_type = item_type;
  clownix_timeout_add(20, timer_snf_defer, (void *) df, NULL, NULL);
}

#define SNF_KIL_DELAY 20

int ovs_snf_is_running(char *name, int num)
{
  t_snf *cur = find_snf_with_name_num(name, num);
  if (!cur)
    return 0;
  if ((cur->pid > 0) && (!snf_pid_is_this_tap(cur->pid, cur->tap)))
    {
    KTRC("%s %d has an snf record but its process %d is gone", name, num,
         cur->pid);
    return 0;
    }
  return 1;
}

void ovs_dyn_snf_stop_process(char *tap)
{
  t_snf *cur = find_snf(tap);
  t_kil_req *kr;
  int pid;
  if (!cur)
    {
    /* the eth of a guest that never ran, a qemu that did not start: no
       snf was ever started, nothing to stop */
    pid = snf_find_pid_of_tap(tap);
    if (!pid)
      snf_hist(tap, "stop asked, no record and no process: nothing to stop");
    else
      {
      snf_hist_dump(tap, "stop asked, no record, a process running");
      KERR("WARNING stop asked for snf %s: no record of it and its process "
           "%d runs, killing it", tap, pid);
      snf_kill_pid(pid, tap, "no record of it");
      }
    }
  else
    {
    /* Marked now, not when the kill lands */
    /* One kil per snf, however many times the stop is asked for. */
    if (cur->being_stopped)
      {
      snf_hist(tap, "stop asked again, a kil is already on its way");
      return;
      }
    cur->being_stopped = 1;
    snf_hist(tap, "stop asked for, kil armed");
    kr = (t_kil_req *) clownix_malloc(sizeof(t_kil_req), 34);
    memset(kr, 0, sizeof(t_kil_req));
    strncpy(kr->tap, tap, MAX_NAME_LEN-1);
    kr->gen = cur->gen;
    clownix_timeout_add(SNF_KIL_DELAY, timer_kil_req, (void *) kr, NULL,
                        NULL);
    }
}

int ovs_dyn_snf_start_process(char *name, int num, int item_type,
                              char *vhost, t_fct_cb cb)
{
  t_snf *cur;
  char *root = g_root_path;
  char name_num[MAX_NAME_LEN];
  char tap[MAX_NAME_LEN];
  char *tp = tap;

  /* A capture started for a machine being deleted outlives it. */
  if (((item_type == item_type_ceth) || (item_type == item_type_keth)) &&
      (!eth_wants_snf(name, num)))
    {
    KTRC("snf start for %s %d: its machine is going or its eth is no longer "
         "sniffed, none started", name, num);
    return -1;
    }
  memset(name_num, 0, MAX_NAME_LEN);
  snprintf(name_num, MAX_NAME_LEN-1, "%s_%d", name, num);
  memset(tap, 0, MAX_NAME_LEN);
  utils_get_snf_tap_name(tap, vhost);
  cur = find_snf(tap);
  if ((cur) && (!cur->being_stopped) && (!strcmp(cur->name, name)) &&
      (cur->num == num) && ((cur->pid == 0) ||
      (snf_pid_is_this_tap(cur->pid, cur->tap))))
    {
    /* A restart of it is already under way or up: kept, not replaced. */
    snf_hist(tap, "start asked, the snf already there is kept");
    if (!cur->callback_process_up)
      cur->callback_process_up = cb;
    return 0;
    }
  if (cur)
    {
    KERR("WARNING snf start for %s %d vhost %s: an snf of that tap is still "
         "running, stopping it and starting the new one when it has gone",
         name, num, vhost);
    ovs_dyn_snf_stop_process(tap);
    defer_snf_start(name, num, item_type, vhost, cb);
    return 0;
    }
  if ((item_type != item_type_keth) && (item_type != item_type_ceth) &&
      (item_type != item_type_tap) && (item_type != item_type_c2c) &&
      (item_type != item_type_nat) && (item_type != item_type_a2b) &&
      (item_type != item_type_phy))
    {
    KERR("ERROR snf start for %s %d vhost %s: %d is not an item type that "
         "can be sniffed", name, num, vhost, item_type);
    return -1;
    }
  cur = (t_snf *) malloc(sizeof(t_snf));
  memset(cur, 0, sizeof(t_snf));
  g_snf_gen += 1;
  cur->gen = g_snf_gen;
  strncpy(cur->tap, tap, MAX_NAME_LEN-1);
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->num = num;
  cur->item_type = item_type;
  strncpy(cur->vhost, vhost, MAX_NAME_LEN-1);
  if (snprintf(cur->socket, MAX_PATH_LEN, "%s/%s/c%s", root, SNF_DIR, tp)
      >= MAX_PATH_LEN)
    KOUT("ERROR the snf socket path does not fit: %s/%s/c%s",
         root, SNF_DIR, tp);
  cur->callback_process_up = cb;
  if (g_head_snf)
    g_head_snf->prev = cur;
  cur->next = g_head_snf;
  g_head_snf = cur;
  snf_hist_forget(tap);
  snf_hist(tap, "start: process launched for %s %d on vhost %s",
           name, num, vhost);
  snf_start(tap, name_num);
    {
    t_start_check *chk;
    chk = (t_start_check *) clownix_malloc(sizeof(t_start_check), 34);
    memset(chk, 0, sizeof(t_start_check));
    strncpy(chk->tap, tap, MAX_NAME_LEN-1);
    chk->gen = cur->gen;
    clownix_timeout_add(SNF_START_CHECK, timer_start_check, (void *) chk,
                        NULL, NULL);
    }
  strncpy(cur->name_num, name_num, MAX_NAME_LEN-1);
  cur->dumpcap_pid = dumpcap_start(cur, name_num);
  snf_hist(tap, "its dumpcap started as pid %d", cur->dumpcap_pid);
  g_nb_snf += 1;
  return 0;
}

void ovs_snf_resp_msg_vhost_up(int is_ko, char *vhost, char *name, int num)
{
  t_vm   *vm;
  int nb_eth, item_type;
  t_eth_table *eth_tab;
  int cnt_exists = cnt_eth_table(name, &nb_eth, &eth_tab);
  int found = 0;

  if (cnt_exists)
    {
    if ((num >=0 ) && (num < nb_eth))
      found = 1;
    else
      KERR("ERROR vhost_up ack for %s eth %d, it has %d eth", name, num, nb_eth);
    item_type = item_type_ceth;
    }
  else
    {
    vm = cfg_get_vm(name);
    if (vm)
      { 
      found = 1;
      nb_eth = vm->kvm.nb_tot_eth;
      eth_tab = vm->kvm.eth_table; 
      if ((num >=0 ) && (num < nb_eth))
        found = 1;
      item_type = item_type_keth;
      }
    }

  if (found == 0)
    KTRC("vhost_up response for %s %d %s arrived after the object was "
         "deleted", name, num, vhost);
  else if (is_ko)
    KERR("ERROR vhost_up REFUSED for %s %d vhost %s", name, num, vhost);
  else 
    {
    if (item_type == item_type_ceth)
      cnt_vhost_up(name, num, 1);
    else
      kvm_vhost_up(name, num, 1);
    if (eth_tab[num].endp_type == endp_type_eths)
      ovs_dyn_snf_start_process(name, num, item_type, vhost, NULL);
    }
}

void ovs_snf_resp_msg_add_lan(int is_ko, char *vhost, char *name, int num, char *lan)
{
  t_snf *cur = find_snf_with_name_num(name, num);
  if (!cur)
    KTRC("snf add response for %s %d on lan %s: no snf record", name, num,
         lan);
  else if (is_ko)
    {
    cur->nb_add_ko += 1;
    snf_hist(cur->tap, "ovs REFUSED the mirror add on lan %s (ko=%d)",
             lan, cur->nb_add_ko);
    if (cur->being_stopped)
      KTRC("snf %s %d: ovs refused the add to lan %s, and this snf is "
           "being stopped anyway", name, num, lan);
    else
      {
      snf_hist_dump(cur->tap, "the mirror add was refused by ovs");
      KERR("ERROR snf add REFUSED BY OVS for %s %d vhost %s on lan %s. This "
           "snf now exists and captures nothing. add_ok=%d add_ko=%d "
           "del_ko=%d, dumpcap_pid=%d, being_stopped=%d",
           name, num, vhost, lan, cur->nb_add_ok, cur->nb_add_ko,
           cur->nb_del_ko, cur->dumpcap_pid, cur->being_stopped);
      }
    }
  else if (!cur->llid)
    {
    snf_hist_dump(cur->tap, "ovs cabled the mirror but the snf has no llid");
    KERR("ERROR snf add for %s %d on lan %s: ovs accepted it but the snf "
         "has no llid, so the mac topology is not told", name, num, lan);
    }
  else
    {
    cur->nb_add_ok += 1;
    snf_hist(cur->tap, "ovs made the mirror on lan %s (ok=%d)", lan,
             cur->nb_add_ok);
    if (cur->mirror_on)
      {
      snf_hist_dump(cur->tap, "the mirror add succeeded while one was on");
      KERR("WARNING snf add for %s %d on lan %s SUCCEEDED while its mirror "
           "was already on -- a second add is what makes ovs refuse the "
           "port next time", name, num, lan);
      }
    cur->mirror_on = 1;
    mactopo_snf_add(name, num, lan, cur->llid);
    }
}

void ovs_snf_resp_msg_del_lan(int is_ko, char *vhost, char *name, int num, char *lan)
{
  t_snf *cur = find_snf_with_name_num(name, num);
  if (!cur)
    KTRC("snf del response for %s %d on lan %s: no snf record", name, num,
         lan);
  else if (!cur->llid)
    {
    /* no llid, never a mirror: the del of the snf before this one, whose
       mac topology went with its record (free_snf) */
    if (is_ko)
      KERR("ERROR snf del REFUSED BY OVS for %s %d vhost %s on lan %s, the "
           "mirror of the previous snf of that eth", name, num, vhost, lan);
    else
      KTRC("snf del for %s %d on lan %s answers the previous snf of that "
           "eth", name, num, lan);
    }
  else
    {
    if (is_ko)
      {
      cur->nb_del_ko += 1;
      snf_hist_dump(cur->tap, "the mirror del was refused by ovs");
      KERR("ERROR snf del REFUSED BY OVS for %s %d vhost %s on lan %s. "
           "add_ok=%d add_ko=%d del_ko=%d. A del that finds no mirror row "
           "means the add never created one -- check whether add_ok is 0 "
           "here", name, num, vhost, lan, cur->nb_add_ok, cur->nb_add_ko,
           cur->nb_del_ko);
      }
    mactopo_snf_del(name, num, lan, cur->llid);
    }
}

int ovs_snf_send_add_snf_lan(char *name, int num, char *vhost, char *lan)
{
  t_snf *cur = find_snf_with_name_num(name, num);
  if (!cur)
    KTRC("snf add request for %s %d vhost %s lan %s: no snf record",
         name, num, vhost, lan);
  else
    {
    if ((cur->lan[0]) && (!strcmp(cur->lan, lan)))
      {
      KTRC("snf %s %d is already being added to lan %s, second add skipped",
           name, num, lan);
      return 0;
      }
    if (cur->being_stopped)
      {
      memset(cur->lan_wanted, 0, MAX_NAME_LEN);
      KTRC("snf %s %d is being stopped, the add to lan %s is skipped",
           name, num, lan);
      return 0;
      }
    /* Not before the process is up. */
    if (!cur->pid)
      {
      memset(cur->lan_wanted, 0, MAX_NAME_LEN);
      strncpy(cur->lan_wanted, lan, MAX_NAME_LEN-1);
      snf_hist(cur->tap, "add to lan %s deferred, the process is not up "
               "yet, it will be re-sent when the pid arrives", lan);
      return 0;
      }
    if (cur->lan[0])
      {
      if (!lan_get_with_name(cur->lan))
        {
        KTRC("snf %s %d still named lan %s, which no longer exists: its "
             "mirror went with the bridge, forgetting it", name, num,
             cur->lan);
        cur->mirror_on = 0;
        snf_hist(cur->tap, "its lan %s no longer exists, the mirror went "
                 "with the bridge", cur->lan);
        }
      else if (cur->mirror_on)
        {
        KERR("WARNING snf %s %d moves from lan %s to lan %s with no del in "
             "between: removing the first mirror before making the second",
             name, num, cur->lan, lan);
        cur->mirror_on = 0;
        snf_hist(cur->tap, "moving from lan %s to lan %s, removing the "
                 "first mirror", cur->lan, lan);
        msg_send_del_snf_lan(name, num, vhost, cur->lan);
        }
      else
        KTRC("snf %s %d moves from lan %s to lan %s, no mirror to remove",
             name, num, cur->lan, lan);
      }
    memset(cur->lan, 0, MAX_NAME_LEN);
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    snf_hist(cur->tap, "mirror add sent for lan %s", lan);
    }
  return(msg_send_add_snf_lan(name, num, vhost, lan));
}

int ovs_snf_send_del_snf_lan(char *name, int num, char *vhost, char *lan)
{
  t_snf *cur = find_snf_with_name_num(name, num);
  int result = 0, sent = 0;
  if (!cur)
    KTRC("snf del request for %s %d vhost %s lan %s: no snf record",
         name, num, vhost, lan);
  else
    {
    memset(cur->lan_wanted, 0, MAX_NAME_LEN);
    /* Is the caller uncabling the lan this mirror is actually ON? */
    if ((cur->lan[0]) && (strcmp(cur->lan, lan)))
      KERR("WARNING snf del for %s %d vhost %s asks to uncable lan %s but "
           "this snf's mirror was made on lan %s (the drv removes the "
           "mirror from whichever bridge holds it)",
           name, num, vhost, lan, cur->lan);
    memset(cur->lan, 0, MAX_NAME_LEN);
    /* Only when ovs actually made the mirror. */
    if (!cur->mirror_on)
      {
      KTRC("snf del for %s %d on lan %s skipped, ovs has no mirror for it",
           name, num, lan);
      }
    else if (lan_get_with_name(lan))
      {
      cur->mirror_on = 0;
      snf_hist(cur->tap, "mirror del sent for lan %s", lan);
      result = msg_send_del_snf_lan(name, num, vhost, lan);
      sent = 1;
      }
    /* no del sent, no response to clear the mac topology: cleared here */
    if ((!sent) && (cur->llid))
      mactopo_snf_del(name, num, lan, cur->llid);
    }
  return (result);
}

/* A sync asked while its snf or air capture starts waits for it: 50 tries
   of 200 ms. */
#define SYNC_WAIT_TRIES 50
#define SYNC_WAIT_MAX 64
typedef struct t_sync_wait
{
  char name[MAX_NAME_LEN];
  int llid;
  int tid;
  int num;
  int cmd;
  int tries;
} t_sync_wait;
static int g_nb_sync_wait;

static void sync_send(t_snf *cur, int llid, int tid, char *name, int num,
                      int cmd)
{
  char msg[MAX_PATH_LEN];
  if (cur->cli_llid)
    {
    KERR("WARNING wireshark sync for %s %d refused: a client request is "
           "already outstanding on this snf", name, num);
    send_sync_wireshark_resp(llid, tid, name, num, 1);
    }
  else
    {
    cur->cli_llid = llid;
    cur->cli_tid = tid;
    snprintf(msg, MAX_PATH_LEN-1,
    "cloonsnf_sync_wireshark_req %s %d %d", name, num, cmd);
    rpct_send_sigdiag_msg(cur->llid, type_hop_snf, msg);
    }
}

static void timer_sync_wait(void *data)
{
  t_sync_wait *w = (t_sync_wait *) data;
  t_snf *cur = find_snf_with_name_num(w->name, w->num);
  if (!msg_exist_channel(w->llid))
    KERR("WARNING wireshark sync for %s %d: its client left while the snf "
           "started", w->name, w->num);
  else if ((cur) && (cur->pid == 0) && (w->tries < SYNC_WAIT_TRIES))
    {
    w->tries += 1;
    clownix_timeout_add(20, timer_sync_wait, (void *) w, NULL, NULL);
    return;
    }
  else if (!cur)
    {
    KERR("ERROR wireshark sync for %s %d refused: its snf went while it "
           "started", w->name, w->num);
    send_sync_wireshark_resp(w->llid, w->tid, w->name, w->num, 1);
    }
  else if (cur->pid == 0)
    {
    KERR("ERROR wireshark sync for %s %d refused: its snf not ready after "
           "10 s", w->name, w->num);
    send_sync_wireshark_resp(w->llid, w->tid, w->name, w->num, 1);
    }
  else
    sync_send(cur, w->llid, w->tid, w->name, w->num, w->cmd);
  g_nb_sync_wait -= 1;
  clownix_free(w, __FUNCTION__);
}

/* An air captured whose dumpcap starts (next beat, once suid_power made its
   fifo): the sync waits for it, as for a snf. */
static void timer_air_wait(void *data)
{
  t_sync_wait *w = (t_sync_wait *) data;
  if (!msg_exist_channel(w->llid))
    KERR("WARNING wireshark on the air %s: its client left while its "
           "capture started", w->name);
  else if (wifi_air_is_up(w->name))
    send_sync_wireshark_resp(w->llid, w->tid, w->name, w->num, 2);
  else if (!wifi_air_snf_on(w->name))
    {
    KERR("WARNING wireshark on the air %s refused: its capture stopped "
           "while it started", w->name);
    send_sync_wireshark_resp(w->llid, w->tid, w->name, w->num, 1);
    }
  else if (w->tries < SYNC_WAIT_TRIES)
    {
    w->tries += 1;
    clownix_timeout_add(20, timer_air_wait, (void *) w, NULL, NULL);
    return;
    }
  else
    {
    KERR("ERROR wireshark on the air %s refused: its capture not up after "
           "10 s", w->name);
    send_sync_wireshark_resp(w->llid, w->tid, w->name, w->num, 1);
    }
  g_nb_sync_wait -= 1;
  clownix_free(w, __FUNCTION__);
}

/* A sync that waits, SYNC_WAIT_MAX of them at most. */
static void sync_wait_arm(int llid, int tid, char *name, int num, int cmd,
                          t_fct_timeout cb)
{
  t_sync_wait *w;
  if (g_nb_sync_wait >= SYNC_WAIT_MAX)
    {
    KERR("WARNING wireshark sync for %s %d refused: %d syncs already wait",
           name, num, g_nb_sync_wait);
    send_sync_wireshark_resp(llid, tid, name, num, 1);
    return;
    }
  w = (t_sync_wait *) clownix_malloc(sizeof(t_sync_wait), 34);
  memset(w, 0, sizeof(t_sync_wait));
  strncpy(w->name, name, MAX_NAME_LEN-1);
  w->llid = llid;
  w->tid = tid;
  w->num = num;
  w->cmd = cmd;
  g_nb_sync_wait += 1;
  clownix_timeout_add(20, cb, (void *) w, NULL, NULL);
}

void ovs_snf_sync_wireshark_req(int llid, int tid, char *name, int num, int cmd)
{
  t_snf *cur = find_snf_with_name_num(name, num);
  if (wifi_air_in_use(name))
    {
    /* an air has no snf: its capture is up, starting, or off */
    if (wifi_air_is_up(name))
      send_sync_wireshark_resp(llid, tid, name, num, 2);
    else if (wifi_air_snf_on(name))
      sync_wait_arm(llid, tid, name, num, cmd, timer_air_wait);
    else
      {
      KERR("WARNING wireshark on the air %s refused: not captured, cnf %s "
           "sniff=on", name, name);
      send_sync_wireshark_resp(llid, tid, name, num, 1);
      }
    }
  else if (!cur)
    {
    KERR("ERROR wireshark sync for %s %d refused: no snf of that name and "
           "eth", name, num);
    send_sync_wireshark_resp(llid, tid, name, num, 1);
    }
  else if (cur->pid != 0)
    sync_send(cur, llid, tid, name, num, cmd);
  else
    sync_wait_arm(llid, tid, name, num, cmd, timer_sync_wait);
}

void ovs_snf_init(void)
{
  char *root = cfg_get_root_work();
  char *net = cfg_get_cloonix_name();
  char *bin_snf = utils_get_ovs_snf_bin_dir();
  g_nb_snf = 0;
  g_head_snf = NULL;
  memset(g_cloonix_net, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_bin_snf, 0, MAX_PATH_LEN);
  strncpy(g_cloonix_net, net, MAX_NAME_LEN-1);
  strncpy(g_root_path, root, MAX_PATH_LEN-1);
  strncpy(g_bin_snf, bin_snf, MAX_PATH_LEN-1);
  clownix_timeout_add(300, timer_heartbeat, NULL, NULL, NULL);
}

