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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "machine_create.h"
#include "utils_cmd_line_maker.h"
#include "event_subscriber.h"
#include "dpdk_xyx.h"
#include "dpdk_ovs.h"
#include "fmt_diag.h"
#include "dpdk_msg.h"


typedef struct t_ethd_cnx
{
  char name[MAX_NAME_LEN];
  int num;
  int ready;
  char lan[MAX_NAME_LEN];
  int waiting_ack_add_lan;
  int waiting_ack_del_lan;
  int attached_lan_ok;
  int llid;
  int tid;
  struct t_ethd_cnx *prev;
  struct t_ethd_cnx *next;
} t_ethd_cnx;

typedef struct t_kvm_cnx
{
  char name[MAX_NAME_LEN];
  int nb_tot_eth;
  t_eth_table eth_tab[MAX_DPDK_VM];
  int  dyn_vm_add_eth_done;
  int  dyn_vm_add_eth_todo;
  int  dyn_vm_add_acked;
  int  dyn_vm_add_not_acked;
  struct t_kvm_cnx *prev;
  struct t_kvm_cnx *next;
} t_kvm_cnx;

typedef struct t_add_lan
{
  char name[MAX_NAME_LEN];
  int num;
  char lan[MAX_NAME_LEN];
  int llid;
  int tid;
  int count;
  struct t_add_lan *prev;
  struct t_add_lan *next;
} t_add_lan;

static t_kvm_cnx *g_head_kvm;
static t_add_lan *g_head_lan;
static t_ethd_cnx *g_head_ethd;
static int g_nb_ethd;

/*****************************************************************************/
static t_ethd_cnx *ethd_find(char *name, int num)
{
  t_ethd_cnx *cur = g_head_ethd;
  if (strlen(name))
    {
    while(cur && (strcmp(cur->name, name) || (cur->num != num)))
      {
      cur = cur->next;
      }
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void ethd_alloc(char *name, int num)
{
  t_ethd_cnx *cur  = ethd_find(name, num);
  if (cur)
    KERR("ERROR KVMETH ALLOC ETHD %s %d", name, num);
  else
    {
    cur  = (t_ethd_cnx *) clownix_malloc(sizeof(t_ethd_cnx), 6);
    memset(cur, 0, sizeof(t_ethd_cnx));
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    cur->num = num;
    if (g_head_ethd)
      g_head_ethd->prev = cur;
    cur->next = g_head_ethd;
    g_head_ethd = cur;
    g_nb_ethd += 1;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void ethd_free(char *name, int num)
{
  t_ethd_cnx *cur  = ethd_find(name, num);
  if (!cur)
    KERR("ERROR %s %d", name, num);
  else
    {
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == g_head_ethd)
      g_head_ethd = cur->next;
    clownix_free(cur, __FUNCTION__);
    g_nb_ethd -= 1;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_kvm_cnx *kvm_find(char *name)
{
  t_kvm_cnx *cur = g_head_kvm;
  while(name[0] && cur && strcmp(cur->name, name))
    cur = cur->next;
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void kvm_alloc(char *name, int nb_tot_eth, t_eth_table *eth_tab)
{
  t_kvm_cnx *cur  = (t_kvm_cnx *) clownix_malloc(sizeof(t_kvm_cnx), 6);
  memset(cur, 0, sizeof(t_kvm_cnx));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->nb_tot_eth = nb_tot_eth;
  memcpy(cur->eth_tab, eth_tab, nb_tot_eth * sizeof(t_eth_table));
  cur->dyn_vm_add_acked = 1; 
  if (g_head_kvm)
    g_head_kvm->prev = cur;
  cur->next = g_head_kvm;
  g_head_kvm = cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void kvm_free(char *name)
{
  t_kvm_cnx *cur = kvm_find(name);
  if (!cur)
    KERR("%s", name);
  else
    {
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == g_head_kvm)
      g_head_kvm = cur->next;
    clownix_free(cur, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_add_lan *lan_find(char *name)
{
  t_add_lan *cur = g_head_lan;
  while(cur && strcmp(cur->name, name))
    cur = cur->next;
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void lan_alloc(int llid, int tid, char *name, int num, char *lan)
{
  t_add_lan *cur  = (t_add_lan *) clownix_malloc(sizeof(t_add_lan), 6);
  memset(cur, 0, sizeof(t_add_lan));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->lan, lan, MAX_NAME_LEN-1);
  cur->num = num; 
  cur->llid = llid; 
  cur->tid = tid; 
  if (g_head_lan)
    g_head_lan->prev = cur;
  cur->next = g_head_lan;
  g_head_lan = cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void lan_free(t_add_lan *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_lan)
    g_head_lan = cur->next;
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static int dpdk_kvm_add(char *name, int num, int eth_type)
{
  int result;
  if (eth_type == endp_type_ethd)
    {
    if (fmt_tx_add_ethds(0, name, num))
      KERR("ERROR %s %d", name, num);
    else
      {
      ethd_alloc(name, num);
      result = 0;
      }
    }
  else if (eth_type == endp_type_eths)
    result = dpdk_xyx_add(0, 0, name, num, endp_type_eths);
  else
    KOUT("%d", eth_type);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void try_connect_kvm(t_kvm_cnx *cur)
{
  char *endp_path;
  int i, success = 0, some_more;
  if ((cur->dyn_vm_add_eth_done == 0) &&
      (cur->dyn_vm_add_acked == 1))
    {
    for (i = cur->dyn_vm_add_eth_todo; i < cur->nb_tot_eth; i++)
      {
      if ((cur->eth_tab[i].eth_type == endp_type_ethd) ||
          (cur->eth_tab[i].eth_type == endp_type_eths))
        {
        endp_path = utils_get_dpdk_endp_path(cur->name, i);
        if (!access(endp_path, F_OK))
          {
          success = 1;
          cur->dyn_vm_add_eth_todo = i+1;
          if (dpdk_kvm_add(cur->name, i, cur->eth_tab[i].eth_type))
            KERR("ERROR %s %d", cur->name, i);
          else
            cur->dyn_vm_add_acked = 0; 
          break;
          }
        }
      }
    if (success)
      {
      some_more = 0;
      for (i = cur->dyn_vm_add_eth_todo; i < cur->nb_tot_eth; i++)
        {
        if ((cur->eth_tab[i].eth_type == endp_type_ethd) ||
            (cur->eth_tab[i].eth_type == endp_type_eths))
          some_more = 1;
        }
      if (some_more == 0)
        {
        cur->dyn_vm_add_eth_done = 1; 
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_beat(void *data)
{
  t_kvm_cnx *next, *cur  = g_head_kvm;
  int ovs_ok = dpdk_ovs_still_present();
  t_vm *vm;
  t_add_lan *next_cl, *cl = g_head_lan;
  
  while(cur)
    {
    next = cur->next; 
    vm = cfg_get_vm(cur->name);
    if (ovs_ok != 1)
      {
      KERR("ERROR KVMETH OVS NOT READY %s", cur->name);
      }
    else if ((vm != NULL) && (vm->vm_to_be_killed == 1))
      {
      KERR("ERROR KVMETH VM KILL %s", cur->name);
      kvm_free(cur->name);
      }
    else if (vm == NULL)
      {
      KERR("WARNING KVMETH VM NOT PRESENT %s", cur->name);
      kvm_free(cur->name);
      }
    else
      {
      try_connect_kvm(cur);
      if (cur->dyn_vm_add_acked == 1)
        cur->dyn_vm_add_not_acked = 0;
      else
        {
        cur->dyn_vm_add_not_acked += 1;
        if (cur->dyn_vm_add_not_acked == 500)
          {
          KERR("ERROR %s", cur->name);
          poweroff_vm(0, 0, vm);
          }
        }
      }
    cur = next;
    }
  while(cl)
    {
    next_cl = cl->next;
    if ((dpdk_xyx_exists(cl->name, cl->num)) &&
        (dpdk_xyx_ready(cl->name, cl->num)))
      {
      if (dpdk_xyx_lan_empty(cl->name, cl->num))
        dpdk_xyx_add_lan(cl->llid, cl->tid, cl->name, cl->num, cl->lan);
      else
        {
        KERR("ERROR KVMETH ADD %s %d %s", cl->name, cl->num, cl->lan);
        utils_send_status_ko(&(cl->llid), &(cl->tid), "lan existing");
        }
      lan_free(cl);
      }
    else
      {
      cl->count += 1;
      if (cl->count > 500)
        {
        KERR("ERROR KVMETH ADD %s %d %s", cl->name, cl->num, cl->lan);
        lan_free(cl);
        }
      }
    cl = next_cl;
    }
  clownix_timeout_add(10, timer_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_kvm_exists(char *name, int num)
{
  int result = 0;
  t_ethd_cnx *cur = ethd_find(name, num);
  if (cur)
    result = endp_type_ethd;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_kvm_add_whole_vm(char *name, int nb_tot_eth, t_eth_table *eth_tab)
{
  int i;
  int must_add = 0;
  char *endp_path;
  t_kvm_cnx *cur = kvm_find(name);
  if (cur != NULL)
    KERR("ERROR KVMETH VM %s", name);
  else
    {
    for (i = 0; i < nb_tot_eth; i++)
      {
      if ((eth_tab[i].eth_type == endp_type_ethd) ||
          (eth_tab[i].eth_type == endp_type_eths))
        {
        endp_path = utils_get_dpdk_endp_path(name, i);
        unlink(endp_path);
        must_add = 1;
        }
      }
    if (must_add == 1)
      kvm_alloc(name, nb_tot_eth, eth_tab);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_kvm_del(int llid, int tid, char *name, int num, int eth_type)
{
  int result = -1;
  t_ethd_cnx *cur;
  if (eth_type == endp_type_ethd)
    {
    cur  = ethd_find(name, num);
    if (cur != NULL)
      {
      if ((strlen(cur->lan)) &&
          ((cur->waiting_ack_add_lan == 1) ||
          (cur->attached_lan_ok == 1)))
        {
        if (cur->waiting_ack_del_lan == 1)
          KERR("ERROR %s %d", name, num);
        if (dpdk_msg_send_del_lan_ethd(cur->lan, name, num))
          KERR("ERROR %s %s %d", cur->lan, name, num);
        cur->attached_lan_ok = 0;
        cur->waiting_ack_del_lan = 1;
        }
      if (fmt_tx_del_ethds(0, name, num))
        KERR("ERROR %s %d", name, num);
      }
    else
      KERR("ERROR DELETH %s %d", name, num);
    }
  else
    {
    result = dpdk_xyx_del(llid, tid, name, num);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_kvm_resp_add_lan(int is_ko, char *lan, char *name, int num)
{
  t_ethd_cnx *cur = ethd_find(name, num);
  if (cur)
    {
    cur->attached_lan_ok = 1;
    cur->waiting_ack_add_lan = 0;
    utils_send_status_ok(&(cur->llid),&(cur->tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
  else
    {
    dpdk_xyx_resp_add_lan(is_ko, lan, name, num);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_kvm_resp_del_lan(int is_ko, char *lan, char *name, int num)
{
  t_ethd_cnx *cur = ethd_find(name, num);
  if (cur)
    {
    cur->attached_lan_ok = 0;
    cur->waiting_ack_del_lan = 0;
    memset(cur->lan, 0, MAX_NAME_LEN);
    utils_send_status_ok(&(cur->llid),&(cur->tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
  else
    {
    dpdk_xyx_resp_del_lan(is_ko, lan, name, num);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_kvm_add_lan(int llid, int tid, char *name, int num, char *lan,
                     int endp_type)
{
  int result = -1;
  t_ethd_cnx *cur;
  if (endp_type == endp_type_ethd)
    {
    cur  = ethd_find(name, num);
    if (cur == NULL)
      KERR("ERROR ETHD %s %d %s", name, num, lan);
    else if (cur->ready != 1)
      KERR("ERROR ETHD %s %d %s", name, num, lan);
    else if (cur->waiting_ack_del_lan)
      KERR("ERROR ETHD %s %d %s", name, num, lan);
    else if (cur->waiting_ack_add_lan)
      KERR("ERROR ETHD %s %d %s", name, num, lan);
    else if (cur->attached_lan_ok)
      KERR("ERROR ETHD %s %d %s", name, num, lan);
    else
      {
      if (dpdk_msg_send_add_lan_ethd(lan, name, num))
        KERR("ERROR %s %d %s", name, num, lan);
      else
        {
        if (strlen(cur->lan))
          {
          KERR("ERROR %s %d %s", name, num, lan);
          memset(cur->lan, 0, MAX_NAME_LEN);
          }
        cur->waiting_ack_add_lan = 1;
        strncpy(cur->lan, lan, MAX_NAME_LEN-1);
        cur->llid = llid;
        cur->tid = tid;
        result = 0;
        }
      }
    }
  else
    {
    if (dpdk_xyx_exists(name, num))
      {
      if (lan_find(name) == NULL)
        {
        lan_alloc(llid, tid, name, num, lan);
        result = 0;
        }
      else
        KERR("ERROR KVMETH ADDLAN %s %d %s", name, num, lan);
      }
    else
      KERR("ERROR KVMETH ADDLAN %s %d %s", name, num, lan);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int dpdk_kvm_del_lan(int llid, int tid, char *name, int num, char *lan,
                     int endp_type)
{
  int result = -1;
  t_ethd_cnx *cur;
  if (endp_type == endp_type_ethd)
    {
    cur  = ethd_find(name, num);
    if (cur == NULL)
      KERR("ERROR ETHD %s %d %s", name, num, lan);
    else if (cur->ready != 1)
      KERR("ERROR ETHD %s %d %s", name, num, lan);
    else if (cur->waiting_ack_del_lan)
      KERR("ERROR ETHD %s %d %s", name, num, lan);
    else if (cur->waiting_ack_add_lan)
      KERR("ERROR ETHD %s %d %s", name, num, lan);
    else if (cur->attached_lan_ok == 0)
      KERR("ERROR ETHD %s %d %s", name, num, lan);
    else if (strcmp(lan, cur->lan))
      KERR("ERROR ETHD %s %d %s %s", name, num, lan, cur->lan);
    else
      {
      if (dpdk_msg_send_del_lan_ethd(lan, name, num))
        KERR("ERROR %s %s %d", lan, name, num);
      else
        {
        cur->waiting_ack_del_lan = 1;
        cur->llid = llid;
        cur->tid = tid;
        result = 0;
        }
      }
    }
  else
    {
    result = dpdk_xyx_del_lan(llid, tid, name, num, lan);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_kvm_resp_add(int is_ko, char *name, int num)
{
  t_ethd_cnx *ethd;
  t_kvm_cnx *cur = kvm_find(name);
  if (cur == NULL)
    KERR("ERROR KVM %s %d", name, num);
  else
    {
    ethd = ethd_find(name, num);
    cur->dyn_vm_add_acked = 1; 
    if (ethd == NULL)
      dpdk_xyx_resp_add(is_ko, name, num);
    else
      ethd->ready = 1;
    if (cur->dyn_vm_add_eth_done == 1) 
      kvm_free(cur->name);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_kvm_resp_add_eths2(int is_ko, char *name, int num)
{
  if (is_ko)
    {
    KERR("ERROR RESP ADDETH END KO %s %d", name, num);
    dpdk_xyx_eths2_resp_ok(0, name, num);
    }
  else
    dpdk_xyx_eths2_resp_ok(1, name, num);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void dpdk_kvm_resp_del(int is_ko, char *name, int num)
{
  t_ethd_cnx *cur;
  char *endp_path = utils_get_dpdk_endp_path(name, num);
  cur  = ethd_find(name, num);
  if (cur != NULL)
    {
    ethd_free(name, num);
    }
  else
    {
    dpdk_xyx_resp_del(is_ko, name, num);
    }
  unlink(endp_path);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *translate_topo_endp_ethd(int *nb)
{
  t_ethd_cnx *cur = g_head_ethd;
  int len, nb_endp = 0;
  t_topo_endp *endp;
  len = g_nb_ethd * sizeof(t_topo_endp);
  endp = (t_topo_endp *) clownix_malloc( len, 13);
  memset(endp, 0, len);
  while(cur)
    {
    len = sizeof(t_lan_group_item);
    endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
    memset(endp[nb_endp].lan.lan, 0, len);
    strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
    endp[nb_endp].num = cur->num;
    endp[nb_endp].type = endp_type_ethd;
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

/*****************************************************************************/
void dpdk_kvm_init(void)
{
  g_nb_ethd = 0;
  g_head_kvm = NULL;
  g_head_lan = NULL;
  clownix_timeout_add(5, timer_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

