/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "ovs_c2c.h"
#include "llid_trace.h"
#include "msg.h"
#include "cnt.h"
#include "c2c_peer.h"
#include "lan_to_name.h"
#include "ovs_snf.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "mactopo.h"

static char g_cloonix_net[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_bin_c2c[MAX_PATH_LEN];

static t_ovs_c2c *g_head_c2c;
static int g_nb_c2c;

int get_glob_req_self_destruction(void);

enum {
  state_idle = 0,
  state_up_initialised,
  state_up_process_running,
  state_up_lan_ovs_req,
  state_up_lan_ovs_resp,
  state_down_initialised,
  state_down_vhost_stopped,
  state_down_ovs_req_del_lan,
  state_down_ovs_resp_del_lan,
};


/****************************************************************************/
static void nb_to_text(int state, char *resp)
{
  memset(resp, 0, MAX_NAME_LEN);
  switch (state)
    {
    case state_idle:
      strncpy(resp, "state_idle", MAX_NAME_LEN-1);
      break;
    case state_up_initialised:
      strncpy(resp, "state_up_initialised", MAX_NAME_LEN-1);
      break;
    case state_up_process_running:
      strncpy(resp, "state_up_process_running", MAX_NAME_LEN-1);
      break;
    default:
      KOUT("%d", state);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void state_progress_up(t_ovs_c2c *cur, int state)
{
//  char *locnet = cfg_get_cloonix_name();
  char olab[MAX_NAME_LEN];
  char nlab[MAX_NAME_LEN];
  nb_to_text(cur->state_up, olab);
  cur->state_up = state;
  nb_to_text(cur->state_up, nlab);
//  KERR("%s %s  %s ----> %s", locnet, cur->name, olab, nlab);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_ovs_c2c *find_c2c(char *name)
{
  t_ovs_c2c *cur = g_head_c2c;
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
static void free_c2c(t_ovs_c2c *cur)
{
  t_ovs_c2c *ncur;
  cfg_free_obj_id(cur->c2c_id);
  layout_del_sat(cur->name);
  if (cur->llid)
    {
    llid_trace_free(cur->llid, 0, __FUNCTION__);
    hop_event_free(cur->llid);
    }
  if (strlen(cur->lan_added))
    {
    KERR("%s %s", cur->name, cur->lan_added);
    mactopo_del_req(item_c2c, cur->name, 0, cur->lan_added);
    lan_del_name(cur->lan_added, item_c2c, cur->name, 0);
    }
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_c2c)
    g_head_c2c = cur->next;
  g_nb_c2c -= 1;
  if (cur->must_restart)
    {
    KERR("WARNING RESTARTING %s", cur->name);
    ovs_c2c_add(0,0, cur->name, cur->topo.loc_udp_ip, cur->topo.dist_cloon,
                cur->topo.dist_tcp_ip, cur->topo.dist_tcp_port,
                cur->dist_passwd, cur->topo.dist_udp_ip);
    ncur = find_c2c(cur->name);
    if (!ncur)
      KERR("ERROR %s", cur->name);
    else if (strlen(cur->must_restart_lan))
      strncpy(ncur->lan_waiting, cur->must_restart_lan, MAX_NAME_LEN-1);
    }
  free(cur);
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_ovs_c2c *alloc_c2c_base(char *name, char *dist_name)
{
  char *root = g_root_path;
  uint8_t mc[6];
  int id = cfg_alloc_obj_id();
  t_ovs_c2c *cur = (t_ovs_c2c *) malloc(sizeof(t_ovs_c2c));
  if (!cur)
    KOUT("%s", name);
  memset(cur, 0, sizeof(t_ovs_c2c));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->c2c_id = id;
  cur->topo.endp_type = endp_type_c2cv;
  mc[0] = 0x2;
  mc[1] = 0xFF & rand();
  mc[2] = 0xFF & rand();
  mc[3] = 0xFF & rand();
  mc[4] = cur->c2c_id % 100;
  mc[5] = 0xFF & rand();
  snprintf(cur->socket, MAX_NAME_LEN-1, "%s/%s/c%s", root, C2C_DIR, name);
  snprintf(cur->vhost, (MAX_NAME_LEN-1), "%s%d_0", OVS_BRIDGE_PORT, id);
  snprintf(cur->mac, (MAX_NAME_LEN-1),
           "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
           mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);

  strncpy(cur->topo.name, name, MAX_NAME_LEN-1);
  strncpy(cur->topo.dist_cloon, dist_name, MAX_NAME_LEN-1);

  if (g_head_c2c)
    g_head_c2c->prev = cur;
  cur->next = g_head_c2c;
  g_head_c2c = cur;
  g_nb_c2c += 1;
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_c2c_master(char *name, char *dist_name,
                             uint32_t loc_udp_ip, uint32_t dist_ip,
                             uint16_t dist_port, char *dist_passwd,
                             uint32_t dist_udp_ip)
{
  uint32_t localhost;
  t_ovs_c2c *cur = alloc_c2c_base(name, dist_name);
  memset(cur->dist_passwd, 0, MSG_DIGEST_LEN);
  strncpy(cur->dist_passwd, dist_passwd, MSG_DIGEST_LEN-1);
  cur->topo.local_is_master = 1;
  cur->topo.dist_tcp_ip = dist_ip;
  cur->topo.dist_tcp_port = dist_port;
  cur->topo.loc_udp_ip = loc_udp_ip;
  cur->topo.dist_udp_ip = dist_udp_ip;
  cfg_hysteresis_send_topo_info();
  ip_string_to_int(&localhost, "127.0.0.1");
  if ((dist_ip != localhost) && (loc_udp_ip == localhost))
    {
    KERR("WARNING: giving distant c2c (not localhost) the local 127.0.0.1 address");
    KERR("WARNING: distant c2c probably will not be able to reach back to us");
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_ovs_c2c *alloc_c2c_peer(int llid, int tid, char *name, char *dist_name)
{
  t_ovs_c2c *cur = alloc_c2c_base(name, dist_name);
  cur->topo.tcp_connection_peered = 1;
  cur->peer_llid = llid;
  cur->ref_tid = tid;
  cfg_hysteresis_send_topo_info();
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void destroy_c2c(t_ovs_c2c *cur)
{
  if (cur->destroy_c2c_done == 0)
    {
    cur->destroy_c2c_done = 1;
    wrap_send_c2c_peer_ping(cur, -1);
    if ((cur->llid) && msg_exist_channel(cur->llid))
      rpct_send_kil_req(cur->llid, type_hop_c2c);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void carefull_free_c2c(char *name)
{
  t_ovs_c2c *cur = find_c2c(name);
  int val = 0;
  if (!cur)
    KOUT("ERROR %s", name);
  if ((cur->destroy_c2c_req != 0) ||
      (cur->free_c2c_req != 0))
    KERR("ERROR %s %d %d", name, cur->destroy_c2c_req, cur->free_c2c_req);
  else
    {
    if (strlen(cur->lan_added))
      {
      if (!strlen(cur->lan_attached))
        KERR("ERROR %s %s", name, cur->lan_added);
      mactopo_del_req(item_c2c, cur->name, 0, cur->lan_added);
      val = lan_del_name(cur->lan_added, item_c2c, cur->name, 0);
      memset(cur->lan_added, 0, MAX_NAME_LEN);
      if (val == 2*MAX_LAN)
        free_c2c(cur);
      else
        {
        if (!strlen(cur->lan_attached))
          KERR("ERROR %s %s", name, cur->lan_added);
        else
          msg_send_del_lan_endp(ovsreq_del_c2c_lan, name, 0,
                                cur->vhost, cur->lan_attached);
        cur->free_c2c_req = 1;
        }
      }
    else
      free_c2c(cur);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void carefull_destroy_c2c(char *name)
{
  t_ovs_c2c *cur = find_c2c(name);
  int val = 0;
  if (!cur)
    KOUT("ERROR %s", name);
  if ((cur->destroy_c2c_req != 0) ||
      (cur->free_c2c_req != 0))
    KERR("ERROR %s %d %d", name, cur->destroy_c2c_req, cur->free_c2c_req);
  else
    {    
    if (strlen(cur->lan_added))
      {
      if (!strlen(cur->lan_attached))
        KERR("ERROR %s %s", name, cur->lan_added);
      mactopo_del_req(item_c2c, cur->name, 0, cur->lan_added);
      val = lan_del_name(cur->lan_added, item_c2c, cur->name, 0);
      memset(cur->lan_added, 0, MAX_NAME_LEN);
      if (val == 2*MAX_LAN)
        destroy_c2c(cur);
      else
        {
        if (!strlen(cur->lan_attached))
          KERR("ERROR %s %s", name, cur->lan_added);
        else
          msg_send_del_lan_endp(ovsreq_del_c2c_lan, name, 0,
                                cur->vhost, cur->lan_attached);
        cur->destroy_c2c_req = 1;
        }
      }
    else
      destroy_c2c(cur);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void c2c_start(char *name, char *vhost)
{
  static char *argv[7];
  argv[0] = g_bin_c2c;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = name;
  argv[4] = vhost;
  argv[5] = NULL;
  pid_clone_launch(utils_execv, process_demonized, NULL,
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
    if (hop_event_alloc(llid, type_hop_c2c, name, 0))
      KERR(" ");
    llid_trace_alloc(llid, name, 0, 0, type_llid_trace_endp_ovsdb);
    rpct_send_pid_req(llid, type_hop_c2c, name, 0);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_ovs_c2c_attempt(t_ovs_c2c *cur)
{
  int llid = try_connect(cur->socket, cur->name);
  if (llid)
    {
    cur->llid = llid;
    cur->count = 0;
    }
  else
    {
    cur->count += 1;
    if (cur->count == 20)
      {
      KERR("%s %s", cur->socket, cur->name);
      free_c2c(cur);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_c2c_suidroot_attempt(t_ovs_c2c *cur)
{
  char *msg = "c2c_suidroot";
  if (!msg_exist_channel(cur->llid))
    free_c2c(cur);
  else
    {
    cur->count += 1;
    if (cur->count == 20)
      {
      KERR("ERROR %s %s", cur->socket, cur->name);
      free_c2c(cur);
      }
    else
      {
      if (!msg_exist_channel(cur->llid))
        KERR("ERROR %s %s", cur->socket, cur->name);
      else
        rpct_send_sigdiag_msg(cur->llid, type_hop_c2c, msg);
      hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void count_management_and_pid_req(t_ovs_c2c *cur)
{
  cur->count += 1;
  if (cur->count == 2)
    {
    rpct_send_pid_req(cur->llid, type_hop_c2c, cur->name, 0);
    cur->count = 0;
    }
  if (cur->closed_count > 0)
    { 
    cur->closed_count -= 1;
    if (cur->closed_count == 0)
      {
      free_c2c(cur);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(void *data)
{
  t_ovs_c2c *next, *cur = g_head_c2c;
  char *locnet = cfg_get_cloonix_name();
  char err[MAX_PATH_LEN];
  if (get_glob_req_self_destruction())
    return;
  while(cur)
    {
    next = cur->next;
    if (cur->destroy_c2c_done)
      free_c2c(cur);
    else if (cur->llid == 0)
      connect_ovs_c2c_attempt(cur);
    else if (cur->suid_root_done == 0)
      send_c2c_suidroot_attempt(cur);
    else if (!msg_exist_channel(cur->llid))
      carefull_free_c2c(cur->name);
    else
      {
      if (cur->topo.tcp_connection_peered)
        cur->peer_watchdog_count += 1;
      if (cur->peer_watchdog_count >= 10)
        {
        KERR("ERROR %s %s", locnet, cur->name);
        if (strlen(cur->lan_attached))
          strncpy(cur->must_restart_lan, cur->lan_attached, MAX_NAME_LEN-1);
        else if (strlen(cur->lan_added))
          strncpy(cur->must_restart_lan, cur->lan_added, MAX_NAME_LEN-1);
        else if (strlen(cur->lan_waiting))
          strncpy(cur->must_restart_lan, cur->lan_waiting, MAX_NAME_LEN-1);
        carefull_destroy_c2c(cur->name);
        if (cur->topo.local_is_master == 1)
          cur->must_restart = 1;
        }
      else
        {
        count_management_and_pid_req(cur);
        if (cur->topo.local_is_master == 1)
          {
          if (cur->peer_llid == 0)
            wrap_try_connect_to_peer(cur);
          else if (cur->topo.tcp_connection_peered == 0)
            wrap_send_c2c_peer_create(cur, 0);
          else if (cur->topo.udp_connection_peered == 1)
            wrap_send_c2c_peer_ping(cur, 2);
          else if (cur->udp_loc_port_chosen == 1)
            wrap_send_c2c_peer_ping(cur, 1);
          else
            wrap_send_c2c_peer_ping(cur, 0);
          }
        else
          {
          if (cur->topo.udp_connection_peered == 1)
            wrap_send_c2c_peer_ping(cur, 2);
          else if (cur->udp_loc_port_chosen == 1)
            wrap_send_c2c_peer_ping(cur, 1);
          else
            wrap_send_c2c_peer_ping(cur, 0);
          }
        if ((cur->udp_loc_port_chosen == 1) &&
            (cur->peer_conf_done == 0))
          {
          if (!wrap_send_c2c_peer_conf(cur, 0))
            cur->peer_conf_done = 1;
          }
        if ((cur->topo.tcp_connection_peered == 1) &&
            (cur->topo.udp_connection_peered == 1) &&
            (strlen(cur->lan_waiting)))
          {
          lan_add_name(cur->lan_waiting, item_c2c, cur->name, 0);
          if (mactopo_add_req(item_c2c, cur->name, 0, cur->lan_waiting,
                              NULL, NULL, err))
            {
            KERR("ERROR %s %s %s", cur->name, cur->lan_waiting, err);
            lan_del_name(cur->lan_waiting, item_c2c, cur->name, 0);
            utils_send_status_ko(&cur->cli_llid, &cur->cli_tid, err);
            }
          else if (msg_send_add_lan_endp(ovsreq_add_c2c_lan, cur->name, 0,
                                    cur->vhost, cur->lan_waiting))
            {
            KERR("ERROR %s %s", cur->name, cur->lan_waiting);
            lan_del_name(cur->lan_waiting, item_c2c, cur->name, 0);
            utils_send_status_ko(&cur->cli_llid, &cur->cli_tid, "msg_send ko");
            }
          else
            {
            strncpy(cur->lan_added, cur->lan_waiting, MAX_NAME_LEN-1);
            strncpy(cur->topo.lan, cur->lan_waiting, MAX_NAME_LEN-1);
            utils_send_status_ok(&cur->cli_llid, &cur->cli_tid);
            cfg_hysteresis_send_topo_info();
            }
          memset(cur->lan_waiting, 0, MAX_NAME_LEN);
          }
        }
      }
    cur = next;
    }
  clownix_timeout_add(100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_receive_probe_udp(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  char msg[MAX_PATH_LEN];
  if (!cur)
    KERR("ERROR %s %s", locnet, name);
  else
    {
    if (cur->topo.local_is_master == 0)
      {
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "c2c_send_probe_udp %s", name);
      if (!msg_exist_channel(cur->llid))
        KERR("ERROR %s %s", locnet, cur->name);
      else
        rpct_send_sigdiag_msg(cur->llid, type_hop_c2c, msg);
      hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
      }
    else
      {
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "c2c_enter_traffic_udp %s", name);
      if (!msg_exist_channel(cur->llid))
        KERR("ERROR %s %s", locnet, cur->name);
      else
        rpct_send_sigdiag_msg(cur->llid, type_hop_c2c, msg);
      hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_dist_udp_ip_port_done(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    KERR("ERROR %s %s", locnet, name);
  else
    {
    cur->udp_connection_tx_configured = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_get_udp_port_done(char *name, uint16_t port, int peer_status)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    KERR("ERROR %s %s %hu", locnet, name, port);
  else if (peer_status)
    KERR("ERROR %s %s", locnet, cur->name);
  else
    {
    cur->topo.loc_udp_port = port;
    cur->udp_loc_port_chosen = 1;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void snf_started(char *name, int num, char *vhost)
{
  t_ovs_c2c *cur = find_c2c(name);

  if (cur == NULL)
    KERR("ERROR %s %d", name, num);
  else
    {
    if (strlen(cur->lan_attached))
      {
      ovs_snf_send_add_snf_lan(name, num, cur->vhost, cur->lan_attached);
      }
    else
      {
      cur->must_call_snf_started = 1;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_peer_add(int llid, int tid, char *name, char *dist, char *loc)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (cur)
    KERR("ERROR %s %s %s", locnet, name, dist);
  else if (strcmp(loc, locnet))
    KERR("ERROR %s %s %s %s", locnet, name, dist, loc);
  else
    {
    cur = alloc_c2c_peer(llid, tid, name, dist);
    c2c_start(name, cur->vhost);
    wrap_send_c2c_peer_create(cur, 1);
    state_progress_up(cur, state_up_initialised);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_peer_add_ack(int llid, int tid, char *name,
                          char *dist, char *loc, int ack)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    KERR("ERROR %s %s %s", locnet, name, dist);
  else if (strcmp(loc, locnet))
    KERR("ERROR %s %s %s %s", locnet, name, dist, loc);
  else if (cur->topo.local_is_master == 0)
    KERR("ERROR %s %s %s %s", locnet, name, dist, loc);
   else if (tid != cur->ref_tid)
    KERR("ERROR %s %s %d %d", locnet, name, tid, cur->ref_tid);
  else
    {
    cur->topo.tcp_connection_peered = 1;
    cfg_hysteresis_send_topo_info();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_pid_resp(int llid, char *name, int pid)
{
  t_ovs_c2c *cur = find_c2c(name);
  char msg[MAX_PATH_LEN];

  if (!cur)
    KERR("%s %d", name, pid);
  else
    {
    if ((cur->pid == 0) && (cur->suid_root_done == 1))
      {
      layout_add_sat(name, 0, 0);
      cur->pid = pid;
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "c2c_get_udp_port %s", name);
      if (!msg_exist_channel(cur->llid))
        KERR("ERROR %s", cur->name);
      else
        rpct_send_sigdiag_msg(cur->llid, type_hop_c2c, msg);
      hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
      }
    else if (cur->pid && (cur->pid != pid))
      KERR("ERROR %s %d", name, pid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_c2c_diag_llid(int llid)
{
  t_ovs_c2c *cur = g_head_c2c;
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
void ovs_c2c_poldiag_resp(int llid, int tid, char *line)
{
    KERR("ERROR %s", line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_sigdiag_resp(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  uint16_t udp_port;
  t_ovs_c2c *cur;
  if (sscanf(line,
  "c2c_suidroot_ko %s", name) == 1)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", g_cloonix_net, line);
    else
      KERR("ERROR Started c2c: %s %s", g_cloonix_net, name);
    }
  else if (sscanf(line,
  "c2c_suidroot_ok %s", name) == 1)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", g_cloonix_net, line);
    else
      {
      cur->suid_root_done = 1;
      cur->count = 0;
      }
    }
  else if (sscanf(line,
  "c2c_get_udp_port_ko %s", name) == 1)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", g_cloonix_net, line);
    else
      c2c_get_udp_port_done(name, 0, -1);
    }
  else if (sscanf(line,
  "c2c_get_udp_port_ok %s udp_port=%hu", name, &udp_port) == 2)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", g_cloonix_net, line);
    else
      c2c_get_udp_port_done(name, udp_port, 0);
    }
  else if (sscanf(line,
  "c2c_set_dist_udp_ip_port_ok %s", name) == 1)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", g_cloonix_net, line);
    else
      c2c_dist_udp_ip_port_done(name);
    }
  else if (sscanf(line,
  "c2c_receive_probe_udp %s", name) == 1)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", g_cloonix_net, line);
    else
      c2c_receive_probe_udp(name);
    }
  else if (sscanf(line,
  "c2c_enter_traffic_udp_ack %s", name) == 1)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", g_cloonix_net, line);
    else
      {
      cur->topo.udp_connection_peered = 1;
      cfg_hysteresis_send_topo_info();
      }
    }

  else
    KERR("ERROR c2c: %s %s", g_cloonix_net, line);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int ovs_c2c_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_ovs_c2c *cur = g_head_c2c;
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
    cur = g_head_c2c;
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
void ovs_c2c_llid_closed(int llid)
{
  t_ovs_c2c *cur = g_head_c2c;
  while(cur)
    {
    if (cur->llid == llid)
      cur->closed_count = 2;
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_resp_add_lan(int is_ko, char *name, int num,
                         char *vhost, char *lan)
{
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    {
    mactopo_add_resp(0, name, num, lan);
    KERR("ERROR %d %s", is_ko, name); 
    }
  else
    {
    if (strlen(cur->lan_attached))
      KERR("ERROR: %s %s", name, cur->lan_attached);
    memset(cur->lan_waiting, 0, MAX_NAME_LEN);
    memset(cur->lan_attached, 0, MAX_NAME_LEN);
    memset(cur->topo.lan, 0, MAX_NAME_LEN);
    if (is_ko)
      {
      mactopo_add_resp(0, name, num, lan);
      KERR("ERROR %d %s", is_ko, name);
      }
    else
      {
      mactopo_add_resp(item_c2c, name, num, lan);
      strncpy(cur->lan_attached, lan, MAX_NAME_LEN);
      strncpy(cur->topo.lan, lan, MAX_NAME_LEN);
      if (cur->must_call_snf_started)
        {
        snf_started(name, num, vhost);
        cur->must_call_snf_started = 0;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_resp_del_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan)
{
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    {
    mactopo_del_resp(0, name, num, lan);
    KERR("ERROR %d %s", is_ko, name);
    }
  else if (cur->destroy_c2c_req == 1)
    {
    mactopo_del_resp(item_c2c, name, num, lan);
    memset(cur->lan_attached, 0, MAX_NAME_LEN);
    memset(cur->lan_waiting, 0, MAX_NAME_LEN);
    memset(cur->topo.lan, 0, MAX_NAME_LEN);
    destroy_c2c(cur);
    cur->destroy_c2c_req = 0;
    }
  else if (cur->free_c2c_req == 1)
    {
    mactopo_del_resp(item_c2c, name, num, lan);
    free_c2c(cur);
    }
  else
    {
    mactopo_del_resp(item_c2c, name, num, lan);
    memset(cur->lan_attached, 0, MAX_NAME_LEN);
    memset(cur->lan_waiting, 0, MAX_NAME_LEN);
    memset(cur->topo.lan, 0, MAX_NAME_LEN);
    if (is_ko)
      KERR("ERROR %d %s", is_ko, name);
    }
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
t_ovs_c2c *ovs_c2c_exists(char *name)
{
  return (find_c2c(name));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_ovs_c2c *ovs_c2c_get_first(int *nb_c2c)
{
  t_ovs_c2c *result = g_head_c2c;
  *nb_c2c = g_nb_c2c;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *ovs_c2c_translate_topo_endp(int *nb)
{
  t_ovs_c2c *cur;
  int len, count = 0, nb_endp = 0;
  t_topo_endp *endp;
  cur = g_head_c2c;
  while(cur)
    {
    if (strlen(cur->topo.lan))
      count += 1;
    cur = cur->next;
    }
  endp = (t_topo_endp *) clownix_malloc(count * sizeof(t_topo_endp), 13);
  memset(endp, 0, count * sizeof(t_topo_endp));
  cur = g_head_c2c;
  while(cur)
    {
    if (strlen(cur->topo.lan))
      {
      len = sizeof(t_lan_group_item);
      endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
      memset(endp[nb_endp].lan.lan, 0, len);
      strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
      endp[nb_endp].num = 0;
      endp[nb_endp].type = cur->topo.endp_type;
      strncpy(endp[nb_endp].lan.lan[0].lan, cur->topo.lan, MAX_NAME_LEN-1);
      endp[nb_endp].lan.nb_lan = 1;
      nb_endp += 1;
      }
    cur = cur->next;
    }
  *nb = nb_endp;
  return endp;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_add(int llid, int tid, char *name, uint32_t loc_udp_ip,
                 char *dist_name, uint32_t dist_ip, uint16_t dist_port,
                 char *dist_passwd, uint32_t dist_udp_ip)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (cur)
    {
    KERR("ERROR %s", name);
    if (llid)
      send_status_ko(llid, tid, "Exists already");
    }
  else if (!strcmp(locnet, dist_name))
    {
    KERR("ERROR %s", name);
    if (llid)
      send_status_ko(llid, tid, "Cannot c2c to our own name");
    }
  else
    {
    alloc_c2c_master(name, dist_name, loc_udp_ip, dist_ip, dist_port,
                     dist_passwd, dist_udp_ip);
    cur = find_c2c(name);
    if (!cur)
      KOUT(" ");
    if (llid)
      send_status_ok(llid, tid, "OK");
    c2c_start(name, cur->vhost);
    state_progress_up(cur, state_up_initialised);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_del(int llid, int tid, char *name)
{
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    {
    KERR("ERROR %s", name);
    if (llid)
      send_status_ko(llid, tid, "Does not exist");
    }
  else if (cur->destroy_c2c_req)
    {
    KERR("ERROR %s", name);
    if (llid)
      send_status_ko(llid, tid, "Not ready");
    }
  else if (cur->free_c2c_req)
    {
    KERR("ERROR %s", name);
    if (llid)
      send_status_ko(llid, tid, "Not ready");
    }
  else
    {
    if (llid)
      send_status_ok(llid, tid, "OK");
    carefull_destroy_c2c(cur->name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_add_lan(int llid, int tid, char *name, char *lan)
{
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    {
    KERR("ERROR %s", name);
    send_status_ko(llid, tid, "Does not exist");
    }
  else if (cur->destroy_c2c_req)
    {
    KERR("ERROR %s", name);
    send_status_ko(llid, tid, "Not ready");
    }
  else if (cur->free_c2c_req)
    {
    KERR("ERROR %s", name);
    send_status_ko(llid, tid, "Not ready");
    }
  else if (strlen(cur->lan_attached))
    {
    KERR("WARNING LAN ALREADY ATTACHED %s %s", name, cur->lan_attached);
    send_status_ko(llid, tid, "Lan exists");
    }
  else if (strlen(cur->lan_added))
    {
    KERR("ERROR %s %s", name, cur->lan_added);
    send_status_ko(llid, tid, "Lan added exists");
    }
  else if (strlen(cur->lan_waiting))
    {
    KERR("ERROR %s %s", name, cur->lan_waiting);
    send_status_ko(llid, tid, "Lan waiting exists");
    }
  else if (cur->cli_llid || cur->cli_tid)
    {
    KERR("ERROR %s %s", name, cur->lan_waiting);
    send_status_ko(llid, tid, "Probleme past req");
    }
  else if (cur->topo.tcp_connection_peered != 1)
    {
    KERR("WARNING C2C TCP NOT PEERED %s", name);
    send_status_ko(llid, tid, "Not ready tcp_connection_peered");
    }
  else
    {
    strncpy(cur->lan_waiting, lan, MAX_NAME_LEN-1);
    cur->cli_llid = llid;
    cur->cli_tid = tid;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_del_lan(int llid, int tid, char *name, char *lan)
{
  t_ovs_c2c *cur = find_c2c(name);
  int val = 0;
  if (!cur)
    {
    KERR("ERROR %s", name);
    send_status_ko(llid, tid, "Does not exist");
    }
  else if (cur->destroy_c2c_req)
    {
    KERR("ERROR %s", name);
    send_status_ko(llid, tid, "Not ready");
    }
  else if (cur->free_c2c_req)
    {
    KERR("ERROR %s", name);
    send_status_ko(llid, tid, "Not ready");
    }
  else
    {
    if (!strlen(cur->lan_added))
      KERR("ERROR: %s %s", name, lan);
    else
      {
      mactopo_del_req(item_c2c, cur->name, 0, cur->lan_added);
      val = lan_del_name(cur->lan_added, item_c2c, cur->name, 0);
      memset(cur->lan_added, 0, MAX_NAME_LEN);
      }
    if (val == 2*MAX_LAN)
      {
      memset(cur->lan_attached, 0, MAX_NAME_LEN);
      memset(cur->lan_waiting, 0, MAX_NAME_LEN);
      memset(cur->topo.lan, 0, MAX_NAME_LEN);
      cfg_hysteresis_send_topo_info();
      send_status_ok(llid, tid, "OK");
      }
    else if (msg_send_del_lan_endp(ovsreq_del_c2c_lan, name, 0, cur->vhost, lan))
      send_status_ko(llid, tid, "ERROR");
    else
      send_status_ok(llid, tid, "OK");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_ovs_c2c *ovs_c2c_find_with_peer_llid(int llid)
{
  t_ovs_c2c *cur = g_head_c2c;
  while(cur)
    {
    if (llid == cur->peer_llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_ovs_c2c *ovs_c2c_find_with_peer_llid_connect(int llid)
{
  t_ovs_c2c *cur = g_head_c2c;
  while(cur)
    {
    if (llid == cur->peer_llid_connect)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_peer_conf(int llid, int tid, char *name, int peer_status,
                       char *dist,     char *loc,
                       uint32_t dist_udp_ip,   uint32_t loc_udp_ip,
                       uint16_t dist_udp_port, uint16_t loc_udp_port)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    KERR("ERROR %s %s", locnet, name);
  else if (tid != cur->ref_tid)
    KERR("ERROR %s %s %d %d", locnet, name, tid, cur->ref_tid);
  else if (peer_status == -1)
    KERR("ERROR %s %s", locnet, name);
  else
    {
    cur->topo.dist_udp_port = dist_udp_port;
    if (cur->topo.local_is_master == 0)
      {
      cur->topo.dist_udp_ip = dist_udp_ip;
      cur->topo.loc_udp_ip = loc_udp_ip;
      }
    cur->udp_dist_port_chosen = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_set_dist_udp_ip_port(char *name, uint32_t ip, uint16_t port)
{
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    KERR("%s %s", locnet, name);
  else
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1, "c2c_set_dist_udp_ip_port %s %x %hu",
             name, ip, port);
    if (!msg_exist_channel(cur->llid))
      KERR("ERROR %s", cur->name);
    else
      rpct_send_sigdiag_msg(cur->llid, type_hop_c2c, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void master_and_slave_ping_process(t_ovs_c2c *cur, int peer_status)
{
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  cur->peer_watchdog_count = 0;
  if ((cur->udp_connection_tx_configured == 0) &&
      (peer_status > 0) &&
      (cur->udp_loc_port_chosen == 1)  &&
      (cur->udp_dist_port_chosen == 1))
    {
    if ((cur->topo.dist_udp_ip == 0) || (cur->topo.dist_udp_port == 0))
      KERR("ERROR %s %s", locnet, cur->name);
    else
      c2c_set_dist_udp_ip_port(cur->name,
                               cur->topo.dist_udp_ip,
                               cur->topo.dist_udp_port);
    }
  else if ((cur->topo.local_is_master == 0) &&
           (cur->topo.udp_connection_peered == 0) &&
           (peer_status > 1))
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1, "c2c_enter_traffic_udp %s", cur->name);
    if (!msg_exist_channel(cur->llid))
      KERR("ERROR %s", cur->name);
    else
      rpct_send_sigdiag_msg(cur->llid, type_hop_c2c, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
  else if ((cur->topo.udp_connection_peered == 1) &&
           (peer_status > 1) &&
           (cur->state_up == state_up_initialised))
    {
    state_progress_up(cur, state_up_process_running);
    cfg_hysteresis_send_topo_info();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_send_probe_udp(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    KERR("%s %s", locnet, name);
  else
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1, "c2c_send_probe_udp %s", name);
    if (!msg_exist_channel(cur->llid))
      KERR("ERROR %s", cur->name);
    else
      rpct_send_sigdiag_msg(cur->llid, type_hop_c2c, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_peer_ping(int llid, int tid, char *name, int peer_status)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (cur)
    {
    if (tid != cur->ref_tid)
      {
      KERR("ERROR %s %s %d %d", locnet, name, tid, cur->ref_tid);
      carefull_destroy_c2c(cur->name);
      }
    else if (peer_status == -1)
      {
      KERR("ERROR %s %s", locnet, name);
      if (strlen(cur->lan_attached))
        strncpy(cur->must_restart_lan, cur->lan_attached, MAX_NAME_LEN-1);
      else if (strlen(cur->lan_added))
        strncpy(cur->must_restart_lan, cur->lan_added, MAX_NAME_LEN-1);
      else if (strlen(cur->lan_waiting))
        strncpy(cur->must_restart_lan, cur->lan_waiting, MAX_NAME_LEN-1);
      carefull_destroy_c2c(cur->name);
      if (cur->topo.local_is_master == 1)
        cur->must_restart = 1;
      }
    else
      {
      master_and_slave_ping_process(cur, peer_status);
      if (cur->topo.local_is_master == 1)
        {
        if ((cur->udp_connection_tx_configured) &&
            (cur->topo.udp_connection_peered == 0))
          {
          cur->udp_probe_qty_sent += 1;
          if (cur->udp_probe_qty_sent > 15)
            {
            KERR("ERROR %s %s %d", locnet, cur->name, cur->udp_probe_qty_sent);
            carefull_destroy_c2c(cur->name);
            }
          c2c_send_probe_udp(cur->name);
          }
        }
      else
        { 
        if (cur->topo.udp_connection_peered == 1)
          wrap_send_c2c_peer_ping(cur, 2);
        else if (cur->udp_loc_port_chosen == 1)
          wrap_send_c2c_peer_ping(cur, 1);
        else
          wrap_send_c2c_peer_ping(cur, 0);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/
     
/****************************************************************************/
int ovs_c2c_mac_mangle(char *name, uint8_t *mac)
{
  char msg[MAX_PATH_LEN];
  t_ovs_c2c *cur = find_c2c(name);
  int result = -1;
  if (!cur)
    KERR("ERROR %s", name);
  else
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,
    "c2c_mac_mangle %s %hhX:%hhX:%hhX:%hhX:%hhX:%hhX", cur->name,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (!msg_exist_channel(cur->llid))
      KERR("ERROR %s", cur->name);
    else
      rpct_send_sigdiag_msg(cur->llid, type_hop_c2c, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  ovs_c2c_dyn_snf(char *name, int val)
{
  int result = -1;
  t_ovs_c2c *cur = find_c2c(name);
  char tap[MAX_NAME_LEN];
  char *vh;

  if (val)
    {
    if (cur->topo.endp_type == endp_type_c2cs)
      KERR("ERROR %s", name);
    else
      {
      cur->del_snf_ethv_sent = 0;
      cur->topo.endp_type = endp_type_c2cs;
      ovs_dyn_snf_start_process(name, 0, item_type_c2c,
                                cur->vhost, snf_started);
      result = 0;
      }
    }
  else
    {
    if (cur->topo.endp_type == endp_type_c2cv)
      KERR("ERROR %s", name);
    else
      {
      cur->topo.endp_type = endp_type_c2cv;
      memset(tap, 0, MAX_NAME_LEN);
      vh = cur->vhost;
      snprintf(tap, MAX_NAME_LEN-1, "s%s", vh);
      if (cur->del_snf_ethv_sent == 0)
        {
        cur->del_snf_ethv_sent = 1;
        if (strlen(cur->lan_added))
          {
          if (ovs_snf_send_del_snf_lan(name, 0, cur->vhost, cur->lan_added))
            KERR("ERROR DEL KVMETH %s %s", name, cur->lan_added);
          }
        ovs_dyn_snf_stop_process(tap);
        }
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_init(void)
{
  char *root = cfg_get_root_work();
  char *net = cfg_get_cloonix_name();
  char *bin_c2c = utils_get_ovs_c2c_bin_dir();

  g_nb_c2c = 0;
  g_head_c2c = NULL;
  memset(g_cloonix_net, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_bin_c2c, 0, MAX_PATH_LEN);
  strncpy(g_cloonix_net, net, MAX_NAME_LEN-1);
  strncpy(g_root_path, root, MAX_PATH_LEN-1);
  strncpy(g_bin_c2c, bin_c2c, MAX_PATH_LEN-1);
  clownix_timeout_add(100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

