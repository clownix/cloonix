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
#include "suid_power.h"
#include "endp_evt.h"
#include "edp_mngt.h"
#include "edp_evt.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "endp_mngt.h"
#include "edp_sock.h"
#include "edp_vhost.h"
#include "pci_dpdk.h"
#include "utils_cmd_line_maker.h"
#include "dpdk_dyn.h"
#include "dpdk_d2d.h"
#include "vhost_eth.h"


static t_edp *g_head_edp;
static int g_nb_edp;

/****************************************************************************/
static t_edp *edp_alloc(char *name, int endp_type)
{
  t_edp *cur;
  cur = (t_edp *) clownix_malloc(sizeof(t_edp), 13);
  memset(cur, 0, sizeof(t_edp));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->endp_type = endp_type;
  if (g_head_edp)
    g_head_edp->prev = cur;
  cur->next = g_head_edp;
  g_head_edp = cur;
  g_nb_edp += 1;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_edp *edp_mngt_edp_find(char *name)
{
  t_edp *cur = g_head_edp;
  while(cur)
    {
    if ((!strcmp(name, cur->name)))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_mngt_edp_free(t_edp *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_head_edp == cur)
    g_head_edp = cur->next;
  if (g_nb_edp < 1)
    KOUT(" ");
  g_nb_edp -= 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_edp *edp_mngt_lan_find(char *lan)
{
  t_edp *cur = g_head_edp;
  while(cur)
    {
    if (!strcmp(lan, cur->lan))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_edp *edp_mngt_name_lan_find(char *name, char *lan)
{
  t_edp *cur = g_head_edp;
  while(cur)
    {
    if (strlen(cur->lan))
      {
      if ((!strcmp(name, cur->name)) &&
          (!strcmp(lan, cur->lan)))
        break;
      }
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *edp_mngt_translate_topo_endp(int *nb)
{
  t_edp *cur = g_head_edp;
  int len, nb_endp = 0;
  t_topo_endp *endp;
  endp = (t_topo_endp *) clownix_malloc(g_nb_edp * sizeof(t_topo_endp),13);
  memset(endp, 0, g_nb_edp * sizeof(t_topo_endp));
  while(cur)
    {
    len = sizeof(t_lan_group_item);
    endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
    memset(endp[nb_endp].lan.lan, 0, len);
    strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
    endp[nb_endp].num = 0;
    endp[nb_endp].type = cur->endp_type;
    if (strlen(cur->lan))
      {
      if (cur->flag_lan_ok) 
        {
        strncpy(endp[nb_endp].lan.lan[0].lan, cur->lan, MAX_NAME_LEN-1);
        endp[nb_endp].lan.nb_lan = 1;
        }
      }
    nb_endp += 1;
    cur = cur->next;
    }
  *nb = nb_endp;
  return endp;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_mngt_set_eth_type(t_edp *cur, int eth_type)
{
  if ((cur->eth_type == 0) && (eth_type != eth_type_none))
    KERR("%d %d", cur->eth_type, eth_type);
  else if ((cur->eth_type != eth_type_none) && (eth_type != eth_type_none))
    KERR("%d %d", cur->eth_type, eth_type);
  else
    {
    if (eth_type == eth_type_none)
      cur->eth_type = eth_type_none;
    else if (eth_type == eth_type_dpdk)
      cur->eth_type = eth_type_dpdk;
    else if (eth_type == eth_type_vhost)
      cur->eth_type = eth_type_vhost;
    else if (eth_type == eth_type_sock)
      cur->eth_type = eth_type_sock;
    else
      KOUT("%d", eth_type);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_mngt_exists(char *name, int *endp_type)
{
  int result = 0;
  t_edp *cur = edp_mngt_edp_find(name);
  *endp_type = 0;
  if (cur)
    {
    *endp_type = cur->endp_type;
    result = 1; 
    } 
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_mngt_get_qty(void)
{
  return (g_nb_edp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_mngt_lan_exists(char *lan, int *eth_type, int *endp_type)
{
  int result = 0;
  t_edp *cur = g_head_edp;
  while(cur)
    {
    if (strlen(cur->lan))
      {
      if (!strcmp(lan, cur->lan))
        {
        *endp_type = cur->endp_type;
        *eth_type = cur->eth_type;
        result = 1;
        }
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_mngt_add_lan(int llid, int tid, char *name, char *lan, char *err)
{
  t_edp *cur;
  int eth_type, type, result = -1;
  cur = edp_mngt_edp_find(name); 
  if (cur == NULL)
    {
    KERR("%s %s", name, lan);
    snprintf(err, MAX_PATH_LEN, "Not found: %s", name);
    }
  else if ((llid) && (cur->llid))
    {
    KERR("%s %s", name, lan);
    snprintf(err, MAX_PATH_LEN, "Command running on %s", name);
    }
  else if ((strlen(cur->lan)) &&
           (cur->eth_type != eth_type_none))
    {
    KERR("%s %s", name, lan);
    snprintf(err, MAX_PATH_LEN, "Phy %s is attached to %s", name, cur->lan);
    }
  else if (endp_evt_lan_is_in_use(lan, &type))
    {
    if ((type != endp_type_kvm_sock)  &&
        (type != endp_type_kvm_dpdk)  &&
        (type != endp_type_kvm_vhost) &&
        (type != endp_type_nat)       &&
        (type != endp_type_tap)       &&
        (type != endp_type_snf)       &&
        (type != endp_type_c2c))
      {
      snprintf(err, MAX_PATH_LEN, "Lan %s incompatible type", lan);
      KERR("%s", err);
      } 
    else if (cur->endp_type == endp_type_pci)
      {
      snprintf(err, MAX_PATH_LEN, "%s SOCK not compat pci", lan); 
      KERR("%s", err);
      } 
    else 
      result = 0;
    }
  else if (vhost_lan_exists(lan))
    {
    if (cur->endp_type == endp_type_pci)
      {
      snprintf(err, MAX_PATH_LEN, "%s VHOST not compat pci", lan);
      KERR("%s", err);
      } 
    else if (cur->endp_type == endp_type_snf)
      {
      snprintf(err, MAX_PATH_LEN, "%s VHOST not compat snf", lan);
      KERR("%s", err);
      }
    else if (cur->endp_type == endp_type_nat)
      {
      snprintf(err, MAX_PATH_LEN, "%s VHOST not compat nat", lan);
      KERR("%s", err);
      }
    else
      result = 0;
    }
  else
    result = 0;
  if (result == 0)
    {
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    eth_type = edp_evt_action_add_lan(cur, llid, tid);
    if (eth_type == eth_type_none)
      utils_send_status_ok(&(cur->llid), &(cur->tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_mngt_del_lan(int llid, int tid, char *name, char *lan, char *err)
{
  t_edp *cur = edp_mngt_edp_find(name); 
  int result = -1;
  if (cur == NULL)
    {
    KERR("%s %s", name, lan);
    snprintf(err, MAX_PATH_LEN, "Not found: %s", name);
    }
  else if ((llid) && (cur->llid))
    {
    KERR("%s %s", name, lan);
    snprintf(err, MAX_PATH_LEN, "Command running on %s", name);
    }
  else if (strlen(cur->lan) == 0)
    {
    KERR("%s %s", name, lan);
    snprintf(err, MAX_PATH_LEN, "Phy %s has no lan", name);
    }
  else if (strcmp(cur->lan, lan))
    {
    KERR("%s %s", name, lan);
    snprintf(err,MAX_PATH_LEN,"%s not attached but %s is.", lan, cur->lan);
    }
  else
    {
    edp_evt_action_del_lan(cur, llid, tid);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_mngt_add(int llid, int tid, char *name, int endp_type)
{
  t_edp *cur = edp_mngt_edp_find(name); 
  int result;
  if ((endp_type != endp_type_phy) &&
      (endp_type != endp_type_pci) &&
      (endp_type != endp_type_snf) &&
      (endp_type != endp_type_nat) &&
      (endp_type != endp_type_tap))
    {
    KERR("%s not compatible endp", name);
    result = -1;
    }
  else if (cur != NULL)
    {
    KERR("%s already exists", name);
    result = -1;
    }
  else if ((endp_type == endp_type_phy) &&
           (!suid_power_get_phy_info(name)))
    {
    KERR("%s edp not found", name);
    result = -1;
    }
  else if ((endp_type == endp_type_pci) &&
           (!suid_power_get_pci_info(name)))
    {
    KERR("%s pci not found", name);
    result = -1;
    }
  else
    {
    if (endp_type == endp_type_phy)
      suid_power_ifup_phy(name);
    cur = edp_alloc(name, endp_type); 
    cur->llid = llid;
    cur->tid = tid;
    edp_mngt_set_eth_type(cur, eth_type_none); 
    if ((endp_type == endp_type_phy) ||
        (endp_type == endp_type_snf) ||
        (endp_type == endp_type_nat) ||
        (endp_type == endp_type_tap))
      suid_power_rec_name(name, 1);
    layout_add_sat(name, llid);
    utils_send_status_ok(&(cur->llid), &(cur->tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_mngt_del(int llid, int tid, char *name)
{
  t_edp *cur = edp_mngt_edp_find(name); 
  int result = -1;
  if (cur == NULL)
    {
    KERR("%s", name);
    result = -1;
    }
  else if ((llid) && (cur->llid))
    {
    KERR("%s", name);
    result = -1;
    }
  else
    {
    edp_evt_action_del_lan(cur, llid, tid);
    edp_mngt_edp_free(cur);
    layout_del_sat(name);
    if ((cur->endp_type == endp_type_phy) ||
        (cur->endp_type == endp_type_snf) ||
        (cur->endp_type == endp_type_nat) ||
        (cur->endp_type == endp_type_tap))
      suid_power_rec_name(name, 0);
    utils_send_status_ok(&(cur->llid), &(cur->tid));
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    result = 0;
    }
 return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_mngt_del_all(void)
{
  t_edp *next, *cur = g_head_edp;
  while(cur)
    {
    next = cur->next;
    edp_mngt_del(0, 0, cur->name);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_edp *edp_mngt_get_head_edp(int *nb_edp)
{
  *nb_edp = g_nb_edp;
  return g_head_edp;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_add_nat_vm(void *data)
{
  char *name = (char *) data;
  t_vm *vm = cfg_get_vm(name);
  char err[MAX_PATH_LEN];
  char cisconat[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  int num;
  if (!vm)
    KERR("%s", name);
  else
    {
    memset(cisconat, 0, MAX_NAME_LEN);
    memset(lan, 0, MAX_NAME_LEN);
    snprintf(cisconat, MAX_NAME_LEN-1, "nat_%s", name);
    snprintf(lan, MAX_NAME_LEN-1, "lan_nat_%s", name);
    num = vm->kvm.vm_config_param;
    if (vm->kvm.eth_table[num].eth_type == eth_type_sock)
      {
      if (endp_evt_add_lan(0, 0, name, num, lan, 0))
        KERR("%s %d", name, num);
      else if (edp_mngt_add_lan(0, 0, cisconat, lan, err))
        KERR("ERROR CREATING CISCO NAT LAN %s %s", lan, err);
      }
    else if (vm->kvm.eth_table[num].eth_type == eth_type_dpdk)
      {
      if (!dpdk_dyn_eth_exists(name, num))
        KERR("%s %d", name, num);
      else
        {
        if (dpdk_dyn_add_lan_to_eth(0, 0, lan, name, num, err))
          KERR("%s %d %s", name, num, err);
        else if (edp_mngt_add_lan(0, 0, cisconat, lan, err))
          KERR("ERROR CREATING CISCO NAT LAN %s %s", lan, err);
        }
      }
    else
      KERR("%s %d", name, num);
    } 
  free(data);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void edp_mngt_cisco_nat_create(char *name)
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
    if (edp_mngt_add(0, 0, cisconat, endp_type_nat))
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void edp_mngt_cisco_nat_destroy(char *name)
{
  char cisconat[MAX_NAME_LEN];
  memset(cisconat, 0, MAX_NAME_LEN);
  snprintf(cisconat, MAX_NAME_LEN-1, "nat_%s", name);
  edp_mngt_del(0, 0, cisconat);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void edp_mngt_kvm_lan_exists(char *lan, int *dpdk, int *vhost, int *sock)
{ 
  *dpdk = (dpdk_dyn_lan_exists(lan)) + (dpdk_d2d_lan_exists(lan));
  *vhost = vhost_lan_exists(lan);
  *sock = endp_evt_lan_is_in_use_by_vm_or_c2c(lan);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_edp *edp_mngt_get_first(int *nb_enp)
{
  *nb_enp = g_nb_edp;
  return g_head_edp;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_mngt_init(void)
{
  g_nb_edp   = 0;
  g_head_edp = NULL;
  edp_sock_init();
  edp_vhost_init();
  edp_evt_init();
  pci_dpdk_init();
}
/*--------------------------------------------------------------------------*/
