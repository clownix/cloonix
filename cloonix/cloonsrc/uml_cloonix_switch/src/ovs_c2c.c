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
#include "proxycrun.h"
#include "c2c_chainlan.h"

static char g_cloonix_net[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_bin_c2c[MAX_PATH_LEN];

static t_ovs_c2c *g_head_c2c;
static int g_nb_c2c;


/****************************************************************************/
static void ovs_llid_to_tap_process_broken(t_ovs_c2c *cur)
{
  if (cur->closed_llid_received == 0)
    {
    cur->closed_llid_received = 1;
    cur->ovs_llid = 0;
    cur->closed_count = 2;
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void nb_to_text(int state, char *resp)
{
  memset(resp, 0, MAX_NAME_LEN);
  switch (state)
    {
    case state_idle:
      strncpy(resp, "state_idle", MAX_NAME_LEN-1);
      break;

    case state_master_c2c_start_done:
      strncpy(resp, "state_master_c2c_start_done", MAX_NAME_LEN-1);
      break;

    case state_master_up_initialised:
      strncpy(resp, "state_master_up_initialised", MAX_NAME_LEN-1);
      break;

    case state_master_try_connect_to_peer:
      strncpy(resp, "state_master_try_connect_to_peer", MAX_NAME_LEN-1);
      break;

    case state_master_connection_peered:
      strncpy(resp, "state_master_connection_peered", MAX_NAME_LEN-1);
      break;

    case state_master_udp_peer_conf_sent:
      strncpy(resp, "state_master_udp_peer_conf_sent", MAX_NAME_LEN-1);
      break;

    case state_master_udp_peer_conf_received:
      strncpy(resp, "state_master_udp_peer_conf_received", MAX_NAME_LEN-1);
      break;

    case state_udp_connection_tx_configured:
      strncpy(resp, "state_udp_connection_tx_configured", MAX_NAME_LEN-1);
      break;

    case state_master_up_process_running:
      strncpy(resp, "state_master_up_process_running", MAX_NAME_LEN-1);
      break;

    case state_slave_up_initialised:
      strncpy(resp, "state_slave_up_initialised", MAX_NAME_LEN-1);
      break;

    case state_slave_connection_peered:
      strncpy(resp, "state_slave_connection_peered", MAX_NAME_LEN-1);
      break;

    case state_slave_udp_peer_conf_sent:
      strncpy(resp, "state_slave_udp_peer_conf_sent", MAX_NAME_LEN-1);
      break;

    case state_slave_udp_peer_conf_received:
      strncpy(resp, "state_slave_udp_peer_conf_received", MAX_NAME_LEN-1);
      break;

    case state_slave_up_process_running:
      strncpy(resp, "state_slave_up_process_running", MAX_NAME_LEN-1);
      break;

    default:
      KOUT("%d", state);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_state_progress_up(t_ovs_c2c *cur, int state)
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
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *ncur;
  cfg_free_obj_id(cur->c2c_id);
  layout_del_sat(cur->name);
  if (cur->ovs_llid)
    {
    llid_trace_free(cur->ovs_llid, 0, __FUNCTION__);
    cur->ovs_llid = 0;
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
    KERR("WARNING RESTARTING %s %s", locnet, cur->name);
    ovs_c2c_add(0,0, cur->name, cur->topo.loc_udp_ip, cur->topo.dist_cloon,
                cur->topo.dist_tcp_ip, cur->topo.dist_tcp_port,
                cur->dist_passwd, cur->topo.dist_udp_ip,
                cur->c2c_udp_port_low);
    ncur = find_c2c(cur->name);
    if (!ncur)
      KERR("ERROR %s %s", locnet, cur->name);
    }
  c2c_chainlan_partial_del(cur);
  if ((cur->topo.local_is_master == 1) && (cur->must_restart == 0))
    c2c_chainlan_del(cur);
  free(cur);
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void init_get_udp_port(t_ovs_c2c *cur)
{
  if ((cur->get_udp_port_done == 0) &&
      (cur->topo.tcp_connection_peered == 1) &&
      (msg_exist_channel(cur->ovs_llid)))
    {
    cur->get_udp_port_done = 1;
    proxycrun_transmit_req_udp(cur->name, cur->c2c_udp_port_low);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void init_dist_udp_ip_port(char *name, uint32_t ip, uint16_t port)
{
  proxycrun_transmit_dist_udp_ip_port(name, ip, port);
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
  if ((!name) || (!strlen(name)))
    KOUT("ERROR");
  if ((!dist_name) || (!strlen(dist_name)))
    KOUT("ERROR");
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
  c2c_chainlan_add(cur);
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_c2c_master(char *name, char *dist_name,
                             uint32_t loc_udp_ip, uint32_t dist_ip,
                             uint16_t dist_port, char *dist_passwd,
                             uint32_t dist_udp_ip, uint16_t c2c_udp_port_low)
{
  char *locnet = cfg_get_cloonix_name();
  uint32_t localhost;
  t_ovs_c2c *cur = alloc_c2c_base(name, dist_name);
  memset(cur->dist_passwd, 0, MSG_DIGEST_LEN);
  strncpy(cur->dist_passwd, dist_passwd, MSG_DIGEST_LEN-1);
  cur->topo.local_is_master = 1;
  cur->topo.dist_tcp_ip = dist_ip;
  cur->topo.dist_tcp_port = dist_port;
  cur->topo.loc_udp_ip = loc_udp_ip;
  cur->topo.dist_udp_ip = dist_udp_ip;
  cur->c2c_udp_port_low = c2c_udp_port_low;
  ip_string_to_int(&localhost, "127.0.0.1");
  if ((dist_ip != localhost) && (loc_udp_ip == localhost))
    {
    KERR("WARNING: %s distant c2c has the local 127.0.0.1", locnet);
    KERR("WARNING: distant c2c probably will not be able to reach us");
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_ovs_c2c *alloc_c2c_peer(int llid, int tid, char *name,
                                 char *dist_name, uint16_t c2c_udp_port_low)
{
  t_ovs_c2c *cur = alloc_c2c_base(name, dist_name);
  cur->c2c_udp_port_low = (int) c2c_udp_port_low;
  init_get_udp_port(cur);
  cur->peer_llid = llid;
  cur->ref_tid = tid;
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void carefull_free_c2c(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    KOUT("ERROR %s %s", locnet, name);
  if ((cur->destroy_c2c_req != 0) ||
      (cur->free_c2c_req != 0))
    KERR("ERROR %s %s %d %d", locnet, name, cur->destroy_c2c_req,
                              cur->free_c2c_req);
  else
    free_c2c(cur);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void carefull_destroy_c2c(t_ovs_c2c *cur)
{
  char *locnet = cfg_get_cloonix_name();
  if ((cur->destroy_c2c_req != 0) ||
      (cur->free_c2c_req != 0))
    KERR("ERROR %s %s %d %d", locnet, cur->name,
                              cur->destroy_c2c_req,
                              cur->free_c2c_req);
  else if (cur->destroy_c2c_done == 0)
    {
    cur->destroy_c2c_done = 1;

    wrap_send_c2c_peer_ping(cur, status_peer_ping_error);
    if ((cur->ovs_llid) && msg_exist_channel(cur->ovs_llid))
      rpct_send_kil_req(cur->ovs_llid, type_hop_c2c);
    }
  if ((!cur->recv_delete_req_from_client) &&
      (cur->topo.local_is_master == 1))
    cur->must_restart = 1;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void c2c_start(char *name, char *vhost, char *type)
{
  static char *argv[7];
  argv[0] = g_bin_c2c;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = name;
  argv[4] = vhost;
  argv[5] = type;
  argv[6] = NULL;
  pid_clone_launch(utils_execv, process_demonized, NULL,
                   (void *) argv, NULL, NULL, name, -1, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int try_connect(char *socket, char *name)
{
  char *locnet = cfg_get_cloonix_name();
  int llid = string_client_unix(socket, uml_clownix_switch_error_cb,
                                uml_clownix_switch_rx_cb, name);
  if (llid)
    {
    if (hop_event_alloc(llid, type_hop_c2c, name, 0))
      KERR("ERROR %s", locnet);
    llid_trace_alloc(llid, name, 0, 0, type_llid_trace_endp_ovsdb);
    rpct_send_pid_req(llid, type_hop_c2c, name, 0);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_ovs_c2c_attempt(t_ovs_c2c *cur)
{
  char *locnet = cfg_get_cloonix_name();
  int llid = try_connect(cur->socket, cur->name);
  if (llid)
    {
    cur->ovs_llid = llid;
    init_get_udp_port(cur);
    cur->count = 0;
    }
  else
    {
    cur->count += 1;
    if (cur->count == 20)
      {
      KERR("ERROR %s %s %s", locnet, cur->socket, cur->name);
      free_c2c(cur);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_c2c_suidroot_attempt(t_ovs_c2c *cur)
{
  char *locnet = cfg_get_cloonix_name();
  char *msg = "c2c_suidroot";
  if (!msg_exist_channel(cur->ovs_llid))
    free_c2c(cur);
  else
    {
    cur->count += 1;
    if (cur->count == 20)
      {
      KERR("ERROR %s %s %s", locnet, cur->socket, cur->name);
      free_c2c(cur);
      }
    else
      {
      rpct_send_sigdiag_msg(cur->ovs_llid, type_hop_c2c, msg);
      hop_event_hook(cur->ovs_llid, FLAG_HOP_SIGDIAG, msg);
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
    if ((cur->ovs_llid) && (msg_exist_channel(cur->ovs_llid)))
      rpct_send_pid_req(cur->ovs_llid, type_hop_c2c, cur->name, 0);
    cur->count = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(void *data)
{
  char returned_attlan[MAX_NAME_LEN];
  char *locnet = cfg_get_cloonix_name();
  char label[MAX_NAME_LEN];
  char *name;
  int must_go_fast = 0;
  t_ovs_c2c *next, *cur = g_head_c2c;

  while(cur)
    {
    next = cur->next;
    nb_to_text(cur->state_up, label);
    name = cur->name;

    if (cur->closed_count > 0)
      { 
      cur->closed_count -= 1;
      if (cur->closed_count == 0)
        {
        KERR("WARNING FREE %s %s", locnet, cur->name);
        carefull_destroy_c2c(cur);
        }
      }

    if ((cur->topo.udp_connection_peered == 1) &&
        (cur->topo.local_is_master == 1))
      {
      cur->probe_watchdog_count += 1;
      if (cur->probe_watchdog_count > 5)
        {
        KERR("ERROR PROBE %s %s", locnet, cur->name);
        carefull_destroy_c2c(cur);
        }
      }
    if ((cur->state_up == state_master_up_process_running) ||
        (cur->state_up == state_slave_up_process_running))
      {
      cur->peer_watchdog_count += 1;
      if (cur->peer_watchdog_count > 5)
        {
        KERR("ERROR PEER %s %s", locnet, cur->name);
        carefull_destroy_c2c(cur);
        }
      }

    if (cur->destroy_c2c_done)
      {
      free_c2c(cur);
      }
    else if ((cur->state_up == state_master_c2c_start_done) ||
             (cur->state_up == state_slave_up_initialised))
      {
      if (cur->state_up == state_master_c2c_start_done)
        {
        c2c_state_progress_up(cur, state_master_up_initialised);
        }
      else if (cur->state_up == state_slave_up_initialised)
        {
        cur->topo.tcp_connection_peered = 1;
        c2c_state_progress_up(cur, state_slave_connection_peered);
        wrap_send_c2c_peer_create(cur, 1);
        }
      else
        KOUT("ERROR %s %s %s", locnet, name, label);
      cfg_hysteresis_send_topo_info();
      }
    else if ((cur->ovs_llid == 0) && (!cur->closed_llid_received))
      connect_ovs_c2c_attempt(cur);
    else if (cur->suid_root_done == 0)
      send_c2c_suidroot_attempt(cur);
    else if (!msg_exist_channel(cur->ovs_llid))
      carefull_free_c2c(name);
    else
      {
      count_management_and_pid_req(cur);
      if (cur->topo.local_is_master == 1)
        {
        if ((cur->peer_llid == 0) && (cur->pair_llid == 0))
          wrap_try_connect_to_peer(cur);
        else if (cur->topo.tcp_connection_peered == 0)
          wrap_send_c2c_peer_create(cur, 0);
        }
      if (cur->topo.udp_connection_peered == 1)
        wrap_send_c2c_peer_ping(cur, status_peer_ping_two);
      else if (cur->udp_loc_port_chosen == 1)
        wrap_send_c2c_peer_ping(cur, status_peer_ping_one);
      else
        wrap_send_c2c_peer_ping(cur, status_peer_ping_zero);
      if ((cur->udp_loc_port_chosen == 1) &&
          (cur->peer_conf_done == 0))
        {
        if (!wrap_send_c2c_peer_conf(cur, 0))
          cur->peer_conf_done = 1;
        }
      }

    if (c2c_chainlan_action_heartbeat(cur, returned_attlan))
      {
      if (strlen(returned_attlan))
        strncpy(cur->topo.attlan, returned_attlan, MAX_NAME_LEN);
      else
        memset(cur->topo.attlan, 0, MAX_NAME_LEN);
      cfg_hysteresis_send_topo_info();
      }

    if ((cur->state_up != state_master_up_process_running) &&
        (cur->state_up != state_slave_up_process_running))
      {
      must_go_fast = 1;
      switch(cur->state_up)
        {
      
#define MAX_REPEAT1 10
#define MAX_REPEAT2 15

        case state_master_c2c_start_done:
          cur->count_state_master_c2c_start_done += 1;
          if (cur->count_state_master_c2c_start_done > MAX_REPEAT1)
            KERR("WARNING %s %s %s", locnet, name, label); 
          if (cur->count_state_master_c2c_start_done > MAX_REPEAT2)
            carefull_destroy_c2c(cur);
        break;
        case state_master_up_initialised:
          cur->count_state_master_up_initialised += 1;
          if (cur->count_state_master_up_initialised > MAX_REPEAT1)
            KERR("WARNING %s %s %s", locnet, name, label); 
          if (cur->count_state_master_up_initialised > MAX_REPEAT2)
            carefull_destroy_c2c(cur);
        break;
        case state_master_try_connect_to_peer:
          cur->count_state_master_try_connect_to_peer += 1;
          if (cur->count_state_master_try_connect_to_peer > MAX_REPEAT2)
            {
            KERR("WARNING CONNECT PEER  %s %s %s", locnet, name, label); 
            cur->count_state_master_try_connect_to_peer = 0;
            }
        break;
        case state_master_connection_peered:
          cur->count_state_master_connection_peered += 1;
          if (cur->count_state_master_connection_peered > MAX_REPEAT1)
            KERR("WARNING %s %s %s", locnet, name, label); 
          if (cur->count_state_master_connection_peered > MAX_REPEAT2)
            carefull_destroy_c2c(cur);
        break;
        case state_master_udp_peer_conf_sent:
          cur->count_state_master_udp_peer_conf_sent += 1;
          if (cur->count_state_master_udp_peer_conf_sent > MAX_REPEAT1)
            KERR("WARNING %s %s %s", locnet, name, label); 
          if (cur->count_state_master_udp_peer_conf_sent > MAX_REPEAT2)
            carefull_destroy_c2c(cur);
        break;
        case state_master_udp_peer_conf_received:
          cur->count_state_master_udp_peer_conf_received += 1;
          if (cur->count_state_master_udp_peer_conf_received > MAX_REPEAT1)
            KERR("WARNING %s %s %s", locnet, name, label); 
          if (cur->count_state_master_udp_peer_conf_received > MAX_REPEAT2)
            carefull_destroy_c2c(cur);
        break;

        case state_udp_connection_tx_configured:
          cur->count_state_udp_connection_tx_configured += 1;
          if (cur->count_state_udp_connection_tx_configured > MAX_REPEAT1)
            KERR("WARNING %s %s %s", locnet, name, label); 
          if (cur->count_state_udp_connection_tx_configured > MAX_REPEAT2)
            carefull_destroy_c2c(cur);
        break;

        case state_slave_up_initialised:
          cur->count_state_slave_up_initialised += 1;
          if (cur->count_state_slave_up_initialised > MAX_REPEAT1)
            KERR("WARNING %s %s %s", locnet, name, label); 
          if (cur->count_state_slave_up_initialised > MAX_REPEAT2)
            carefull_destroy_c2c(cur);
        break;
        case state_slave_connection_peered:
          cur->count_state_slave_connection_peered += 1;
          if (cur->count_state_slave_connection_peered > MAX_REPEAT1)
            KERR("WARNING %s %s %s", locnet, name, label); 
          if (cur->count_state_slave_connection_peered > MAX_REPEAT2)
            carefull_destroy_c2c(cur);
        break;
        case state_slave_udp_peer_conf_sent:
          cur->count_state_slave_udp_peer_conf_sent += 1;
          if (cur->count_state_slave_udp_peer_conf_sent > MAX_REPEAT1)
            KERR("WARNING %s %s %s", locnet, name, label); 
          if (cur->count_state_slave_udp_peer_conf_sent > MAX_REPEAT2)
            carefull_destroy_c2c(cur);
        break;
        case state_slave_udp_peer_conf_received:
          cur->count_state_slave_udp_peer_conf_received += 1;
          if (cur->count_state_slave_udp_peer_conf_received > MAX_REPEAT1)
            KERR("WARNING %s %s %s", locnet, name, label); 
          if (cur->count_state_slave_udp_peer_conf_received > MAX_REPEAT2)
            carefull_destroy_c2c(cur);
        break;

        default:
          KOUT("ERROR %d", cur->state_up); 
        }
      }


    cur = next;
    }
  if (must_go_fast)
    clownix_timeout_add(50, timer_heartbeat, NULL, NULL, NULL);
  else
    clownix_timeout_add(150, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_receive_probe_udp(char *name, uint8_t probe_idx)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  char msg[MAX_PATH_LEN];
  char dist_udp_ip_string[MAX_NAME_LEN];
  if (!cur)
    KERR("ERROR %s %s %hhu", locnet, name, probe_idx);
  else
    {
    cur->probe_watchdog_count = 0;
    if (cur->topo.local_is_master == 0)
      {
      int_to_ip_string (cur->topo.dist_udp_ip, dist_udp_ip_string);
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1,
               "c2c_send_probe_udp %s ip %s port %hu probe_idx %hhu",
               name, dist_udp_ip_string, cur->topo.dist_udp_port, probe_idx);
      if (!msg_exist_channel(cur->ovs_llid))
        {
        KERR("ERROR %s %s %hhu", locnet, cur->name, probe_idx);
        ovs_llid_to_tap_process_broken(cur);
        }
      else
        {
        rpct_send_sigdiag_msg(cur->ovs_llid, type_hop_c2c, msg);
        hop_event_hook(cur->ovs_llid, FLAG_HOP_SIGDIAG, msg);
        }
      }
    else
      {
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1,
               "c2c_enter_traffic_udp %s probe_idx %hhu", name, probe_idx);
      if (!msg_exist_channel(cur->ovs_llid))
        {
        KERR("ERROR %s %s %hhu", locnet, cur->name, probe_idx);
        ovs_llid_to_tap_process_broken(cur);
        }
      else
        {
        rpct_send_sigdiag_msg(cur->ovs_llid, type_hop_c2c, msg);
        hop_event_hook(cur->ovs_llid, FLAG_HOP_SIGDIAG, msg);
        }
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
    c2c_state_progress_up(cur, state_udp_connection_tx_configured);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_udp_port_done(char *name, uint16_t port, int peer_status)
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

/****************************************************************************/
void ovs_c2c_peer_add(int llid, int tid, char *name, char *dist,
                      char *loc, uint16_t c2c_udp_port_low)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (cur)
    KERR("ERROR %s %s %s", locnet, name, dist);
  else if (strcmp(loc, locnet))
    KERR("ERROR %s %s %s %s", locnet, name, dist, loc);
  else
    {
    cur = alloc_c2c_peer(llid, tid, name, dist, c2c_udp_port_low);
    c2c_start(name, cur->vhost, "slave");
    c2c_state_progress_up(cur, state_slave_up_initialised);
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
    c2c_state_progress_up(cur, state_master_connection_peered);
    init_get_udp_port(cur);
    cfg_hysteresis_send_topo_info();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_transmit_get_free_udp_port(char *name, uint16_t udp_port)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  char msg[MAX_PATH_LEN];
  memset(msg, 0, MAX_PATH_LEN);
  if (!cur)
    KERR("ERROR %s %s %hu", locnet, name, udp_port);
  else
    {
    snprintf(msg, MAX_PATH_LEN-1,
             "c2c_get_free_udp_port %s proxycrun %hu", name, udp_port);
    if (!msg_exist_channel(cur->ovs_llid))
      KERR("ERROR %s %s %hu", locnet, cur->name, udp_port);
    else
      {
      rpct_send_sigdiag_msg(cur->ovs_llid, type_hop_c2c, msg);
      hop_event_hook(cur->ovs_llid, FLAG_HOP_SIGDIAG, msg);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_pid_resp(int llid, char *name, int pid)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    KERR("ERROR %s %s %d", locnet, name, pid);
  else
    {
    if ((cur->pid == 0) && (cur->suid_root_done == 1))
      {
      layout_add_sat(name, 0, 0);
      cur->pid = pid;
      }
    else if (cur->pid && (cur->pid != pid))
      KERR("ERROR %s %s %d", locnet, name, pid);
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
    if (cur->ovs_llid == llid)
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
  char *locnet = cfg_get_cloonix_name();
  KERR("ERROR %s %s", locnet, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_proxy_dist_udp_ip_port_OK(char *name,
                                       uint32_t ip, uint16_t udp_port)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  cur = find_c2c(name);
  if (!cur)
    KERR("ERROR c2c: %s %s", locnet, name);
  else
    c2c_dist_udp_ip_port_done(name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_proxy_dist_tcp_ip_port_OK(char *name,
                                       uint32_t ip, uint16_t tcp_port)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  cur = find_c2c(name);
  if (!cur)
    KERR("ERROR c2c: %s", locnet);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_sigdiag_resp(int llid, int tid, char *line)
{
  char *locnet = cfg_get_cloonix_name();
  char name[MAX_NAME_LEN];
  uint16_t udp_port;
  t_ovs_c2c *cur;
  uint8_t probe_idx;
  if (sscanf(line,
  "c2c_suidroot_ko %s", name) == 1)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", locnet, line);
    else
      KERR("ERROR Started c2c: %s %s", locnet, name);
    }
  else if (sscanf(line,
  "c2c_suidroot_ok %s", name) == 1)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", locnet, line);
    else
      {
      cur->suid_root_done = 1;
      cur->count = 0;
      }
    }
  else if (sscanf(line,
  "c2c_get_free_udp_port_ko %s", name) == 1)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", locnet, line);
    else
      free_udp_port_done(name, 0, -1);
    }
  else if (sscanf(line,
  "c2c_get_free_udp_port_ok %s udp_port=%hu", name, &udp_port) == 2)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", locnet, line);
    else
      free_udp_port_done(name, udp_port, 0);
    }
  else if (sscanf(line,
  "c2c_set_dist_udp_ip_port_ok %s", name) == 1)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", locnet, line);
    else
      c2c_dist_udp_ip_port_done(name);
    }
  else if (sscanf(line,
  "c2c_receive_probe_udp %s %hhu", name, &probe_idx) == 2)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s %hhu", locnet, line, probe_idx);
    else
      c2c_receive_probe_udp(name, probe_idx);
    }
  else if (sscanf(line,
  "c2c_enter_traffic_udp_ack %s", name) == 1)
    {
    cur = find_c2c(name);
    if (!cur)
      KERR("ERROR c2c: %s %s", locnet, line);
    else
      cur->topo.udp_connection_peered = 1;
    }

  else
    KERR("ERROR c2c: %s %s", locnet, line);
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
void ovs_c2c_llid_closed(int llid, int from_clone)
{
  t_ovs_c2c *cur = g_head_c2c;
  if (!from_clone)
    {
    while(cur)
      {
      if (cur->ovs_llid == llid)
        ovs_llid_to_tap_process_broken(cur);
      cur = cur->next;
      }
    }
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
    if (strlen(cur->topo.attlan))
      count += 1;
    cur = cur->next;
    }
  endp = (t_topo_endp *) clownix_malloc(count * sizeof(t_topo_endp), 13);
  memset(endp, 0, count * sizeof(t_topo_endp));
  cur = g_head_c2c;
  while(cur)
    {
    if (strlen(cur->topo.attlan))
      {
      len = sizeof(t_lan_group_item);
      endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
      memset(endp[nb_endp].lan.lan, 0, len);
      strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
      endp[nb_endp].num = 0;
      endp[nb_endp].type = cur->topo.endp_type;
      strncpy(endp[nb_endp].lan.lan[0].lan, cur->topo.attlan, MAX_NAME_LEN-1);
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
                 char *dist_passwd, uint32_t dist_udp_ip,
                 uint16_t c2c_udp_port_low)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (cur)
    {
    KERR("ERROR %s %s", locnet, name);
    if (llid)
      send_status_ko(llid, tid, "Exists already");
    }
  else if (!strcmp(locnet, dist_name))
    {
    KERR("ERROR %s %s", locnet, name);
    if (llid)
      send_status_ko(llid, tid, "Cannot c2c to our own name");
    }
  else
    {
    alloc_c2c_master(name, dist_name, loc_udp_ip, dist_ip, dist_port,
                     dist_passwd, dist_udp_ip, c2c_udp_port_low);
    cur = find_c2c(name);
    if (!cur)
      KOUT(" ");
    if (llid)
      send_status_ok(llid, tid, "OK");
    c2c_start(name, cur->vhost, "master");
    c2c_state_progress_up(cur, state_master_c2c_start_done);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_c2c_del(int llid, int tid, char *name)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    {
    KERR("ERROR %s %s", locnet, name);
    if (llid)
      send_status_ko(llid, tid, "Does not exist");
    }
  else if (cur->destroy_c2c_req)
    {
    KERR("ERROR %s %s", locnet, name);
    if (llid)
      send_status_ko(llid, tid, "Not ready");
    }
  else if (cur->free_c2c_req)
    {
    KERR("ERROR %s %s", locnet, name);
    if (llid)
      send_status_ko(llid, tid, "Not ready");
    }
  else if (!cur->topo.local_is_master)
    {
    if (llid)
      {
      KERR("ERROR %s %s Slave, cannot be deleted", locnet, name);
      send_status_ko(llid, tid, "Slave, cannot be deleted"); 
      }
    else if (tid == 7777)
      {
      cur->recv_delete_req_from_client = 1;
      carefull_destroy_c2c(cur);
      }
    } 
  else
    {
    if (llid)
      send_status_ok(llid, tid, "OK");
    cur->recv_delete_req_from_client = 1;
    carefull_destroy_c2c(cur);
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
t_ovs_c2c *ovs_c2c_find_with_pair_llid(int llid)
{
  t_ovs_c2c *cur = g_head_c2c;
  while(cur)
    {
    if (llid == cur->pair_llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_ovs_c2c *ovs_c2c_find_with_peer_listen_llid(int llid)
{
  t_ovs_c2c *cur = g_head_c2c;
  while(cur)
    {
    if (llid == cur->peer_listen_llid)
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
      c2c_state_progress_up(cur, state_slave_udp_peer_conf_received);
      }
    else
      c2c_state_progress_up(cur, state_master_udp_peer_conf_received);
    cur->udp_dist_port_chosen = 1;
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
  char dist_udp_ip_string[MAX_NAME_LEN];
  if (!cur)
    KERR("ERROR %s %s", locnet, name);
  else
    {
    int_to_ip_string (cur->topo.dist_udp_ip, dist_udp_ip_string);
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,
             "c2c_send_probe_udp %s ip %s port %hu probe_idx 0",
             name, dist_udp_ip_string, cur->topo.dist_udp_port);
    if (!msg_exist_channel(cur->ovs_llid))
      {
      KERR("ERROR %s %s", locnet, cur->name);
      carefull_destroy_c2c(cur);
      }
    else
      {
      rpct_send_sigdiag_msg(cur->ovs_llid, type_hop_c2c, msg);
      hop_event_hook(cur->ovs_llid, FLAG_HOP_SIGDIAG, msg);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void master_and_slave_ping_process(t_ovs_c2c *cur, int peer_status)
{
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  char *name = cur->name;
  uint32_t ip = cur->topo.dist_udp_ip;
  uint16_t port = cur->topo.dist_udp_port;
  char label[MAX_NAME_LEN];

  nb_to_text(cur->state_up, label);
  cur->peer_watchdog_count = 0;
  if ((cur->udp_connection_tx_configured == 0) &&
      (peer_status > 0) &&
      (cur->udp_loc_port_chosen == 1)  &&
      (cur->udp_dist_port_chosen == 1))
    {
    if ((ip == 0) || (port == 0))
      KERR("ERROR %s %s %s", locnet, cur->name, label);
    else
      init_dist_udp_ip_port(name, ip, port);
    }
  else if ((cur->topo.local_is_master == 0) &&
           (cur->topo.udp_connection_peered == 0) &&
           (peer_status > 1))
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,
             "c2c_enter_traffic_udp %s probe_idx 0", cur->name);
    if (!msg_exist_channel(cur->ovs_llid))
      {
      KERR("ERROR %s %s %s", locnet, cur->name, label);
      carefull_destroy_c2c(cur);
      }
    else
      {
      rpct_send_sigdiag_msg(cur->ovs_llid, type_hop_c2c, msg);
      hop_event_hook(cur->ovs_llid, FLAG_HOP_SIGDIAG, msg);
      }
    }
  else if ((cur->topo.udp_connection_peered == 1) &&
           (peer_status > 1))
    {
    if ((cur->state_up != state_master_up_process_running) &&
        (cur->state_up != state_slave_up_process_running))
      {
      if ((cur->state_up == state_master_udp_peer_conf_received) ||
          (cur->state_up == state_udp_connection_tx_configured))
        c2c_state_progress_up(cur, state_master_up_process_running);
      else if (cur->state_up == state_slave_udp_peer_conf_sent)
        c2c_state_progress_up(cur, state_slave_up_process_running);
      else if (cur->state_up == state_master_udp_peer_conf_sent)
        { }
      else
        KERR("ERROR %s %s %s", locnet, cur->name, label);
      cfg_hysteresis_send_topo_info();
      }
    }
  if (cur->udp_connection_tx_configured)
    c2c_send_probe_udp(cur->name);
  if (cur->topo.local_is_master == 0)
    {
    if (cur->topo.udp_connection_peered == 1)
      wrap_send_c2c_peer_ping(cur, status_peer_ping_two);
    else if (cur->udp_loc_port_chosen == 1)
      wrap_send_c2c_peer_ping(cur, status_peer_ping_one);
    else
      wrap_send_c2c_peer_ping(cur, status_peer_ping_zero);
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
      carefull_destroy_c2c(cur);
      }
    else if (peer_status == -1)
      {
      if ((cur->topo.local_is_master == 1) &&
          (cur->recv_delete_req_from_client))
        KERR("WARNING %s %s peer is down, delete req", locnet, name);
      else
        {
        KERR("ERROR %s %s", locnet, name);
        carefull_destroy_c2c(cur);
        }
      }
    else
      {
      master_and_slave_ping_process(cur, peer_status);
      }
    }
}
/*--------------------------------------------------------------------------*/
     
/****************************************************************************/
int ovs_c2c_mac_mangle(char *name, uint8_t *mac)
{
  return c2c_chainlan_store_cmd(name, cmd_mac_mangle, mac);
  char *locnet = cfg_get_cloonix_name();
  char msg[MAX_PATH_LEN];
  t_ovs_c2c *cur = find_c2c(name);
  int result = -1;
  if (!cur)
    KERR("ERROR %s %s", locnet,  name);
  else
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,
    "c2c_mac_mangle %s %02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX", cur->name,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    if (!msg_exist_channel(cur->ovs_llid))
      {
      KERR("ERROR %s %s", locnet, cur->name);
      carefull_destroy_c2c(cur);
      }
    else
      {
      rpct_send_sigdiag_msg(cur->ovs_llid, type_hop_c2c, msg);
      hop_event_hook(cur->ovs_llid, FLAG_HOP_SIGDIAG, msg);
      }
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  ovs_c2c_dyn_snf(char *name, int val)
{
  int result = -1;
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  char tap[MAX_NAME_LEN];
  char *vh;

  if (val)
    {
    if (cur->topo.endp_type == endp_type_c2cs)
      KERR("ERROR %s %s", locnet, name);
    else
      {
      cur->del_snf_ethv_sent = 0;
      cur->topo.endp_type = endp_type_c2cs;
      ovs_dyn_snf_start_process(name, 0, item_type_c2c,
                                cur->vhost, c2c_chainlan_snf_started);
      c2c_chainlan_update_endp_type(name, endp_type_c2cs);
      result = 0;
      }
    }
  else
    {
    if (cur->topo.endp_type == endp_type_c2cv)
      KERR("ERROR %s %s", locnet, name);
    else
      {
      cur->topo.endp_type = endp_type_c2cv;
      memset(tap, 0, MAX_NAME_LEN);
      vh = cur->vhost;
      snprintf(tap, MAX_NAME_LEN-1, "s%s", vh);
      if (cur->del_snf_ethv_sent == 0)
        {
        cur->del_snf_ethv_sent = 1;
        if (strlen(cur->topo.attlan))
          {
          if (ovs_snf_send_del_snf_lan(name, 0, cur->vhost, cur->topo.attlan))
            KERR("ERROR DEL KVMETH %s %s %s", locnet, name, cur->topo.attlan);
          }
        ovs_dyn_snf_stop_process(tap);
        }
      c2c_chainlan_update_endp_type(name, endp_type_c2cv);
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

  c2c_chainlan_init();
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

