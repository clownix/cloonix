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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "a2b_dpdk_process.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "dpdk_a2b.h"
#include "llid_trace.h"

typedef struct t_a2b_dpdk
{
  char name[MAX_NAME_LEN];
  char socket[MAX_PATH_LEN];
  int count;
  int llid;
  int pid;
  int closed_count;
  int suid_root_done;
  int watchdog_count;
  struct t_a2b_dpdk *prev;
  struct t_a2b_dpdk *next;
} t_a2b_dpdk;

static char g_ascii_cpu_mask[MAX_NAME_LEN];
static char g_cloonix_net[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_bin_a2b[MAX_PATH_LEN];
static t_a2b_dpdk *g_head_a2b_dpdk;

char *get_memid(void);
int get_glob_req_self_destruction(void);
uint32_t get_cpu_mask(void);

/****************************************************************************/
static t_a2b_dpdk *find_a2b_dpdk(char *name)
{
  t_a2b_dpdk *cur = g_head_a2b_dpdk;
  while(cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_a2b_dpdk *find_a2b_dpdk_with_llid(int llid)
{
  t_a2b_dpdk *cur = g_head_a2b_dpdk;
  while(cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_a2b_dpdk(t_a2b_dpdk *cur)
{
  if (cur->llid)
    {
    llid_trace_free(cur->llid, 0, __FUNCTION__);
    hop_event_free(cur->llid);
    }
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_a2b_dpdk)
    g_head_a2b_dpdk = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void a2b_dpdk_start(char *name)
{
  static char *argv[6];
  argv[0] = g_bin_a2b;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = name;
  argv[4] = g_ascii_cpu_mask;
  argv[5] = get_memid();
  argv[6] = NULL;
  pid_clone_launch(utils_execve, process_demonized, NULL,
                   (void *) argv, NULL, NULL, name, -1, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int try_connect(char *socket, char *name)
{
  int llid = string_client_unix(socket, uml_clownix_switch_error_cb,
                                uml_clownix_switch_rx_cb, name);
  if (llid)
    {
    if (hop_event_alloc(llid, type_hop_a2b_dpdk, name, 0))
      KERR(" ");
    llid_trace_alloc(llid, name, 0, 0, type_llid_trace_endp_ovsdb);
    rpct_send_pid_req(llid, type_hop_a2b_dpdk, name, 0);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(void *data)
{
  t_a2b_dpdk *next, *cur = g_head_a2b_dpdk;
  int llid;
  char *msg = "cloonixa2b_suidroot";
  if (get_glob_req_self_destruction())
    return;
  while(cur)
    {
    next = cur->next;
    cur->watchdog_count += 1;
    if (cur->llid == 0)
      {
      llid = try_connect(cur->socket, cur->name);
      if (llid)
        {
        cur->llid = llid;
        cur->count = 0;
        }
      else
        {
        cur->count += 1;
        if (cur->count == 50)
          {
          KERR("%s", cur->socket);
          dpdk_a2b_event_from_a2b_dpdk_process(cur->name, -1);
          free_a2b_dpdk(cur);
          }
        }
      }
    else if (cur->suid_root_done == 0)
      {
      cur->count += 1;
      if (cur->count == 50)
        {
        KERR("%s", cur->socket);
        dpdk_a2b_event_from_a2b_dpdk_process(cur->name, -1);
        free_a2b_dpdk(cur);
        }
      else
        {
        rpct_send_sigdiag_msg(cur->llid, type_hop_a2b_dpdk, msg);
        hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
        }
      }
    else if (cur->watchdog_count >= 150)
      {
      dpdk_a2b_event_from_a2b_dpdk_process(cur->name, -1);
      free_a2b_dpdk(cur);
      }
    else
      {
      cur->count += 1;
      if (cur->count == 5)
        {
        rpct_send_pid_req(cur->llid, type_hop_a2b_dpdk, cur->name, 0);
        cur->count = 0;
        }
      if (cur->closed_count > 0)
        { 
        cur->closed_count -= 1;
        if (cur->closed_count == 0)
          {
          dpdk_a2b_event_from_a2b_dpdk_process(cur->name, 0);
          free_a2b_dpdk(cur);
          }
        }
      }
    cur = next;
    }
  clownix_timeout_add(20, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void a2b_dpdk_pid_resp(int llid, int tid, char *name, int pid)
{
  t_a2b_dpdk *cur = find_a2b_dpdk(name);
  if (!cur)
    KERR("%s %d", name, pid);
  else
    {
    cur->watchdog_count = 0;
    if ((cur->pid == 0) && (cur->suid_root_done == 1))
      {
      cur->pid = pid;
      dpdk_a2b_event_from_a2b_dpdk_process(name, 1);
      }
    else if (cur->pid && (cur->pid != pid))
      {
      KERR("%s %d", name, pid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int a2b_dpdk_diag_llid(int llid)
{
  t_a2b_dpdk *cur = g_head_a2b_dpdk;
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void a2b_dpdk_sigdiag_resp(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  t_a2b_dpdk *cur;
  if (sscanf(line,
  "cloonixa2b_suidroot_ko %s", name) == 1)
    {
    cur = find_a2b_dpdk(name);
    if (!cur)
      KERR("ERROR a2b_dpdk: %s %s", g_cloonix_net, line);
    else
      {
      KERR("Started a2b_dpdk: %s %s", g_cloonix_net, line);
      dpdk_a2b_event_from_a2b_dpdk_process(cur->name, -1);
      }
    }
  else if (sscanf(line,
  "cloonixa2b_suidroot_ok %s", name) == 1)
    {
    cur = find_a2b_dpdk(name);
    if (!cur)
      KERR("ERROR a2b_dpdk: %s %s", g_cloonix_net, line);
    else
      {
      cur->suid_root_done = 1;
      cur->count = 0;
      }
    }
  else if (sscanf(line,
  "cloonixa2b_vhost_start_ok %s", name) == 1)
    {
    cur = find_a2b_dpdk(name);
    if (!cur)
      KERR("ERROR a2b_dpdk: %s %s", g_cloonix_net, line);
    else
      {
      dpdk_a2b_vhost_started(name);
      }
    }
  else
    KERR("ERROR a2b_dpdk: %s %s", g_cloonix_net, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void a2b_dpdk_start_vhost(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  t_a2b_dpdk *cur = find_a2b_dpdk(name);
  if (!cur)
    {
    KERR("%s %s", locnet, name);
    }
  else if (cur->pid == 0)
    {
    KERR("%s %s", locnet, name);
    }
  else
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,
    "cloonixa2b_vhost_start %s", name);
    rpct_send_sigdiag_msg(cur->llid, type_hop_a2b_dpdk, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void a2b_dpdk_stop_vhost(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  t_a2b_dpdk *cur = find_a2b_dpdk(name);
  if (!cur)
    {
    KERR("%s %s", locnet, name);
    }
  else if (cur->pid == 0)
    {
    KERR("%s %s", locnet, name);
    }
  else
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,
    "cloonixa2b_vhost_stop %s", name);
    rpct_send_sigdiag_msg(cur->llid, type_hop_a2b_dpdk, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void a2b_dpdk_cnf(char *name, int dir, int type, int val)
{
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  t_a2b_dpdk *cur = find_a2b_dpdk(name);
  if (!cur)
    {
    KERR("%s %s", locnet, name);
    }
  else
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1, 
    "cloonixa2b_vhost_cnf %s dir=%d type=%d val=%d", name, dir, type, val);
    rpct_send_sigdiag_msg(cur->llid, type_hop_a2b_dpdk, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void a2b_dpdk_start_stop_process(char *name, int on)
{
  t_a2b_dpdk *cur = find_a2b_dpdk(name);
  char *a2b = utils_get_dpdk_a2b_dir();
  char msg[MAX_PATH_LEN];
  if (on)
    {
    if (cur)
      KERR("%s", name);
    else
      {
      cur = (t_a2b_dpdk *) malloc(sizeof(t_a2b_dpdk));
      memset(cur, 0, sizeof(t_a2b_dpdk));
      strncpy(cur->name, name, MAX_NAME_LEN-1);
      snprintf(cur->socket, MAX_NAME_LEN-1, "%s/%s", a2b, name);
      if (g_head_a2b_dpdk)
        g_head_a2b_dpdk->prev = cur;
      cur->next = g_head_a2b_dpdk;
      g_head_a2b_dpdk = cur; 
      a2b_dpdk_start(name);
      }
    }
  else
    {
    if (!cur)
      KERR("%s", name);
    else
      {
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "rpct_send_kil_req to %s", name);
      rpct_send_kil_req(cur->llid, type_hop_a2b_dpdk);
      hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  a2b_dpdk_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_a2b_dpdk *cur = g_head_a2b_dpdk;
  int i, result = 0;
  event_print("%s", __FUNCTION__);
  while(cur)
    {
    if (cur->pid)
      result++;
    cur = cur->next;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid),5);
    memset(glob_lst, 0, result*sizeof(t_lst_pid));
    cur = g_head_a2b_dpdk;
    i = 0;
    while(cur)
      {
      if (cur->pid)
        {
        strncpy(glob_lst[i].name, cur->name, MAX_NAME_LEN-1);
        glob_lst[i].pid = cur->pid;
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void a2b_dpdk_llid_closed(int llid)
{
  t_a2b_dpdk *cur = g_head_a2b_dpdk;
  while(cur)
    {
    if (cur->llid == llid)
      {
      cur->closed_count = 2;
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int a2b_dpdk_name_exists_with_llid(int llid, char *name)
{
  int result = 0;
  t_a2b_dpdk *cur = find_a2b_dpdk_with_llid(llid);
  memset(name, 0, MAX_NAME_LEN);
  if (cur)
    {
    strncpy(name, cur->name, MAX_NAME_LEN-1);
    result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int a2b_dpdk_llid_exists_with_name(char *name)
{
  int result = 0;
  t_a2b_dpdk *cur = find_a2b_dpdk(name);
  if (cur)
    result = cur->llid;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void a2b_dpdk_init(void)
{
  char *root = cfg_get_root_work();
  char *net = cfg_get_cloonix_name();
  char *bin_a2b = utils_get_a2b_dpdk_bin_path();

  g_head_a2b_dpdk = NULL;
  memset(g_cloonix_net, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_bin_a2b, 0, MAX_PATH_LEN);
  memset(g_ascii_cpu_mask, 0, MAX_NAME_LEN);
  snprintf(g_ascii_cpu_mask, MAX_NAME_LEN-1, "%X",  get_cpu_mask());
  strncpy(g_cloonix_net, net, MAX_NAME_LEN-1);
  strncpy(g_root_path, root, MAX_PATH_LEN-1);
  strncpy(g_bin_a2b, bin_a2b, MAX_PATH_LEN-1);
  clownix_timeout_add(100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

