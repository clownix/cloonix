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
#include "dpdk_tap.h"
#include "dpdk_d2d.h"
#include "dpdk_a2b.h"
#include "dpdk_nat.h"
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
#include "tabmac.h"

/****************************************************************************/
typedef struct t_dtap
{
  char name[MAX_NAME_LEN];
  char strmac[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  int waiting_ack_add_tap;
  int waiting_ack_del_tap;
  int waiting_ack_add_lan;
  int waiting_ack_del_lan;
  int timer_count;
  int llid;
  int tid;
  int ms;
  int pkt_tx;
  int pkt_rx;
  int byte_tx;
  int byte_rx;
  struct t_dtap *prev;
  struct t_dtap *next;
} t_dtap;
/*--------------------------------------------------------------------------*/

static t_dtap *g_head_dtap;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dtap *get_dtap(char *name)
{ 
  t_dtap *cur = g_head_dtap;
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
static t_dtap *alloc_dtap(char *name, char *lan)
{
  t_dtap *cur = (t_dtap *)clownix_malloc(sizeof(t_dtap), 5);
  memset(cur, 0, sizeof(t_dtap));
  memcpy(cur->name, name, MAX_NAME_LEN-1);
  memcpy(cur->lan, lan, MAX_NAME_LEN-1);
  if (g_head_dtap)
    g_head_dtap->prev = cur;
  cur->next = g_head_dtap;
  g_head_dtap = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_dtap(t_dtap *cur)
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
  if (g_head_dtap == cur)
    g_head_dtap = cur->next;
  clownix_free(cur, __FUNCTION__);
  edp_evt_update_non_fix_del(eth_type_dpdk, name, lan);
  dpdk_msg_vlan_exist_no_more(lan);
  tabmac_process_possible_change();
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_tap_msg_beat(void *data)
{
  t_dtap *next, *cur = g_head_dtap;
  while(cur)
    {
    next = cur->next;
    if ((cur->waiting_ack_add_tap != 0) ||
        (cur->waiting_ack_del_tap != 0) ||
        (cur->waiting_ack_add_lan != 0) ||
        (cur->waiting_ack_del_lan != 0))
      {
      cur->timer_count += 1;
      if (cur->timer_count > 20)
        {
        KERR("Timeout %s  addtap:%d deltap:%d addlan:%d dellan:%d",
             cur->name, cur->waiting_ack_add_tap, cur->waiting_ack_del_tap,
             cur->waiting_ack_add_lan, cur->waiting_ack_del_lan);
        utils_send_status_ko(&(cur->llid), &(cur->tid), "timeout error");
        free_dtap(cur);
        }
      }
    cur = next;
    }
  clownix_timeout_add(50, timer_tap_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_get_qty(void)
{
  int result = 0;
  t_dtap *cur = g_head_dtap; 
  while(cur)
    {
    result += 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_resp_del(int is_ko, char *name)
{
  t_dtap *cur = get_dtap(name);
  if (cur == NULL)
    KERR("%s %d", name, is_ko);
  else
    {
    if (is_ko)
      utils_send_status_ko(&(cur->llid), &(cur->tid), "openvswitch error");
    else
      utils_send_status_ok(&(cur->llid), &(cur->tid));
    if (cur->waiting_ack_add_tap != 0)
      KERR("%s", name);
    free_dtap(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void resp_lan_add(t_dtap *cur, int is_ko, char *lan, char *name)
{
  if (is_ko)
    {
    KERR("%s %s", name, lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "openvswitch error");
    }
  else
    {
    edp_evt_update_non_fix_add(eth_type_dpdk, name, lan);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    tabmac_process_possible_change();
    utils_send_status_ok(&(cur->llid), &(cur->tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void resp_lan_del(t_dtap *cur, int is_ko, char *lan, char *name)
{ 
  if (is_ko)
    {
    KERR("%s %s", name, lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "openvswitch error");
    }
  else if (cur->waiting_ack_del_tap != 0)
    {
    KERR("%s %s", name, lan);
    }
  else if (fmt_tx_del_tap(0, name))
    {
    KERR("%s %s", name, lan);
    free_dtap(cur);
    }
  else
    {
    cur->waiting_ack_del_tap = 1;
    cur->timer_count = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void resp_lan(int is_add, int is_ko, char *lan, char *name)
{
  t_dtap *cur = get_dtap(name);
  if (cur)
    {
    if (strcmp(lan, cur->lan))
      KERR("%s %s %s", lan, name, cur->lan);
    else 
      {
      if (is_add)
        {
        if ((cur->waiting_ack_add_lan == 0) ||
            (cur->waiting_ack_del_lan == 1))
          KERR("%d %s %s %d", is_ko, lan, name, cur->waiting_ack_add_lan);
        resp_lan_add(cur, is_ko, lan, name);
        }
      else
        {
        if ((cur->waiting_ack_add_lan == 1) ||
            (cur->waiting_ack_del_lan == 0))
          KERR("%d %s %s %d", is_ko, lan, name, cur->waiting_ack_del_lan);
        resp_lan_del(cur, is_ko, lan, name);
        }
      cur->waiting_ack_add_lan = 0;
      cur->waiting_ack_del_lan = 0;
      cur->timer_count = 0;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_resp_add_lan(int is_ko, char *lan, char *name)
{
  resp_lan(1, is_ko, lan, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_resp_del_lan(int is_ko, char *lan, char *name)
{
  resp_lan(0, is_ko, lan, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_resp_add(int is_ko, char *name, char *strmac)
{
  t_dtap *cur = get_dtap(name);
  if (cur == NULL)
    KERR("%s", name);
  else if (!cur->llid)
    KERR("%s", name);
  else if (is_ko)
    {
    KERR("%s", cur->name);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "openvswitch error");
    free_dtap(cur);
    }
  else 
    {
    if (dpdk_msg_send_add_lan_tap(cur->lan, name))
      KERR("%s %s", cur->lan, name);
    else
      {
      if (cur->waiting_ack_add_tap == 0)
        KERR("%s", name);
      if (cur->waiting_ack_del_tap != 0)
        KERR("%s", name);
      strncpy(cur->strmac, strmac, MAX_NAME_LEN-1);
      cur->waiting_ack_add_lan = 1;
      cur->waiting_ack_add_tap = 0;
      cur->waiting_ack_del_tap = 0;
      cur->timer_count = 0;
      } 
    } 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_add(int llid, int tid, char *name, char *lan)
{
  int result = -1;
  t_dtap *cur = get_dtap(name);
  if (cur)
    KERR("%s", name);
  else if (fmt_tx_add_tap(0, name))
    KERR("%s", name);
  else
    {
    cur = alloc_dtap(name, lan);
    cur->waiting_ack_add_tap = 1;
    cur->llid = llid;
    cur->tid = tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_del(int llid, int tid, char *name)
{
  int result = -1;
  t_dtap *cur = get_dtap(name);
  if (!cur)
    KERR("%s", name);
  else if (cur->waiting_ack_del_lan != 0)
    KERR("%s", name);
  else if (dpdk_msg_send_del_lan_tap(cur->lan, name))
    KERR("%s %s", cur->lan, name);
  else
    {
    cur->waiting_ack_del_lan = 1;
    cur->timer_count = 0;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_exist(char *name)
{
  int result = 0;
  if (get_dtap(name))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_lan_exists(char *lan)
{
  t_dtap *cur = g_head_dtap;
  int result = 0;
  while(cur)
    {
    if (!strcmp(lan, cur->lan))
      result = 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *dpdk_tap_get_mac(char *name)
{
  char *result = NULL;
  t_dtap *cur = get_dtap(name);
  if (cur)
    result = cur->strmac;
  return result; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *dpdk_tap_get_next(char *name)
{
  char *result = NULL;
  t_dtap *cur;
  if (name == NULL)
    cur = g_head_dtap;
  else
    {
    cur = get_dtap(name);
    if (cur)
      cur = cur->next;
    } 
  if (cur)
    result = cur->name;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_lan_exists_in_tap(char *name, char *lan)
{
  int result = 0;
  t_dtap *cur = get_dtap(name);
  if(cur)
    {
    if (!strcmp(lan, cur->lan))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_end_ovs(void)
{
  t_dtap *next, *cur = g_head_dtap;
  while(cur)
    {
    next = cur->next;
    if (cur->waiting_ack_del_lan != 0)
      KERR("%s %s", cur->lan, cur->name);
    else
      {
      if (dpdk_msg_send_del_lan_tap(cur->lan, cur->name))
        KERR("%s %s", cur->lan, cur->name);
      else
        cur->waiting_ack_del_lan = 1;
      }
    if (cur->waiting_ack_del_tap != 0)
      KERR("%s %s", cur->lan, cur->name);
    else
      {
      if (fmt_tx_del_tap(0, cur->name))
        {
        KERR("%s", cur->name);
        free_dtap(cur);
        }
      else
        cur->waiting_ack_del_tap = 1; 
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_eventfull(char *name, int ms, int ptx, int btx, int prx, int brx)
{
  int result = -1;
  t_dtap *cur = get_dtap(name);
  if (cur)
    {
    cur->ms       = ms;
    cur->pkt_tx  += ptx;
    cur->pkt_rx  += prx;
    cur->byte_tx += btx;
    cur->byte_rx += brx;
    stats_counters_update_endp_tx_rx(cur->name, 0, ms, ptx, btx, prx, brx);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_tap_collect_dpdk(t_eventfull_endp *eventfull)
{
  int result = 0;;
  t_dtap *cur = g_head_dtap;
  while(cur)
    {
    strncpy(eventfull[result].name, cur->name, MAX_NAME_LEN-1);
    eventfull[result].type = endp_type_tap;
    eventfull[result].num  = 0;
    eventfull[result].ms   = cur->ms;
    eventfull[result].ptx  = cur->pkt_tx;
    eventfull[result].prx  = cur->pkt_rx;
    eventfull[result].btx  = cur->byte_tx;
    eventfull[result].brx  = cur->byte_rx;
    cur->pkt_tx  = 0;
    cur->pkt_rx  = 0;
    cur->byte_tx = 0;
    cur->byte_rx = 0;
    result += 1;
    cur = cur->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_init(void)
{
  g_head_dtap = NULL;
  clownix_timeout_add(50, timer_tap_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
