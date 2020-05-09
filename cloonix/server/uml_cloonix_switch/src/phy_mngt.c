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
#include "phy_mngt.h"
#include "phy_evt.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "endp_mngt.h"
#include "phy_sock.h"
#include "phy_vhost.h"
#include "pci_dpdk.h"
#include "utils_cmd_line_maker.h"
#include "dpdk_dyn.h"
#include "vhost_eth.h"


static t_phy *g_head_phy;
static int g_nb_phy;


/****************************************************************************/
static int lan_exists_somewhere(char *name, char *lan)
{
  int type, result, dpdk, vhost, sock;
  dpdk = dpdk_dyn_lan_exists(lan);
  vhost = vhost_lan_exists(lan);
  sock = endp_evt_lan_is_in_use_by_other(name, lan, &type);
  result = dpdk + vhost + sock;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_phy *phy_alloc(char *name, int endp_type)
{
  t_phy *cur;
  cur = (t_phy *) clownix_malloc(sizeof(t_phy), 13);
  memset(cur, 0, sizeof(t_phy));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->endp_type = endp_type;
  if (g_head_phy)
    g_head_phy->prev = cur;
  cur->next = g_head_phy;
  g_head_phy = cur;
  g_nb_phy += 1;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_phy *phy_mngt_phy_find(char *name)
{
  t_phy *cur = g_head_phy;
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
void phy_mngt_phy_free(t_phy *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_head_phy == cur)
    g_head_phy = cur->next;
  if (g_nb_phy < 1)
    KOUT(" ");
  g_nb_phy -= 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void phy_mngt_lowest_clean_lan(t_phy *cur)
{
  cur->lan.nb_lan = 0;
  clownix_free(cur->lan.lan[0].lan, __FUNCTION__);
  memset(&(cur->lan), 0, sizeof(t_lan_group));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_phy *phy_mngt_lan_find(char *lan)
{
  t_phy *cur = g_head_phy;
  while(cur)
    {
    if (cur->lan.nb_lan)
      {
      if (!strcmp(lan, cur->lan.lan[0].lan))
        {
        break;
        }
      }
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *phy_mngt_translate_topo_endp(int *nb)
{
  t_phy *cur = g_head_phy;
  int len, nb_lan, nb_endp = 0;
  t_topo_endp *endp;
  char *lan, *name;
  endp = (t_topo_endp *) clownix_malloc(g_nb_phy * sizeof(t_topo_endp),13);
  memset(endp, 0, g_nb_phy * sizeof(t_topo_endp));
  while(cur)
    {
    name = cur->name;
    nb_lan = cur->lan.nb_lan;
    if (nb_lan == 1)
      lan = cur->lan.lan[0].lan;
    else if (nb_lan == 0)
      lan = NULL;
    else
      KOUT("%d", nb_lan);
    len = nb_lan * sizeof(t_lan_group_item);
    endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
    memset(endp[nb_endp].lan.lan, 0, len);
    strncpy(endp[nb_endp].name, name, MAX_NAME_LEN-1);
    endp[nb_endp].num = 0;
    endp[nb_endp].type = cur->endp_type;
    if (lan)
      {
      if ((cur->flag_lan_ok) ||
          (!lan_exists_somewhere(name, lan) &&
          (cur->eth_type == eth_type_none)))
        {
        strncpy(endp[nb_endp].lan.lan[0].lan, lan, MAX_NAME_LEN-1);
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
void phy_mngt_set_eth_type(t_phy *cur, int eth_type)
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
int phy_mngt_exists(char *name, int *endp_type)
{
  int result = 0;
  t_phy *cur = phy_mngt_phy_find(name);
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
int phy_mngt_get_qty(void)
{
  return (g_nb_phy);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int phy_mngt_lan_exists(char *lan, int *eth_type, int *endp_type)
{
  int result = 0;
  t_phy *cur = g_head_phy;
  while(cur)
    {
    if (cur->lan.nb_lan)
      {
      if (!strcmp(lan, cur->lan.lan[0].lan))
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
int phy_mngt_add_lan(int llid, int tid, char *name, char *lan, char *err)
{
  t_phy *cur = phy_mngt_lan_find(lan);
  int eth_type, type, result = -1, len;
  if (cur)
    {
    snprintf(err, MAX_PATH_LEN-1,
             "Lan already exists %s %s in %s", name, lan, cur->name);
    KERR("%s", err);
    }
  else
    {
    cur = phy_mngt_phy_find(name); 
    if (cur == NULL)
      {
      snprintf(err, MAX_PATH_LEN, "Not found: %s", name);
      KERR("%s", err);
      }
    else if ((cur->lan.nb_lan == 1) &&
             (cur->eth_type != eth_type_none))
      {
      snprintf(err, MAX_PATH_LEN, "Phy %s is attached to %s",
                                  name, cur->lan.lan[0].lan);
      KERR("%s", err);
      }
    else if (endp_evt_lan_is_in_use(lan, &type))
      {
      if ((type != endp_type_kvm_sock)  &&
          (type != endp_type_kvm_dpdk)  &&
          (type != endp_type_kvm_vhost) &&
          (type != endp_type_tap)       &&
          (type != endp_type_snf)       &&
          (type != endp_type_c2c))
        {
        snprintf(err, MAX_PATH_LEN, 
                "Lan %s is attached to incompatible type", lan);
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
      else
        result = 0;
      }
    else
      result = 0;
    if (result == 0)
      {
      len = sizeof(t_lan_group_item);
      cur->lan.nb_lan = 1;
      cur->lan.lan=(t_lan_group_item *)clownix_malloc(len, 2);
      strncpy(cur->lan.lan[0].lan, lan, MAX_NAME_LEN-1);
      eth_type = phy_evt_update_lan_add(cur, llid, tid);
      if (eth_type == eth_type_none)
        {
        send_status_ok(llid, tid, "OK");
        cur->llid = 0;
        cur->tid = 0;
        }
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int phy_mngt_del_lan(int llid, int tid, char *name, char *lan, char *err)
{
  t_phy *cur = phy_mngt_phy_find(name); 
  int result = -1;
  if (cur == NULL)
    {
    KERR("%s %s", name, lan);
    snprintf(err, MAX_PATH_LEN, "Not found: %s", name);
    }
  else
    {
    if (cur->lan.nb_lan == 0)
      {
      KERR("%s %s", name, lan);
      snprintf(err, MAX_PATH_LEN, "Phy %s has no lan", name);
      }
    else if (strcmp(cur->lan.lan[0].lan, lan))
      {
      KERR("%s %s", name, lan);
      snprintf(err, MAX_PATH_LEN, "Lan %s is not attached, %s is.",
                                  lan, cur->lan.lan[0].lan);
      }
    else
      {
      phy_evt_del_inside(cur);
      phy_mngt_lowest_clean_lan(cur);
      phy_mngt_set_eth_type(cur, eth_type_none); 
      event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int phy_mngt_add(int llid, int tid, char *name, int endp_type)
{
  t_phy *cur = phy_mngt_phy_find(name); 
  int result;
  if ((endp_type != endp_type_phy) &&
      (endp_type != endp_type_pci) &&
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
    KERR("%s phy not found", name);
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
    cur = phy_alloc(name, endp_type); 
    phy_mngt_set_eth_type(cur, eth_type_none); 
    layout_add_sat(name, llid);
    send_status_ok(llid, tid, "OK");
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int phy_mngt_del(int llid, int tid, char *name)
{
  t_phy *cur = phy_mngt_phy_find(name); 
  int result = -1;
  if (cur == NULL)
    {
    KERR("%s", name);
    result = -1;
    }
  else
    {
    phy_evt_del_inside(cur);
    phy_mngt_phy_free(cur);
    layout_del_sat(name);
    send_status_ok(llid, tid, "OK");
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    result = 0;
    }
 return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void phy_mngt_del_all(void)
{
  t_phy *next, *cur = g_head_phy;
  while(cur)
    {
    next = cur->next;
    phy_mngt_del(0, 0, cur->name);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_phy *phy_mngt_get_head_phy(int *nb_phy)
{
  *nb_phy = g_nb_phy;
  return g_head_phy;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void phy_mngt_init(void)
{
  g_nb_phy   = 0;
  g_head_phy = NULL;
  phy_sock_init();
  phy_vhost_init();
  phy_evt_init();
  pci_dpdk_init();
}
/*--------------------------------------------------------------------------*/
