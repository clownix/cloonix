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
#include "endp_mngt.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "llid_trace.h"
#include "machine_create.h"
#include "file_read_write.h"
#include "dpdk_ovs.h"
#include "dpdk_dyn.h"
#include "dpdk_nat.h"
#include "dpdk_d2d.h"
#include "dpdk_msg.h"
#include "fmt_diag.h"
#include "qmp.h"
#include "system_callers.h"
#include "stats_counters.h"
#include "dpdk_msg.h"
#include "edp_mngt.h"
#include "edp_evt.h"
#include "nat_dpdk_process.h"
#include "snf_dpdk_process.h"
#include "tabmac.h"


/****************************************************************************/
typedef struct t_dnat
{
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  int waiting_ack_add_lan;
  int waiting_ack_del_lan;
  int timer_count;
  int llid;
  int tid;
  int var_dpdk_start_stop_process;
  int to_be_destroyed;
  int ms;
  int pkt_tx;
  int pkt_rx;
  int byte_tx;
  int byte_rx;
  struct t_dnat *prev;
  struct t_dnat *next;
} t_dnat;
/*--------------------------------------------------------------------------*/

static t_dnat *g_head_dnat;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dnat *get_dnat(char *name)
{ 
  t_dnat *cur = g_head_dnat;
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
static t_dnat *alloc_dnat(char *name, char *lan)
{
  t_dnat *cur = (t_dnat *)clownix_malloc(sizeof(t_dnat), 5);
  memset(cur, 0, sizeof(t_dnat));
  memcpy(cur->name, name, MAX_NAME_LEN-1);
  memcpy(cur->lan, lan, MAX_NAME_LEN-1);
  if (g_head_dnat)
    g_head_dnat->prev = cur;
  cur->next = g_head_dnat;
  g_head_dnat = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_dnat(t_dnat *cur)
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
  if (g_head_dnat == cur)
    g_head_dnat = cur->next;
  clownix_free(cur, __FUNCTION__);
  edp_evt_update_non_fix_del(eth_type_dpdk, name, lan);
  tabmac_process_possible_change();
  dpdk_msg_vlan_exist_no_more(lan);
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_nat_msg_beat(void *data)
{
  t_dnat *next, *cur = g_head_dnat;
  while(cur)
    {
    next = cur->next;
      if ((cur->waiting_ack_add_lan) ||
          (cur->waiting_ack_del_lan))
        {
        cur->timer_count += 1;
        if (cur->timer_count > 20)
          {
          KERR("TIMEOUT %s addlan:%d dellan:%d", 
               cur->name, cur->waiting_ack_add_lan,
               cur->waiting_ack_del_lan);
          cur->timer_count = 0;
          if (cur->to_be_destroyed == 1)
            {
            if (cur->var_dpdk_start_stop_process)
              {
              nat_dpdk_start_stop_process(cur->name, cur->lan, 0);
              cur->var_dpdk_start_stop_process = 0;
              }
            free_dnat(cur);
            }
          }
        }
    cur = next;
    }
  clownix_timeout_add(50, timer_nat_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_get_qty(void)
{
  int result = 0;
  t_dnat *cur = g_head_dnat; 
  while(cur)
    {
    result += 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_nat_resp_add_lan(int is_ko, char *lan, char *name)
{
  t_dnat *cur = get_dnat(name);
  if (!cur)
    KERR("ERROR %s %s", name, lan);
  else if (strcmp(lan, cur->lan))
    KERR("ERROR %s %s %s", lan, name, cur->lan);
  else if (cur->waiting_ack_add_lan == 0)
    KERR("ERROR %d %s %s", is_ko, lan, name);
  else if (cur->waiting_ack_del_lan == 1)
    KERR("ERROR %d %s %s", is_ko, lan, name);
  else if (cur->var_dpdk_start_stop_process == 0)
    KERR("ERROR %d %s %s", is_ko, lan, name);
  else if (is_ko)
    {
    KERR("ERROR %s %s", name, lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "openvswitch error");
    cur->waiting_ack_add_lan = 0;
    }
  else
    {
    cur->waiting_ack_add_lan = 0;
    edp_evt_update_non_fix_add(eth_type_dpdk, name, lan);
    tabmac_process_possible_change();
    utils_send_status_ok(&(cur->llid), &(cur->tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
   }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_nat_resp_del_lan(int is_ko, char *lan, char *name)
{
  int is_ok = 0;
  t_dnat *cur = get_dnat(name);
  if (!cur)
    KERR("%s %s", lan, name);
  else
    {
    if (strcmp(lan, cur->lan))
      KERR("%s %s %s", lan, name, cur->lan);
    else if (cur->waiting_ack_add_lan == 1)
      KERR("%d %s %s", is_ko, lan, name);
    else if (cur->waiting_ack_del_lan == 0)
      KERR("%d %s %s", is_ko, lan, name);
    else if (cur->var_dpdk_start_stop_process)
      KERR("ERROR %s %s", name, cur->lan);
    else
      is_ok = 1;
    if (is_ok)
      {
      utils_send_status_ok(&(cur->llid), &(cur->tid));
      free_dnat(cur);
      }
    else
      {
      utils_send_status_ko(&(cur->llid), &(cur->tid), "error del");
      free_dnat(cur);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_nat_event_from_nat_dpdk_process(char *name, char *lan, int on)
{
  t_dnat *cur = get_dnat(name);
  if (!cur)
    {
    KERR("%s %s", name, lan);
    }
  else if (strcmp(cur->lan, lan))
    {
    KERR("%s %s %s", name, lan, cur->lan);
    }
  else if ((on == -1) || (on == 0))
    {
    if (on == -1)
      KERR("ERROR %s %s", name, lan);
    cur->var_dpdk_start_stop_process = 0;
    cur->to_be_destroyed = 1;
    if (dpdk_msg_send_del_lan_nat(lan, name))
      KERR("ERROR %s %s", name, lan);
    }
  else
    {
    if (cur->var_dpdk_start_stop_process == 0)
      KERR("ERROR %s %s", name, lan);
    else if (cur->waiting_ack_add_lan != 0)
      KERR("ERROR %s %s", name, lan);
    else if (dpdk_msg_send_add_lan_nat(lan, name))
      KERR("ERROR %s %s", lan, name);
    else
      cur->waiting_ack_add_lan = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_add(int llid, int tid, char *name, char *lan)
{
  int result = -1;
  t_dnat *cur = get_dnat(name);
  if (cur)
    KERR("%s", name);
  else
    {
    cur = alloc_dnat(name, lan);
    cur->llid = llid;
    cur->tid = tid;
    cur->var_dpdk_start_stop_process = 1;
    nat_dpdk_start_stop_process(name, lan, 1);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_del(int llid, int tid, char *name)
{
  int result = -1;
  t_dnat *cur = get_dnat(name);
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
  else
    {
    if (cur->waiting_ack_del_lan)
      {
      KERR("%s %s", name, cur->lan);
      utils_send_status_ko(&(cur->llid), &(cur->tid), "wait del");
      }
    else
      {
      cur->llid = llid;
      cur->tid = tid;
      }
    nat_dpdk_start_stop_process(name, cur->lan, 0);
    cur->var_dpdk_start_stop_process = 0;
    cur->waiting_ack_del_lan = 1;
    cur->to_be_destroyed = 1;
    cur->timer_count = 0;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_exist(char *name)
{
  int result = 0;
  if (get_dnat(name))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_lan_exists(char *lan)
{
  t_dnat *cur = g_head_dnat;
  int result = 0;
  while(cur)
    {
    if (!strcmp(lan, cur->lan))
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
char *dpdk_nat_get_next(char *name)
{
  char *result = NULL;
  t_dnat *cur;
  if (name == NULL)
    cur = g_head_dnat;
  else
    {
    cur = get_dnat(name);
    if (cur)
      cur = cur->next;
    }
  if (cur)
    result = cur->name;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_lan_exists_in_nat(char *name, char *lan)
{
  int result = 0;
  t_dnat *cur = get_dnat(name);
  if(cur)
    {
    if (!strcmp(lan, cur->lan))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_nat_end_ovs(void)
{
  t_dnat *next, *cur = g_head_dnat;
  while(cur)
    {
    next = cur->next;
    dpdk_nat_del(0, 0, cur->name);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *dpdk_nat_get_mac(char *name, int num)
{
  char *result = NULL;
  if (num == 0)
    result = NAT_MAC_GW;
  else if (num == 1)
    result = NAT_MAC_DNS;
  else if (num == 2)
    result = NAT_MAC_CISCO;
  else
    KOUT("%s %d", name, num);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_eventfull(char *name, int ms, int ptx, int btx, int prx, int brx)
{
  int result = -1;
  t_dnat *cur = get_dnat(name);
  if (cur)
    {
    cur->ms      = ms;
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
int dpdk_nat_collect_dpdk(t_eventfull_endp *eventfull)
{
  int result = 0;;
  t_dnat *cur = g_head_dnat;
  while(cur)
    {
    strncpy(eventfull[result].name, cur->name, MAX_NAME_LEN-1);
    eventfull[result].type = endp_type_nat;
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
void dpdk_nat_init(void)
{
  g_head_dnat = NULL;
  clownix_timeout_add(50, timer_nat_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
