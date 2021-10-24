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
#include "dpdk_ovs.h"
#include "dpdk_a2b.h"
#include "dpdk_d2d.h"
#include "dpdk_msg.h"
#include "qmp.h"
#include "system_callers.h"
#include "stats_counters.h"
#include "dpdk_msg.h"
#include "a2b_dpdk_process.h"
#include "suid_power.h"


enum {
  state_idle = 0,
  state_up_process_req,
  state_up_process_started,
  state_up_process_vhost_req_on,
  state_up_process_vhost_resp_on,
  state_up_lan_ovs_req0,
  state_up_lan_ovs_resp0,
  state_up_lan_ovs_req1,
  state_up_lan_ovs_resp1,
  state_down_initialised,
  state_down_vhost_stopped,
  state_down_ovs_req_del_lan,
  state_down_ovs_resp_del_lan,
};

/*--------------------------------------------------------------------------*/
static t_a2b_cnx *g_head_a2b;
static int g_nb_a2b;
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
    case state_up_lan_ovs_req0:
      strcpy(label, "state_up_lan_ovs_req0");
      break;
    case state_up_lan_ovs_resp0:
      strcpy(label, "state_up_lan_ovs_resp0");
      break;
    case state_up_lan_ovs_req1:
      strcpy(label, "state_up_lan_ovs_req1");
      break;
    case state_up_lan_ovs_resp1:
      strcpy(label, "state_up_lan_ovs_resp1");
      break;
    default:
      KOUT("%d", state);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void state_progress_up(t_a2b_cnx *cur, int state)
{
  char *locnet = cfg_get_cloonix_name();
  char olab[MAX_NAME_LEN];
  char nlab[MAX_NAME_LEN];
  nb_to_text(cur->state_up, olab);
  cur->state_up = state;
  nb_to_text(cur->state_up, nlab);
//  KERR("%s %s  %s ----> %s", locnet, cur->name, olab, nlab);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_a2b_cnx *find_a2b(char *name)
{ 
  t_a2b_cnx *cur = g_head_a2b;
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
static t_a2b_cnx *alloc_a2b(int llid, int tid, char *name)
{
  t_a2b_cnx *cur = (t_a2b_cnx *)clownix_malloc(sizeof(t_a2b_cnx), 5);
  memset(cur, 0, sizeof(t_a2b_cnx));
  memcpy(cur->name, name, MAX_NAME_LEN-1);
  cur->add_llid = llid;
  cur->add_tid = tid;
  if (g_head_a2b)
    g_head_a2b->prev = cur;
  cur->next = g_head_a2b;
  g_head_a2b = cur;
  g_nb_a2b += 1;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_a2b(t_a2b_cnx *cur)
{
  char name[MAX_NAME_LEN];
  char lan0[MAX_NAME_LEN];
  char lan1[MAX_NAME_LEN];
  int i;
  memset(name, 0, MAX_NAME_LEN);
  memset(lan0, 0, MAX_NAME_LEN);
  memset(lan1, 0, MAX_NAME_LEN);
  strncpy(name, cur->name, MAX_NAME_LEN-1); 
  strncpy(lan0, cur->side[0].lan, MAX_NAME_LEN-1); 
  strncpy(lan1, cur->side[1].lan, MAX_NAME_LEN-1); 
  for (i=0; i<2; i++)
    cur->side[i].attached_lan_ok = 0; 
  if (strlen(lan0))
    {
    memset(cur->side[0].lan, 0, MAX_NAME_LEN);
    dpdk_msg_vlan_exist_no_more(lan0);
    }
  if (strlen(lan1))
    {
    memset(cur->side[1].lan, 0, MAX_NAME_LEN);
    dpdk_msg_vlan_exist_no_more(lan1);
    }
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_head_a2b == cur)
    g_head_a2b = cur->next;
  clownix_free(cur, __FUNCTION__);
  g_nb_a2b -= 1;
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_a2b_msg_beat(void *data)
{
  t_a2b_cnx *next, *cur = g_head_a2b;
  int i;
  while(cur)
    {
    next = cur->next;
    if ((cur->process_started_and_running == 1) &&
        (cur->state_up == state_up_process_started)) 
      {
      a2b_dpdk_start_vhost(cur->name);
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
    for (i=0; i<2; i++)
      {
      if ((cur->side[i].waiting_ack_add_lan == 1) ||
          (cur->side[i].waiting_ack_del_lan == 1))
        {
        cur->side[i].timer_count += 1;
        if (cur->side[i].timer_count > 20)
          {
          KERR("ERROR TIMEOUT %s add:%d del:%d", cur->name,
               cur->side[i].waiting_ack_add_lan,
               cur->side[i].waiting_ack_del_lan);
          cur->side[i].timer_count = 0;
          cur->to_be_destroyed = 1;
          }
        }
      }
    if (cur->to_be_destroyed == 1)
      {
      if ((cur->side[0].waiting_ack_add_lan == 1)  ||
          (cur->side[0].attached_lan_ok == 1)      ||
          (cur->side[1].waiting_ack_add_lan == 1)  ||
          (cur->side[1].attached_lan_ok == 1))
        cur->to_be_destroyed_count += 1;
      else
        a2b_dpdk_start_stop_process(cur->name, 0);
      if (cur->to_be_destroyed_count == 20)
        {
        KERR("ERROR TIMEOUT %s", cur->name);
        a2b_dpdk_start_stop_process(cur->name, 0);
        }
      }
    cur = next;
    }
  clownix_timeout_add(50, timer_a2b_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_a2b_get_qty(void)
{
  int result = 0;
  t_a2b_cnx *cur = g_head_a2b; 
  while(cur)
    {
    result += 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_a2b_resp_add_lan(int is_ko, char *lan, char *name, int num)
{
  t_a2b_cnx *cur = find_a2b(name);
  if ((num != 0) && (num != 1))
    KERR("ERROR %s %s %d", name, lan, num);
  else if (!cur)
    KERR("ERROR %s %s %d", name, lan, num);
  else if (strcmp(cur->side[num].lan, lan))
    KERR("ERROR %s %s %s %d", lan, name, cur->side[num].lan, num);
  else
    {
    if (cur->side[num].waiting_ack_add_lan == 0)
      KERR("ERROR %d %s %s", is_ko, lan, name);
    if (cur->side[num].waiting_ack_del_lan == 1)
      KERR("ERROR %d %s %s", is_ko, lan, name);
    if (is_ko)
      {
      KERR("ERROR %s %s", name, lan);
      cur->side[num].waiting_ack_add_lan = 0;
      utils_send_status_ko(&(cur->side[num].llid),
                           &(cur->side[num].tid), "openvswitch error");
      }
    else
      {
      utils_send_status_ok(&(cur->side[num].llid), &(cur->side[num].tid));
      cur->side[num].waiting_ack_add_lan = 0;
      cur->side[num].attached_lan_ok = 1;
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      if (num == 0)
        state_progress_up(cur, state_up_lan_ovs_resp0);
      else
        state_progress_up(cur, state_up_lan_ovs_resp1);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_a2b_resp_del_lan(int is_ko, char *lan, char *name, int num)
{
  t_a2b_cnx *cur = find_a2b(name);
  if ((num != 0) && (num != 1))
    KERR("ERROR %s %s %d", name, lan, num);
  else if (!cur)
    KERR("ERROR %s %s", lan, name);
  else if (strcmp(lan, cur->side[num].lan))
    KERR("ERROR %s %s %s", lan, name, cur->side[num].lan);
  else
    {
    if (cur->side[num].attached_lan_ok == 1) 
      KERR("ERROR %d %s %s", is_ko, lan, name);
    if (cur->side[num].waiting_ack_add_lan == 1)
      KERR("ERROR %d %s %s", is_ko, lan, name);
    if (cur->side[num].waiting_ack_del_lan == 0)
      KERR("ERROR %d %s %s", is_ko, lan, name);
    cur->side[num].attached_lan_ok = 0; 
    cur->side[num].waiting_ack_del_lan = 0;
    memset(cur->side[num].lan, 0, MAX_NAME_LEN);
    utils_send_status_ok(&(cur->side[num].llid),&(cur->side[num].tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_a2b_event_from_a2b_dpdk_process(char *name, int on)
{
  t_a2b_cnx *cur = find_a2b(name);
  int i;
  char *lan;
  if (cur)
    {
    if ((on == -1) || (on == 0))
      {
      cur->to_be_destroyed = 1;
      if (on == -1)
        {
        KERR("ERROR %s", name);
        for (i=0; i<2; i++)
          {
          lan = cur->side[i].lan;
          if (strlen(lan))
            {
            dpdk_msg_send_del_lan_a2b(lan, name, i);
            KERR("ERROR %s %s %d", name, lan, i);
            }
          }
        }
      if (cur->add_llid)
        {
        KERR("ERROR %s", name);
        utils_send_status_ko(&(cur->add_llid), &(cur->add_tid), "error");
        }
      free_a2b(cur);
      }
    else
      {
      for (i=0; i<2; i++)
        {
        lan = cur->side[i].lan;
        if (strlen(lan))
          {    
          cur->side[i].attached_lan_ok = 0;
          }
        }
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
int dpdk_a2b_add_lan(int llid, int tid, char *name, int num, char *lan)
{
 int result = -1;
  t_a2b_cnx *cur = find_a2b(name);
  if (cur == NULL)
    {
    KERR("ERROR %s %d %s", name, num, lan);
    }
  else if ((num != 0) && (num != 1))
    {
    KERR("ERROR %s %d %s", name, num, lan);
    }
  else if (strlen(cur->side[num].lan))
    {
    KERR("ERROR %s %s %s", name, lan, cur->side[num].lan);
    }
  else if (cur->vhost_started_and_running == 0)
    {
    KERR("ERROR %s %d %s", name, num, lan);
    }
  else if (dpdk_msg_send_add_lan_a2b(lan, name, num))
    {
    KERR("ERROR %s %d %s", name, num, lan);
    }
  else
    {
    memset(cur->side[num].lan, 0, MAX_NAME_LEN);
    strncpy(cur->side[num].lan, lan, MAX_NAME_LEN-1);
    cur->side[num].waiting_ack_add_lan = 1;
    cur->side[num].llid = llid;
    cur->side[num].tid = tid;
    if (num == 0)
      state_progress_up(cur, state_up_lan_ovs_req0);
    else
      state_progress_up(cur, state_up_lan_ovs_req1);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_a2b_del_lan(int llid, int tid, char *name, int num, char *lan)
{
 int result = -1;
  t_a2b_cnx *cur = find_a2b(name);
  if (cur == NULL)
    {
    KERR("ERROR %s %d %s", name, num, lan);
    }
  else if (!strlen(cur->side[num].lan))
    {
    KERR("ERROR %s %s %s", name, lan, cur->side[num].lan);
    }
  else if (strcmp(lan, cur->side[num].lan))
    {
    KERR("ERROR %s %d %s", name, num, lan);
    }
  else if ((cur->side[num].waiting_ack_add_lan == 0) && 
           (cur->side[num].attached_lan_ok == 0))
    {
    KERR("ERROR %s %d %s", name, num, lan);
    }
  else 
    {
    if (cur->side[num].waiting_ack_del_lan == 1)
      KERR("ERROR %s %d %s", name, num, lan);
    if (dpdk_msg_send_del_lan_a2b(lan, name, num))
      KERR("ERROR %s %d %s", name, num, lan);
    cur->side[num].attached_lan_ok = 0;
    cur->side[num].waiting_ack_del_lan = 1;
    cur->side[num].llid = llid;
    cur->side[num].tid = tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_a2b_add(int llid, int tid, char *name)
{
  int result = -1;
  t_a2b_cnx *cur = find_a2b(name);
  if (cur)
    KERR("ERROR %s", name);
  else
    {
    cur = alloc_a2b(llid, tid, name);
    a2b_dpdk_start_stop_process(name, 1);
    result = 0;
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    state_progress_up(cur, state_up_process_req);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_a2b_del(char *name)
{
  int i, result = -1;
  t_a2b_cnx *cur = find_a2b(name);
  char *lan;
  if (!cur)
    KERR("ERROR %s", name);
  else
    {
    a2b_dpdk_stop_vhost(name);
    for (i=0; i<2; i++)
      {
      lan = cur->side[i].lan;
      if ((strlen(lan)) &&
          ((cur->side[i].waiting_ack_add_lan == 1) ||
           (cur->side[i].attached_lan_ok == 1)))
        {
        if (cur->side[i].waiting_ack_del_lan == 1)
          KERR("ERROR %s %d %s", name, i, lan);
        if (dpdk_msg_send_del_lan_a2b(lan, name, i))
          KERR("ERROR %s %d %s", name, i, lan);
        cur->side[i].attached_lan_ok = 0;
        cur->side[i].waiting_ack_del_lan = 1;
        }
      }
    cur->to_be_destroyed = 1;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_a2b_lan_exists(char *lan)
{
  t_a2b_cnx *cur = g_head_a2b;
  int i, result = 0;
  while(cur)
    {
    for (i=0; i<2; i++)
      {
      if ((!strcmp(lan, cur->side[i].lan)) &&
          ((cur->side[i].waiting_ack_add_lan == 1) ||
          (cur->side[i].attached_lan_ok == 1)))
        {
        result = 1;
        break;
        }
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_a2b_lan_exists_in_a2b(char *name, int num, char *lan)
{
  int result = 0;
  t_a2b_cnx *cur = find_a2b(name);
  if ((num != 0) && (num != 1))
    KERR("ERROR %d", num);
  else if(cur)
    {
    if (!strcmp(lan, cur->side[num].lan))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_a2b_end_ovs(void)
{
  t_a2b_cnx *next, *cur = g_head_a2b;
  while(cur)
    {
    next = cur->next;
    dpdk_a2b_del(cur->name);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_a2b_exists(char *name)
{
  int result = 0;
  t_a2b_cnx *cur = find_a2b(name);
  if (cur)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_a2b_cnx *dpdk_a2b_get_first(int *nb_a2b)
{
  *nb_a2b = g_nb_a2b;
  return g_head_a2b;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *translate_topo_endp_a2b(int *nb)
{
  t_a2b_cnx *cur = g_head_a2b;
  int i, len, nb_endp = 0;
  t_topo_endp *endp;
  len = (2 * g_nb_a2b) * sizeof(t_topo_endp);
  endp = (t_topo_endp *) clownix_malloc( len, 13);
  memset(endp, 0, len);
  while(cur)
    {
    for (i=0; i<2; i++)
      {
      len = sizeof(t_lan_group_item);
      endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
      memset(endp[nb_endp].lan.lan, 0, len);
      strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
      endp[nb_endp].num = i;
      endp[nb_endp].type = endp_type_a2b;
      if (strlen(cur->side[i].lan))
        {
        strncpy(endp[nb_endp].lan.lan[0].lan,cur->side[i].lan,MAX_NAME_LEN-1);
        endp[nb_endp].lan.nb_lan = 1;
        }
      nb_endp += 1;
      }
    cur = cur->next;
    }
  *nb = nb_endp;
  return endp;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_a2b_cnf(char *name, int dir, int type, int val)
{
  t_a2b_cnx *cur = find_a2b(name);
  if (!cur)
    {
    KERR("ERROR %s %d %d %d", name, dir, type, val);
    }
  else if ((dir != 0) && (dir != 1))
    {
    KERR("ERROR %s %d %d %d", name, dir, type, val);
    }
  else if (type == a2b_type_delay)
    {
    cur->side[dir].delay = val;
    a2b_dpdk_cnf(name, dir, type, val);
    }
  else if (type == a2b_type_loss)
    {
    cur->side[dir].loss = val;
    a2b_dpdk_cnf(name, dir, type, val);
    }
  else if (type == a2b_type_qsize)
    {
    cur->side[dir].qsize = val;
    a2b_dpdk_cnf(name, dir, type, val);
    }
  else if (type == a2b_type_bsize)
    {
    cur->side[dir].bsize = val;
    a2b_dpdk_cnf(name, dir, type, val);
    }
  else if (type == a2b_type_brate)
    {
    cur->side[dir].brate = val;
    a2b_dpdk_cnf(name, dir, type, val);
    }
  else
    KERR("ERROR %s %d %d %d", name, dir, type, val);
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_a2b_vhost_started(char *name)
{
  t_a2b_cnx *cur = find_a2b(name);
  if (!cur)
    KERR("ERROR %s", name);
  else
    {
    cur->vhost_started_and_running = 1;
    utils_send_status_ok(&(cur->add_llid),&(cur->add_tid));
    state_progress_up(cur, state_up_process_vhost_resp_on);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_a2b_init(void)
{
  g_head_a2b = NULL;
  g_nb_a2b = 0;
  clownix_timeout_add(50, timer_a2b_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
