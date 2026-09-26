/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* gui_server_mngt.c — launch and monitor cloonix-gui-server. Mirrors
   suid_power.c in uml_cloonix_switch */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "pid_clone.h"
#include "automates.h"
#include "utils_cmd_line_maker.h"
#include "hop_event.h"
#include "llid_trace.h"
#include "uml_clownix_switch.h"
#include "gui_server_mngt.h"

static void timer_watchdog_gui(void *data);

int get_killing_cloonix(void);

/* Beats are 10 ms. 8 s of silence: KERR, 10 s: KOUT. */
#define GUI_WARN_CYCLES  80
#define GUI_KILL_CYCLES 100
#define GUI_MONITOR_BEATS 10
#define GUI_MONITOR_MS  (GUI_MONITOR_BEATS * 10)

static char g_bin_gui[MAX_PATH_LEN];
static char g_snf_dir[MAX_PATH_LEN];
static char g_web_port[MAX_NAME_LEN];
static char g_ctrl_sock[MAX_PATH_LEN];  /* Unix socket path for RPC */
static char g_doors_path[MAX_PATH_LEN]; /* cloonix main doors socket */
static char g_passwd[MSG_DIGEST_LEN];   /* cloonix network password */
static int  g_web_port_num;
static int  g_gui_server_pid;
static int  g_initialised;
static int  g_stopping;
static int  g_llid;                     /* RPC channel to gui-server */
static int  g_nb_pid_resp;
static int  g_nb_pid_resp_warning;

static void gui_server_start(void);
static void timer_restart(void *data);

static void process_died(void *data, int status, char *nm)
{
  if (g_stopping)
    return;

  g_gui_server_pid = 0;

  if (get_glob_req_self_destruction() || get_killing_cloonix())
    return;

  KERR("WARNING cloonix-gui-server died unexpectedly port=%s snf=%s",
       g_web_port, g_snf_dir);
  clownix_timeout_add(1000, timer_restart, NULL, NULL, NULL);
}

static void timer_connect(void *data)
{
  int llid;
  char *ctrl = (char *) data;
  static int count = 0;

  if (get_glob_req_self_destruction() || g_stopping)
    return;

  llid = string_client_unix(ctrl,
                             uml_clownix_switch_error_cb,
                             uml_clownix_switch_rx_cb,
                             "gui_server");
  if (llid)
    {
    count = 0;
    g_llid = llid;
    g_nb_pid_resp = 0;
    g_nb_pid_resp_warning = 0;
    llid_trace_alloc(llid, "gui_server", 0, 0, type_llid_trace_endp_suid);
    rpct_send_pid_req(llid, type_hop_gui_server, "gui_server", 0);
    clownix_timeout_add(GUI_MONITOR_BEATS, timer_watchdog_gui, NULL, NULL, NULL);
    KTRC("gui_server RPC connected llid=%d port=%s", llid, g_web_port);
    }
  else
    {
    count++;
    if (count < 20)
      clownix_timeout_add(50, timer_connect, data, NULL, NULL);
    else
      KOUT("FATAL cloonix-gui-server RPC socket not reachable after 10s: %s",
           ctrl);
    }
}

static void timer_watchdog_gui(void *data)
{
  static int old_nb_pid_resp;

  if (g_stopping || !g_initialised)
    return;
  if (get_glob_req_self_destruction() || get_killing_cloonix())
    return;

  if (old_nb_pid_resp == g_nb_pid_resp)
    g_nb_pid_resp_warning++;
  else
    g_nb_pid_resp_warning = 0;

  if (g_nb_pid_resp_warning >= GUI_KILL_CYCLES)
    {
    KOUT("FATAL cloonix-gui-server not responding for %dms, port=%s",
         GUI_KILL_CYCLES * GUI_MONITOR_MS, g_web_port);
    }
  else if (g_nb_pid_resp_warning == GUI_WARN_CYCLES)
    {
    KERR("WARNING cloonix-gui-server not responding for %dms, port=%s",
         GUI_WARN_CYCLES * GUI_MONITOR_MS, g_web_port);
    }

  if (g_llid && msg_exist_channel(g_llid))
    rpct_send_pid_req(g_llid, type_hop_gui_server, "gui_server", 0);

  old_nb_pid_resp = g_nb_pid_resp;
  clownix_timeout_add(GUI_MONITOR_BEATS, timer_watchdog_gui, NULL, NULL, NULL);
}

void gui_server_pid_resp(int llid, int tid, char *name, int num,
                         int toppid, int pid)
{
  if (llid != g_llid)
    KERR("ERROR gui_server_pid_resp bad llid %d %d", llid, g_llid);
  if (tid != type_hop_gui_server)
    KERR("ERROR gui_server_pid_resp bad tid %d", tid);
  g_nb_pid_resp++;
  if (pid > 0)
    g_gui_server_pid = pid;
}

int gui_server_get_pid(void)
{
  return g_gui_server_pid;
}

void gui_server_llid_closed(int llid, int from_clone)
{
  if ((!from_clone) && (llid == g_llid))
    {
    if (!g_stopping)
      KERR("ERROR unexpected gui-server RPC disconnection llid=%d port=%s",
           llid, g_web_port);
    g_llid = 0;
    if (g_gui_server_pid > 0)
      kill(g_gui_server_pid, SIGTERM);
    g_gui_server_pid = 0;
    }
}

static void timer_restart(void *data)
{
  gui_server_start();
}

static void gui_server_start(void)
{
  static char *argv[] = { g_bin_gui, g_snf_dir, g_web_port,
                           g_ctrl_sock, g_doors_path, g_passwd, NULL };

  if (!g_initialised)
    {
    KERR("ERROR gui_server_start called before gui_server_init");
    return;
    }
  if (get_glob_req_self_destruction())
    {
    KERR("WARNING gui_server_start: self_destruction in progress");
    return;
    }
  if (get_killing_cloonix())
    {
    KERR("WARNING gui_server_start: killing_cloonix in progress");
    return;
    }
  if (access(g_bin_gui, X_OK))
    {
    KERR("ERROR cloonix-gui-server binary not found: %s", g_bin_gui);
    return;
    }
  unlink(g_ctrl_sock);
  g_llid = 0;
  g_gui_server_pid = 0;
  g_nb_pid_resp = 0;
  g_nb_pid_resp_warning = 0;
  pid_clone_launch(utils_execv, process_died, NULL, (void *) argv,
                   NULL, NULL, "gui_server", -1, 1);
  KTRC("cloonix-gui-server launched: %s %s %s %s %s [passwd]",
       g_bin_gui, g_snf_dir, g_web_port, g_ctrl_sock, g_doors_path);
  clownix_timeout_add(200, timer_connect,
                      (void *) g_ctrl_sock, NULL, NULL);
}

void gui_server_init(char *net, char *root_work, int web_port,
                     char *passwd, char *doors_addr)
{
  memset(g_bin_gui,    0, MAX_PATH_LEN);
  memset(g_snf_dir,    0, MAX_PATH_LEN);
  memset(g_web_port,   0, MAX_NAME_LEN);
  memset(g_ctrl_sock,  0, MAX_PATH_LEN);
  memset(g_doors_path, 0, MAX_PATH_LEN);
  memset(g_passwd,     0, MSG_DIGEST_LEN);
  g_gui_server_pid    = 0;
  g_initialised       = 0;
  g_stopping          = 0;
  g_llid              = 0;
  g_nb_pid_resp       = 0;
  g_nb_pid_resp_warning = 0;
  g_web_port_num      = web_port;

  if (web_port <= 0)
    {
    KTRC("gui_server_init: no cloonix_web port for net=%s"
         " — gui-server not started", net);
    return;
    }

  snprintf(g_bin_gui,    MAX_PATH_LEN-1, "%s", pthexec_cloonfs_gui_server());
  snprintf(g_snf_dir,    MAX_PATH_LEN-1, "%s/snf", root_work);
  snprintf(g_web_port,   MAX_NAME_LEN-1, "%d", web_port);
  snprintf(g_ctrl_sock,  MAX_PATH_LEN-1, "%s/%s", root_work, GUI_SERVER_SOCK_DIR);
  if (get_running_in_crun())
    snprintf(g_doors_path, MAX_PATH_LEN-1, "%s_%s/proxy_pmain.sock",
             PROXYSHARE_IN, cfg_get_cloonix_name());
  else
    strncpy(g_doors_path, doors_addr, MAX_PATH_LEN-1);
  strncpy(g_passwd,      passwd, MSG_DIGEST_LEN-1);

  KTRC("gui_server_init: net=%s bin=%s snf=%s port=%s ctrl=%s doors=%s",
       net, g_bin_gui, g_snf_dir, g_web_port, g_ctrl_sock, g_doors_path);

  g_initialised = 1;
  gui_server_start();
}

void gui_server_stop(void)
{
  if (!g_initialised || g_web_port_num <= 0)
    return;
  g_stopping = 1;
  if (g_llid && msg_exist_channel(g_llid))
    {
    rpct_send_kil_req(g_llid, 0);
    g_llid = 0;
    }
  if (g_gui_server_pid > 0)
    {
    KTRC("gui_server_stop: SIGTERM to pid=%d port=%s",
         g_gui_server_pid, g_web_port);
    kill(g_gui_server_pid, SIGTERM);
    g_gui_server_pid = 0;
    }
}
