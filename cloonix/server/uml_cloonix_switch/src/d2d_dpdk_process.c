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
#include <errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "d2d_dpdk_process.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "dpdk_tap.h"
#include "dpdk_dyn.h"
#include "dpdk_d2d.h"
#include "dpdk_ovs.h"
#include "llid_trace.h"

typedef struct t_d2d_dpdk
{
  char name[MAX_NAME_LEN];
  char socket[MAX_PATH_LEN];
  int count;
  int llid;
  int pid;
  int closed_count;
  int suid_root_done;
  int watchdog_count;
  struct t_d2d_dpdk *prev;
  struct t_d2d_dpdk *next;
} t_d2d_dpdk;

static char g_ascii_cpu_mask[MAX_NAME_LEN];
static char g_cloonix_net[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_bin_d2d[MAX_PATH_LEN];
static t_d2d_dpdk *g_head_d2d_dpdk;

int get_glob_req_self_destruction(void);
uint32_t get_cpu_mask(void);

/****************************************************************************/
static t_d2d_dpdk *find_d2d_dpdk(char *name)
{
  t_d2d_dpdk *cur = g_head_d2d_dpdk;
  while(cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_d2d_dpdk(t_d2d_dpdk *cur)
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
  if (cur == g_head_d2d_dpdk)
    g_head_d2d_dpdk = cur->next;
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void d2d_dpdk_start(char *name)
{
  static char *argv[6];
  argv[0] = g_bin_d2d;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = name;
  argv[4] = g_ascii_cpu_mask;
  argv[5] = NULL;
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
    if (hop_event_alloc(llid, type_hop_d2d_dpdk, name, 0))
      KERR(" ");
    llid_trace_alloc(llid, name, 0, 0, type_llid_trace_endp_ovsdb);
    rpct_send_pid_req(NULL, llid, type_hop_d2d_dpdk, name, 0);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(void *data)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_dpdk *next, *cur = g_head_d2d_dpdk;
  int llid;
  char *msg = "cloonixd2d_suidroot";
  if (get_glob_req_self_destruction())
    return;
  while(cur)
    {
    next = cur->next;
    if (cur->llid == 0)
      {
      llid = try_connect(cur->socket, cur->name);
      if (llid)
        {
        cur->llid = llid;
        }
      else
        {
        cur->count += 1;
        if (cur->count == 20)
          {
          KERR("%s %s", locnet, cur->socket);
          dpdk_d2d_event_from_d2d_dpdk_process(cur->name, -1);
          free_d2d_dpdk(cur);
          }
        }
      }
    else if (cur->suid_root_done == 0)
      {
      rpct_send_diag_msg(NULL, cur->llid, type_hop_d2d_dpdk, msg);
      hop_event_hook(cur->llid, FLAG_HOP_DIAG, msg);
      cur->suid_root_done = 1;
      }
    else
      {
      cur->watchdog_count += 1;
      if (cur->watchdog_count >= 25)
        {
        dpdk_d2d_event_from_d2d_dpdk_process(cur->name, 0);
        free_d2d_dpdk(cur);
        }
      cur->count += 1;
      if (cur->count == 5)
        {
        rpct_send_pid_req(NULL, cur->llid, type_hop_d2d_dpdk, cur->name, 0);
        cur->count = 0;
        }
      if (cur->closed_count > 0)
        { 
        cur->closed_count -= 1;
        if (cur->closed_count == 0)
          {
          dpdk_d2d_event_from_d2d_dpdk_process(cur->name, 0);
          free_d2d_dpdk(cur);
          }
        }
      }
    cur = next;
    }
  clownix_timeout_add(20, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void d2d_dpdk_pid_resp(int llid, int tid, char *name, int pid)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_dpdk *cur = find_d2d_dpdk(name);
  if (!cur)
    {
    KERR("%s %s %d", locnet, name, pid);
    }
  else
    {
    cur->watchdog_count = 0;
    if (cur->pid == 0)
      {
      dpdk_d2d_event_from_d2d_dpdk_process(name, 1);
      cur->pid = pid;
      }
    else if (cur->pid != pid)
      {
      KERR("%s %s %d", locnet, name, pid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int d2d_dpdk_diag_llid(int llid)
{
  t_d2d_dpdk *cur = g_head_d2d_dpdk;
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
void d2d_dpdk_diag_resp(int llid, int tid, char *line)
{
  char *locnet = cfg_get_cloonix_name();
  uint16_t udp_port;
  char name[MAX_NAME_LEN];
  if (!strcmp(line,
  "cloonixd2d_suidroot_ko"))
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    KERR("%s Started d2d_dpdk: %s", locnet, line);
    }
  else if (!strcmp(line,
  "cloonixd2d_suidroot_ok"))
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    }
  else if (sscanf(line,
  "cloonixd2d_vhost_start_ok %s", name) == 1)
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    dpdk_d2d_vhost_started(name);
    }
  else if (sscanf(line,
  "cloonixd2d_get_udp_port_ko %s", name) == 1)
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    dpdk_d2d_get_udp_port_done(name, 0, -1);
    }
  else if (sscanf(line,
  "cloonixd2d_get_udp_port_ok %s udp_port=%hu", name, &udp_port) == 2)
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    dpdk_d2d_get_udp_port_done(name, udp_port, 0);
    }
  else if (sscanf(line,
  "cloonixd2d_set_dist_udp_ip_port_ok %s", name) == 1)
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    dpdk_d2d_dist_udp_ip_port_done(name);
    }
  else if (sscanf(line,
  "cloonixd2d_receive_probe_udp %s", name) == 1)
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    dpdk_d2d_receive_probe_udp(name);
    }

  else
    KERR("%s ERROR d2d_dpdk: %s", locnet, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void d2d_dpdk_start_stop_process(char *name, int on)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_dpdk *cur = find_d2d_dpdk(name);
  char *d2d = utils_get_dpdk_d2d_dir();
  char msg[MAX_PATH_LEN];
  if (on)
    {
    if (cur)
      {
      KERR("%s %s", locnet, name);
      }
    else
      {
      cur = (t_d2d_dpdk *)clownix_malloc(sizeof(t_d2d_dpdk), 5);
      memset(cur, 0, sizeof(t_d2d_dpdk));
      strncpy(cur->name, name, MAX_NAME_LEN-1);
      snprintf(cur->socket, MAX_NAME_LEN-1, "%s/%s", d2d, name);
      if (g_head_d2d_dpdk)
        g_head_d2d_dpdk->prev = cur;
      cur->next = g_head_d2d_dpdk;
      g_head_d2d_dpdk = cur; 
      d2d_dpdk_start(name);
      }
    }
  else
    {
    if (!cur)
      {
      KERR("%s %s", locnet, name);
      }
    else
      {
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "rpct_send_kil_req to %s", name);
      rpct_send_kil_req(NULL, cur->llid, type_hop_d2d_dpdk);
      hop_event_hook(cur->llid, FLAG_HOP_DIAG, msg);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  d2d_dpdk_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_d2d_dpdk *cur = g_head_d2d_dpdk;
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
    cur = g_head_d2d_dpdk;
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
void d2d_dpdk_llid_closed(int llid)
{
  t_d2d_dpdk *cur = g_head_d2d_dpdk;
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
void d2d_dpdk_start_vhost(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  t_d2d_dpdk *cur = find_d2d_dpdk(name);
  if (!cur)
    {
    KERR("%s %s", locnet, name);
    }
  else
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1, "cloonixd2d_vhost_start %s", name);
    rpct_send_diag_msg(NULL, cur->llid, type_hop_d2d_dpdk, msg);
    hop_event_hook(cur->llid, FLAG_HOP_DIAG, msg);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void d2d_dpdk_get_udp_port(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  t_d2d_dpdk *cur = find_d2d_dpdk(name);
  if (!cur)
    {
    KERR("%s %s", locnet, name);
    }
  else
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1, "cloonixd2d_get_udp_port %s", name);
    rpct_send_diag_msg(NULL, cur->llid, type_hop_d2d_dpdk, msg);
    hop_event_hook(cur->llid, FLAG_HOP_DIAG, msg);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void d2d_dpdk_set_dist_udp_ip_port(char *name, uint32_t ip, uint16_t port)
{
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  t_d2d_dpdk *cur = find_d2d_dpdk(name);
  if (!cur)
    {
    KERR("%s %s", locnet, name);
    }
  else 
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,
    "cloonixd2d_set_dist_udp_ip_port %s %x %hu", name, ip, port);
    rpct_send_diag_msg(NULL, cur->llid, type_hop_d2d_dpdk, msg);
    hop_event_hook(cur->llid, FLAG_HOP_DIAG, msg);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void d2d_dpdk_send_probe_udp(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  t_d2d_dpdk *cur = find_d2d_dpdk(name);
  if (!cur)
    {
    KERR("%s %s", locnet, name);
    }
  else
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,
    "cloonixd2d_send_probe_udp %s", name);
    rpct_send_diag_msg(NULL, cur->llid, type_hop_d2d_dpdk, msg);
    hop_event_hook(cur->llid, FLAG_HOP_DIAG, msg);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void d2d_dpdk_init(void)
{
  char *root = cfg_get_root_work();
  char *net = cfg_get_cloonix_name();
  char *bin_d2d = utils_get_d2d_dpdk_bin_path();

  g_head_d2d_dpdk = NULL;
  memset(g_cloonix_net, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_bin_d2d, 0, MAX_PATH_LEN);
  memset(g_ascii_cpu_mask, 0, MAX_NAME_LEN);
  snprintf(g_ascii_cpu_mask, MAX_NAME_LEN-1, "%X",  get_cpu_mask());
  strncpy(g_cloonix_net, net, MAX_NAME_LEN-1);
  strncpy(g_root_path, root, MAX_PATH_LEN-1);
  strncpy(g_bin_d2d, bin_d2d, MAX_PATH_LEN-1);
  clownix_timeout_add(100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

