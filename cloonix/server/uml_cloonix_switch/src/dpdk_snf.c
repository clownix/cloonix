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
typedef struct t_dlan
{
  char lan[MAX_NAME_LEN];
  int  waiting_ack_add_lan;
  int  waiting_ack_del_lan;
  int  timer_count_lan;
  int  llid;
  int  tid;
  int  snf_dpdk_start_process;
} t_dlan;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_dsnf
{
  char name[MAX_NAME_LEN];
  int to_be_destroyed;
  t_dlan *head_lan;
  struct t_dsnf *prev;
  struct t_dsnf *next;
} t_dsnf;
/*--------------------------------------------------------------------------*/

static t_dsnf *g_head_dsnf;
static int g_timer_snf_waiting;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dsnf *get_dsnf(char *name)
{ 
  t_dsnf *dsnf = g_head_dsnf;
  while(dsnf)
    {
    if (!strcmp(name, dsnf->name))
      break;
    dsnf = dsnf->next;
    }
  return dsnf;
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dlan *get_dlan(t_dsnf *dsnf, char *lan)
{
  t_dlan *cur = dsnf->head_lan;
  if ((cur) && (!strcmp(cur->lan, lan)))
    return cur;
  else
    return NULL;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dlan *alloc_dlan(t_dsnf *dsnf, char *lan)
{
  t_dlan *dlan = NULL;
  if (dsnf->head_lan)
    KERR("ALREADY LAN IN SNF %s %s", dsnf->name, lan);
  else
    {
    dsnf->head_lan = (t_dlan *) clownix_malloc(sizeof(t_dlan), 5);
    memset(dsnf->head_lan, 0, sizeof(t_dlan));
    memcpy(dsnf->head_lan->lan, lan, MAX_NAME_LEN-1);
    dlan = dsnf->head_lan;
    }
  return dlan;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_dlan(t_dsnf *dsnf)
{
  char lan[MAX_NAME_LEN];
  if (dsnf == NULL)
    KOUT(" ");
  if (dsnf->head_lan == NULL) 
    KOUT(" ");
  memset(lan, 0, MAX_NAME_LEN);
  strncpy(lan, dsnf->head_lan->lan, MAX_NAME_LEN-1);
  clownix_free(dsnf->head_lan, __FUNCTION__);
  dsnf->head_lan = NULL;
  edp_evt_lan_del_done(eth_type_dpdk, lan);
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
  if ((!dpdk_dyn_lan_exists(lan)) && (!dpdk_snf_lan_exists(lan)))
    dpdk_msg_vlan_exist_no_more(lan);
  snf_dpdk_process_possible_change(lan);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dsnf *alloc_dsnf(char *name)
{
  t_dsnf *dsnf = (t_dsnf *)clownix_malloc(sizeof(t_dsnf), 5);
  memset(dsnf, 0, sizeof(t_dsnf));
  memcpy(dsnf->name, name, MAX_NAME_LEN-1);
  if (g_head_dsnf)
    g_head_dsnf->prev = dsnf;
  dsnf->next = g_head_dsnf;
  g_head_dsnf = dsnf;
  return dsnf;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_dsnf(char *name)
{
  t_dsnf *dsnf = get_dsnf(name);
  if (!dsnf)
    KERR("%s", name);
  else
    {
    if (dsnf->head_lan) 
      free_dlan(dsnf);
    if (dsnf->prev)
      dsnf->prev->next = dsnf->next;
    if (dsnf->next)
      dsnf->next->prev = dsnf->prev;
    if (g_head_dsnf == dsnf)
      g_head_dsnf = dsnf->next;
    clownix_free(dsnf, __FUNCTION__);
    }
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_snf_msg_beat(void *data)
{
  t_dsnf *next, *dsnf = g_head_dsnf;
  t_dlan *cur;
  while(dsnf)
    {
    next = dsnf->next;
    cur = dsnf->head_lan;
    if (cur)
      {
      if ((cur->waiting_ack_add_lan != 0) || (cur->waiting_ack_del_lan != 0))
        {
        cur->timer_count_lan += 1;
        if (cur->timer_count_lan > 15)
          {
          KERR("Time %s", dsnf->name);
          if (dsnf->to_be_destroyed == 1)
            free_dsnf(dsnf->name);
          }
        }
      }
    dsnf = next;
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
  result += g_timer_snf_waiting;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void resp_lan(int is_add, int is_ko, char *lan, char *name)
{
  t_dsnf *dsnf = get_dsnf(name);
  t_dlan *dlan;
  if (dsnf)
    {
    dlan = get_dlan(dsnf, lan);
    if (!dlan)
      KERR("%d %s %s", is_ko, lan, name);
    else
      {
      if (is_add)
        {
        if ((dlan->waiting_ack_add_lan == 0) ||
            (dlan->waiting_ack_del_lan == 1))
          KERR("%d %s %s %d", is_ko, lan, name, dlan->waiting_ack_add_lan);
        }
      else
        {
        if ((dlan->waiting_ack_add_lan == 1) ||
            (dlan->waiting_ack_del_lan == 0))
          KERR("%d %s %s %d", is_ko, lan, name, dlan->waiting_ack_del_lan);
        }
      if (is_ko)
        {
        KERR("Resp KO %s %s", lan, dsnf->name);
        send_status_ko(dlan->llid, dlan->tid, "Resp KO");
        free_dlan(dsnf);
        }
      else
        {
        dlan->waiting_ack_add_lan = 0;
        dlan->waiting_ack_del_lan = 0;
        send_status_ok(dlan->llid, dlan->tid, "OK");
        dlan->timer_count_lan = 0;
        dlan->llid = 0;
        dlan->tid = 0;
        if (is_add == 0)
          {
          if (dsnf->to_be_destroyed == 1)
            free_dsnf(dsnf->name);
          else
            free_dlan(dsnf);
          }
        else
          {
          if (dlan->snf_dpdk_start_process)
            KERR("%s %s", name, lan);
          else
            {
            dlan->snf_dpdk_start_process = 1;
            snf_dpdk_start_process(name, lan, 1);
            }
          edp_evt_lan_add_done(eth_type_dpdk, lan);
          event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
          snf_dpdk_process_possible_change(lan);
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_snf_resp_add_lan(int is_ko, char *lan, char *name)
{
  resp_lan(1, is_ko, lan, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_snf_resp_del_lan(int is_ko, char *lan, char *name)
{
  resp_lan(0, is_ko, lan, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_snf_add_lan(int llid, int tid, char *lan, char *name)
{
  int result = -1;
  t_dsnf *dsnf = get_dsnf(name);
  t_dlan *dlan;
  if (dsnf == NULL)
    KERR("%s %s", lan, name);
  else
    {
    dlan = get_dlan(dsnf, lan);
    if (dlan != NULL)
      KERR("%s %s", lan, name);
    else
      {
      if (dpdk_msg_send_add_lan_snf(lan, name))
        KERR("%s %s", lan, name);
      else
        {
        dlan = alloc_dlan(dsnf, lan);
        if (dlan)
          {
          dlan->waiting_ack_add_lan = 1;
          dlan->timer_count_lan = 0;
          dlan->llid = llid;
          dlan->tid = tid;
          result = 0;
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int dpdk_snf_del_ovs_lan(char *lan, char *name)
{
  int result = -1;
  t_dsnf *dsnf = get_dsnf(name);
  t_dlan *dlan;
  if (dsnf == NULL)
    KERR("%s %s", lan, name);
  else
    {
    dlan = get_dlan(dsnf, lan);
    if (dlan == NULL)
      KERR("%s %s", lan, name);
    else if (dlan->snf_dpdk_start_process == 1)
      KERR("%s %s", lan, name);
    else if (dlan->waiting_ack_del_lan)
      KERR("%s %s", lan, name);
    else if (dpdk_msg_send_del_lan_snf(lan, name))
      KERR("%s %s", lan, name);
    else
      {
      dlan->waiting_ack_del_lan = 1;
      dlan->timer_count_lan = 0;
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_snf_add(int llid, int tid, char *name)
{
  int result = -1;
  t_dsnf *dsnf = get_dsnf(name);
  if (dsnf)
    KERR("%s", name);
  else
    {
    dsnf = alloc_dsnf(name);
    send_status_ok(llid, tid, "OK");
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_snf_event_from_snf_dpdk_process(char *name, char *lan, int on)
{
  t_dsnf *dsnf = get_dsnf(name);
  t_dlan *cur;
  if (!dsnf)
    KERR("ERROR TODO %s %s", name, lan);
  else if (on == -1)
    {
    KERR("ERROR %s %s", name, lan);
    cur = dsnf->head_lan;
    if (cur)
      {
      cur->snf_dpdk_start_process = 0;
      if (strcmp(cur->lan, lan))
        KERR("%s %s %s", name, lan, cur->lan);
      if (dpdk_snf_del_ovs_lan(cur->lan, name))
        KERR("%s %s", name, lan);
      free_dsnf(name);
      }
    else
      KERR("ERROR TODO %s %s", name, lan);
    }
  else if (on == 0)
    {
    cur = dsnf->head_lan;
    if (cur)
      {
      cur->snf_dpdk_start_process = 0;
      if (strcmp(cur->lan, lan))
        KERR("%s %s %s", name, lan, cur->lan);
      if (dpdk_snf_del_ovs_lan(cur->lan, name))
        KERR("%s %s", name, lan);
      free_dsnf(name);
      }
    else
      KERR("ERROR TODO %s %s", name, lan);
    }
  else
    {
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_snf_del(char *name)
{
  t_dsnf *dsnf = get_dsnf(name);
  t_dlan *cur;
  if (!dsnf)
    KERR("%s", name);
  else
    {
    cur = dsnf->head_lan;
    if (cur)
      {
      dsnf->to_be_destroyed = 1;
      if (cur->snf_dpdk_start_process == 1)
        {
        snf_dpdk_start_process(name, cur->lan, 0);
        cur->snf_dpdk_start_process = 0;
        }
      else
        {
        if (dpdk_snf_del_ovs_lan(cur->lan, name))
          KERR("%s %s", name, cur->lan);
        }
      }
    else
      free_dsnf(name);
    }
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
  t_dsnf *dsnf = g_head_dsnf;
  int result = 0;
  while(dsnf)
    {
    if (get_dlan(dsnf, lan) != NULL)
      result = 1;
    dsnf = dsnf->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_snf_lan_exists_in_snf(char *name, char *lan)
{
  int result = 0;
  t_dsnf *dsnf = get_dsnf(name);
  t_dlan *dlan;
  if(dsnf)
    {
    dlan = get_dlan(dsnf, lan);
    if (dlan != NULL)
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_snf_end_ovs(void)
{
  t_dsnf *ndsnf, *dsnf = g_head_dsnf;
  while(dsnf)
    {
    ndsnf = dsnf->next;
    dpdk_snf_del(dsnf->name);
    dsnf = ndsnf;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_snf_init(void)
{
  g_head_dsnf = NULL;
  clownix_timeout_add(50, timer_snf_msg_beat, NULL, NULL, NULL);
  g_timer_snf_waiting = 0;
}
/*--------------------------------------------------------------------------*/
