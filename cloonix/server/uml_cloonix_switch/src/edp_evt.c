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
#include "dpdk_dyn.h"
#include "endp_evt.h"
#include "edp_mngt.h"
#include "edp_evt.h"
#include "edp_phy.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "endp_mngt.h"
#include "edp_sock.h"
#include "pci_dpdk.h"
#include "dpdk_tap.h"
#include "dpdk_snf.h"
#include "dpdk_nat.h"
#include "utils_cmd_line_maker.h"

void snf_globtopo_small_event(char *name, int num_evt, char *path);

/****************************************************************************/
static void erase_inside_lan_if_lan_used(t_edp *cur)
{
  int dpdk, sock;
  if (strlen(cur->lan))
    {
    edp_mngt_kvm_lan_exists(cur->lan, &dpdk, &sock);
    if (dpdk  || sock)
      memset(cur->lan, 0, MAX_NAME_LEN);
    }
  edp_mngt_set_eth_type(cur, eth_type_none); 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int eth_type_update(char *name, char *lan)
{
  int result, dpdk, sock;
  edp_mngt_kvm_lan_exists(lan, &dpdk, &sock);
  if (dpdk)
    {
    if (sock)
      KERR(" ");
    result = eth_type_dpdk;
    }
  else if (sock)
    {
    if (dpdk)
      KERR(" ");
    result = eth_type_sock;
    }
  else
    {
    result = eth_type_none;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int edp_del_inside_dpdk(t_edp *cur, char *name, char *lan)
{
  int result = -1;
  if (cur->endp_type == endp_type_phy)
    {
    if (!cur->flag_lan_ok)
      KERR("%s %s", name, lan);
    else
      {
      if (edp_phy_del_dpdk(lan, name))
        KERR("%s %s", name, lan);
      else
        result = 0;
      }
    }
  else if (cur->endp_type == endp_type_tap)
    {
    if (!dpdk_tap_exist(name))
      KERR("%s %s", name, lan);
    else
      {
      if (dpdk_tap_del(cur->llid, cur->tid, name))
        KERR("%s %s", name, lan);
      else
        result = 0;
      }
    }
  else if (cur->endp_type == endp_type_snf)
    {
    if (!dpdk_snf_exist(name))
      KERR("%s %s", name, lan);
    else
      {
      if (dpdk_snf_del(cur->llid, cur->tid, name))
        KERR("%s %s", name, lan);
      else
        result = 0;
      }
    }
  else if (cur->endp_type == endp_type_nat)
    {
    if (!dpdk_nat_exist(name))
      KERR("%s %s", name, lan);
    else
      {
      if (dpdk_nat_del(cur->llid, cur->tid, name))
        KERR("%s %s", name, lan);
      else
        result = 0;
      }
    }
  else if (cur->endp_type == endp_type_pci)
    {
    if (!cur->flag_lan_ok)
      KERR("%s %s", name, lan);
    else
      {
      if (pci_dpdk_del(lan, name))
        KERR("%s %s", name, lan);
      else
        {
        utils_send_status_ok(&(cur->llid), &(cur->tid));
        result = 0;
        }
      }
    }
  else
    KOUT("%d", cur->endp_type);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int edp_del_inside_sock(t_edp *cur, char *name, char *lan)
{
  int tidx, type, result = -1;
  if (!endp_mngt_exists(name, 0, &type))
    KERR("%s %s", name, lan);
  else
    {
    if (!endp_evt_lan_find(name, 0, lan, &tidx))
      KERR("%s %s", name, lan);
    else
      {
      if (endp_evt_del_lan(name, 0, tidx, lan))
        KERR("%s %s", name, lan);
      else
        result = 0;
      }
    if (type == endp_type_snf) 
      snf_globtopo_small_event(name, snf_evt_capture_off, NULL);
    endp_mngt_stop(name, 0);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int begin_dpdk_tap(t_edp *cur)
{
  int result = eth_type_none;
  if (dpdk_tap_add(cur->llid, cur->tid, cur->name, cur->lan))
    {
    KERR("%s %s", cur->name, cur->lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "tap create error");
    }
  else
    {
    cur->llid = 0;
    cur->tid = 0;
    result = eth_type_dpdk;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int begin_dpdk_phy(t_edp *cur)
{
  int result = eth_type_none;
  if (edp_phy_add_dpdk(cur->llid, cur->tid, cur->lan, cur->name))
    {
    KERR("%s %s", cur->name, cur->lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "phy create error");
    erase_inside_lan_if_lan_used(cur);
    }
  else
    {
    cur->llid = 0;
    cur->tid = 0;
    result = eth_type_dpdk;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int begin_dpdk_snf(t_edp *cur)
{
  int result = eth_type_none;
  if (dpdk_snf_add(cur->llid, cur->tid, cur->name, cur->lan))
    {
    KERR("%s %s", cur->name, cur->lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "snf create error");
    }
  else
    {
    cur->llid = 0;
    cur->tid = 0;
    result = eth_type_dpdk;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int begin_dpdk_nat(t_edp *cur)
{ 
  int result = eth_type_none;
  if (dpdk_nat_add(cur->llid, cur->tid, cur->name, cur->lan))
    {
    KERR("%s %s", cur->name, cur->lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "nat create error");
    }
  else
    {
    cur->llid = 0;
    cur->tid = 0;
    result = eth_type_dpdk;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/**************************************************************************/
static int begin_eth_type_dpdk(t_edp *cur)
{
  int result = eth_type_none;
  if (cur->endp_type == endp_type_phy)
    {
    result = begin_dpdk_phy(cur);
    }
  else if (cur->endp_type == endp_type_tap)
    {
    result = begin_dpdk_tap(cur);
    }
  else if (cur->endp_type == endp_type_snf)
    {
    result = begin_dpdk_snf(cur);
    }
  else if (cur->endp_type == endp_type_nat)
    {
    result = begin_dpdk_nat(cur);
    }
  else if (cur->endp_type == endp_type_pci)
    {
    if (pci_dpdk_add(cur->llid, cur->tid, cur->lan, cur->name))
      {
      KERR("%s %s", cur->name, cur->lan);
      utils_send_status_ko(&(cur->llid), &(cur->tid), "pci create error");
      erase_inside_lan_if_lan_used(cur);
      }
    else
      result = eth_type_dpdk;
    }
  else
    KOUT("%d", cur->endp_type);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int begin_eth_type_sock(t_edp *cur)
{
  int endp_type;
  int result = eth_type_none;

  if (endp_mngt_exists(cur->name, 0, &endp_type))
    {
    KERR("%s %s", cur->name, cur->lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "sock endp exists");
    erase_inside_lan_if_lan_used(cur);
    }
  else if (endp_mngt_start(0, 0, cur->name, 0, cur->endp_type))
    {
    KERR("%s %s", cur->name, cur->lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "sock endp bad start");
    erase_inside_lan_if_lan_used(cur);
    }
  else
    {
    edp_sock_timer_add(cur->llid, cur->tid, cur->name, cur->lan);
    result = eth_type_sock;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int timer_update_type_dpdk(t_edp *cur)
{
  int flag = 0;
  if (cur->endp_type == endp_type_tap)
    {
    if (dpdk_tap_exist(cur->name))
      {
      if (dpdk_tap_lan_exists(cur->lan))
        flag = 1;
      }
    }
  else if (cur->endp_type == endp_type_snf)
    {
    if (dpdk_snf_exist(cur->name))
      {
      if (dpdk_snf_lan_exists(cur->lan))
        flag = 1;
      }
    }
  else if (cur->endp_type == endp_type_nat)
    {
    if (dpdk_nat_exist(cur->name))
      {
      if (dpdk_nat_lan_exists(cur->lan))
        flag = 1;
      }
    }
  else if (cur->endp_type == endp_type_phy) 
    {
    if (dpdk_dyn_lan_exists(cur->lan))
      flag = 1;
    }
  else if (cur->endp_type == endp_type_pci)
    {
    if (dpdk_dyn_lan_exists(cur->lan))
      {
      if (cur->dpdk_attach_ok)
        flag = 1;
      }
    }
  else
    KOUT("%d", cur->endp_type);
  return flag;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int timer_update_type_sock(t_edp *cur)
{
  int tidx, type, flag = 0;
  if (endp_mngt_exists(cur->name, 0, &type))
    {
    if (endp_evt_lan_find(cur->name, 0, cur->lan, &tidx))
      flag = 1;
    }
  return flag;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_flag_lan(t_edp *cur, int flag)
{
  if (cur->flag_lan_ok != flag)
    {
    if (cur->flag_lan_ok == 0)
      {
      cur->flag_lan_ok = 1;
      utils_send_status_ok(&(cur->llid), &(cur->tid));
      }
    else
      {
      cur->flag_lan_ok = 0;
      }
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
  if ((flag == 0) && (cur->count_flag_ko > 2))
    {
    KERR("CLEANING LAN %s %s", cur->lan, cur->name);
    erase_inside_lan_if_lan_used(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_update_flag_lan(void *data)
{
  int flag, nb_edp;
  t_edp *cur = edp_mngt_get_head_edp(&nb_edp);
  while(cur)
    {
    if (cur->llid)
      cur->count_llid_on += 1;
    else
      cur->count_llid_on = 0;
    if (cur->count_llid_on > 5)
      {
      KERR("%d %s %s", cur->eth_type, cur->name, cur->lan);
      cur->llid = 0;
      cur->tid = 0;
      }
    if (strlen(cur->lan))
      {
      cur->count_flag_ko += 1;
      if (cur->eth_type == eth_type_none)
        {
        flag = 1;
        }
      else
        {
        flag = 0;
        if (cur->eth_type == eth_type_sock)
          {
          flag = timer_update_type_sock(cur);
          }
        else if (cur->eth_type == eth_type_dpdk)
          {
          flag = timer_update_type_dpdk(cur);
          }
        else
          KOUT("%d", cur->eth_type);
        }
      if (flag)
        cur->count_flag_ko = 0;
      set_flag_lan(cur, flag);
      }
    cur = cur->next;
    }
  clownix_timeout_add(50, timer_update_flag_lan, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_evt_action_add_lan(t_edp *cur, int llid, int tid)
{
  int eth_type;
  eth_type = eth_type_update(cur->name, cur->lan);
  cur->llid = llid;
  cur->tid = tid;
  if (eth_type == eth_type_dpdk)
    eth_type = begin_eth_type_dpdk(cur);
  else if (eth_type == eth_type_sock)
    eth_type = begin_eth_type_sock(cur);
  else if (eth_type == eth_type_none)
    utils_send_status_ok(&(cur->llid), &(cur->tid));
  else
    KOUT("%d", eth_type);
  edp_mngt_set_eth_type(cur, eth_type);
  return eth_type;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_action_del_lan(t_edp *cur, int llid, int tid)
{
  cur->llid = llid;
  cur->tid = tid;
  if (cur->eth_type == eth_type_dpdk)
    {
    if (edp_del_inside_dpdk(cur, cur->name, cur->lan))
      {
      KERR("%s %s", cur->name, cur->lan);
      utils_send_status_ko(&(cur->llid), &(cur->tid), "dpdk del error");
      }
    else
      {
      cur->llid = 0;
      cur->tid = 0;
      }
    }
  else if (cur->eth_type == eth_type_sock)
    {
    if (edp_del_inside_sock(cur, cur->name, cur->lan))
      {
      KERR("%s %s", cur->name, cur->lan);
      utils_send_status_ko(&(cur->llid), &(cur->tid), "sock del error");
      }
    else
      {
      utils_send_status_ok(&(cur->llid), &(cur->tid));
      }
    }
  else if (cur->eth_type == eth_type_none)
    {
    utils_send_status_ok(&(cur->llid), &(cur->tid));
    }
  else
    KOUT("%d", cur->eth_type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_change_phy_topo(void)
{
  int nb_edp;
  t_edp *next, *cur = edp_mngt_get_head_edp(&nb_edp);
  while(cur)
    {
    next = cur->next;
    if ((cur->endp_type == endp_type_phy) &&
        (!suid_power_get_phy_info(cur->name)) &&
        (strlen(cur->lan)))
      {
      KERR("%s", cur->lan);
      edp_evt_action_del_lan(cur, 0, 0);
      edp_mngt_edp_free(cur);
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_change_pci_topo(void)
{
  int nb_edp;
  t_edp *next, *cur = edp_mngt_get_head_edp(&nb_edp);
  while(cur)
    {
    next = cur->next;
    if ((cur->endp_type == endp_type_pci) &&
        (!suid_power_get_pci_info(cur->name)) &&
        (strlen(cur->lan)))
      {
      KERR("%s", cur->lan);
      edp_evt_action_del_lan(cur, 0, 0);
      edp_mngt_edp_free(cur);
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_update_fix_type_add(int eth_type, char *name, char *lan)
{
  int nb_edp, new_eth_type;
  t_edp *cur = edp_mngt_get_head_edp(&nb_edp);
  while(cur)
    {
    if (strlen(cur->lan))
      {
      if ((!strcmp(lan, cur->lan)) && (strcmp(name, cur->name)))
        {
        if (cur->eth_type == eth_type_none)
          {
          new_eth_type = edp_evt_action_add_lan(cur, 0, 0);
          if (new_eth_type != eth_type)
            KERR("%d %d", new_eth_type, eth_type);
          }
        }
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_update_fix_type_del(int eth_type, char *name, char *lan)
{
  int nb_edp, new_eth_type;
  t_edp *cur = edp_mngt_get_head_edp(&nb_edp);
  while(cur)
    {
    if (strlen(cur->lan))
      {
      if ((!strcmp(lan, cur->lan)) && (strcmp(name, cur->name)))
        {
        if ((cur->eth_type != eth_type_none) &&
            (cur->eth_type == eth_type))
          {
          new_eth_type = eth_type_update(cur->name, cur->lan);
          if (new_eth_type == eth_type_none)
            {
            edp_evt_action_del_lan(cur, 0, 0);
            }
          }
        }
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_update_non_fix_add(int eth_type, char *name, char *lan)
{
  t_edp *cur = edp_mngt_name_lan_find(name, lan);
  if (cur)
    {
    if (cur->eth_type == eth_type_sock)
      {
      if (eth_type != eth_type_sock)
        KERR("WRONG TYPE LAN: %s %d %d", lan, cur->eth_type, eth_type);
      else
        cur->flag_lan_ok = 1;
      }
    else if (cur->eth_type == eth_type_dpdk)
      {
      if (eth_type != eth_type_dpdk)
        KERR("WRONG TYPE LAN: %s %d %d", lan, cur->eth_type, eth_type);
      else
        cur->flag_lan_ok = 1;
      }
    else
      {
      KERR("WRONG TYPE: %s %d", lan, eth_type);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_update_non_fix_del(int eth_type, char *name, char *lan)
{
  t_edp *cur = edp_mngt_name_lan_find(name, lan);
  if (cur)
    {
    erase_inside_lan_if_lan_used(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_end_eth_type_dpdk(char *lan, int status)
{
  t_edp *cur = edp_mngt_lan_find(lan);
  if (!cur)
    KERR("%s %d", lan, status);
  else
    {
    cur->dpdk_attach_ok = status;
    if (status == 0)
      erase_inside_lan_if_lan_used(cur);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_init(void)
{
  clownix_timeout_add(200, timer_update_flag_lan, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
