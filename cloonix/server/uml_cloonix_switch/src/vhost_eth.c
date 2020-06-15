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


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "machine_create.h"
#include "event_subscriber.h"
#include "utils_cmd_line_maker.h"
#include "fmt_diag.h"
#include "vhost_eth.h"
#include "edp_mngt.h"
#include "edp_evt.h"
#include "edp_evt.h"
#include "dpdk_ovs.h"
#include "suid_power.h"

/****************************************************************************/
typedef struct t_vhlan
{
  char name[MAX_NAME_LEN];
  int  num;
  char lan[MAX_NAME_LEN];
  char vhost_ifname[IFNAMSIZ];
  int  msg_tid;
  int  cli_llid;
  int  cli_tid;
  int  count;
  int  ack_lan_wait;
  int  ack_port_wait;
  int  dummy_activated;
  struct t_vhlan *prev;
  struct t_vhlan *next;
} t_vhlan;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_vheth
{
  char name[MAX_NAME_LEN];
  int  num;
  char vhost_ifname[IFNAMSIZ];
  t_vhlan *head_lan;
  struct t_vheth *prev;
  struct t_vheth *next;
} t_vheth;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_vhm
{
  char name[MAX_NAME_LEN];
  t_vheth *head_eth;
  struct t_vhm *prev;
  struct t_vhm *next;
} t_vhm;

static t_vhm *g_head_vhm;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_vhlan *vlan_find(t_vheth *eth, char *lan)
{
  t_vhlan *cur = eth->head_lan;
  while (cur)
    {
    if (!strcmp(cur->lan, lan))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_vhlan *vlan_alloc(t_vheth *eth, char *lan)
{
  t_vhlan *cur = vlan_find(eth, lan);
  if (cur)
    KERR("%s %d %s", eth->name, eth->num, lan);
  else
    {
    cur = (t_vhlan *) clownix_malloc(sizeof(t_vhlan), 14);
    memset(cur, 0, sizeof(t_vhlan));
    strncpy(cur->name, eth->name, MAX_NAME_LEN-1);
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    strncpy(cur->vhost_ifname, eth->vhost_ifname, IFNAMSIZ-1);
    cur->num = eth->num;
    if (eth->head_lan)
      eth->head_lan->prev = cur;
    cur->next = eth->head_lan;
    eth->head_lan = cur;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void lowest_vlan_free(t_vheth *eth, t_vhlan *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == eth->head_lan)
    eth->head_lan = cur->next;
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vlan_free(t_vheth *eth, char *lan)
{
  t_vhlan *cur = vlan_find(eth, lan);
  if (!cur)
    KERR("%s %d %s", eth->name, eth->num, lan);
  else
    {
    if (cur->dummy_activated == 1)
      KERR("%s %d %s", eth->name, eth->num, lan);
    if (vhost_lan_exists(lan) == 1)
      {
      if (fmt_tx_del_vhost_lan(0, cur->name, cur->num, cur->lan))
        KERR("%s %d %s", cur->name, cur->num, cur->lan);
      }
    lowest_vlan_free(eth, cur);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_vheth *eth_find(t_vhm *vhm, int num)
{
  t_vheth *cur = vhm->head_eth;
  while (cur)
    {
    if (cur->num == num)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_vheth *eth_alloc(t_vhm *vhm, int num, char *ifname)
{
  t_vheth *cur = eth_find(vhm, num);
  if (cur)
    KERR("%s %d", vhm->name, num);
  else
    {
    cur = (t_vheth *) clownix_malloc(sizeof(t_vheth), 14);
    memset(cur, 0, sizeof(t_vheth));
    strncpy(cur->name, vhm->name, MAX_NAME_LEN-1);
    strncpy(cur->vhost_ifname, ifname, IFNAMSIZ-1);
    cur->num = num;
    if (vhm->head_eth)
      vhm->head_eth->prev = cur;
    cur->next = vhm->head_eth;
    vhm->head_eth = cur;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void eth_free(t_vhm *vhm, int num)
{
  t_vheth *cur = eth_find(vhm, num);
  if (!cur)
    KERR("%s %d", vhm->name, num);
  else if (cur->head_lan)
    KERR("%s %d %s", vhm->name, num, cur->head_lan->lan);
  else 
    {
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == vhm->head_eth)
      vhm->head_eth = cur->next;
    clownix_free(cur, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_vhm *vhm_find(char *name)
{
  t_vhm *cur = g_head_vhm;
  while(cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_vhm *vhm_alloc(char *name)
{
  t_vhm *cur = vhm_find(name);
  if (cur)
    KERR("%s", name);
  else
    {
    cur = (t_vhm *) clownix_malloc(sizeof(t_vhm), 4);
    memset(cur, 0, sizeof(t_vhm));
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    if (g_head_vhm)
      g_head_vhm->prev = cur;
    cur->next = g_head_vhm;
    g_head_vhm = cur;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vhm_free(char *name)
{
  t_vhm *cur = vhm_find(name);
  if (!cur)
    KERR("%s", name);
  else if (cur->head_eth)
    KERR("%s %d", cur->name, cur->head_eth->num);
  else
    {
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == g_head_vhm)
      g_head_vhm = cur->next;
    clownix_free(cur, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_resp_cli(t_vhlan *cur, int is_ok)
{ 
  if (is_ok)
    utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
  else
    utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), "fail");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_vm_beat(void *data)
{
  t_vhm *vhm = g_head_vhm;
  t_vheth *next_eth, *eth;
  t_vhlan *cur;
  char lan[MAX_NAME_LEN];
  while(vhm)
    {
    if (!dpdk_ovs_still_present())
      machine_death(vhm->name, error_death_noovs);
    else
      {
      eth = vhm->head_eth;
      while(eth)
        {
        next_eth = eth->next;
        cur = eth->head_lan;
        if (cur)
          {
          if (cur->cli_llid)
            {
            cur->count += 1;
            if (cur->count > 3)
              {
              memset(lan, 0, MAX_NAME_LEN);
              strncpy(lan, cur->lan, MAX_NAME_LEN-1);
              KERR("TIMEOUT: %s eth: %s %d", cur->lan, cur->name, cur->num);
              send_resp_cli(cur, 0);
              vlan_free(eth, cur->lan);
              event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
              }
            }
          }
        eth = next_eth;
        }
      }
    vhm = vhm->next;
    }
  clownix_timeout_add(100, timer_vm_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_vhlan *ack_get_vhlan(char *lan, char *name, int num)
{
  t_vhm *vhm = vhm_find(name);
  t_vheth *eth;
  t_vhlan *cur;
  if (!vhm)
    KERR("Vm not found: %s", name);
  else
    {
    eth = eth_find(vhm, num);
    if (!eth)
      KERR("Eth not found: %s %d", name, num);
    else
      {
      cur = vlan_find(eth, lan);
      if (!cur)
        KERR("Lan %s not found in: %s %d", lan, name, num);
      }
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int add_del_lan(int add, char *lan, char *name, int num,
                       char *err, t_vheth **peth, t_vhlan **vhlan)
{
  int result = -1;
  t_vhm *vhm = vhm_find(name);
  t_vheth *eth;
  t_vhlan *ptr_vhlan;
  (*vhlan) = NULL;
  memset(err, 0, MAX_PRINT_LEN);
  if (!vhm)
    snprintf(err, MAX_PRINT_LEN-1, "Vm not found: %s", name);
  else
    {
    eth = eth_find(vhm, num);
    if (!eth)
      snprintf(err, MAX_PRINT_LEN-1, "Eth not found: %s %d", name, num);
    else
      {
      ptr_vhlan = vlan_find(eth, lan);
      if (add)
        {
        if (ptr_vhlan)
          {
          snprintf(err, MAX_PRINT_LEN-1, "Lan %s already in Eth: %s %d",
                   lan, name, num);
          }
        else if (eth->head_lan)
          {
          snprintf(err, MAX_PRINT_LEN-1, "Lan %s already in Eth: %s %d",
                   eth->head_lan->lan, name, num);
          }
        else
          {
          ptr_vhlan = vlan_alloc(eth, lan);
          (*peth)  = eth;
          (*vhlan) = ptr_vhlan;
          result = 0;
          }
        }
      else
        {
        if (!ptr_vhlan)
          {
          snprintf(err, MAX_PRINT_LEN-1, "Lan %s not found in Eth: %s %d",
                   lan, name, num);
          }
        else if (ptr_vhlan->cli_llid)
          {
          snprintf(err, MAX_PRINT_LEN-1, "Lan %s Eth: %s %d waiting ack",
                   lan, name, num);
          }
        else
          {
          (*peth)  = eth;
          (*vhlan) = ptr_vhlan;
          result = 0;
          }
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int do_dummy_lan(char *name, int num, int dummy_on)
{
  int result = -1;
  t_vhm *vhm = vhm_find(name);
  t_vheth *eth;
  t_vhlan *cur;
  if (!vhm)
    KERR("Vm not found: %s", name);
  else
    {
    eth = eth_find(vhm, num);
    if (!eth)
      KERR("Eth not found: %s %d", name, num);
    else
      {
      cur = eth->head_lan;
      if (cur)
        {
        if (vhost_lan_exists(cur->lan) != 1)
          KERR("Vlan used: %s %d", name, num);
        else
          {
          if (dummy_on)
            {
            if (fmt_tx_del_vhost_lan(0, cur->name, cur->num, cur->lan))
              KERR("%s %d %s", cur->name, cur->num, cur->lan);
            else 
              cur->dummy_activated = 1;
            }
          else
            {
            cur->dummy_activated = 0;
            if (fmt_tx_add_vhost_lan(0, name, num, cur->lan))
              KERR("%s %d %s %s", name, num, cur->lan, cur->vhost_ifname);
            if (fmt_tx_add_vhost_port(0,name,num,cur->vhost_ifname,cur->lan))
              KERR("%s %d %s %s", name, num, cur->lan, cur->vhost_ifname);
            suid_power_ifup_phy(cur->vhost_ifname);
            }
          result = 0;
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void vhost_eth_add_ack_lan(int tid, int is_ok, char *lan, char *name, int num)
{
  t_vhm *vhm = vhm_find(name);
  t_vheth *eth;
  t_vhlan *cur = ack_get_vhlan(lan, name, num);
  if (!cur)
    KERR("%d %d %s %s %d", tid, is_ok, lan, name, num);
  else if (tid != 0)
    {
    if (tid != cur->msg_tid)
      KERR("%d %d", tid, cur->msg_tid);
    if (!cur->ack_lan_wait)
      KERR("%s %s %d", lan, name, num);
    else
      {
      cur->msg_tid = 0;
      cur->ack_lan_wait = 0;
      if (is_ok == 0)
        {
        KERR("%s %s %d", lan, name, num);
        send_resp_cli(cur, 0);
        eth = eth_find(vhm, num);
        vlan_free(eth, cur->lan);
        event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
        }
      else
        {
        cur->ack_port_wait = 1;
        if (fmt_tx_add_vhost_port(tid, name, num, cur->vhost_ifname, lan))
          KERR("%s %d %s %s", name, num, lan, cur->vhost_ifname);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vhost_eth_add_ack_port(int tid, int is_ok, char *lan, char *name, int num)
{
  t_vhm *vhm = vhm_find(name);
  t_vheth *eth;
  t_vhlan *cur = ack_get_vhlan(lan, name, num);
  if (!cur)
    KERR("%d %d %s %s %d", tid, is_ok, lan, name, num);
  else if (tid != 0)
    {
    if (tid != cur->msg_tid)
      KERR("%d %d", tid, cur->msg_tid);
    if (!cur->ack_port_wait)
      KERR("%s %s %d", lan, name, num);
    else
      {
      cur->msg_tid = 0;
      cur->ack_port_wait = 0;
      if (is_ok == 0)
        {
        send_resp_cli(cur, 0);
        KERR("%s %s %d", lan, name, num);
        eth = eth_find(vhm, num);
        vlan_free(eth, cur->lan);
        }
      else
        {
        suid_power_ifup_phy(cur->vhost_ifname);
        edp_evt_update_fix_type_add(eth_type_vhost, name, lan);
        utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
        event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vhost_eth_del_ack_port(int tid, int is_ok, char *lan, char *name, int num)
{
  t_vhm *vhm = vhm_find(name);
  t_vheth *eth;
  t_vhlan *cur;
  if (tid != 0)
    {
    cur = ack_get_vhlan(lan, name, num);
    if (!cur)
      KERR("%d %d %s %s %d", tid, is_ok, lan, name, num);
    else
      {
      if (tid != cur->msg_tid)
        KERR("%d %d", tid, cur->msg_tid);
      if (!cur->ack_port_wait)
        KERR("%d %d %s %s %d", tid, is_ok, lan, name, num);
      else
        {
        cur->msg_tid = 0;
        cur->ack_port_wait = 0;
        send_resp_cli(cur, is_ok);
        if (is_ok == 0)
          KERR("%s %s %d", lan, name, num);
        }
      edp_evt_update_fix_type_del(eth_type_vhost, name, lan);
      eth = eth_find(vhm, num);
      vlan_free(eth, cur->lan);
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vhost_eth_del_ack_lan(int tid, int is_ok, char *lan, char *name, int num)
{
  if (tid != 0)
    {
    KERR("%d %d %s %s %d", tid, is_ok, lan, name, num);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int vhost_lan_exists(char *lan)
{
  int result = 0;
  t_vhm *vcur = g_head_vhm;
  t_vheth *ecur;
  while(vcur)
    {
    ecur = vcur->head_eth;
    while(ecur)
      {
      if (vlan_find(ecur, lan))
        {
        result += 1;
        }
      ecur = ecur->next;
      }
    vcur = vcur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int vhost_lan_info(char *lan, char *vhost_ifname, char *name, int *num)
{
  int result = 0;
  t_vhm *vcur = g_head_vhm;
  t_vheth *ecur;
  while(vcur)
    {
    ecur = vcur->head_eth;
    while(ecur)
      {
      if (vlan_find(ecur, lan))
        {
        result += 1;
        memset(name, 0, MAX_NAME_LEN);
        memset(vhost_ifname, 0, IFNAMSIZ);
        strncpy(name, vcur->name, MAX_NAME_LEN-1);
        strncpy(vhost_ifname, ecur->vhost_ifname, IFNAMSIZ-1);
        *num = ecur->num;
        }
      ecur = ecur->next;
      }
    vcur = vcur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int vhost_eth_exists(char *name, int num)
{
  int result = 0;
  t_vhm *vhm = vhm_find(name);
  t_vheth *eth;
  if (vhm)
    {
    eth = eth_find(vhm, num);
    if (eth)
      result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int vhost_eth_add_lan(int llid, int tid, char *lan, int vm_id,
                      char *name, int num, char *info)
{
  int result = -1;
  t_vheth *eth;
  t_vhlan *cur;
  char *ifname;
  if (!add_del_lan(1, lan, name, num, info, &eth, &cur))
    {
    if (cur->ack_port_wait)
      KERR("%s %d %s %s", name, num, lan, cur->vhost_ifname);
    else
      {
      cur->msg_tid = utils_get_next_tid();
      cur->cli_llid = llid;
      cur->cli_tid = tid;
      if (vhost_lan_exists(lan) > 1)
        {
        cur->ack_port_wait = 1;
        ifname = cur->vhost_ifname;
        if (fmt_tx_add_vhost_port(cur->msg_tid, name, num, ifname, lan))
          KERR("%s %d %s %s", name, num, lan, ifname);
        }
      else
        {
        cur->ack_lan_wait = 1;
        if (fmt_tx_add_vhost_lan(cur->msg_tid, name, num, lan))
          KERR("%s %d %s", name, num, lan);
        }
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int vhost_eth_del_lan(int llid, int tid, char *lan,
                      char *name, int num, char *info)
{
  int result = -1;
  t_vheth *eth;
  t_vhlan *cur;
  char *ifname;
  if (!add_del_lan(0, lan, name, num, info, &eth, &cur))
    {
    if (cur->ack_port_wait)
      KERR("%s %d %s %s", name, num, lan, cur->vhost_ifname);
    else
      {
      ifname = cur->vhost_ifname;
      suid_power_ifdown_phy(ifname);
      if (cur->dummy_activated)
        {
        if (vhost_lan_exists(lan) != 1)
          KERR("%s %d %s %s", name, num, lan, ifname);
        lowest_vlan_free(eth, cur);
        event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
        }
      else
        {
        cur->msg_tid = utils_get_next_tid();
        cur->cli_llid = llid;
        cur->cli_tid = tid;
        cur->ack_port_wait = 1;
        if (fmt_tx_del_vhost_port(cur->msg_tid, name, num, ifname, lan))
          KERR("%s %d %s %s", name, num, lan, ifname);
        }
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vhost_eth_add_vm(char *name, int nb_tot_eth, t_eth_table *eth_tab)
{
  int i;
  t_vhm *vhm = vhm_find(name);
  if (vhm)
    KERR("%s", name);
  else
    {
    for (i=0; i<nb_tot_eth; i++)
      {
      if (eth_tab[i].eth_type == eth_type_vhost) 
        {
        vhm = vhm_alloc(name);
        break;
        }
      }
    if (vhm)
      {
      for (i=0; i<nb_tot_eth; i++)
        {
        if (eth_tab[i].eth_type == eth_type_vhost)
          {
          eth_alloc(vhm, i, eth_tab[i].vhost_ifname);
          }
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vhost_eth_del_vm(char *name, int nb_tot_eth, t_eth_table *eth_tab)
{
  int i;
  t_vhm *vhm = vhm_find(name);
  t_vheth *eth;
  t_vhlan *cur;
  char *ifname;
  if (vhm)
    {
    for (i=0; i<nb_tot_eth; i++)
      {
      if (eth_tab[i].eth_type == eth_type_vhost)
        {
        eth = eth_find(vhm, i); 
        if (!eth)
          KERR("%s %d", name, i);
        else
          {
          if (eth->head_lan)
            {
            cur = eth->head_lan;
            if (cur->dummy_activated == 1)
              lowest_vlan_free(eth, cur);
            else
              {
              ifname = cur->vhost_ifname; 
              if (fmt_tx_del_vhost_port(0, name, i, ifname, cur->lan))
                KERR("%s %d %s %s", name, i, cur->lan, ifname);
              vlan_free(eth, cur->lan);
              }
            }
          eth_free(vhm, i);
          }
        }
      }
    vhm_free(name);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *vhost_eth_translate_topo_endp(int *nb)
{
  int len, nb_endp = 0, nb_endp_vlan = 0, nb_lan = 1;
  t_topo_endp *result;
  t_vhm *vhm = g_head_vhm;
  t_vheth *eth;
  t_vhlan *cur;
  while(vhm)
    {
    eth = vhm->head_eth;
    while(eth)
      {
      nb_endp += 1;
      eth = eth->next;
      }
    vhm = vhm->next;
    }
  result = (t_topo_endp *) clownix_malloc(nb_endp*sizeof(t_topo_endp),13);
  memset(result, 0, nb_endp*sizeof(t_topo_endp));
  vhm = g_head_vhm;
  while(vhm)
    {
    eth = vhm->head_eth;
    while(eth)
      {
      cur = eth->head_lan;
      if ((cur) && (cur->cli_llid == 0))
        {
        len = nb_lan * sizeof(t_lan_group_item);
        result[nb_endp_vlan].lan.lan=(t_lan_group_item *)clownix_malloc(len,2);
        memset(result[nb_endp_vlan].lan.lan, 0, len);
        strncpy(result[nb_endp_vlan].name, cur->name, MAX_NAME_LEN-1);
        result[nb_endp_vlan].num = cur->num;
        result[nb_endp_vlan].type = endp_type_kvm_vhost;
        result[nb_endp_vlan].lan.nb_lan = nb_lan;
        strncpy(result[nb_endp_vlan].lan.lan[0].lan, cur->lan, MAX_NAME_LEN-1);
        nb_endp_vlan += 1;
        }
      eth = eth->next;
      }
    vhm = vhm->next;
    }
  *nb = nb_endp_vlan;
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void vhost_eth_tap_rename(char *name, int num, char *new_eth_name)
{
  t_vhm *vhm = vhm_find(name);
  t_vheth *eth;
  t_vhlan *cur;
  t_vm *vm = cfg_get_vm(name);;
  char *ifname;
  if (!vm)
    KERR("Vm not found: %s", name);
  else if (!vhm)
    KERR("Vm not found: %s", name);
  else
    {
    if ((num < 0) || (num >= vm->kvm.nb_tot_eth))
      KOUT("%d %d", num, vm->kvm.nb_tot_eth);
    if (vm->kvm.eth_table[num].eth_type != eth_type_vhost)
      KERR("%d %d", vm->kvm.eth_table[num].eth_type, eth_type_vhost);
    else
      {
      memset(vm->kvm.eth_table[num].vhost_ifname, 0, IFNAMSIZ);
      strncpy(vm->kvm.eth_table[num].vhost_ifname, new_eth_name, IFNAMSIZ-1);
      eth = eth_find(vhm, num);
      if (!eth)
        KERR("Eth not found: %s %d", name, num);
      else
        {
        memset(eth->vhost_ifname, 0, IFNAMSIZ);
        strncpy(eth->vhost_ifname, new_eth_name, IFNAMSIZ-1);
        cur = eth->head_lan;
        while (cur)
          {
          memset(cur->vhost_ifname, 0, IFNAMSIZ);
          strncpy(cur->vhost_ifname, new_eth_name, IFNAMSIZ-1);
          cur = cur->next;
          }
        }
      ifname = vhost_ident_get(vm->kvm.vm_id, num);
      if (strcmp(ifname, new_eth_name))
        {
        suid_power_ifup_phy(new_eth_name);
        if (do_dummy_lan(name, num, 1))
          KERR("%s %d", name, num);
        }
      else
        {
        suid_power_ifdown_phy(new_eth_name);
        if (do_dummy_lan(name, num, 0))
          KERR("%s %d", name, num);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void vhost_eth_init(void)
{
  g_head_vhm = 0;
  clownix_timeout_add(100, timer_vm_beat, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

