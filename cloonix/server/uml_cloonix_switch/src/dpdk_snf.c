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
#include "dpdk_snf.h"
#include "dpdk_msg.h"
#include "fmt_diag.h"
#include "qmp.h"
#include "system_callers.h"
#include "stats_counters.h"
#include "dpdk_msg.h"
#include "edp_mngt.h"
#include "edp_evt.h"
#include "snf_dpdk_process.h"


/****************************************************************************/
typedef struct t_dsnf
{
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  int  waiting_ack_add_lan;
  int  waiting_ack_del_lan;
  int  timer_count;
  int  llid;
  int  tid;
  int  var_dpdk_start_stop_process;
  int to_be_destroyed;
  struct t_dsnf *prev;
  struct t_dsnf *next;
} t_dsnf;
/*--------------------------------------------------------------------------*/

static t_dsnf *g_head_dsnf;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dsnf *get_dsnf(char *name)
{ 
  t_dsnf *cur = g_head_dsnf;
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
static t_dsnf *alloc_dsnf(char *name, char *lan)
{
  t_dsnf *cur = (t_dsnf *)clownix_malloc(sizeof(t_dsnf), 5);
  memset(cur, 0, sizeof(t_dsnf));
  memcpy(cur->name, name, MAX_NAME_LEN-1);
  memcpy(cur->lan, lan, MAX_NAME_LEN-1);
  if (g_head_dsnf)
    g_head_dsnf->prev = cur;
  cur->next = g_head_dsnf;
  g_head_dsnf = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_dsnf(t_dsnf *cur)
{
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  memset(name, 0, MAX_NAME_LEN);
  memset(lan, 0, MAX_NAME_LEN);
  strncpy(name, cur->name, MAX_NAME_LEN-1);
  strncpy(lan, cur->lan, MAX_NAME_LEN-1);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_head_dsnf == cur)
    g_head_dsnf = cur->next;
  clownix_free(cur, __FUNCTION__);
  dpdk_msg_vlan_exist_no_more(lan);
  edp_evt_update_non_fix_del(eth_type_dpdk, name, lan);
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_snf_msg_beat(void *data)
{
  t_dsnf *next, *cur = g_head_dsnf;
  while(cur)
    {
    next = cur->next;
    if ((cur->waiting_ack_add_lan != 0) ||
        (cur->waiting_ack_del_lan != 0))
        {
        cur->timer_count += 1;
        if (cur->timer_count > 15)
          {
          KERR("Time %s", cur->name);
          if (cur->to_be_destroyed == 1)
            {
            if (cur->var_dpdk_start_stop_process)
              {
              snf_dpdk_start_stop_process(cur->name, cur->lan, 0);
              cur->var_dpdk_start_stop_process = 0;
              }
            free_dsnf(cur);
            }
          }
        }
    cur = next;
    }
  clownix_timeout_add(50, timer_snf_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_snf_get_qty(void)
{
  int result = 0;
  t_dsnf *cur = g_head_dsnf; 
  while(cur)
    {
    result += 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_snf_resp_add_lan(int is_ko, char *lan, char *name)
{
  t_dsnf *cur = get_dsnf(name);
  if (!cur)
    KERR("%s %s", name, lan);
  else if (strcmp(lan, cur->lan))
    KERR("%s %s %s", lan, name, cur->lan);
  else
    {
    if (cur->waiting_ack_add_lan == 0)
      KERR("%d %s %s", is_ko, lan, name);
    if (cur->waiting_ack_del_lan == 1)
      KERR("%d %s %s", is_ko, lan, name);
    if (is_ko)
      {
      KERR("%s %s", name, lan);
      cur->waiting_ack_add_lan = 0;
      utils_send_status_ko(&(cur->llid), &(cur->tid), "openvswitch error");
      }
    else
      {
      if (cur->var_dpdk_start_stop_process)
        {
        KERR("%s %s", name, lan);
        cur->waiting_ack_add_lan = 0;
        utils_send_status_ko(&(cur->llid), &(cur->tid), "not coherent");
        }
      else
        {
        cur->waiting_ack_add_lan = 1;
        cur->var_dpdk_start_stop_process = 1;
        snf_dpdk_start_stop_process(name, lan, 1);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_snf_resp_del_lan(int is_ko, char *lan, char *name)
{
  t_dsnf *cur = get_dsnf(name);
  if (!cur)
    {
    KERR("%s %s", lan, name);
    }
  else if (strcmp(lan, cur->lan))
    KERR("%s %s %s", lan, name, cur->lan);
  else
    {
    if (cur->waiting_ack_add_lan == 1)
      KERR("%d %s %s", is_ko, lan, name);
    if (cur->waiting_ack_del_lan == 0)
      KERR("%d %s %s", is_ko, lan, name);
    cur->waiting_ack_del_lan = 1;
    if (cur->var_dpdk_start_stop_process == 0)
      {
      KERR("ERROR %s %s", name, cur->lan);
      utils_send_status_ko(&(cur->llid), &(cur->tid), "not coherent");
      free_dsnf(cur);
      }
    else
      {
      snf_dpdk_start_stop_process(name, cur->lan, 0);
      cur->var_dpdk_start_stop_process = 0;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_snf_event_from_snf_dpdk_process(char *name, char *lan, int on)
{
  t_dsnf *cur = get_dsnf(name);
  if (!cur)
    {
    KERR("%s %s", name, lan);
    }
  else if (strcmp(cur->lan, lan))
    {
    KERR("%s %s %s", name, lan, cur->lan);
    }
  else
    {
    if ((on == -1) || (on == 0))
      {
      if (on == -1)
        KERR("ERROR %s %s", name, lan);
      cur->var_dpdk_start_stop_process = 0;
      if (cur->waiting_ack_del_lan)
        {
        utils_send_status_ok(&(cur->llid), &(cur->tid));
        }
      free_dsnf(cur);
      }
    else
      {
      if (cur->var_dpdk_start_stop_process == 0)
        KERR("ERROR %s %s", name, lan);
      if (cur->waiting_ack_add_lan == 0)
        KERR("ERROR %s %s", name, lan);
      cur->waiting_ack_add_lan = 0;
      edp_evt_update_non_fix_add(eth_type_dpdk, name, lan);
      snf_dpdk_process_possible_change();
      utils_send_status_ok(&(cur->llid), &(cur->tid));
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_snf_add(int llid, int tid, char *name, char *lan)
{
  int result = -1;
  t_dsnf *cur = get_dsnf(name);
  if (cur)
    {
    KERR("%s", name);
    }
  else if (dpdk_msg_send_add_lan_snf(lan, name))
    {
    KERR("%s %s", lan, name);
    }
  else
    {
    cur = alloc_dsnf(name, lan);
    cur->waiting_ack_add_lan = 1;
    cur->llid = llid;
    cur->tid = tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_snf_del(int llid, int tid, char *name)
{
  int result = -1;
  t_dsnf *cur = get_dsnf(name);
  if (!cur)
    {
    KERR("%s", name);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "not found");
    }
  else if (cur->waiting_ack_add_lan)
    {
    KERR("%s %s", name, cur->lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "wait add");
    }
  else if (cur->waiting_ack_del_lan)
    {
    KERR("%s %s", name, cur->lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "wait del");
    }
  else if (dpdk_msg_send_del_lan_snf(cur->lan, name))
    {
    KERR("%s %s", cur->lan, name);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "ovs no-connect");
    }
  else
    {
    cur->to_be_destroyed = 1;
    cur->waiting_ack_del_lan = 1;
    cur->timer_count = 0;
    cur->llid = llid;
    cur->tid = tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_snf_exist(char *name)
{
  int result = 0;
  if (get_dsnf(name))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_snf_lan_exists(char *lan)
{
  t_dsnf *cur = g_head_dsnf;
  int result = 0;
  while(cur)
    {
    if (!strcmp(cur->lan, lan))
      result = 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_snf_lan_exists_in_snf(char *name, char *lan)
{
  int result = 0;
  t_dsnf *cur = get_dsnf(name);
  if(cur)
    {
    if (!strcmp(cur->lan, lan))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_snf_end_ovs(void)
{
  t_dsnf *next, *cur = g_head_dsnf;
  while(cur)
    {
    next = cur->next;
    dpdk_snf_del(0, 0, cur->name);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_snf_init(void)
{
  g_head_dsnf = NULL;
  clownix_timeout_add(50, timer_snf_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
