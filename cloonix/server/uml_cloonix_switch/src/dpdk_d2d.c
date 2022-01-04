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
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "dpdk_d2d.h"
#include "suid_power.h"
#include "dpdk_d2d_peer.h"
#include "event_subscriber.h"
#include "d2d_dpdk_process.h"
#include "dpdk_msg.h"
#include "utils_cmd_line_maker.h"
#include "stats_counters.h"

enum {
  state_idle = 0,
  state_up_initialised,
  state_up_process_started,
  state_up_process_eal_init,
  state_up_process_vhost_req_on,
  state_up_process_vhost_resp_on,
  state_up_lan_ovs_req,
  state_up_lan_ovs_resp,
  state_down_initialised,
  state_down_vhost_stopped,
  state_down_ovs_req_del_lan,
  state_down_ovs_resp_del_lan,
};

int get_glob_req_self_destruction(void);


static t_d2d_cnx *g_head_d2d;
static int g_nb_d2d;

/****************************************************************************/
static char *nb_to_text_raw(int state)
{
  char *result;
  switch (state)
    {
    case state_idle:
      result = "state_idle";
      break;
    case state_up_initialised:
      result = "state_up_initialised";
      break;
    case state_up_process_started:
      result = "state_up_process_started";
      break;
    case state_up_process_eal_init:
      result = "state_up_process_eal_init";
      break;
    case state_up_process_vhost_req_on:
      result = "state_up_process_vhost_req_on";
      break;
    case state_up_process_vhost_resp_on:
      result = "state_up_process_vhost_resp_on";
      break;
    case state_up_lan_ovs_req:
      result = "state_up_lan_ovs_req";
      break;
    case state_up_lan_ovs_resp:
      result = "state_up_lan_ovs_resp";
      break;
    case state_down_initialised:
      result = "state_down_initialised";
      break;
    case state_down_vhost_stopped:
      result = "state_down_vhost_stopped";
      break;
    case state_down_ovs_req_del_lan:
      result = "state_down_ovs_req_del_lan";
      break;
    case state_down_ovs_resp_del_lan:
      result = "state_down_ovs_resp_del_lan";
      break;
    default:
      KOUT("%d", state);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void nb_to_text(int state, char *label)
{
  char *txt = nb_to_text_raw(state);
  memset(label, 0, MAX_NAME_LEN);
  strncpy(label, txt, MAX_NAME_LEN-1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void state_progress_up(t_d2d_cnx *cur, int state)
{
//char *locnet = cfg_get_cloonix_name();
  char olab[MAX_NAME_LEN];
  char nlab[MAX_NAME_LEN];
  nb_to_text(cur->state_up, olab);
  cur->state_up = state;
  nb_to_text(cur->state_up, nlab);
//KERR("%s %s  %s ----> %s", locnet, cur->name, olab, nlab);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void state_progress_down(t_d2d_cnx *cur, int state)
{
//char *locnet = cfg_get_cloonix_name();
  char olab[MAX_NAME_LEN];
  char nlab[MAX_NAME_LEN];
  nb_to_text(cur->state_down, olab);
  cur->state_down = state;
  nb_to_text(cur->state_down, nlab);
//KERR("%s %s  %s ----> %s", locnet, cur->name, olab, nlab);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_d2d_get_qty(void)
{
  return g_nb_d2d;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void transmit_peer_ping(t_d2d_cnx *cur)
{
  if (cur->udp_connection_peered == 1)
    wrap_send_d2d_peer_ping(cur, 2);
  else if (cur->udp_loc_port_chosen == 1)
    wrap_send_d2d_peer_ping(cur, 1);
  else
    wrap_send_d2d_peer_ping(cur, 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_d2d_cnx *alloc_d2d(char *name, char *dist)
{
  t_d2d_cnx *cur = (t_d2d_cnx *)clownix_malloc(sizeof(t_d2d_cnx), 5);
  memset(cur, 0, sizeof(t_d2d_cnx));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->dist_cloonix, dist, MAX_NAME_LEN-1);
  if (g_head_d2d)
    g_head_d2d->prev = cur;
  cur->next = g_head_d2d;
  g_head_d2d = cur;
  g_nb_d2d += 1;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_d2d(t_d2d_cnx *cur)
{
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  memset(name, 0, MAX_NAME_LEN);
  memset(lan, 0, MAX_NAME_LEN);
  strncpy(name, cur->name, MAX_NAME_LEN-1);
  if (strlen(cur->lan))
    strncpy(lan, cur->lan, MAX_NAME_LEN-1);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_d2d)
    g_head_d2d = cur->next;
  clownix_free(cur, __FUNCTION__);
  g_nb_d2d -= 1;
  if (strlen(lan))
    {
    dpdk_msg_vlan_exist_no_more(lan);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dpdk_d2d_reset_con_params(t_d2d_cnx *cur)
{
  char *locnet = cfg_get_cloonix_name();
  if (cur->peer_llid)
    KERR("ERROR %s %s", locnet, cur->name);
  if (cur->received_del_lan_req)
    KERR("ERROR %s %s", locnet, cur->name);
  if (cur->peer_llid_connect)
    KERR("ERROR %s %s", locnet, cur->name);
  if (cur->lan_add_cli_llid)
    KERR("ERROR %s %s", locnet, cur->name);
  if (cur->lan_del_cli_llid)
    KERR("ERROR %s %s", locnet, cur->name);
  cur->loc_udp_port = 0;
  cur->dist_udp_port = 0;
  cur->lan_ovs_is_attached = 0; 
  cur->udp_connection_probed = 0;
  cur->udp_connection_peered = 0;
  cur->udp_dist_port_chosen  = 0;
  cur->udp_loc_port_chosen   = 0;
  cur->udp_connection_tx_configured = 0;
  cur->udp_probe_qty_sent    = 0;
  cur->peer_bad_status_received = 0;
  cur->watchdog_count        = 0;
  cur->vhost_started_and_running = 0;
  cur->lan_add_cli_llid      = 0;
  cur->lan_add_cli_tid       = 0;
  cur->lan_del_cli_llid      = 0;
  cur->lan_del_cli_tid       = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_peer_bad_status_received(t_d2d_cnx *cur)
{
  char *locnet = cfg_get_cloonix_name();
  if ((cur->waiting_ack_del_lan == 0) &&
      (cur->waiting_ack_add_lan == 0) &&
      (cur->lan_ovs_is_attached == 0) &&
      (cur->tcp_connection_peered == 0))
    {
    KERR("ERROR %s %s PEER BAD STATUS RECEIVED", locnet, cur->name);
    wrap_disconnect_to_peer(cur);
    if ((cur->local_is_master == 0) ||
        (cur->master_del_req == 1))
      {
      free_d2d(cur);
      }
    else
      {
      dpdk_d2d_reset_con_params(cur);
      }
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
  else
    {
    if ((cur->waiting_ack_del_lan != 0) ||
        (cur->waiting_ack_add_lan != 0))
      {
      KERR("ERROR %s %s PEER BAD STATUS RECEIVED", locnet, cur->name);
      cur->process_waiting_error += 1;
      if (cur->process_waiting_error == 20)
        {
        utils_send_status_ko(&(cur->lan_add_cli_llid),
                             &(cur->lan_add_cli_tid),
                             "openvswitch timeout error");
        KERR("ERROR WAITING %s %s %d %d", locnet, cur->name, 
                            cur->waiting_ack_del_lan,
                            cur->waiting_ack_add_lan); 
        cur->waiting_ack_add_lan = 0;
        cur->waiting_ack_del_lan = 0;
        cur->process_waiting_error = 0;
        }
      }
    else
      {
      KERR("ERROR %s %s PEER BAD STATUS RECEIVED", locnet, cur->name);
      if (cur->lan_ovs_is_attached)
        {
        if (!strlen(cur->lan))
          KERR("ERROR %s %s", locnet, cur->name);
        else
          {
          if (!dpdk_msg_send_del_lan_d2d(cur->lan, cur->name))
            cur->waiting_ack_del_lan = 1;
          else
            KERR("ERROR %s %s", locnet, cur->name);
          }
        cur->lan_ovs_is_attached = 0;
        }
      }
    if (cur->tcp_connection_peered)
      {
      d2d_dpdk_start_stop_process(cur->name, 0);
      cur->tcp_connection_peered = 0;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_watchdog_count(t_d2d_cnx *cur)
{
  char *locnet = cfg_get_cloonix_name();
  cur->watchdog_count += 1;
  if (cur->watchdog_count == 5)
    {
    KERR("WARNING WATCHDOG PING %s %s", locnet, cur->name);
    cur->peer_bad_status_received = 1;
    wrap_send_d2d_peer_ping(cur, -1);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void process_waiting_add_lan(t_d2d_cnx *cur)
{
  if (cur->vhost_started_and_running == 1)
    {
    if ((strlen(cur->lan)) &&
        (cur->waiting_ack_del_lan == 0) &&
        (cur->waiting_ack_add_lan == 0) &&
        (cur->peer_bad_status_received == 0) &&
        (cur->lan_ovs_is_attached == 0))
      {
      dpdk_msg_send_add_lan_d2d(cur->lan, cur->name);
      cur->waiting_ack_add_lan = 1;
      state_progress_up(cur, state_up_lan_ovs_req);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_d2d_msg_beat(void *data)
{
  t_d2d_cnx *next, *cur = g_head_d2d;
  if (get_glob_req_self_destruction())
    return;
  while(cur)
    {
    next = cur->next;
    process_watchdog_count(cur);
    if(cur->peer_bad_status_received == 1)
      {
      process_peer_bad_status_received(cur);
      }
    else 
      {
      if (cur->local_is_master == 1)
        {
        if (cur->peer_llid == 0)
          wrap_try_connect_to_peer(cur);
        else if (cur->tcp_connection_peered == 0)
          wrap_send_d2d_peer_create(cur, 0);
        else
          transmit_peer_ping(cur);
        }
      process_waiting_add_lan(cur);
      }
    cur = next;
    }
  clownix_timeout_add(100, timer_d2d_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_d2d_cnx *dpdk_d2d_find_with_peer_llid(int llid)
{
  t_d2d_cnx *cur = g_head_d2d;
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
t_d2d_cnx *dpdk_d2d_find_with_peer_llid_connect(int llid)
{
  t_d2d_cnx *cur = g_head_d2d;
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
t_d2d_cnx *dpdk_d2d_find(char *name)
{
  t_d2d_cnx *cur = g_head_d2d;
  while(cur)
    {
    if (!strcmp(name, cur->name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_d2d_cnx *dpdk_d2d_get_first(int *nb_d2d)
{
  t_d2d_cnx *result = g_head_d2d;
  *nb_d2d = g_nb_d2d;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_event_from_d2d_dpdk_process(char *name, int on)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s %d", locnet, name, on);
    }
  else
    {
    if (on == 1)
      {
      if (cur->state_up != state_up_initialised)
        KERR("ERROR BAD STATE: %s", nb_to_text_raw(cur->state_up));
      else
        {
        d2d_dpdk_get_udp_port(name);
        state_progress_up(cur, state_up_process_started);
        }
      }
    else
      {
      if (on == -1)
        KERR("ERROR %s %s %d", locnet, name, on);
      cur->peer_bad_status_received = 1;
      cur->tcp_connection_peered = 0;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_resp_add_lan(int is_ko, char *lan, char *name)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s %s %d", locnet, name, lan, is_ko);
    }
  else if (strcmp(lan, cur->lan))
    {
    KERR("ERROR %s %s %s %d", locnet, name, lan, is_ko);
    }
  else if (cur->waiting_ack_add_lan == 0)
    {
    KERR("ERROR %s %s %s %d", locnet, name, lan, is_ko);
    }
  else if (is_ko)
    {
    KERR("ERROR %s %s %s %d", locnet, name, lan, is_ko);
    utils_send_status_ko(&(cur->lan_add_cli_llid),
                         &(cur->lan_add_cli_tid),
                         "openvswitch error");
    cur->waiting_ack_add_lan = 0;
    }
  else
    {
    cur->lan_ovs_is_attached = 1;
    cur->waiting_ack_add_lan = 0;
    utils_send_status_ok(&(cur->lan_add_cli_llid),&(cur->lan_add_cli_tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    state_progress_up(cur, state_up_lan_ovs_resp);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_resp_del_lan(int is_ko, char *lan, char *name)
{ 
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s %s %d", locnet, name, lan, is_ko);
    }
  else if (strcmp(lan, cur->lan))
    {
    KERR("ERROR %s %s %s %d", locnet, name, lan, is_ko);
    }
  else if (cur->waiting_ack_del_lan == 0)
    {
    KERR("ERROR %s %s %s %d", locnet, name, lan, is_ko);
    }
  else if (is_ko) 
    {
    KERR("ERROR %s %s %s %d", locnet, name, lan, is_ko);
    utils_send_status_ko(&(cur->lan_del_cli_llid),
                         &(cur->lan_del_cli_tid),
                         "openvswitch status error");
    }
  else
    {
    state_progress_down(cur, state_down_ovs_resp_del_lan);
    if (cur->received_del_lan_req)
      {
      memset(cur->lan, 0, MAX_NAME_LEN);
      cur->received_del_lan_req = 0;
      }
    cur->lan_ovs_is_attached = 0;
    dpdk_msg_vlan_exist_no_more(lan);
    utils_send_status_ok(&(cur->lan_del_cli_llid),&(cur->lan_del_cli_tid));
    }
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
  cur->waiting_ack_del_lan = 0; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_d2d_lan_exists(char *lan)
{
  int result = 0;
  t_d2d_cnx *cur = g_head_d2d;
  while(cur)
    {
    if ((strlen(cur->lan)) &&
        (!strcmp(lan, cur->lan)) &&
        (cur->lan_ovs_is_attached == 1))
      {
      result += 1;
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_d2d_lan_exists_in_d2d(char *name, char *lan, int activ)
{
  int result = 0;
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (cur)
    {
    if ((strlen(cur->lan)) &&
        (!strcmp(lan, cur->lan)))
      {
      if (activ) 
        {
        if (cur->lan_ovs_is_attached == 1)
          result = 1;
        }
      else
        result = 1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_peer_conf(int llid, int tid, char *name, int peer_status,
                        char *dist,     char *loc,
                        uint32_t dist_udp_ip,   uint32_t loc_udp_ip,
                        uint16_t dist_udp_port, uint16_t loc_udp_port)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s", locnet, name);
    }
  else if (tid != cur->ref_tid)
    {
    KERR("ERROR %s %s %d %d", locnet, name, tid, cur->ref_tid);
    }
  else if (peer_status == -1)
    {
    KERR("ERROR %s %s", locnet, name);
    cur->peer_bad_status_received = 1;
    }
  else
    {
    cur->dist_udp_port = dist_udp_port;
    if (cur->local_is_master == 0)
      {
      cur->dist_udp_ip = dist_udp_ip; 
      cur->loc_udp_ip = loc_udp_ip; 
      }
    cur->udp_dist_port_chosen = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void master_and_slave_ping_process(t_d2d_cnx *cur, int peer_status)
{
  char *locnet = cfg_get_cloonix_name();
  cur->watchdog_count = 0;
  if ((cur->udp_connection_tx_configured == 0) &&
      (peer_status > 0) &&
      (cur->udp_loc_port_chosen == 1)  &&
      (cur->udp_dist_port_chosen == 1))
    {
    if ((cur->dist_udp_ip == 0) || (cur->dist_udp_port == 0))
      KERR("ERROR %s %x %hu", locnet, cur->dist_udp_ip, cur->dist_udp_port);
    else
      d2d_dpdk_set_dist_udp_ip_port(cur->name, cur->dist_udp_ip,
                                    cur->dist_udp_port);
    }
  else if ((cur->udp_connection_peered == 1) &&
           (peer_status > 1) &&
           (cur->state_up == state_up_process_started))
    {
    d2d_dpdk_eal_init(cur->name);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    state_progress_up(cur, state_up_process_eal_init);
    }
  else if ((cur->vhost_started_and_running == 0) &&
           (cur->udp_connection_peered == 1) &&
           (peer_status > 1) &&
           (cur->state_up == state_up_process_eal_init))
    {
    d2d_dpdk_start_vhost(cur->name);
    state_progress_up(cur, state_up_process_vhost_req_on);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_peer_ping(int llid, int tid, char *name, int peer_status)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s", locnet, name); 
    }
  else if (tid != cur->ref_tid)
    {
    KERR("ERROR %s %s %d %d", locnet, name, tid, cur->ref_tid); 
    } 
  else if (peer_status == -1)
    {
    cur->peer_bad_status_received = 1;
    }
  else
    {
    master_and_slave_ping_process(cur, peer_status);
    if (cur->local_is_master == 1)
      {
      if ((cur->udp_connection_tx_configured) &&
          (cur->udp_connection_peered == 0))
        {
        cur->udp_probe_qty_sent += 1;
        if (cur->udp_probe_qty_sent > 25)
          {
          KERR("ERROR %s %s %d", locnet, cur->name, cur->udp_probe_qty_sent);
          cur->udp_probe_qty_sent = 0;
          }
        d2d_dpdk_send_probe_udp(cur->name);
        }
      }
    else
      {
      transmit_peer_ping(cur);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_d2d_add(char *name, uint32_t loc_udp_ip,
                 char *dist, uint32_t dist_ip, uint16_t dist_port,
                 char *dist_passwd, uint32_t dist_udp_ip)
{
  int result = -1;
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  char *locnet = cfg_get_cloonix_name();
  if (cur)
    {
    KERR("ERROR %s %s %s", locnet, name, dist); 
    }
  else
    {
    cur = alloc_d2d(name, dist);
    strncpy(cur->dist_passwd, dist_passwd, MSG_DIGEST_LEN-1);
    cur->dist_tcp_ip = dist_ip;
    cur->dist_tcp_port = dist_port;
    cur->dist_udp_ip = dist_udp_ip;
    cur->loc_udp_ip = loc_udp_ip;
    cur->local_is_master = 1;
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    state_progress_up(cur, state_up_initialised);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void dpdk_d2d_peer_add(int llid, int tid, char *name, char *dist, char *loc)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (cur)
    {
    KERR("ERROR %s %s %s", locnet, name, dist); 
    }
  else if (strcmp(loc, locnet))
    {
    KERR("ERROR %s %s %s %s", locnet, name, dist, loc);
    }
  else
    {
    cur = alloc_d2d(name, dist);
    cur->peer_llid = llid;
    cur->ref_tid = tid;
    cur->tcp_connection_peered = 1;
    d2d_dpdk_start_stop_process(name, 1);
    wrap_send_d2d_peer_create(cur, 1);
    state_progress_up(cur, state_up_initialised);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_peer_add_ack(int llid, int tid, char *name,
                           char *dist, char *loc, int ack)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s %s", locnet, name, dist);
    }
  else if (strcmp(loc, locnet))
    {
    KERR("ERROR %s %s %s %s", locnet, name, dist, loc);
    }
  else if (cur->local_is_master == 0)
    {
    KERR("ERROR %s %s %s %s", locnet, name, dist, loc);
    }
   else if (tid != cur->ref_tid)
    {
    KERR("ERROR %s %s %d %d", locnet, name, tid, cur->ref_tid);
    }
  else
    {
    cur->tcp_connection_peered = 1;
    d2d_dpdk_start_stop_process(name, 1);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_d2d_del(char *name, char *err)
{
  int result = -1;
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  char *locnet = cfg_get_cloonix_name();
  memset(err, 0, MAX_PATH_LEN);
  if (!cur)
    {
    snprintf(err, MAX_PATH_LEN-1, "%s d2d %s not found", locnet, name);
    KERR("ERROR %s %s", locnet, name);
    }
  else if (cur->local_is_master == 0)
    {
    snprintf(err, MAX_PATH_LEN-1,
             "%s %s is slave, must kill master", locnet, name);
    KERR("ERROR %s %s", locnet, name);
    }
  else if (cur->waiting_ack_add_lan == 1)
    {
    snprintf(err, MAX_PATH_LEN-1, "%s d2d %s occupied", locnet, name);
    KERR("ERROR %s %s", locnet, name);
    } 
  else
    {
    cur->peer_bad_status_received = 1;
    cur->master_del_req = 1;
    wrap_send_d2d_peer_ping(cur, -1);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_vhost_started(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s", locnet, name);
    }
  else
    {
    cur->vhost_started_and_running = 1;
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    state_progress_up(cur, state_up_process_vhost_resp_on);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_get_udp_port_done(char *name, uint16_t port, int peer_status)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s %hu", locnet, name, port);
    }
  else if (peer_status)
    {
    KERR("ERROR %s %s", locnet, cur->name);
    cur->peer_bad_status_received = 1;
    }
  else
    {
    cur->loc_udp_port = port;
    wrap_send_d2d_peer_conf(cur, 0);
    cur->udp_loc_port_chosen = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_dist_udp_ip_port_done(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s", locnet, name);
    }
  else
    {
    cur->udp_connection_tx_configured = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_receive_probe_udp(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s", locnet, name);
    }
  else
    {
    if (cur->local_is_master == 0)
      {
      d2d_dpdk_send_probe_udp(name);
      }
    cur->udp_connection_peered = 1;
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*--------------------------------------------------------------------------*/
    
/****************************************************************************/
void dpdk_d2d_end_ovs(void)
{   
  t_d2d_cnx *cur = g_head_d2d;
  while(cur)
    {
    cur->peer_bad_status_received = 1;
    cur->master_del_req = 1;
    wrap_send_d2d_peer_ping(cur, -1);
    cur = cur->next;
    }
}   
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_add_lan(int llid, int tid, char *name, char *lan)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s %s", locnet, name, lan);
    send_status_ko(llid, tid, "not found");
    }
  else if (cur->lan_add_cli_llid)
    {
    KERR("ERROR %s %s %s", locnet, name, lan);
    send_status_ko(llid, tid, "command to be processed");
    }
  else if (strlen(cur->lan) != 0)
    {
    KERR("ERROR %s %s %s %s", locnet, name, lan, cur->lan);
    send_status_ko(llid, tid, "a vlan is stored");
    }
  else
    {
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    cur->lan_add_cli_llid = llid;
    cur->lan_add_cli_tid = tid;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_vhost_stopped(char *name)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s", locnet, name);
    }
  else
    {
    state_progress_down(cur, state_down_vhost_stopped);
    if (dpdk_msg_send_del_lan_d2d(cur->lan, cur->name))
      {
      KERR("ERROR %s %s %s", locnet, name, cur->lan);
      send_status_ko(cur->lan_del_cli_llid, cur->lan_del_cli_tid, "impossible");
      if (strlen(cur->lan))
        {
        memset(cur->lan, 0, MAX_NAME_LEN);
        dpdk_msg_vlan_exist_no_more(cur->lan);
        }
      }
    else
      {
      state_progress_down(cur, state_down_ovs_req_del_lan);
      cur->received_del_lan_req = 1;
      cur->waiting_ack_del_lan = 1;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_del_lan(int llid, int tid, char *name, char *lan)
{
  char *locnet = cfg_get_cloonix_name();
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (!cur)
    {
    KERR("ERROR %s %s %s", locnet, name, lan);
    send_status_ko(llid, tid, "not found");
    }
  else if (cur->lan_del_cli_llid)
    {
    KERR("ERROR %s %s %s", locnet, name, lan);
    send_status_ko(llid, tid, "command to be processed");
    }
  else if (strcmp(cur->lan, lan))
    {
    KERR("ERROR %s %s %s %s", locnet, name, lan, cur->lan);
    send_status_ko(llid, tid, "a vlan is stored");
    }
  else if (cur->waiting_ack_del_lan == 1)
    {
    KERR("ERROR %s %s %s", locnet, name, lan);
    send_status_ko(llid, tid, "command to be processed");
    }
  else if (cur->lan_ovs_is_attached == 0)
    {
    KERR("ERROR %s %s %s", locnet, name, lan);
    send_status_ko(llid, tid, "lan not attached");
    }
  else if (cur->vhost_started_and_running != 1)
    {
    KERR("ERROR %s %s %s", locnet, name, lan);
    send_status_ko(llid, tid, "vhost not started");
    }
  else
    {
    state_progress_down(cur, state_down_initialised);
    cur->lan_del_cli_llid = llid;
    cur->lan_del_cli_tid = tid;
    cur->vhost_started_and_running = 0;
    d2d_dpdk_stop_vhost(cur->name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *dpdk_d2d_get_next(char *name)
{
  t_d2d_cnx *cur;
  char *result = NULL;
  if (name)
    {
    cur = dpdk_d2d_find(name);
    if (cur)
      cur = cur->next;
    }
  else
    {
    cur = g_head_d2d;
    }
  if (cur)
    result = cur->name;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *translate_topo_endp_d2d(int *nb)
{
  t_d2d_cnx *cur = g_head_d2d;
  int len, nb_endp = 0;
  t_topo_endp *endp;
  endp = (t_topo_endp *) clownix_malloc(g_nb_d2d * sizeof(t_topo_endp),13);
  memset(endp, 0, g_nb_d2d * sizeof(t_topo_endp));
  while(cur)
    {
    len = sizeof(t_lan_group_item);
    endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
    memset(endp[nb_endp].lan.lan, 0, len);
    strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
    endp[nb_endp].num = 0;
    endp[nb_endp].type = endp_type_d2d;
    if (strlen(cur->lan))
      {
      strncpy(endp[nb_endp].lan.lan[0].lan, cur->lan, MAX_NAME_LEN-1);
      endp[nb_endp].lan.nb_lan = 1;
      }
    nb_endp += 1;
    cur = cur->next;
    }
  *nb = nb_endp;
  return endp;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_d2d_exists(char *name)
{
  int result = 0;
  t_d2d_cnx *cur = dpdk_d2d_find(name);
  if (cur != NULL)
    result =1;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_d2d_init(void)
{
  g_head_d2d = NULL;
  g_nb_d2d = 0;
  clownix_timeout_add(100, timer_d2d_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

