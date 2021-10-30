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
#include "dpdk_nat.h"
#include "dpdk_msg.h"
#include "dpdk_kvm.h"
#include "dpdk_xyx.h"
#include "fmt_diag.h"
#include "qmp.h"
#include "system_callers.h"
#include "stats_counters.h"
#include "dpdk_msg.h"
#include "nat_dpdk_process.h"



static t_nat_cnx *g_head_nat;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_nat_cnx *find_nat(char *name)
{ 
  t_nat_cnx *cur = g_head_nat;
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
static t_nat_cnx *alloc_nat(char *name)
{
  t_nat_cnx *cur = (t_nat_cnx *)clownix_malloc(sizeof(t_nat_cnx), 5);
  memset(cur, 0, sizeof(t_nat_cnx));
  memcpy(cur->name, name, MAX_NAME_LEN-1);
  if (g_head_nat)
    g_head_nat->prev = cur;
  cur->next = g_head_nat;
  g_head_nat = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_nat(t_nat_cnx *cur)
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
  if (g_head_nat == cur)
    g_head_nat = cur->next;
  clownix_free(cur, __FUNCTION__);
  dpdk_msg_vlan_exist_no_more(lan);
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_beat(void *data)
{
  t_nat_cnx *next, *cur = g_head_nat;
  while(cur)
    {
    next = cur->next;

    if (cur->to_be_destroyed == 1)
      {
      if (cur->process_cmd_on == 1)
        {
        cur->process_cmd_on = 0;
        nat_dpdk_start_stop_process(cur->name, 0);
        }
      cur->to_be_destroyed_timer_count += 1;
      if (cur->to_be_destroyed_timer_count == 10)
        utils_send_status_ok(&(cur->del_llid), &(cur->del_tid));
      if (cur->to_be_destroyed_timer_count > 20)
        free_nat(cur);
      }
    if ((cur->waiting_ack_add_lan) || (cur->waiting_ack_del_lan))
      {
      cur->timer_count += 1;
      if (cur->timer_count > 20)
        {
        if (cur->waiting_ack_add_lan)
          {
          KERR("ERROR TIMEOUT ADD LAN %s %s", cur->name, cur->lan);
          utils_send_status_ko(&(cur->add_llid),&(cur->add_tid),"add timeout");
          }
        if (cur->waiting_ack_del_lan)
          {
          KERR("ERROR TIMEOUT DEL LAN %s %s", cur->name, cur->lan);
          utils_send_status_ko(&(cur->del_llid),&(cur->del_tid),"add timeout");
          }
        cur->timer_count = 0;
        cur->waiting_ack_add_lan = 0;
        cur->waiting_ack_del_lan = 0;
        }
      }
    cur = next;
    }
  clownix_timeout_add(50, timer_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_add_nat_vm(void *data)
{
  char *name = (char *) data;
  t_vm *vm = cfg_get_vm(name);
  char cisconat[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  int num;
  if (!vm)
    KERR("ERROR %s", name);
  else
    {
    memset(cisconat, 0, MAX_NAME_LEN);
    memset(lan, 0, MAX_NAME_LEN);
    snprintf(cisconat, MAX_NAME_LEN-1, "nat_%s", name);
    snprintf(lan, MAX_NAME_LEN-1, "lan_nat_%s", name);
    num = vm->kvm.vm_config_param;
    if (vm->kvm.eth_table[num].eth_type == endp_type_ethd)
      {
      if (!dpdk_kvm_exists(name, num))
        KERR("ERROR %s %d", name, num);
      else
        {
        if (dpdk_kvm_add_lan(0, 0, name, num, lan, endp_type_ethd))
          KERR("ERROR %s %d", name, num);
        else if (dpdk_nat_add_lan(0, 0, cisconat, lan))
          KERR("ERROR CREATING CISCO NAT LAN %s %d", name, num);
        }
      }
    else if (vm->kvm.eth_table[num].eth_type == endp_type_eths)
      {
      if (!dpdk_xyx_exists(name, num))
        KERR("ERROR %s %d", name, num);
      else
        {
        if (dpdk_kvm_add_lan(0, 0, name, num, lan, endp_type_eths))
          KERR("ERROR %s %d", name, num);
        else if (dpdk_nat_add_lan(0, 0, cisconat, lan))
          KERR("ERROR CREATING CISCO NAT LAN %s %d", name, num);
        }
      }
    else
      KERR("ERROR %s %d", name, num);
    }
  free(data);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int dpdk_nat_get_qty(void)
{
  int result = 0;
  t_nat_cnx *cur = g_head_nat; 
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
  t_nat_cnx *cur = find_nat(name);
  if (!cur)
    KERR("ERROR %s %s", name, lan);
  if (strcmp(lan, cur->lan))
    KERR("ERROR %s %s %s", lan, name, cur->lan);
  if (cur->waiting_ack_add_lan == 0)
    KERR("ERROR %d %s %s", is_ko, lan, name);
  if (cur->waiting_ack_del_lan == 1)
    KERR("ERROR %d %s %s", is_ko, lan, name);
  cur->waiting_ack_add_lan = 0;
  if (is_ko)
    {
    KERR("ERROR %s %s", name, lan);
    utils_send_status_ko(&(cur->add_llid), &(cur->add_tid), "openvswitch error");
    }
  else
    {
    cur->attached_lan_ok = 1;
    utils_send_status_ok(&(cur->add_llid), &(cur->add_tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
   }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_nat_resp_del_lan(int is_ko, char *lan, char *name)
{
  int is_ok = 0;
  t_nat_cnx *cur = find_nat(name);
  if (!cur)
    KERR("ERROR %s %s", lan, name);
  else
    {
    if (strcmp(lan, cur->lan))
      KERR("ERROR %s %s %s", lan, name, cur->lan);
    if (cur->waiting_ack_add_lan == 1)
      KERR("ERROR %d %s %s", is_ko, lan, name);
    if (cur->waiting_ack_del_lan == 0)
      KERR("ERROR %d %s %s", is_ko, lan, name);
    cur->waiting_ack_del_lan = 0;
    cur->attached_lan_ok = 0;
    memset(cur->lan, 0, MAX_NAME_LEN);
    if (is_ok)
      utils_send_status_ok(&(cur->del_llid), &(cur->del_tid));
    else
      utils_send_status_ko(&(cur->del_llid), &(cur->del_tid), "error del");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_nat_event_from_nat_dpdk_process(char *name, int on)
{
  t_nat_cnx *cur = find_nat(name);
  if (!cur)
    {
    KERR("ERROR %s", name);
    }
  else if ((on == -1) || (on == 0))
    {
    if (on == -1)
      KERR("ERROR %s", name);
    if (cur->to_be_destroyed == 1)
      utils_send_status_ok(&(cur->del_llid), &(cur->del_tid));
    cur->process_running = 0;
    }
  else
    {
    if (cur->process_cmd_on == 0)
      KERR("ERROR %s", name);
    cur->process_cmd_on = 1; 
    cur->process_running = 1; 
    utils_send_status_ok(&(cur->add_llid), &(cur->add_tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_exists(char *name)
{
  int result = 0;
  if (find_nat(name))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_lan_exists(char *lan)
{
  t_nat_cnx *cur = g_head_nat;
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
  t_nat_cnx *cur;
  if (name == NULL)
    cur = g_head_nat;
  else
    {
    cur = find_nat(name);
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
  t_nat_cnx *cur = find_nat(name);
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
  t_nat_cnx *next, *cur = g_head_nat;
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
int dpdk_nat_add_lan(int llid, int tid, char *name, char *lan)
{
  int result = -1;
  t_nat_cnx *cur = find_nat(name);
  if (cur == NULL)
    KERR("ERROR %s", name);
  else if (cur->process_running == 0)
    KERR("ERROR %s %s", name, lan);
  else if (strlen(cur->lan)) 
    KERR("ERROR %s %s %s", name, lan, cur->lan);
  else if (cur->attached_lan_ok == 1)
    KERR("ERROR %s %s", name, lan);
  else if (cur->waiting_ack_add_lan)
    KERR("ERROR %s %s", name, cur->lan);
  else if (cur->waiting_ack_del_lan)
    KERR("ERROR %s %s", name, cur->lan);
  else if (dpdk_msg_send_add_lan_nat(lan, name))
    KERR("ERROR %s %s", lan, name);
  else
    {
    cur->waiting_ack_add_lan = 1;
    cur->add_llid = llid;
    cur->add_tid = tid;
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_del_lan(int llid, int tid, char *name, char *lan)
{
  int result = -1;
  t_nat_cnx *cur = find_nat(name);
  if (cur == NULL)
    KERR("ERROR %s", name);
  else if (cur->process_running == 0)
    KERR("ERROR %s %s", name, lan);
  else if (!strlen(cur->lan))
    KERR("ERROR %s %s", name, lan);
  else if (cur->attached_lan_ok == 0)
    KERR("ERROR %s %s", name, lan);
  else if (cur->waiting_ack_add_lan)
    KERR("ERROR %s %s", name, cur->lan);
  else if (cur->waiting_ack_del_lan)
    KERR("ERROR %s %s", name, cur->lan);
  else if (dpdk_msg_send_del_lan_nat(lan, name))
    KERR("ERROR %s %s", lan, name);
  else
    {
    cur->attached_lan_ok = 0;
    cur->waiting_ack_del_lan = 1;
    cur->del_llid = llid;
    cur->del_tid = tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_add(int llid, int tid, char *name)
{
  int result = -1;
  t_nat_cnx *cur = find_nat(name);
  if (cur)
    KERR("ERROR %s", name);
  else
    {
    cur = alloc_nat(name);
    cur->add_llid = llid;
    cur->add_tid = tid;
    cur->process_cmd_on = 1;
    nat_dpdk_start_stop_process(name, 1);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_nat_del(int llid, int tid, char *name)
{
  int result = -1;
  t_nat_cnx *cur = find_nat(name);
  if (!cur)
    KERR("ERROR %s", name);
  else if (cur->waiting_ack_add_lan)
    KERR("ERROR %s %s", name, cur->lan);
  else
    {
    cur->del_llid = llid;
    cur->del_tid = tid;
    if ((strlen(cur->lan)) && (cur->attached_lan_ok == 1))
      {
      cur->waiting_ack_del_lan = 1;
      if (dpdk_msg_send_del_lan_nat(cur->lan, name))
        KERR("ERROR %s %s", cur->lan, name);
      }
    cur->to_be_destroyed = 1;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_nat_cnx *dpdk_nat_get_first(int *nb_nat)
{
  t_nat_cnx *cur = g_head_nat;
  *nb_nat = 0;
  while(cur)
    {
    *nb_nat += 1;
    cur = cur->next;
    }
  return g_head_nat;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *translate_topo_endp_nat(int *nb)
{
  t_nat_cnx *cur;
  int len, nb_nat, nb_endp = 0;
  t_topo_endp *endp;
  cur = dpdk_nat_get_first(&nb_nat);
  endp = (t_topo_endp *) clownix_malloc(nb_nat * sizeof(t_topo_endp),13);
  memset(endp, 0, nb_nat * sizeof(t_topo_endp));
  while(cur)
    {
    len = sizeof(t_lan_group_item);
    endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
    memset(endp[nb_endp].lan.lan, 0, len);
    strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
    endp[nb_endp].num = 0;
    endp[nb_endp].type = endp_type_nat;
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
void dpdk_nat_cisco_add(char *name)
{
  t_vm *vm = cfg_get_vm(name);
  char cisconat[MAX_NAME_LEN];
  char *nm;
  if (!vm)
    KERR("ERROR CREATING CISCO NAT %s", name);
  else
    {  
    memset(cisconat, 0, MAX_NAME_LEN); 
    snprintf(cisconat, MAX_NAME_LEN-1, "nat_%s", name);
    if (dpdk_nat_add(0, 0, cisconat))
      {
      KERR("ERROR CREATING CISCO NAT %s", cisconat);
      }
    else
      {
      nm = (char *) malloc(MAX_NAME_LEN);
      memset(nm, 0, MAX_NAME_LEN);
      strncpy(nm, name, MAX_NAME_LEN-1);
      clownix_timeout_add(300, timer_add_nat_vm, (void *)nm, NULL, NULL);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_nat_cisco_del(char *name)
{
  char cisconat[MAX_NAME_LEN];
  memset(cisconat, 0, MAX_NAME_LEN);
  snprintf(cisconat, MAX_NAME_LEN-1, "nat_%s", name);
  if (dpdk_nat_del(0, 0, name))
    KERR("ERROR %s", name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_nat_init(void)
{
  g_head_nat = NULL;
  clownix_timeout_add(50, timer_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
