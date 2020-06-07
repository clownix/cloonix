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
#include "dpdk_nat.h"
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


/****************************************************************************/
typedef struct t_dlan
{
  char lan[MAX_NAME_LEN];
  int  waiting_ack_add_lan;
  int  waiting_ack_del_lan;
  int  timer_count_lan;
  int  llid;
  int  tid;
  int  nat_dpdk_start_process;
} t_dlan;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_dnat
{
  char name[MAX_NAME_LEN];
  int to_be_destroyed;
  t_dlan *head_lan;
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
static int g_timer_nat_waiting;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dnat *get_dnat(char *name)
{ 
  t_dnat *dnat = g_head_dnat;
  while(dnat)
    {
    if (!strcmp(name, dnat->name))
      break;
    dnat = dnat->next;
    }
  return dnat;
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dlan *get_dlan(t_dnat *dnat, char *lan)
{
  t_dlan *cur = dnat->head_lan;
  if ((cur) && (!strcmp(cur->lan, lan)))
    return cur;
  else
    return NULL;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dlan *alloc_dlan(t_dnat *dnat, char *lan)
{
  t_dlan *dlan = NULL;
  if (dnat->head_lan)
    KERR("ALREADY LAN IN SNF %s %s", dnat->name, lan);
  else
    {
    dnat->head_lan = (t_dlan *) clownix_malloc(sizeof(t_dlan), 5);
    memset(dnat->head_lan, 0, sizeof(t_dlan));
    memcpy(dnat->head_lan->lan, lan, MAX_NAME_LEN-1);
    dlan = dnat->head_lan;
    }
  return dlan;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_dlan(t_dnat *dnat)
{
  char lan[MAX_NAME_LEN];
  if (dnat == NULL)
    KOUT(" ");
  if (dnat->head_lan == NULL) 
    KOUT(" ");
  memset(lan, 0, MAX_NAME_LEN);
  strncpy(lan, dnat->head_lan->lan, MAX_NAME_LEN-1);
  clownix_free(dnat->head_lan, __FUNCTION__);
  dnat->head_lan = NULL;
  edp_evt_lan_del_done(eth_type_dpdk, lan);
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
  snf_dpdk_process_possible_change(lan);
  if ((!dpdk_dyn_lan_exists(lan)) && (!dpdk_nat_lan_exists(lan)))
    dpdk_msg_vlan_exist_no_more(lan);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dnat *alloc_dnat(char *name)
{
  t_dnat *dnat = (t_dnat *)clownix_malloc(sizeof(t_dnat), 5);
  memset(dnat, 0, sizeof(t_dnat));
  memcpy(dnat->name, name, MAX_NAME_LEN-1);
  if (g_head_dnat)
    g_head_dnat->prev = dnat;
  dnat->next = g_head_dnat;
  g_head_dnat = dnat;
  return dnat;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_dnat(char *name)
{
  t_dnat *dnat = get_dnat(name);
  if (!dnat)
    KERR("%s", name);
  else
    {
    if (dnat->head_lan) 
      free_dlan(dnat);
    if (dnat->prev)
      dnat->prev->next = dnat->next;
    if (dnat->next)
      dnat->next->prev = dnat->prev;
    if (g_head_dnat == dnat)
      g_head_dnat = dnat->next;
    clownix_free(dnat, __FUNCTION__);
    }
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_nat_msg_beat(void *data)
{
  t_dnat *next, *dnat = g_head_dnat;
  t_dlan *cur;
  while(dnat)
    {
    next = dnat->next;
    cur = dnat->head_lan;
    if (cur)
      {
      if ((cur->waiting_ack_add_lan != 0) || (cur->waiting_ack_del_lan != 0))
        {
        cur->timer_count_lan += 1;
        if (cur->timer_count_lan > 15)
          {
          KERR("Time %s", dnat->name);
          if (dnat->to_be_destroyed == 1)
            free_dnat(dnat->name);
          }
        }
      }
    dnat = next;
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
  result += g_timer_nat_waiting;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void resp_lan(int is_add, int is_ko, char *lan, char *name)
{
  t_dnat *dnat = get_dnat(name);
  t_dlan *dlan;
  if (dnat)
    {
    dlan = get_dlan(dnat, lan);
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
        KERR("Resp KO %s %s", lan, dnat->name);
        send_status_ko(dlan->llid, dlan->tid, "Resp KO");
        free_dlan(dnat);
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
          if (dnat->to_be_destroyed == 1)
            free_dnat(dnat->name);
          else
            free_dlan(dnat);
          }
        else
          {
          if (dlan->nat_dpdk_start_process)
            KERR("%s %s", name, lan);
          else
            {
            dlan->nat_dpdk_start_process = 1;
            nat_dpdk_start_process(name, lan, 1);
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
void dpdk_nat_resp_add_lan(int is_ko, char *lan, char *name)
{
  resp_lan(1, is_ko, lan, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_nat_resp_del_lan(int is_ko, char *lan, char *name)
{
  resp_lan(0, is_ko, lan, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_add_lan(int llid, int tid, char *lan, char *name)
{
  int result = -1;
  t_dnat *dnat = get_dnat(name);
  t_dlan *dlan;
  if (dnat == NULL)
    KERR("%s %s", lan, name);
  else
    {
    dlan = get_dlan(dnat, lan);
    if (dlan != NULL)
      KERR("%s %s", lan, name);
    else
      {
      if (dpdk_msg_send_add_lan_nat(lan, name))
        KERR("%s %s", lan, name);
      else
        {
        dlan = alloc_dlan(dnat, lan);
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
static int dpdk_nat_del_ovs_lan(char *lan, char *name)
{
  int result = -1;
  t_dnat *dnat = get_dnat(name);
  t_dlan *dlan;
  if (dnat == NULL)
    KERR("%s %s", lan, name);
  else
    {
    dlan = get_dlan(dnat, lan);
    if (dlan == NULL)
      KERR("%s %s", lan, name);
    else if (dlan->nat_dpdk_start_process == 1)
      KERR("%s %s", lan, name);
    else if (dlan->waiting_ack_del_lan)
      KERR("%s %s", lan, name);
    else if (dpdk_msg_send_del_lan_nat(lan, name))
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
int dpdk_nat_add(int llid, int tid, char *name)
{
  int result = -1;
  t_dnat *dnat = get_dnat(name);
  if (dnat)
    KERR("%s", name);
  else
    {
    dnat = alloc_dnat(name);
    send_status_ok(llid, tid, "OK");
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_nat_event_from_nat_dpdk_process(char *name, char *lan, int on)
{
  t_dnat *dnat = get_dnat(name);
  t_dlan *cur;
  if (!dnat)
    KERR("ERROR TODO %s %s", name, lan);
  else if (on == -1)
    {
    KERR("ERROR %s %s", name, lan);
    cur = dnat->head_lan;
    if (cur)
      {
      cur->nat_dpdk_start_process = 0;
      if (strcmp(cur->lan, lan))
        KERR("%s %s %s", name, lan, cur->lan);
      if (dpdk_nat_del_ovs_lan(cur->lan, name))
        KERR("%s %s", name, lan);
      free_dnat(name);
      }
    else
      KERR("ERROR TODO %s %s", name, lan);
    }
  else if (on == 0)
    {
    cur = dnat->head_lan;
    if (cur)
      {
      cur->nat_dpdk_start_process = 0;
      if (strcmp(cur->lan, lan))
        KERR("%s %s %s", name, lan, cur->lan);
      if (dpdk_nat_del_ovs_lan(cur->lan, name))
        KERR("%s %s", name, lan);
      free_dnat(name);
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
void dpdk_nat_del(char *name)
{
  t_dnat *dnat = get_dnat(name);
  t_dlan *cur;
  if (!dnat)
    KERR("%s", name);
  else
    {
    cur = dnat->head_lan;
    if (cur)
      {
      dnat->to_be_destroyed = 1;
      if (cur->nat_dpdk_start_process == 1)
        {
        nat_dpdk_start_process(name, cur->lan, 0);
        cur->nat_dpdk_start_process = 0;
        }
      else
        {
        if (dpdk_nat_del_ovs_lan(cur->lan, name))
          KERR("%s %s", name, cur->lan);
        }
      }
    else
      free_dnat(name);
    }
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
  t_dnat *dnat = g_head_dnat;
  int result = 0;
  while(dnat)
    {
    if (get_dlan(dnat, lan) != NULL)
      result = 1;
    dnat = dnat->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *dpdk_nat_get_next_matching_lan(char *lan, char *name)
{
  char *result = NULL;
  t_dnat *dnat;
  if (name == NULL)
    dnat = g_head_dnat;
  else
    {
    dnat = get_dnat(name);
    if (dnat)
      dnat = dnat->next;
    }
  while(dnat)
    {
    if (get_dlan(dnat, lan) != NULL)
      {
      result = dnat->name;
      break;
      }
    dnat = dnat->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_lan_exists_in_nat(char *name, char *lan)
{
  int result = 0;
  t_dnat *dnat = get_dnat(name);
  t_dlan *dlan;
  if(dnat)
    {
    dlan = get_dlan(dnat, lan);
    if (dlan != NULL)
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_nat_end_ovs(void)
{
  t_dnat *ndnat, *dnat = g_head_dnat;
  while(dnat)
    {
    ndnat = dnat->next;
    dpdk_nat_del(dnat->name);
    dnat = ndnat;
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
  t_dnat *dnat = get_dnat(name);
  if (dnat)
    {
    dnat->ms      = ms;
    dnat->pkt_tx  += ptx;
    dnat->pkt_rx  += prx;
    dnat->byte_tx += btx;
    dnat->byte_rx += brx;
    stats_counters_update_endp_tx_rx(dnat->name,0,ms,ptx,btx,prx,brx);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_nat_collect_dpdk(t_eventfull_endp *eventfull)
{
  int result = 0;;
  t_dnat *dnat = g_head_dnat;
  while(dnat)
    {
    strncpy(eventfull[result].name, dnat->name, MAX_NAME_LEN-1);
    eventfull[result].type = endp_type_nat;
    eventfull[result].num  = 0;
    eventfull[result].ms   = dnat->ms;
    eventfull[result].ptx  = dnat->pkt_tx;
    eventfull[result].prx  = dnat->pkt_rx;
    eventfull[result].btx  = dnat->byte_tx;
    eventfull[result].brx  = dnat->byte_rx;
    dnat->pkt_tx  = 0;
    dnat->pkt_rx  = 0;
    dnat->byte_tx = 0;
    dnat->byte_rx = 0;
    result += 1;
    dnat = dnat->next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
void dpdk_nat_init(void)
{
  g_head_dnat = NULL;
  clownix_timeout_add(50, timer_nat_msg_beat, NULL, NULL, NULL);
  g_timer_nat_waiting = 0;
}
/*--------------------------------------------------------------------------*/
