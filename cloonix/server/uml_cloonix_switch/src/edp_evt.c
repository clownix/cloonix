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
#include "vhost_eth.h"
#include "endp_evt.h"
#include "edp_mngt.h"
#include "edp_evt.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "endp_mngt.h"
#include "edp_sock.h"
#include "edp_vhost.h"
#include "pci_dpdk.h"
#include "dpdk_tap.h"
#include "dpdk_snf.h"
#include "dpdk_nat.h"
#include "utils_cmd_line_maker.h"

void snf_globtopo_small_event(char *name, int num_evt, char *path);

/****************************************************************************/
static int eth_type_update(char *name, char *lan)
{
  int type, result, dpdk, vhost, sock;
  dpdk = dpdk_dyn_lan_exists(lan);
  vhost = vhost_lan_exists(lan);
  sock = endp_evt_lan_is_in_use_by_other(name, lan, &type);
  if (dpdk)
    {
    if (vhost || sock)
      KERR("%d %d", vhost, sock);
    result = eth_type_dpdk;
    }
  else if (vhost)
    {
    if (dpdk || sock)
      KERR("%d %d", dpdk, sock);
    result = eth_type_vhost;
    }
  else if (sock)
    {
    if (dpdk || vhost)
      KERR("%d %d", dpdk, vhost);
    if ((type != endp_type_kvm_sock) &&
        (type != endp_type_phy)  &&
        (type != endp_type_tap)  &&
        (type != endp_type_nat)  &&
        (type != endp_type_c2c)  &&
        (type != endp_type_snf))
      KOUT("%d", type); 
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
static void edp_del_inside_dpdk(t_edp *cur, char *name, char *lan)
{
  if (cur->endp_type == endp_type_phy)
    {
    KERR("%s %s", name, lan);
    }
  else if (cur->endp_type == endp_type_tap)
    {
    if (dpdk_tap_exist(name))
      {
      dpdk_tap_del(name);
      }
    }
  else if (cur->endp_type == endp_type_snf)
    {
    if (dpdk_snf_exist(name))
      {
      dpdk_snf_del(name);
      }
    }
  else if (cur->endp_type == endp_type_nat)
    {
    if (dpdk_nat_exist(name))
      {
      dpdk_nat_del(name);
      }
    }
  else if (cur->endp_type == endp_type_pci)
    {
    if (cur->flag_lan_ok)
      {
      if (pci_dpdk_del(lan, name))
        KERR("%s %s", name, lan);
      }
    }
  else
    KOUT("%d", cur->endp_type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void edp_del_inside_vhost(t_edp *cur, char *name, char *lan)
{
  char *ifname, *peer_name;
  int peer_num;
  if (cur->endp_type == endp_type_phy)
    {
    if (cur->flag_lan_ok)
      {
      if (edp_vhost_del_port(name, lan))
        KERR("%s %s", name, lan);
      }
    }
  else if (cur->endp_type == endp_type_tap)
    {
    if (cur->flag_lan_ok)
      {
      peer_name = cur->vhost.peer_name;
      peer_num = cur->vhost.peer_num;
      ifname = edp_vhost_get_ifname(peer_name, peer_num);
      suid_power_ifname_change(peer_name, peer_num, name, ifname);
      }
    }
  else if (cur->endp_type == endp_type_pci)
    {
    KERR("%s %s", name, lan);
    }
  else
    KOUT("%d", cur->endp_type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void edp_del_inside_sock(t_edp *cur, char *name, char *lan)
{
  int tidx, type;
  if (endp_mngt_exists(name, 0, &type))
    {
    if (endp_evt_lan_find(name, 0, lan, &tidx))
      {
      if (endp_evt_del_lan(name, 0, tidx, lan))
        KERR("%s %s", name, lan);
      }
    if (type == endp_type_snf) 
      snf_globtopo_small_event(name, snf_evt_capture_off, NULL);
    endp_mngt_stop(name, 0);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int begin_dpdk_snf(t_edp *cur)
{
  char info[MAX_PATH_LEN];
  int llid = cur->llid, tid = cur->tid;
  char *name = cur->name;
  char *lan = cur->lan.lan[0].lan;
  int result = eth_type_none;
  if (dpdk_snf_add(llid, tid, name))
    {
    snprintf(info,MAX_PATH_LEN-1,"Error add dpdk snf %s %s", name, lan);
    KERR("%s", info);
    send_status_ko(llid, tid, info);
    }
  else
    {
    if (dpdk_snf_add_lan(llid, tid, lan, name))
      {
      snprintf(info,MAX_PATH_LEN-1,"Error add lan dpdk snf %s %s",name,lan);
      KERR("%s", info);
      send_status_ko(llid, tid, info);
      }
    else
      result = eth_type_dpdk;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int begin_dpdk_nat(t_edp *cur)
{ 
  char info[MAX_PATH_LEN];
  int llid = cur->llid, tid = cur->tid;
  char *name = cur->name;
  char *lan = cur->lan.lan[0].lan;
  int result = eth_type_none;
  if (dpdk_nat_add(llid, tid, name))
    {
    snprintf(info,MAX_PATH_LEN-1,"Error add dpdk nat %s %s", name, lan);
    KERR("%s", info);
    send_status_ko(llid, tid, info);
    }
  else
    {
    if (dpdk_nat_add_lan(llid, tid, lan, name))
      {
      snprintf(info,MAX_PATH_LEN-1,"Error add lan dpdk nat %s %s",name,lan);
      KERR("%s", info);
      send_status_ko(llid, tid, info);
      }
    else
      result = eth_type_dpdk;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/**************************************************************************/
static int begin_eth_type_dpdk(t_edp *cur)
{
  char info[MAX_PATH_LEN];
  int llid = cur->llid, tid = cur->tid;
  char *name = cur->name;
  char *lan = cur->lan.lan[0].lan;
  int result = eth_type_none;
  if (cur->endp_type == endp_type_phy)
    {
    snprintf(info,MAX_PATH_LEN-1, "DPDK NEEDS PCI, not PHY");
    KERR("%s", info);
    send_status_ko(llid, tid, info);
    }
  else if (cur->endp_type == endp_type_tap)
    {
    if (dpdk_tap_add(llid, tid, name))
      {
      snprintf(info,MAX_PATH_LEN-1,"Error add dpdk tap %s %s", name, lan);
      KERR("%s", info);
      send_status_ko(llid, tid, info);
      }
    else
      {
      if (dpdk_tap_add_lan(llid, tid, lan, name))
        {
        snprintf(info,MAX_PATH_LEN-1,"Error add lan dpdk tap %s %s",name,lan);
        KERR("%s", info);
        send_status_ko(llid, tid, info);
        }
      else
        result = eth_type_dpdk;
      }
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
    if (pci_dpdk_add(llid, tid, lan, name))
      {
      snprintf(info,MAX_PATH_LEN-1,"Openvswitch unreacheable %s %s",name,lan);
      KERR("%s", info);
      send_status_ko(llid, tid, info);
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
static int begin_eth_type_vhost(t_edp *cur)
{
  char vhost_ifname[IFNAMSIZ];
  char vm_name[MAX_NAME_LEN];
  char info[MAX_PATH_LEN];
  int vhost, num, llid = cur->llid, tid = cur->tid;
  char *name = cur->name;
  char *lan = cur->lan.lan[0].lan;
  int result = eth_type_none;
  if (cur->endp_type == endp_type_phy)
    {
    if (edp_vhost_add_port(llid, tid, name, lan))
      {
      snprintf(info,MAX_PATH_LEN-1,
               "Openvswitch not reacheable %s %s",name,lan);
      KERR("%s", info);
      send_status_ko(llid, tid, info);
      }
    else
      result = eth_type_vhost;
    }
  else if (cur->endp_type == endp_type_tap)
    {
    vhost = vhost_lan_info(lan, vhost_ifname, vm_name, &num);
    if (vhost == 0)
      KERR("%s %s", vm_name, lan);
    else
      {
      if (vhost > 1)
        {
        edp_mngt_lowest_clean_lan(cur);
        snprintf(info,MAX_PATH_LEN-1, "Tap must be the first to be attached");
        KERR("%s", info);
        send_status_ko(llid, tid, info);
        }
      else
        {
        memset(cur->vhost.peer_name, 0, MAX_NAME_LEN);
        strncpy(cur->vhost.peer_name, vm_name, MAX_NAME_LEN-1);
        cur->vhost.peer_num = num;
        suid_power_ifname_change(vm_name, num, vhost_ifname, name);
        result = eth_type_vhost;
        }
      }
    }
  else if ((cur->endp_type == endp_type_nat) ||
           (cur->endp_type == endp_type_snf) ||
           (cur->endp_type == endp_type_pci))
    {
    snprintf(info,MAX_PATH_LEN-1, "VHOST not compatible with this type %s", lan);
    KERR("%s", info);
    send_status_ko(llid, tid, info);
    }
  else
    KOUT("%d", cur->endp_type);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int begin_eth_type_sock(t_edp *cur)
{
  char info[MAX_PATH_LEN];
  int endp_type, llid = cur->llid, tid = cur->tid;
  char *name = cur->name;
  char *lan = cur->lan.lan[0].lan;
  int result = eth_type_none;

  if (endp_mngt_exists(name, 0, &endp_type))
    {
    snprintf( info, MAX_PATH_LEN-1, "Phy sock exists %s", name);
    KERR("%s", info);
    send_status_ko(llid, tid, info);
    }
  else if (endp_mngt_start(0, 0, name, 0, cur->endp_type))
    {
    snprintf( info, MAX_PATH_LEN-1, "Phy sock bad start of %s", name);
    KERR("%s", info);
    send_status_ko(llid, tid, info);
    }
  else
    {
    edp_sock_timer_add(llid, tid, name, lan);
    result = eth_type_sock;
    }
  return result;
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
      if (cur->llid)
        {
        send_status_ok(cur->llid, cur->tid, "OK");
        cur->llid = 0;
        cur->tid = 0;
        }
      }
    else
      cur->flag_lan_ok = 0;
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
  if ((flag == 0) && (cur->count_flag_ko > 2)) 
    {
    if ((cur->lan.nb_lan) && (cur->eth_type != eth_type_none))
      {
      KERR("CLEANING LAN %s %s", cur->lan.lan[0].lan, cur->name);
      edp_mngt_lowest_clean_lan(cur);
      edp_mngt_set_eth_type(cur, eth_type_none);
      }
    else
      KERR("CLEANING ERROR %s", cur->name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int timer_update_type_dpdk(t_edp *cur, char *lan)
{
  int flag = 0;
  cur->count_flag_ko += 1;
  if (cur->endp_type == endp_type_tap)
    {
    if (dpdk_tap_exist(cur->name))
      {
      if (dpdk_tap_lan_exists(lan))
        flag = 1;
      }
    }
  else if (cur->endp_type == endp_type_snf)
    {
    if (dpdk_snf_exist(cur->name))
      {
      if (dpdk_snf_lan_exists(lan))
        flag = 1;
      }
    }
  else if (cur->endp_type == endp_type_nat)
    {
    if (dpdk_nat_exist(cur->name))
      {
      if (dpdk_nat_lan_exists(lan))
        flag = 1;
      }
    }
  else if (cur->endp_type == endp_type_pci)
    {
    if (dpdk_dyn_lan_exists(lan))
      {
      if (cur->dpdk_attach_ok)
        flag = 1;
      }
    }
  else if (cur->endp_type == endp_type_phy)
    KERR("ERROR phy not compatible dpdk %s %s", cur->name, lan);
  else
    KOUT("%d", cur->endp_type);
  cur->count_flag_ko -= flag;
  return flag;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int timer_update_type_vhost(t_edp *cur, char *lan)
{
  int flag = 0;
  if (cur->endp_type == endp_type_tap)
    {
    if (vhost_lan_exists(lan))
      flag = 1;
    }
  else if (cur->endp_type == endp_type_snf)
    KERR("ERROR snf not compatible vhost %s %s", cur->name, lan);
  else if (cur->endp_type == endp_type_nat)
    KERR("ERROR nat not compatible vhost %s %s", cur->name, lan);
  else if (cur->endp_type == endp_type_pci)
    KERR("ERROR pci not compatible vhost %s %s", cur->name, lan);
  else if (cur->endp_type == endp_type_phy)
    {
    if (vhost_lan_exists(lan))
      flag = 1;
    }
  else
    KOUT("%d", cur->endp_type);
  return flag;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int timer_update_type_sock(t_edp *cur, char *lan)
{
  int tidx, type, flag = 0;
  if (endp_mngt_exists(cur->name, 0, &type))
    {
    if (endp_evt_lan_find(cur->name, 0, lan, &tidx))
    flag = 1;
    }
  return flag;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_update_flag_lan(void *data)
{
  int flag, nb_edp;
  t_edp *cur = edp_mngt_get_head_edp(&nb_edp);
  char *lan;
  while(cur)
    {
    if ((cur->lan.nb_lan) && (cur->eth_type != eth_type_none))
      {
      flag = 0;
      lan = cur->lan.lan[0].lan;
      if (cur->eth_type == eth_type_sock)
        {
        flag = timer_update_type_sock(cur, lan);
        }
      else if (cur->eth_type == eth_type_vhost)
        {
        flag = timer_update_type_vhost(cur, lan);
        }
      else if (cur->eth_type == eth_type_dpdk)
        {
        flag = timer_update_type_dpdk(cur, lan);
        }
      else
        KOUT("%d", cur->eth_type);
      set_flag_lan(cur, flag);
      }
    cur = cur->next;
    }

  clownix_timeout_add(50, timer_update_flag_lan, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_evt_update_lan_add(t_edp *cur, int llid, int tid)
{
  int eth_type;
  char *lan = NULL;
  if (cur->lan.nb_lan)
    {
    lan = cur->lan.lan[0].lan;
    eth_type = eth_type_update(cur->name, lan);
    cur->llid = llid;
    cur->tid = tid;
    if (eth_type == eth_type_dpdk)
      eth_type = begin_eth_type_dpdk(cur);
    else if (eth_type == eth_type_vhost)
      eth_type = begin_eth_type_vhost(cur);
    else if (eth_type == eth_type_sock)
      eth_type = begin_eth_type_sock(cur);
    else if (eth_type != eth_type_none)
      KOUT("%d", eth_type);
    edp_mngt_set_eth_type(cur, eth_type);
    }
  return eth_type;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_del_inside(t_edp *cur)
{
  char *lan, *name = cur->name;
  if (cur->lan.nb_lan != 0)
    {
    lan = cur->lan.lan[0].lan;
    if (cur->eth_type == eth_type_dpdk)
      edp_del_inside_dpdk(cur, name, lan);
    else if (cur->eth_type == eth_type_vhost)
      edp_del_inside_vhost(cur, name, lan);
    else if (cur->eth_type == eth_type_sock)
      edp_del_inside_sock(cur, name, lan);
    else if (cur->eth_type != eth_type_none)
      KOUT("%d", cur->eth_type);
    }
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
        (cur->lan.nb_lan))
      {
      KERR("%s", cur->lan.lan[0].lan);
      edp_evt_del_inside(cur);
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
        (cur->lan.nb_lan))
      {
      KERR("%s", cur->lan.lan[0].lan);
      edp_evt_del_inside(cur);
      edp_mngt_edp_free(cur);
      }
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_evt_update_eth_type(int llid, int tid, int is_add,
                             int eth_type, char *name, char *lan)
{
  int nb_edp, cur_eth_type, result = -1;
  t_edp *cur = edp_mngt_get_head_edp(&nb_edp);
  char *cur_lan, *cur_name;
  while(cur)
    {
    if (cur->lan.nb_lan)
      {
      cur_name = cur->name;
      cur_lan = cur->lan.lan[0].lan;
      if ((!strcmp(lan, cur_lan)) && (strcmp(name, cur_name)))
        {
        if ((cur->eth_type == eth_type_none) && (is_add))
          {
          cur_eth_type = edp_evt_update_lan_add(cur, llid, tid);
          if (cur_eth_type != eth_type)
            KERR("%d %d", cur_eth_type, eth_type);
          if (cur_eth_type != eth_type_none)
            result = 0;
          }
        else if ((cur->eth_type != eth_type_none) &&
                 (!is_add) &&
                 (cur->eth_type == eth_type))
          {
          eth_type = eth_type_update(cur_name, cur_lan);
          if (eth_type == eth_type_none)
            {
            edp_evt_del_inside(cur);
            edp_mngt_set_eth_type(cur, eth_type_none);
            }
          }
        }
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_lan_add_done(int eth_type, char *lan)
{
  t_edp *cur = edp_mngt_lan_find(lan);
  if (cur)
    {
    if (cur->eth_type != eth_type)
      KERR("WRONG TYPE LAN: %s %d %d", lan, cur->eth_type, eth_type);
    else
      cur->flag_lan_ok = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_evt_lan_del_done(int eth_type, char *lan)
{
  t_edp *cur = edp_mngt_lan_find(lan);
  if (cur)
    cur->flag_lan_ok = 0;
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
      edp_mngt_lowest_clean_lan(cur);
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
