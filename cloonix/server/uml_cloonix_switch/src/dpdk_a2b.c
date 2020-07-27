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
#include "endp_mngt.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "llid_trace.h"
#include "machine_create.h"
#include "file_read_write.h"
#include "dpdk_ovs.h"
#include "dpdk_dyn.h"
#include "dpdk_a2b.h"
#include "dpdk_d2d.h"
#include "dpdk_msg.h"
#include "fmt_diag.h"
#include "qmp.h"
#include "system_callers.h"
#include "stats_counters.h"
#include "dpdk_msg.h"
#include "edp_mngt.h"
#include "edp_evt.h"
#include "a2b_dpdk_process.h"
#include "snf_dpdk_process.h"
#include "suid_power.h"



/*--------------------------------------------------------------------------*/
static t_a2b_cnx *g_head_a2b;
static int g_nb_a2b;
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
  char name0[MAX_NAME_LEN];
  char name1[MAX_NAME_LEN];
  memset(name0, 0, MAX_NAME_LEN);
  memset(name1, 0, MAX_NAME_LEN);
  snprintf(name0, MAX_NAME_LEN-1, "%s0", name);
  snprintf(name1, MAX_NAME_LEN-1, "%s1", name);
  suid_power_rec_name(name0, 1);
  suid_power_rec_name(name1, 1);
  memset(cur, 0, sizeof(t_a2b_cnx));
  memcpy(cur->name, name, MAX_NAME_LEN-1);
  cur->add_llid = llid;
  cur->add_tid = tid;
  a2b_dpdk_start_stop_process(name, 1);
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
  char name0[MAX_NAME_LEN];
  char name1[MAX_NAME_LEN];
  char *nm = cur->name;
  memset(name0, 0, MAX_NAME_LEN);
  memset(name1, 0, MAX_NAME_LEN);
  memset(name, 0, MAX_NAME_LEN);
  memset(lan0, 0, MAX_NAME_LEN);
  memset(lan1, 0, MAX_NAME_LEN);
  snprintf(name0, MAX_NAME_LEN-1, "%s0", nm);
  snprintf(name1, MAX_NAME_LEN-1, "%s1", nm);
  suid_power_rec_name(name0, 0);
  suid_power_rec_name(name1, 0);
  strncpy(name, cur->name, MAX_NAME_LEN-1); 
  strncpy(lan0, cur->side[0].lan, MAX_NAME_LEN-1); 
  strncpy(lan1, cur->side[1].lan, MAX_NAME_LEN-1); 
  a2b_dpdk_start_stop_process(cur->name, 0);
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
    if ((cur->openvswitch_started_and_running == 0) &&
        (dpdk_ovs_muovs_ready()))
      {
      cur->openvswitch_started_and_running = 1;
      a2b_dpdk_start_vhost(cur->name);
      }
    if (cur->add_llid)
      {
      cur->timer_count += 1;
      if (cur->timer_count > 15)
        {
        KERR("TIMEOUT %s", cur->name);
        utils_send_status_ko(&(cur->add_llid), &(cur->add_tid), "timeout");
        cur->timer_count = 0;
        cur->to_be_destroyed = 1;
        }
      }
    for (i=0; i<2; i++)
      {
      if ((cur->side[i].waiting_ack_add_lan) ||
          (cur->side[i].waiting_ack_del_lan))
        {
        cur->side[i].timer_count += 1;
        if (cur->side[i].timer_count > 15)
          {
          KERR("TIMEOUT %s", cur->name);
          cur->side[i].timer_count = 0;
          cur->to_be_destroyed = 1;
          }
        }
      }
    if (cur->to_be_destroyed == 1)
      {
      free_a2b(cur);
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
    KERR("%s %s %d", name, lan, num);
  else if (!cur)
    KERR("%s %s %d", name, lan, num);
  else if (strcmp(cur->side[num].lan, lan))
    KERR("%s %s %s %d", lan, name, cur->side[num].lan, num);
  else
    {
    if (cur->side[num].waiting_ack_add_lan == 0)
      KERR("%d %s %s", is_ko, lan, name);
    if (cur->side[num].waiting_ack_del_lan == 1)
      KERR("%d %s %s", is_ko, lan, name);
    if (is_ko)
      {
      KERR("%s %s", name, lan);
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
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_a2b_resp_del_lan(int is_ko, char *lan, char *name, int num)
{
  t_a2b_cnx *cur = find_a2b(name);
  if ((num != 0) && (num != 1))
    KERR("%s %s %d", name, lan, num);
  else if (!cur)
    KERR("%s %s", lan, name);
  else if (strcmp(lan, cur->side[num].lan))
    KERR("%s %s %s", lan, name, cur->side[num].lan);
  else
    {
    if (cur->side[num].attached_lan_ok == 1) 
      KERR("%d %s %s", is_ko, lan, name);
    if (cur->side[num].waiting_ack_add_lan == 1)
      KERR("%d %s %s", is_ko, lan, name);
    if (cur->side[num].waiting_ack_del_lan == 0)
      KERR("%d %s %s", is_ko, lan, name);
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
      if (on == -1)
        KERR("ERROR %s", name);
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
        event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
        }
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
    KERR("%s", name);
    }
  else if (strlen(cur->side[num].lan))
    {
    KERR("%s %s %s", name, lan, cur->side[num].lan);
    }
  else if (cur->openvswitch_started_and_running == 0)
    {
    KERR("%s %s", lan, name);
    }
  else if (cur->vhost_started_and_running == 0)
    {
    KERR("%s %s", lan, name);
    }
  else if (dpdk_msg_send_add_lan_a2b(lan, name, num))
    {
    KERR("%s %s", lan, name);
    }
  else
    {
    memset(cur->side[num].lan, 0, MAX_NAME_LEN);
    strncpy(cur->side[num].lan, lan, MAX_NAME_LEN-1);
    cur->side[num].waiting_ack_add_lan = 1;
    cur->side[num].llid = llid;
    cur->side[num].tid = tid;
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
    KERR("%s", name);
    }
  else if (!strlen(cur->side[num].lan))
    {
    KERR("%s %s %s", name, lan, cur->side[num].lan);
    }
  else if (strcmp(lan, cur->side[num].lan))
    {
    KERR("%s %s", lan, name);
    }
  else if ((cur->side[num].waiting_ack_add_lan == 0) && 
           (cur->side[num].attached_lan_ok == 0))
    {
    KERR("%s %s", lan, name);
    }
  else if (cur->side[num].waiting_ack_del_lan == 1)
    {
    KERR("%s %s", lan, name);
    }
  else if (dpdk_msg_send_del_lan_a2b(lan, name, num))
    {
    KERR("%s %s", lan, name);
    }
  else
    {
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
    {
    KERR("%s", name);
    }
  else
    {
    cur = alloc_a2b(llid, tid, name);
    result = 0;
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    dpdk_ovs_start_openvswitch_if_not_done();
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_a2b_del(char *name)
{
  int result = -1;
  t_a2b_cnx *cur = find_a2b(name);
  if (!cur)
    {
    KERR("%s", name);
    }
  else
    {
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
          ((cur->side[i].waiting_ack_add_lan) ||
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
    KERR("%d", num);
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
int dpdk_a2b_eventfull(char *name, int num, int ms,
                       int ptx, int btx, int prx, int brx)
{
  int result = -1;
  t_a2b_cnx *cur = find_a2b(name);
  if (cur)
    {
    if ((num != 0) && (num != 1))
      KOUT("%d", num);
    cur->side[num].ms      = ms;
    cur->side[num].pkt_tx  += ptx;
    cur->side[num].pkt_rx  += prx;
    cur->side[num].byte_tx += btx;
    cur->side[num].byte_rx += brx;
    stats_counters_update_endp_tx_rx(cur->name, num, ms, ptx, btx, prx, brx);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_a2b_collect_dpdk(t_eventfull_endp *eventfull)
{
  int i, result = 0;;
  t_a2b_cnx *cur = g_head_a2b;
  while(cur)
    {
    for (i=0; i<2; i++)
      {
      if (strlen(cur->side[i].lan))
        {
        strncpy(eventfull[result].name, cur->name, MAX_NAME_LEN-1);
        eventfull[result].type = endp_type_a2b;
        eventfull[result].num  = 0;
        eventfull[result].ms   = cur->side[i].ms;
        eventfull[result].ptx  = cur->side[i].pkt_tx;
        eventfull[result].prx  = cur->side[i].pkt_rx;
        eventfull[result].btx  = cur->side[i].byte_tx;
        eventfull[result].brx  = cur->side[i].byte_rx;
        cur->side[i].pkt_tx  = 0;
        cur->side[i].pkt_rx  = 0;
        cur->side[i].byte_tx = 0;
        cur->side[i].byte_rx = 0;
        result += 1;
        }
      }
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

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
t_topo_endp *dpdk_a2b_mngt_translate_topo_endp(int *nb)
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
    KERR("%s %d %d %d", name, dir, type, val);
    }
  else if ((dir != 0) && (dir != 1))
    {
    KERR("%s %d %d %d", name, dir, type, val);
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
    {
    KERR("%s %d %d %d", name, dir, type, val);
    }
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_a2b_vhost_started(char *name)
{
  t_a2b_cnx *cur = find_a2b(name);
  if (!cur)
    KERR("%s", name);
  else
    {
    cur->vhost_started_and_running = 1;
    utils_send_status_ok(&(cur->add_llid),&(cur->add_tid));
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
