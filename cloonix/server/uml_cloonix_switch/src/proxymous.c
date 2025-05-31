/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
static int g_port_web;

static char g_own_passwd[MSG_DIGEST_LEN];
static char g_cloonix_net[MAX_NAME_LEN];
static char g_proxymous_bin[MAX_PATH_LEN];
static char g_proxymous_dir[MAX_PATH_LEN];
static char g_socket[MAX_PATH_LEN];
int get_glob_req_self_destruction(void);
int get_running_in_crun(void);

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(void *data)
{
  int llid;
  if (get_glob_req_self_destruction())
    return;
  g_watchdog_count += 1;
  if (g_watchdog_count >= 100)
    {
    KERR("ERROR IMPOSSIBLE CTRL CONNECTION TO PROXYMOUS");
    KERR("ERROR %s", g_socket);
    recv_kill_uml_clownix(0, 0);
    }
  else
    {
    if (g_started == 0)
      {
      if (!get_running_in_crun())
        proxymous_start();
      g_started = 1;
      g_watchdog_count = 0;
      }
    else if (g_llid == 0)
      {
      llid = try_connect(g_socket, "proxymus");
      if (llid)
        {
        proxycrun_init(g_port_web, g_own_passwd);
        g_llid = llid;
        g_count = 0;
        }
      else
        {
        g_count += 1;
        if (g_count == 50)
          KERR("%s", g_socket);
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
    }
  clownix_timeout_add(20, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void proxymous_pid_resp(int llid, char *name, int pid)
{
  g_watchdog_count = 0;
  if (g_pid == 0)
    g_pid = pid;
  else if (g_pid && (g_pid != pid))
    KERR("ERROR %d %d", g_pid, pid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void proxymous_kill(void)
{
  if ((g_llid) && (msg_exist_channel(g_llid)))
    rpct_send_kil_req(g_llid, 0);
  else
    KERR("ERROR SEND KILL PROXYMOUS");
  clownix_timeout_add(5, timer_kill, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void proxymous_init(int pweb, char *passwd)
{
  char *net = cfg_get_cloonix_name();
  char *bin_proxymous = utils_get_proxymous_bin();
  g_started = 0;
  g_pid = 0;
  g_watchdog_count = 0;
  g_llid = 0;
  g_count = 0;
  g_port_web = pweb;
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
/*--------------------------------------------------------------------------*/

