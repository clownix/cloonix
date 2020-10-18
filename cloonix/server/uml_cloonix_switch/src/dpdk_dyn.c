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
#include "pid_clone.h"
#include "dpdk_ovs.h"
#include "dpdk_dyn.h"
#include "dpdk_msg.h"
#include "dpdk_tap.h"
#include "dpdk_snf.h"
#include "dpdk_nat.h"
#include "dpdk_d2d.h"
#include "machine_create.h"
#include "event_subscriber.h"
#include "edp_mngt.h"
#include "edp_evt.h"
#include "snf_dpdk_process.h"
#include "utils_cmd_line_maker.h"
#include "tabmac.h"

/****************************************************************************/
typedef struct t_dlan
{
  char lan[MAX_NAME_LEN];
  int  waiting_ack_add;
  int  waiting_ack_del;
  int  llid;
  int  tid;
  struct t_dlan *prev;
  struct t_dlan *next;
} t_dlan;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_deth
{ 
  int  num;
  t_dlan *head_lan;
  struct t_deth *prev;
  struct t_deth *next;
} t_deth;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_dvm
{
  char name[MAX_NAME_LEN];
  t_deth *head_eth;
  struct t_dvm *prev;
  struct t_dvm *next;
} t_dvm;

static t_dvm *g_head_vm;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dlan *vlan_alloc(t_deth *eth, char *lan, int llid, int tid)
{
  t_dlan *cur = (t_dlan *) clownix_malloc(sizeof(t_dlan), 14); 
  memset(cur, 0, sizeof(t_dlan));
  strncpy(cur->lan, lan, MAX_NAME_LEN-1);
  cur->llid = llid;
  cur->tid = tid;
  if (eth->head_lan)
    eth->head_lan->prev = cur;
  cur->next = eth->head_lan;
  eth->head_lan = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vlan_free(t_deth *eth, t_dlan *lan)
{
  char lan_name[MAX_NAME_LEN];
  memset (lan_name, 0, MAX_NAME_LEN);
  strncpy(lan_name, lan->lan, MAX_NAME_LEN-1);
  if (lan->prev)
    lan->prev->next = lan->next;
  if (lan->next)
    lan->next->prev = lan->prev;
  if (lan == eth->head_lan)
    eth->head_lan = lan->next;
  clownix_free(lan, __FUNCTION__);
  dpdk_msg_vlan_exist_no_more(lan_name);
  tabmac_process_possible_change();
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eth_free(t_dvm *vm)
{
  t_deth *cur;
  t_dlan *nlan, *lan;
  while(vm->head_eth)
    {
    cur = vm->head_eth;
    if (cur->next) 
      cur->next->prev = NULL;
    vm->head_eth = cur->next; 
    lan = cur->head_lan;
    while(lan)
      {
      nlan = lan->next;
      vlan_free(cur, lan);
      lan = nlan;
      }
    clownix_free(cur, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dvm *vm_find(char *name)
{
  t_dvm *vm = g_head_vm;
  while(name[0] && vm && strcmp(vm->name, name))
    vm = vm->next;
  return vm;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vm_eth_alloc(char *name, int num)
{
  t_dvm *vm = vm_find(name);
  t_deth *cur;
  if (vm == NULL)
    {
    vm = (t_dvm *) clownix_malloc(sizeof(t_dvm), 4);
    memset(vm, 0, sizeof(t_dvm));
    strncpy(vm->name, name, MAX_NAME_LEN-1);
    if (g_head_vm)
      g_head_vm->prev = vm;
    vm->next = g_head_vm;
    g_head_vm = vm;
    }
  cur = (t_deth *) clownix_malloc(sizeof(t_deth), 15);
  memset(cur, 0, sizeof(t_deth));
  cur->num = num;
  if (vm->head_eth)
    vm->head_eth->prev = cur;
  cur->next = vm->head_eth;
  vm->head_eth = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vm_free(char *name)
{
  t_dvm *vm = vm_find(name);
  if (!vm)
    KOUT("%s", name);
  else
    {
    eth_free(vm);
    if (vm->prev)
      vm->prev->next = vm->next;
    if (vm->next)
      vm->next->prev = vm->prev;
    if (vm == g_head_vm)
      g_head_vm = vm->next;
    clownix_free(vm, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_deth *eth_find(t_dvm *vm, int num)
{
  t_deth *eth = NULL;
  if (vm)
    {
    eth = vm->head_eth;
    while(eth)
      {
      if (eth->num == num)
        break;
      eth = eth->next;
      }
    }
  return eth;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dlan *vlan_find(t_deth *eth, char *lan)
{
  t_dlan *cur = eth->head_lan;
  while(cur)
    {
    if (!strcmp(cur->lan, lan))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int vlan_exists_in_vm(char *name)
{
  int result = 0;
  t_dvm *vm = vm_find(name);
  t_deth *eth;
  t_dlan *lan;
  if (vm)
    {
    eth = vm->head_eth;
    while(eth)
      {
      lan = eth->head_lan;
      while(lan)
        {
        result += 1;
        lan = lan->next;
        }
      eth = eth->next;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void single_lan_del_req(t_dvm *vm, t_deth *eth, t_dlan *lan)
{
  if (!lan->waiting_ack_del)
    {
    if (dpdk_msg_send_del_lan_eth(lan->lan, vm->name, eth->num))
      KERR("%s %s %d", lan->lan, vm->name, eth->num);
    else
      lan->waiting_ack_del = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void all_lan_del_req(t_dvm *vm)
{
  t_deth *eth;
  t_dlan *lan;
  eth = vm->head_eth;
  while(eth)
    {
    lan = eth->head_lan;
    while(lan)
      {
      single_lan_del_req(vm, eth, lan);
      lan = lan->next;
      }
    eth = eth->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void recv_ack(int is_add, int is_ko, char *lan_name,
                     char *name, int num)
{
  t_dlan *lan;
  t_dvm *vm = vm_find(name);
  t_deth *eth = eth_find(vm, num);
  if (vm)
    {
    if (!eth)
      KOUT("add:%d ko:%d %s %s %d", is_add, is_ko, lan_name, name, num);
    else
      {
      lan = vlan_find(eth, lan_name);
      if(!lan)
        KERR("add:%d ko:%d %s %s %d", is_add, is_ko, lan_name, name, num);
      else
        {
        if ((is_ko) || (!is_add))
          {
          if (is_ko)
            utils_send_status_ko(&(lan->llid), &(lan->tid), "openvswitch error");
          else
            utils_send_status_ok(&(lan->llid), &(lan->tid));
          vlan_free(eth, lan);
          } 
        else
          {
          if (lan->waiting_ack_add != 1)
            KERR("addlan ko:%d %s %s %d", is_ko, lan_name, name, num);
          lan->waiting_ack_add = 0; 
          utils_send_status_ok(&(lan->llid), &(lan->tid));
          }
        event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
        tabmac_process_possible_change();
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_dyn_ack_add_lan_eth_OK(char *lan, char *name, int num)
{
  recv_ack(1, 0, lan, name, num);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_dyn_ack_add_lan_eth_KO(char *lan, char *name, int num)
{
  recv_ack(1, 1, lan, name, num);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_dyn_ack_del_lan_eth_OK(char *lan, char *name, int num)
{
  recv_ack(0, 0, lan, name, num);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_dyn_ack_del_lan_eth_KO(char *lan, char *name, int num)
{
  recv_ack(0, 1, lan, name, num);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_dyn_ack_add_eth_KO(char *name, int num, char *lab)
{
  event_print("Recv vm dyn add eth KO %s %d %s", name, num, lab);
  dpdk_ovs_ack_add_eth_vm(name, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_dyn_ack_add_eth_OK(char *name, int num, char *lab)
{
  event_print("Recv vm dyn add eth OK %s %d %s", name, num, lab);
  vm_eth_alloc(name, num);
  dpdk_ovs_ack_add_eth_vm(name, 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_dyn_add_lan_to_eth(int llid, int tid, char *lan_name,
                            char *name, int num, char *info)
{
  int result = -1;
  t_dvm *vm = vm_find(name);
  t_deth *eth = eth_find(vm, num);
  t_dlan *cur;
  if (!eth)
    sprintf(info, "Not found: %s %d", name, num);
  else
    {
    if (!dpdk_ovs_muovs_ready())
      {
      sprintf(info, "OVS not ready: %s %s %d", lan_name, name, num);
      KERR("%s", info);
      }
    else if(vlan_find(eth, lan_name))
      sprintf(info, "Lan already attached: %s %s %d", lan_name, name, num);
    else if (eth->head_lan)
      sprintf(info, "Eth is in lan: %s %s %d",eth->head_lan->lan,name,num);
    else
      {
      if (dpdk_msg_send_add_lan_eth(lan_name, name, num))
        sprintf(info, "Bad ovs connect: %s %s %d", lan_name, name, num);
      else
        {
        cur = vlan_alloc(eth, lan_name, llid, tid);
        cur->waiting_ack_add = 1;
        event_print("Send vm dyn add lan eth %s %s %d", lan_name, name, num);
        result = 0;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_dyn_del_lan_from_eth(int llid, int tid, char *lan_name,
                              char *name, int num, char *info)
{
  int result = -1;
  t_dvm *vm = vm_find(name);
  t_deth *eth = eth_find(vm, num);
  t_dlan *cur;
  if (!eth)
    sprintf(info, "Not found: %s %d", name, num);
  else
    {
    cur = vlan_find(eth, lan_name);
    if(!cur)
      sprintf(info, "Lan not attached: %s %s %d", lan_name, name, num);
    else
      {
      if (cur->waiting_ack_del != 0)
        KERR("%s %s %d", lan_name, name, num);
      if (dpdk_msg_send_del_lan_eth(lan_name, name, num))
        sprintf(info, "Bad ovs connect: %s %s %d", lan_name, name, num);
      else
        {
        cur->llid = llid;
        cur->tid = tid;
        cur->waiting_ack_del = 1;
        event_print("Send vm dyn del lan eth %s %s %d", lan_name, name, num);
        result = 0;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_dyn_add_eth(char *name, int num, char *strmac)
{
  event_print("Send vm dyn add eth %s %d",name, num);
  if (dpdk_msg_send_add_eth(name, num, strmac))
    {
    machine_death(name, error_death_noovs);
    KERR("%s %d", name, num);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_dyn_del_eth(char *name, int num)
{
  t_dvm *vm = vm_find(name);
  event_print("Send vm eth del %s %d", name, num);
  if (vm)
    {
    if (vlan_exists_in_vm(name))
      all_lan_del_req(vm);
    dpdk_msg_send_del_eth(name, num);
    vm_free(name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_dyn_eth_exists(char *name, int num)
{
  int result = 0;
  t_dvm *vm = vm_find(name);
  t_deth *eth = eth_find(vm, num);
  if (eth != NULL)
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_dyn_topo_endp(char *name, int num, t_topo_endp *endp)
{
  int len, result = 0, nb_lan = 0, i = 0;
  t_dvm *vm = vm_find(name);
  t_deth *eth;
  t_dlan *cur;
  memset(endp, 0, sizeof(t_topo_endp));
  if (vm)
    {
    eth = eth_find(vm, num);
    if (eth)
      {
      cur = eth->head_lan;
      while (cur)
        {
        if ((cur->waiting_ack_add == 0) &&
            (cur->waiting_ack_del == 0))
          {
          result = 1;
          nb_lan += 1;
          }
        cur = cur->next;
        }
      if (result)
        {
        len = nb_lan * sizeof(t_lan_group_item);
        endp->lan.lan = (t_lan_group_item *) clownix_malloc(len, 2); 
        memset(endp->lan.lan, 0, len);
        strncpy(endp->name, vm->name, MAX_NAME_LEN-1);
        endp->num = num;
        endp->type = endp_type_kvm_dpdk;
        endp->lan.nb_lan = nb_lan;
        cur = eth->head_lan;
        while (cur)
          {
          if ((cur->waiting_ack_add == 0) &&
              (cur->waiting_ack_del == 0))
            {
            strncpy(endp->lan.lan[i].lan, cur->lan, MAX_NAME_LEN-1);
            i += 1;
            }
          cur = cur->next;
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_dyn_lan_exists(char *name)
{
  t_dvm *vm = g_head_vm;
  t_deth *eth;
  t_dlan *cur;
  int result = 0;
  while(vm && (result == 0))
    {
    eth = vm->head_eth;
    while(eth && (result == 0))
      {
      cur = eth->head_lan;
      while(cur && (result == 0))
        {
        if (!strcmp(name, cur->lan))
          result = 1;
        cur = cur->next;
        }
      eth = eth->next;
      }
    vm = vm->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_dyn_lan_exists_with_dpdk(char *name)
{
  t_dvm *vm = g_head_vm;
  t_deth *eth;
  t_dlan *cur;
  int result = 0;
  while(vm && (result == 0))
    {
    eth = vm->head_eth;
    while(eth && (result == 0))
      {
      cur = eth->head_lan;
      while(cur && (result == 0))
        {
        if (!strcmp(name, cur->lan))
          result = 1;
        cur = cur->next;
        }
      eth = eth->next;
      }
    vm = vm->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_dyn_lan_exists_in_vm(char *name, int num, char *lan)
{
  int result = 0;
  t_dvm *vm = vm_find(name);
  t_deth *eth = NULL;
  t_dlan *cur;
  if (vm)
    {
    eth = eth_find(vm, num);
    if (eth)
      {
      cur = vlan_find(eth, lan);
      if (cur)
        result = 1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_dyn_is_all_empty(void)
{
  t_dvm *dvm_next, *dvm = g_head_vm;
  int result = 1;
  while(dvm)
    {
    result = 0;
    dvm_next = dvm->next;
    KERR("%s", dvm->name); 
    vm_free(dvm->name);
    dvm = dvm_next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_dyn_init(void)
{
  g_head_vm = NULL;
}
/*--------------------------------------------------------------------------*/

