/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include "event_subscriber.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "llid_trace.h"
#include "machine_create.h"
#include "file_read_write.h"
#include "dpdk_xyx.h"
#include "stats_counters.h"
#include "dpdk_msg.h"
#include "xyx_dpdk_process.h"
#include "suid_power.h"
#include "fmt_diag.h"


enum {
  state_idle = 0,
  state_up_process_req,
  state_up_process_started,
  state_up_process_vhost_req_on,
  state_up_process_vhost_resp_on,
  state_up_lan_ovs_req,
  state_up_lan_ovs_resp,
  state_down_initialised,
  state_down_vhost_stopped,
  state_down_ovs_req_del_lan,
  state_down_ovs_resp_del_lan,
};

/*--------------------------------------------------------------------------*/
static t_xyx_cnx *g_head_xyx;
static int g_nb_xyx;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void nb_to_text(int state, char *label)
{
  switch (state)
    {
    case state_idle:
      strcpy(label, "state_idle");
      break;
    case state_up_process_req:
      strcpy(label, "state_up_process_req");
      break;
    case state_up_process_started:
      strcpy(label, "state_up_process_started");
      break;
    case state_up_process_vhost_req_on:
      strcpy(label, "state_up_process_vhost_req_on");
      break;
    case state_up_process_vhost_resp_on:
      strcpy(label, "state_up_process_vhost_resp_on");
      break;
    case state_up_lan_ovs_req:
      strcpy(label, "state_up_lan_ovs_req");
      break;
    case state_up_lan_ovs_resp:
      strcpy(label, "state_up_lan_ovs_resp");
      break;
    default:
      KOUT("%d", state);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void state_progress_up(t_xyx_cnx *cur, int state)
{
  char *locnet = cfg_get_cloonix_name();
  char olab[MAX_NAME_LEN];
  char nlab[MAX_NAME_LEN];
  nb_to_text(cur->state_up, olab);
  cur->state_up = state;
  nb_to_text(cur->state_up, nlab);
//  KERR("STATE XYX %s %s %d  %s ----> %s", locnet, cur->name, cur->num,
//                                          olab, nlab);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_xyx_cnx *find_xyx(char *name, int num)
{
  t_xyx_cnx *cur = g_head_xyx;
  char cname[MAX_NAME_LEN];
  char ext[6];
  while(cur)
    {
    memset(cname, 0, MAX_NAME_LEN);
    strncpy(cname, name, MAX_NAME_LEN-6);
    if (cur->endp_type == endp_type_eths)
      {
      memset(ext, 0, 6);
      snprintf(ext, 5, "_%d", num);
      strcat(cname, ext);
      }
    if (!strcmp(cname, cur->name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_xyx_cnx *find_xyx_fullname(char *name)
{ 
  t_xyx_cnx *cur = g_head_xyx;
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
static t_xyx_cnx *alloc_xyx(int llid, int tid, char *name, int num,
                            int endp_type)
{
  t_xyx_cnx *cur = (t_xyx_cnx *)clownix_malloc(sizeof(t_xyx_cnx), 5);
  char ext[6];
  memset(cur, 0, sizeof(t_xyx_cnx));
  memset(ext, 0, 6);
  cur->num = num;
  cur->endp_type = endp_type;
  cur->add_llid = llid;
  cur->add_tid = tid;
  strncpy(cur->nm, name, MAX_NAME_LEN-6);
  strncpy(cur->name, name, MAX_NAME_LEN-6);
  if (endp_type == endp_type_tap)
    {
    suid_power_rec_name(cur->nm, 1);
    }
  else if (endp_type == endp_type_phy)
    {
    suid_power_rec_name(cur->nm, 1);
    }
  else if (endp_type == endp_type_eths)
    {
    snprintf(ext, 5, "_%d", num); 
    }
  else
    KOUT("%s %d", name, num);
  strcat(cur->name, ext);
  if (g_head_xyx)
    g_head_xyx->prev = cur;
  cur->next = g_head_xyx;
  g_head_xyx = cur;
  g_nb_xyx += 1;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_xyx(t_xyx_cnx *cur)
{
  if (cur->endp_type == endp_type_tap)
    {
    suid_power_rec_name(cur->nm, 0);
    }
  else if (cur->endp_type == endp_type_phy)
    {
    suid_power_rec_name(cur->nm, 0);
    }
  else if (cur->endp_type == endp_type_eths)
    {
    }
  else
    KOUT("%s %d", cur->name, cur->num);

  if (cur->lan.attached_lan_ok == 1)
    KERR("ERROR %s", cur->lan.lan);
  if (strlen(cur->lan.lan))
    dpdk_msg_vlan_exist_no_more(cur->lan.lan);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_head_xyx == cur)
    g_head_xyx = cur->next;
  clownix_free(cur, __FUNCTION__);
  g_nb_xyx -= 1;
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_xyx_msg_beat(void *data)
{
  t_xyx_cnx *next, *cur = g_head_xyx;
  while(cur)
    {
    next = cur->next;
    if ((cur->process_started_and_running == 1) &&
        (cur->state_up == state_up_process_started)) 
      {
      xyx_dpdk_start_vhost(cur->name);
      state_progress_up(cur, state_up_process_vhost_req_on);
      }
    if (cur->add_llid)
      {
      cur->timer_count += 1;
      if (cur->timer_count > 40)
        {
        KERR("ERROR TIMEOUT %s", cur->name);
        utils_send_status_ko(&(cur->add_llid), &(cur->add_tid), "timeout");
        cur->timer_count = 0;
        cur->to_be_destroyed = 1;
        }
      }
    if ((cur->lan.waiting_ack_add_lan == 1) ||
        (cur->lan.waiting_ack_del_lan == 1))
      {
      cur->lan.timer_count += 1;
      if (cur->lan.timer_count > 20)
        {
        KERR("ERROR TIMEOUT %s add:%d del:%d", cur->name,
                                               cur->lan.waiting_ack_add_lan,
                                               cur->lan.waiting_ack_del_lan);
        cur->lan.timer_count = 0;
        cur->to_be_destroyed = 1;
        }
      }
    if (cur->to_be_destroyed == 1)
      {
      if ((cur->lan.waiting_ack_add_lan == 1)  ||
          (cur->lan.attached_lan_ok == 1))
        cur->to_be_destroyed_count += 1;
      else
        xyx_dpdk_start_stop_process(cur->name, 0);
      if (cur->to_be_destroyed_count == 20)
        {
        KERR("ERROR TIMEOUT %s", cur->name);
        xyx_dpdk_start_stop_process(cur->name, 0);
        }
      }
    cur = next;
    }
  clownix_timeout_add(50, timer_xyx_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_get_qty(void)
{
  int result = 0;
  t_xyx_cnx *cur = g_head_xyx; 
  while(cur)
    {
    result += 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_xyx_resp_add_lan(int is_ko, char *lan, char *name, int num)
{
  t_xyx_cnx *cur = find_xyx(name, num);
  if (!cur)
    KERR("ERROR %s %d %s", name, num, lan);
  else if (strcmp(cur->lan.lan, lan))
    KERR("ERROR %s %s %d %s", lan, name, num, cur->lan.lan);
  else
    {
    if (cur->lan.waiting_ack_add_lan == 0)
      KERR("ERROR %d %s %s %d", is_ko, lan, name, num);
    if (cur->lan.waiting_ack_del_lan == 1)
      KERR("ERROR %d %s %s %d", is_ko, lan, name, num);
    if (is_ko)
      {
      KERR("ERROR %s %d %s", name, num, lan);
      cur->lan.waiting_ack_add_lan = 0;
      utils_send_status_ko(&(cur->lan.llid),
                           &(cur->lan.tid), "openvswitch error");
      }
    else
      {
      utils_send_status_ok(&(cur->lan.llid), &(cur->lan.tid));
      cur->lan.waiting_ack_add_lan = 0;
      cur->lan.attached_lan_ok = 1;
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      state_progress_up(cur, state_up_lan_ovs_resp);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_xyx_resp_del_lan(int is_ko, char *lan, char *name, int num)
{
  t_xyx_cnx *cur = find_xyx(name, num);
  if (!cur)
    KERR("ERROR %s %s %d", lan, name, num);
  else if (strcmp(lan, cur->lan.lan))
    KERR("ERROR %s %s %d %s", lan, name, num, cur->lan.lan);
  else
    {
    if (cur->lan.attached_lan_ok == 1) 
      KERR("ERROR %d %s %s %d", is_ko, lan, name, num);
    if (cur->lan.waiting_ack_add_lan == 1)
      KERR("ERROR %d %s %s %d", is_ko, lan, name, num);
    if (cur->lan.waiting_ack_del_lan == 0)
      KERR("ERROR %d %s %s %d", is_ko, lan, name, num);
    cur->lan.attached_lan_ok = 0; 
    cur->lan.waiting_ack_del_lan = 0;
    memset(cur->lan.lan, 0, MAX_NAME_LEN);
    utils_send_status_ok(&(cur->lan.llid),&(cur->lan.tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_xyx_event_from_xyx_dpdk_process(char *name, int on)
{
  t_xyx_cnx *cur = find_xyx_fullname(name);
  char *lan;
  if (cur)
    {
    lan = cur->lan.lan;
    if ((on == -1) || (on == 0))
      {
      cur->to_be_destroyed = 1;
      if (on == -1)
        {
        KERR("ERROR %s", name);
        if (strlen(lan))
          {
          if (cur->endp_type == endp_type_tap)
            dpdk_msg_send_del_lan_tap(lan, cur->nm);
          else if (cur->endp_type == endp_type_phy)
            dpdk_msg_send_del_lan_phy(lan, cur->nm);
          else if (cur->endp_type == endp_type_eths)
            dpdk_msg_send_del_lan_eths(lan, cur->nm, cur->num);
          else
            KOUT("ERROR  %s", name);
          KERR("ERROR %s %s", name, lan);
          }
        }
      if (cur->add_llid)
        {
        KERR("ERROR %s", name);
        utils_send_status_ko(&(cur->add_llid), &(cur->add_tid), "error");
        }
      free_xyx(cur);
      }
    else
      {
      if (strlen(lan))
        cur->lan.attached_lan_ok = 0;
      if (cur->process_started_and_running != 0)
        KERR("ERROR %s", name);
      cur->process_started_and_running = 1;
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      state_progress_up(cur, state_up_process_started);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_add_lan(int llid, int tid, char *name, int num, char *lan)
{
 int result = -1;
  t_xyx_cnx *cur = find_xyx(name, num);
  if (cur == NULL)
    KERR("ERROR %s %d", name, num);
  else if (strlen(cur->lan.lan))
    KERR("ERROR %s %d %s %s", name, num, lan, cur->lan.lan);
  else if (cur->vhost_started_and_running == 0)
    KERR("ERROR %s %s %d", lan, name, num);
  else if (cur->ovs_started_and_running == 0)
    KERR("ERROR %s %s %d", lan, name, num);
  else
    {
    if (cur->endp_type == endp_type_tap)
      {
      if (dpdk_msg_send_add_lan_tap(lan, name))
        KERR("ERROR %s %s", lan, name);
      else
        result = 0;
      }
    else if (cur->endp_type == endp_type_phy)
      {
      if (dpdk_msg_send_add_lan_phy(lan, name))
        KERR("ERROR %s %s", lan, name);
      else
        result = 0;
      }
    else if (cur->endp_type == endp_type_eths)
      {
      if (dpdk_msg_send_add_lan_eths(lan, name, num))
        KERR("ERROR %s %s %d", lan, name, num);
      else
        result = 0;
      }
    else
      KOUT("ERROR  %s %d", name, num);
    }
  if (result == 0)
    {
    memset(cur->lan.lan, 0, MAX_NAME_LEN);
    strncpy(cur->lan.lan, lan, MAX_NAME_LEN-1);
    cur->lan.waiting_ack_add_lan = 1;
    cur->lan.llid = llid;
    cur->lan.tid = tid;
    state_progress_up(cur, state_up_lan_ovs_req);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_del_lan(int llid, int tid, char *name, int num, char *lan)
{
 int result = -1;
  t_xyx_cnx *cur = find_xyx(name, num);
  if (cur == NULL)
    KERR("ERROR %s %d", name, num);
  else if (!strlen(cur->lan.lan))
    KERR("ERROR %s %d %s", name, num, lan);
  else if (strcmp(lan, cur->lan.lan))
    KERR("ERROR %s %s %d", lan, name, num);
  else if ((cur->lan.waiting_ack_add_lan == 0) && 
           (cur->lan.attached_lan_ok == 0))
    KERR("ERROR %s %s %d", lan, name, num);
  else 
    {
    if (cur->lan.waiting_ack_del_lan == 1)
      KERR("ERROR %s %s %d", lan, name, num);
    if (cur->endp_type == endp_type_tap)
      {
      if (dpdk_msg_send_del_lan_tap(lan, name))
        KERR("ERROR %s %s", lan, name);
      }
    else if (cur->endp_type == endp_type_phy)
      {
      if (dpdk_msg_send_del_lan_phy(lan, name))
        KERR("ERROR %s %s", lan, name);
      }
    else if (cur->endp_type == endp_type_eths)
      {
      if (dpdk_msg_send_del_lan_eths(lan, name, num))
        KERR("ERROR %s %s %d", lan, name, num);
      }
    else
      KOUT("ERROR  %s %d", name, num);
    cur->lan.attached_lan_ok = 0;
    cur->lan.waiting_ack_del_lan = 1;
    cur->lan.llid = llid;
    cur->lan.tid = tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_add(int llid, int tid, char *name, int num, int endp_type)
{
  int result = -1;
  t_xyx_cnx *cur = find_xyx(name, num);

  if (cur)
    KERR("ERROR %s %d", name, num);
  else if ((endp_type != endp_type_tap) &&
           (endp_type != endp_type_phy) &&
           (endp_type != endp_type_eths))
    KERR("ERROR %s %d %d", name, num, endp_type);
  else
    {
    cur = alloc_xyx(llid, tid, name, num, endp_type);
    xyx_dpdk_start_stop_process(cur->name, 1);
    result = 0;
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    state_progress_up(cur, state_up_process_req);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_del(int llid, int tid, char *name, int num)
{
  int result = -1;
  char *lan;
  t_xyx_cnx *cur = find_xyx(name, num);
  if (!cur)
    KERR("ERROR %s %d", name, num);
  else
    {
    lan = cur->lan.lan;
    if ((strlen(lan)) &&
        ((cur->lan.waiting_ack_add_lan == 1) ||
        (cur->lan.attached_lan_ok == 1)))
      {
      if (cur->lan.waiting_ack_del_lan == 1)
        KERR("ERROR %s %s", lan, name);
      if (cur->endp_type == endp_type_tap)
        {
        if (dpdk_msg_send_del_lan_tap(lan, name))
          KERR("ERROR %s %s", lan, name);
        }
      else if (cur->endp_type == endp_type_phy)
        {
        if (dpdk_msg_send_del_lan_phy(lan, name))
          KERR("ERROR %s %s", lan, name);
        }
      else if (cur->endp_type == endp_type_eths)
        {
        if (dpdk_msg_send_del_lan_eths(lan, name, num))
          KERR("ERROR %s %s %d", lan, name, num);
        }
      else
        KOUT("ERROR  %s", name);
      cur->lan.attached_lan_ok = 0;
      cur->lan.waiting_ack_del_lan = 1;
      }
    if (cur->endp_type == endp_type_tap)
      {
      if (fmt_tx_del_tap(0, name))
        KERR("ERROR %s", name);
      }
    else if (cur->endp_type == endp_type_phy)
      {
      if (fmt_tx_del_phy(0, name))
        KERR("ERROR %s", name);
      }
    else if (cur->endp_type == endp_type_eths)
      {
      if (fmt_tx_del_ethds(0, name, num))
        KERR("ERROR %s %d", name, num);
      }
    else
      KOUT("ERROR  %s", name);
    cur->to_be_destroyed = 1;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_lan_exists(char *lan)
{
  t_xyx_cnx *cur = g_head_xyx;
  int result = 0;
  while(cur)
    {
    if ((!strcmp(lan, cur->lan.lan)) &&
        ((cur->lan.waiting_ack_add_lan == 1) ||
        (cur->lan.attached_lan_ok == 1)))
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
int dpdk_xyx_lan_exists_in_xyx(char *name, int num, char *lan)
{
  int result = 0;
  t_xyx_cnx *cur = find_xyx(name, num);
  if(cur)
    {
    if (!strcmp(lan, cur->lan.lan))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_xyx_end_ovs(void)
{
  t_xyx_cnx *next, *cur = g_head_xyx;
  while(cur)
    {
    next = cur->next;
    dpdk_xyx_del(0, 0, cur->nm, cur->num);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_eventfull(char *name, int ms, int ptx, int btx, int prx, int brx)
{
  int result = -1;
  t_xyx_cnx *cur = find_xyx_fullname(name);
  if (cur)
    {
    cur->lan.ms      = ms;
    cur->lan.pkt_tx  += ptx;
    cur->lan.pkt_rx  += prx;
    cur->lan.byte_tx += btx;
    cur->lan.byte_rx += brx;
    stats_counters_update_endp_tx_rx(cur->nm,cur->num,ms,ptx,btx,prx,brx);
    result = 0;
    }
  else
    KERR("ERROR %s", name);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_xyx_collect_dpdk(t_eventfull_endp *eventfull)
{
  int result = 0;;
  t_xyx_cnx *cur = g_head_xyx;
  while(cur)
    {
    if (strlen(cur->lan.lan))
      {
      strncpy(eventfull[result].name, cur->nm, MAX_NAME_LEN-1);
      eventfull[result].type = cur->endp_type;
      eventfull[result].num  = cur->num;
      eventfull[result].ms   = cur->lan.ms;
      eventfull[result].ptx  = cur->lan.pkt_tx;
      eventfull[result].prx  = cur->lan.pkt_rx;
      eventfull[result].btx  = cur->lan.byte_tx;
      eventfull[result].brx  = cur->lan.byte_rx;
      cur->lan.pkt_tx  = 0;
      cur->lan.pkt_rx  = 0;
      cur->lan.byte_tx = 0;
      cur->lan.byte_rx = 0;
      result += 1;
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_exists(char *name, int num)
{
  int result = 0;
  t_xyx_cnx *cur = find_xyx(name, num);
  if (cur)
    result = cur->endp_type;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_exists_fullname(char *name)
{
  int result = 0;
  t_xyx_cnx *cur = find_xyx_fullname(name);
  if (cur)
    result = cur->endp_type;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_name_exists(char *name)
{
  int result = 0;
  t_xyx_cnx *cur = g_head_xyx;
  while(cur)
    {
    if (!strcmp(name, cur->name))
      result = 1;
    if (!strcmp(name, cur->nm))
      result = 1;
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_xyx_cnx *dpdk_xyx_get_first(int *nb_tap, int *nb_phy, int *nb_kvm)
{
  t_xyx_cnx *cur = g_head_xyx;
  *nb_tap = 0;
  *nb_phy = 0;
  *nb_kvm = 0;
  while(cur)
    {
    if (cur->endp_type == endp_type_tap)
      *nb_tap += 1;
    if (cur->endp_type == endp_type_phy)
      *nb_phy += 1;
    if (cur->endp_type == endp_type_eths)
      *nb_kvm += 1;
    cur = cur->next;
    }
  return g_head_xyx;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *translate_topo_endp_xyx(int *nb)
{
  t_xyx_cnx *cur = g_head_xyx;
  int len, nb_endp = 0;
  t_topo_endp *endp;
  len = g_nb_xyx * sizeof(t_topo_endp);
  endp = (t_topo_endp *) clownix_malloc( len, 13);
  memset(endp, 0, len);
  while(cur)
    {
    len = sizeof(t_lan_group_item);
    endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
    memset(endp[nb_endp].lan.lan, 0, len);
    strncpy(endp[nb_endp].name, cur->nm, MAX_NAME_LEN-1);
    endp[nb_endp].num = cur->num;
    endp[nb_endp].type = cur->endp_type;
    if (strlen(cur->lan.lan))
      {
      strncpy(endp[nb_endp].lan.lan[0].lan, cur->lan.lan, MAX_NAME_LEN-1);
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
static void timer_end_eths2_ovs(void *data)
{
  char *name = (char *) data;
  t_xyx_cnx *cur = find_xyx_fullname(name);
  if (cur == NULL)
    KERR("ERROR %s %d", cur->nm, cur->num);
  else
    {
    if (cur->eths_done_ok != 0)
      KERR("ERROR %s %d", cur->nm, cur->num);
    else
      {
      if (fmt_tx_add_eths2(0, cur->nm, cur->num))
        KERR("ERROR %s %d", cur->nm, cur->num);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_start_ovs_req(void *data)
{
  char *name = (char *) data;
  t_xyx_cnx *cur = find_xyx_fullname(name);
  if (cur == NULL)
    {
    KERR("ERROR %s", name);
    clownix_free(data, __FUNCTION__);
    }
  else
    {
    cur->vhost_started_and_running = 1;
    state_progress_up(cur, state_up_process_vhost_resp_on);
    if (cur->endp_type == endp_type_tap)
      {
      if (fmt_tx_add_tap(0, name))
        KERR("ERROR %s", name);
      clownix_free(name, __FUNCTION__);
      }
    else if (cur->endp_type == endp_type_phy)
      {
      if (fmt_tx_add_phy(0, name))
        KERR("ERROR %s", name);
      clownix_free(name, __FUNCTION__);
      }
    else if (cur->endp_type == endp_type_eths)
      {
      if (fmt_tx_add_ethds(0, cur->nm, cur->num))
        {
        KERR("ERROR %s %d", cur->nm, cur->num);
        clownix_free(name, __FUNCTION__);
        }
      else
        {
        clownix_timeout_add(10, timer_end_eths2_ovs, data, NULL, NULL);
        }
      }
    else
      KOUT("ERROR  %s", name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_xyx_vhost_started(char *name)
{
  t_xyx_cnx *cur = find_xyx_fullname(name);
  char *data;
  if (!cur)
    KERR("ERROR %s", name);
  else
    {
    data = (char *) clownix_malloc(MAX_NAME_LEN, 7);
    memset(data, 0, MAX_NAME_LEN); 
    strncpy(data, name, MAX_NAME_LEN-1);
    clownix_timeout_add(30, timer_start_ovs_req, data, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_xyx_resp_add(int is_ko, char *name, int num)
{
  t_xyx_cnx *cur = find_xyx(name, num);
  if (!cur)
    KERR("ERROR %s %d", name, num);
  else if (is_ko)
    {
    KERR("ERROR %s %d", name, num);
    utils_send_status_ko(&(cur->lan.llid), &(cur->lan.tid), "error");
    cur->to_be_destroyed = 1;
    }
  else
    {
    cur->ovs_started_and_running = 1;
    utils_send_status_ok(&(cur->add_llid),&(cur->add_tid));
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_xyx_resp_del(int is_ko, char *name, int num)
{
  t_xyx_cnx *cur = find_xyx(name, num);
  if (!cur)
    KERR("ERROR %s %d", name, num);
  else
    {
    cur->to_be_destroyed = 1;
    if (is_ko)
      {
      KERR("ERROR %s %d", name, num);
      utils_send_status_ko(&(cur->lan.llid), &(cur->lan.tid), "error");
      }
    else
      {
      if (cur->endp_type == endp_type_tap)
        KERR("DELETE TAP OK %s", name);
      else if (cur->endp_type == endp_type_phy)
        KERR("DELETE PHY OK %s", name);
      else if (cur->endp_type == endp_type_eths)
        KERR("DELETE ETH KVM SPY OK %s %d", name, num);
      else
        KOUT("ERROR %s %d", name, num);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_xyx_cnf(char *name, int type, uint8_t *mac)
{
  t_xyx_cnx *cur = find_xyx_fullname(name);
  int i;
  if (!cur)
    {
    KERR("ERROR %s %d", name, type);
    }
  else if (type == xyx_type_mac_mangle)
    {
    if (mac == NULL)
      KOUT(" ");
    cur->arp_mangle = 1;
    for (i=0; i<6; i++)
      cur->arp_mac[i] = mac[i];
    KERR("XYX MAC MANGLE %s %hhX:%hhX:%hhX:%hhX:%hhX:%hhX",
           name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    xyx_dpdk_cnf(name, type, mac);
    }
  else
    KERR("ERROR %s %d %hhX:%hhX:%hhX:%hhX:%hhX:%hhX",
         name, type, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_ready(char *name, int num)
{
  int result = 0;
  t_xyx_cnx *cur = find_xyx(name, num);
  if (cur == NULL)
    KERR("ERROR %s %d", name, num);
  else if (cur->endp_type == endp_type_tap)
    result = (cur->ovs_started_and_running &&
              cur->vhost_started_and_running);
  else if (cur->endp_type == endp_type_phy)
    result = (cur->ovs_started_and_running &&
              cur->vhost_started_and_running);
  else if (cur->endp_type == endp_type_eths)
    result = (cur->ovs_started_and_running &&
              cur->vhost_started_and_running &&
              cur->eths_done_ok);
  else
    KOUT("%d", cur->endp_type);

  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_xyx_lan_empty(char *name, int num)
{
  int result = 1;
  t_xyx_cnx *cur = find_xyx(name, num);
  if (cur == NULL)
    KERR("ERROR %s %d", name, num);
  else if (strlen(cur->lan.lan))
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_xyx_eths2_resp_ok(char *name, int num)
{
  t_xyx_cnx *cur = find_xyx(name, num);
  if (cur == NULL)
    KERR("ERROR %s %d", cur->nm, cur->num);
  else
    cur->eths_done_ok = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_xyx_eths2_vm_pinging(char *name, int num)
{
  t_xyx_cnx *cur = find_xyx(name, num);
  if (cur == NULL)
    KERR("ERROR %s %d", cur->nm, cur->num);
  else
    {
    if (cur->eths_done_ok == 0)
      {
      if (cur->endp_type != endp_type_eths)
        KERR("ERROR %s %d", cur->nm, cur->num);
      else
        {
        if (fmt_tx_add_eths2(0, cur->nm, cur->num))
          KERR("ERROR %s %d", cur->nm, cur->num);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_xyx_init(void)
{
  g_head_xyx = NULL;
  g_nb_xyx = 0;
  clownix_timeout_add(50, timer_xyx_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
