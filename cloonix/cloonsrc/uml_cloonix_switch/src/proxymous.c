/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "uml_clownix_switch.h"
#include "llid_trace.h"
#include "proxycrun.h"

static int g_pid;
static int g_watchdog_count;
static int g_llid;
static int g_count;
static int g_started;

static char g_own_passwd[MSG_DIGEST_LEN];
static char g_cloonix_net[MAX_NAME_LEN];
static char g_proxymous_bin[MAX_PATH_LEN];
static char g_proxymous_dir[MAX_PATH_LEN];
static char g_socket[MAX_PATH_LEN];
int get_glob_req_self_destruction(void);
int get_running_in_crun(void);

static void process_demonized(void *unused_data, int status, char *name)
{
}

static void proxymous_start(void)
{
  char *argv[4];
  argv[0] = g_proxymous_bin;
  argv[1] = g_proxymous_dir;
  argv[2] = g_cloonix_net;
  argv[3] = NULL;
  pid_clone_launch(utils_execv, process_demonized, NULL,
                   (void *) argv, NULL, NULL, "proxymous", -1, 1);
}

static int try_connect(char *socket, char *name)
{
  int llid = string_client_unix(socket, uml_clownix_switch_error_cb,
                                uml_clownix_switch_rx_cb, name);
  if (llid)
    {
    llid_trace_alloc(llid, name, 0, 0, type_llid_trace_proxymous);
    rpct_send_pid_req(llid, type_hop_proxymous, name, 0);
    }
  return llid;
}

/* Counts only until the ctrl connection exists; once connected, the
   verdict on a silent proxymous is proxycrun's, on the wall clock. */
static void timer_heartbeat(void *data)
{
  int llid;
  if (get_glob_req_self_destruction())
    return;
  if (g_started == 0)
    {
    if (!get_running_in_crun())
      proxymous_start();
    g_started = 1;
    g_watchdog_count = 0;
    }
  else if (g_llid == 0)
    {
    g_watchdog_count += 1;
    llid = try_connect(g_socket, "proxymus");
    if (llid)
      {
      proxycrun_init(g_own_passwd);
      g_llid = llid;
      g_count = 0;
      }
    else if (g_watchdog_count >= 100)
      {
      KERR("ERROR IMPOSSIBLE CTRL CONNECTION TO PROXYMOUS %s, stopping the "
           "net", g_socket);
      recv_kill_uml_clownix(0, 0);
      return;
      }
    }
  else
    {
    g_count += 1;
    if (g_count == 5)
      {
      rpct_send_pid_req(g_llid, type_hop_proxymous, "proxymus", 0);
      g_count = 0;
      }
    }
  clownix_timeout_add(20, timer_heartbeat, NULL, NULL, NULL);
}

void proxymous_pid_resp(int llid, char *name, int pid)
{
  if (g_pid == 0)
    g_pid = pid;
  else if (g_pid && (g_pid != pid))
    KERR("ERROR %d %d", g_pid, pid);
}

int proxymous_get_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  int result = 0;
  if (g_pid)
    {
    result = 1;
    glob_lst = (t_lst_pid *)clownix_malloc(sizeof(t_lst_pid),5);
    memset(glob_lst, 0, sizeof(t_lst_pid));
    strncpy(glob_lst[0].name, "proxymous", MAX_NAME_LEN-1);
    glob_lst[0].pid = g_pid;
    }
  *lst_pid = glob_lst;
  return result;
}

void proxymous_llid_closed(int llid, int from_clone)
{
  if ((!from_clone) && (llid == g_llid))
    {
    if (!get_glob_req_self_destruction())
      {
      KERR("ERROR LOST CTRL CONNECTION TO PROXYMOUS");
      }
    if (!get_running_in_crun())
      { 
      if (g_pid)
        kill(g_pid, SIGKILL);
      g_started = 0;
      g_pid = 0;
      g_watchdog_count = 0;
      g_llid = 0;
      g_count = 0;
      }
    }
}

static void timer_kill(void *data)
{
  if (g_pid)
    kill(g_pid, SIGKILL);
  g_started = 0;
  g_pid = 0;
  g_watchdog_count = 0;
  g_llid = 0;
  g_count = 0;
}

void proxymous_kill(void)
{
  if ((g_llid) && (msg_exist_channel(g_llid)))
    rpct_send_kil_req(g_llid, 0);
  else
    KERR("ERROR SEND KILL PROXYMOUS");
  clownix_timeout_add(5, timer_kill, NULL, NULL, NULL);
}

void proxymous_init(char *passwd)
{
  char *net = cfg_get_cloonix_name();
  char *bin_proxymous = utils_get_proxymous_bin();
  g_started = 0;
  g_pid = 0;
  g_watchdog_count = 0;
  g_llid = 0;
  g_count = 0;
  memset(g_own_passwd, 0, MSG_DIGEST_LEN);
  memset(g_cloonix_net, 0, MAX_NAME_LEN);
  memset(g_proxymous_bin, 0, MAX_PATH_LEN);
  memset(g_socket, 0, MAX_PATH_LEN);
  memset(g_proxymous_dir, 0, MAX_PATH_LEN);
  strncpy(g_own_passwd, passwd, MSG_DIGEST_LEN-1);
  strncpy(g_cloonix_net, net, MAX_NAME_LEN-1);
  strncpy(g_proxymous_bin, bin_proxymous, MAX_PATH_LEN-1);
  snprintf(g_socket, MAX_PATH_LEN-1, "%s_%s/%s",PROXYSHARE_IN,net,PROXYMOUS);
  snprintf(g_proxymous_dir, MAX_PATH_LEN-1, "%s_%s", PROXYSHARE_IN, net);
  clownix_timeout_add(10, timer_heartbeat, NULL, NULL, NULL);
}

