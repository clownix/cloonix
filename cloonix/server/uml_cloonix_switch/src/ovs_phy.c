/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include "event_subscriber.h"
#include "utils_cmd_line_maker.h"
#include "uml_clownix_switch.h"
#include "ovs_phy.h"
#include "msg.h"
#include "lan_to_name.h"
#include "ovs_snf.h"
#include "ovs_a2b.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "fmt_diag.h"
#include "mactopo.h"


static t_ovs_phy *g_head_phy;
static int g_nb_phy;

int get_glob_req_self_destruction(void);


/****************************************************************************/
static t_ovs_phy *find_phy(char *name, int num)
{
  t_ovs_phy *cur = g_head_phy;
  while(cur)
    {
    if ((!strcmp(cur->name, name)) && (cur->num_macvlan == num))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovs_phy *find_phy_vhost(char *name)
{
  t_ovs_phy *cur = g_head_phy;
  while(cur)
    {
    if (!strcmp(cur->vhost, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int find_num_macvlan(char *name, int type)
{
  int result = 0, num_macvlan = 0;
  t_ovs_phy *cur;
  if ((type == endp_type_phyms) || (type == endp_type_phymv))
    {
    while(result == 0)
      {
      num_macvlan += 1;
      cur = find_phy(name, num_macvlan);
      if (cur == NULL)
        result = num_macvlan;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_phy(t_ovs_phy *cur)
{
  int item_phy_type;
  cfg_free_obj_id(cur->phy_id);
  if (strlen(cur->lan_added))
    {
    KERR("ERROR %s %s", cur->vhost, cur->lan_added);
    if ((cur->endp_type == endp_type_phyas) ||
        (cur->endp_type == endp_type_phyav))
      item_phy_type = item_phya;
    else if ((cur->endp_type == endp_type_phyms) ||
             (cur->endp_type == endp_type_phymv))
      item_phy_type = item_phym;
    else
      KOUT("ERROR %s %s %d", cur->vhost, cur->lan_added, cur->endp_type);
    mactopo_del_req(item_phy_type, cur->name,
                    cur->num_macvlan, cur->lan_added);
    lan_del_name(cur->lan_added, item_phy_type, cur->vhost, 0);
    }
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_phy)
    g_head_phy = cur->next;
  g_nb_phy -= 1;
  free(cur);
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_phy(int llid, int tid, char *name, int type, int num_macvlan)
{
  int id = cfg_alloc_obj_id();
  t_ovs_phy *cur = (t_ovs_phy *) malloc(sizeof(t_ovs_phy));
  memset(cur, 0, sizeof(t_ovs_phy));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->phy_id = id;
  cur->llid = llid;
  cur->tid = tid;
  cur->num_macvlan = num_macvlan;
  cur->endp_type = type;
  cur->mac_addr[0] = 0x2;
  cur->mac_addr[1] = 0xFF & rand();
  cur->mac_addr[2] = 0xFF & rand();
  cur->mac_addr[3] = 0xFF & rand();
  cur->mac_addr[4] = cur->phy_id % 100;
  cur->mac_addr[5] = 0xFF & rand();
  snprintf(cur->vhost, (MAX_NAME_LEN-1), "%s_%d", name, num_macvlan);
  if (g_head_phy)
    g_head_phy->prev = cur;
  cur->next = g_head_phy;
  g_head_phy = cur; 
  g_nb_phy += 1;
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void snf_process_started(char *name, int num, char *vhost)
{
  t_ovs_phy *cur = find_phy_vhost(name);

  if (cur == NULL)
    KERR("ERROR %s %d %s", name, num, vhost);
  else
    {
    if (strlen(cur->lan))
      ovs_snf_send_add_snf_lan(name, num, cur->vhost, cur->lan);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_phy_resp_add_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan)
{
  t_ovs_phy *vcur, *cur = find_phy_vhost(vhost);
  int item_phy_type;
  if (!cur)
    {
    mactopo_add_resp(0, name, num, lan);
    KERR("ERROR %s %d %s %s", name, num, vhost, lan);
    }
  else
    {
    vcur = find_phy(name, num);
    if (!vcur)
      {
      mactopo_add_resp(0, name, num, lan);
      KERR("ERROR %d %s %d %s", is_ko, name, num, vhost);
      }
    else if (vcur != cur)
      {
      mactopo_add_resp(0, name, num, lan);
      KERR("ERROR %d %s %d %s", is_ko, name, num, vhost);
      }
    else
      {
      if ((cur->endp_type == endp_type_phyas) ||
          (cur->endp_type == endp_type_phyav))
        item_phy_type = item_phya;
      else if ((cur->endp_type == endp_type_phyms) ||
               (cur->endp_type == endp_type_phymv))
        item_phy_type = item_phym;
      else
        KOUT("ERROR %s %s %d", name, lan, cur->endp_type);
      if (strlen(cur->lan))
        KERR("ERROR %s %d %s %s", name, num, vhost, cur->lan);
      memset(cur->lan, 0, MAX_NAME_LEN);
      if (is_ko)
        {
        mactopo_add_resp(0, name, num, lan);
        KERR("ERROR %d %s %d %s %s", is_ko, name, num, vhost, lan);
        utils_send_status_ko(&(cur->llid), &(cur->tid), name);
        }
      else
        {
        mactopo_add_resp(item_phy_type, name, num, lan);
        if (cur->endp_type == endp_type_phyas)
          ovs_dyn_snf_start_process(vhost, 0, item_type_phy, vhost,
                                    snf_process_started);
        cfg_hysteresis_send_topo_info();
        strncpy(cur->lan, lan, MAX_NAME_LEN);
        utils_send_status_ok(&(cur->llid), &(cur->tid));
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_phy_resp_del_lan(int is_ko, char *name, int num, 
                          char *vhost, char *lan)
{
  t_ovs_phy *vcur, *cur = find_phy_vhost(vhost);
  int item_phy_type;
  if (!cur)
    {
    mactopo_del_resp(0, name, num, lan);
    KERR("ERROR %s %d %s %s", name, num, vhost, lan);
    }
  else
    {
    vcur = find_phy(name, num);
    if (!vcur)
      {
      mactopo_del_resp(0, name, num, lan);
      KERR("ERROR %d %s %d %s", is_ko, name, num, vhost);
      }
    else if (vcur != cur)
      {
      mactopo_del_resp(0, name, num, lan);
      KERR("ERROR %d %s %d %s", is_ko, name, num, vhost);
      }
    else 
      {
      if ((cur->endp_type == endp_type_phyas) ||
          (cur->endp_type == endp_type_phyav))
        item_phy_type = item_phya;
      else if ((cur->endp_type == endp_type_phyms) ||
               (cur->endp_type == endp_type_phymv))
        item_phy_type = item_phym;
      else
        KOUT("ERROR %s %s %d", name, lan, cur->endp_type);
      mactopo_del_resp(item_phy_type, name, num, lan);
      if (cur->del_phy_req == 1)
        {
        memset(cur->lan, 0, MAX_NAME_LEN);
        if (msg_send_del_phy(cur->name, cur->vhost, cur->num_macvlan))
          KERR("ERROR %d %s %d %s %s", is_ko, name, num, vhost, lan);
        }
      else
        {
        memset(cur->lan, 0, MAX_NAME_LEN);
        if (is_ko)
          {
          KERR("ERROR %d %s %d %s %s", is_ko, cur->name, num, vhost, lan);
          utils_send_status_ko(&(cur->llid), &(cur->tid), name);
          }
        else
          {
          cfg_hysteresis_send_topo_info();
          utils_send_status_ok(&(cur->llid), &(cur->tid));
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_phy_resp_msg_phy(int is_ko, int is_add,
                          char *name, int num, char *vhost)
{
  t_ovs_phy *vcur, *cur = find_phy_vhost(vhost);
  if (!cur)
    KERR("ERROR %s %d %s", name, num, vhost);
  else
    {
    vcur = find_phy(name, num);
    if (!vcur)
      KERR("ERROR %d %s %d %s", is_ko, name, num, vhost);
    else if (vcur != cur)
      KERR("ERROR %d %s %d %s", is_ko, name, num, vhost);
    else
      {
      if (is_ko)
        {
        KERR("ERROR %d %d %s %d %s", is_ko, is_add, name, num, vhost);
        utils_send_status_ko(&(cur->llid), &(cur->tid), name);
        if (is_add != 0)
          free_phy(cur);
        }
      else
        {
        if (is_add)
          layout_add_sat(vhost, cur->llid);
        else
          layout_del_sat(vhost);
        utils_send_status_ok(&(cur->llid), &(cur->tid));
        } 
      if (is_add == 0)
        free_phy(cur);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_ovs_phy *ovs_phy_get_first(int *nb_phy)
{
  t_ovs_phy *result = g_head_phy;
  *nb_phy = g_nb_phy;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_phy_add(int llid, int tid, char *name, int type)
{
  t_ovs_phy *cur = find_phy(name, 0);
  int num_macvlan = 0;
  char mac[MAX_NAME_LEN];
  unsigned char *mc;

  if ((type != endp_type_phyas) && (type != endp_type_phyav) &&
      (type != endp_type_phyms) && (type != endp_type_phymv))
    send_status_ko(llid, tid, "Bad type");
  else if ((cur) && ((type == endp_type_phyas) || (type == endp_type_phyav)))
    send_status_ko(llid, tid, "Exists already");
  else if (cur)
    {
    send_status_ko(llid, tid, "Bad type");
    KERR("ERROR %s %d", name, type);
    }
  else
    {
    num_macvlan = find_num_macvlan(name, type);
    if (num_macvlan == -1)
      KERR("ERROR %s %d", name, type);
    else
      {
      alloc_phy(llid, tid, name, type, num_macvlan);
      cur = find_phy(name, num_macvlan);
      if (!cur)
        KOUT(" ");
      mc = cur->mac_addr;
      memset(mac, 0, MAX_NAME_LEN);
      snprintf(mac, (MAX_NAME_LEN-1), 
               "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
               mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
      if (msg_send_add_phy(name, cur->vhost, mac, cur->num_macvlan))
        {
        KERR("ERROR %s", name);
        utils_send_status_ko(&(cur->llid), &(cur->tid), name);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_phy_del(int llid, int tid, char *name)
{
  t_ovs_phy *cur = ovs_phy_exists_vhost(name);
  int val;
  int item_phy_type;

  if (!cur)
    {
    KERR("ERROR %s", name);
    if (llid)
      send_status_ko(llid, tid, "Does not exist");
    }
  else if (cur->llid)
    {
    KERR("ERROR %s %d %d", name, cur->llid, cur->del_phy_req);
    if (llid)
      send_status_ko(llid, tid, "Not ready");
    }
  else if (cur->del_phy_req)
    {
    if (llid)
      send_status_ko(llid, tid, "Not ready");
    }
  else
    {
    cur->llid = llid;
    cur->tid = tid;
    if (strlen(cur->lan_added))
      {
      if (!strlen(cur->lan))
        KERR("ERROR %s %s", name, cur->lan_added);
      if ((cur->endp_type == endp_type_phyms) ||
          (cur->endp_type == endp_type_phymv))
        item_phy_type = item_phym;
      else if ((cur->endp_type == endp_type_phyas) ||
               (cur->endp_type == endp_type_phyav))
        item_phy_type = item_phya;
      else
        KOUT("ERROR %s %d", name, cur->endp_type);
      mactopo_del_req(item_phy_type, cur->name,
                      cur->num_macvlan, cur->lan_added);
      val = lan_del_name(cur->lan_added, item_phy_type, cur->vhost, 0);
      memset(cur->lan_added, 0, MAX_NAME_LEN);
      if (val == 2*MAX_LAN)
        {
        if (msg_send_del_phy(cur->name, cur->vhost, cur->num_macvlan))
          {
          KERR("ERROR %s", name);
          utils_send_status_ko(&(cur->llid), &(cur->tid), name);
          }
        }
      else
        {
        msg_send_del_lan_endp(ovsreq_del_phy_lan, cur->name,
                              cur->num_macvlan, cur->vhost, cur->lan);
        cur->del_phy_req = 1;
        }
      }
    else if (msg_send_del_phy(cur->name, cur->vhost, cur->num_macvlan))
      {
      KERR("ERROR %s", name);
      utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_phy_add_lan(int llid, int tid, char *name, char *lan)
{
  t_ovs_phy *cur = ovs_phy_exists_vhost(name);
  int item_phy_type;
  char err[MAX_PATH_LEN];
  if (!cur)
    {
    KERR("ERROR %s", name);
    send_status_ko(llid, tid, "Does not exist");
    }
  else if ((cur->llid) || (cur->del_phy_req))
    {
    KERR("ERROR %s %d %d", name, cur->llid, cur->del_phy_req);
    send_status_ko(llid, tid, "Not ready");
    }
  else if (strlen(cur->lan))
    {
    KERR("ERROR %s %s", name, cur->lan);
    send_status_ko(llid, tid, "Lan exists");
    }
  else
    {
    if ((cur->endp_type == endp_type_phyms) ||
        (cur->endp_type == endp_type_phymv))
      item_phy_type = item_phym;
    else if ((cur->endp_type == endp_type_phyas) ||
             (cur->endp_type == endp_type_phyav))
      item_phy_type = item_phya;
    else
      KOUT("ERROR %s %s", name, lan);
    lan_add_name(lan, item_phy_type, cur->vhost, 0);
    if (mactopo_add_req(item_phy_type, cur->name,  cur->num_macvlan,
                        lan, cur->vhost, cur->mac_addr, err))
      {
      KERR("ERROR %s %d %s %s", name, 0, lan, err);
      lan_del_name(lan, item_phy_type, name, 0);
      }
    else if (msg_send_add_lan_endp(ovsreq_add_phy_lan, cur->name,
                                   cur->num_macvlan, cur->vhost, lan))
      {
      KERR("ERROR %s %s", name, lan);
      send_status_ko(llid, tid, "msg_send_add_lan_endp error");
      lan_del_name(lan, item_phy_type, cur->vhost, 0);
      }
    else
      {
      strncpy(cur->lan_added, lan, MAX_NAME_LEN);
      cur->llid = llid;
      cur->tid = tid;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_phy_del_lan(int llid, int tid, char *name, char *lan)
{
  t_ovs_phy *cur = ovs_phy_exists_vhost(name);
  int item_phy_type, val = 0;
  if (!cur)
    {
    KERR("ERROR %s", name);
    send_status_ko(llid, tid, "Does not exist");
    }
  else if ((cur->llid) || (cur->del_phy_req))
    {
    KERR("ERROR %s %d %d", name, cur->llid, cur->del_phy_req);
    send_status_ko(llid, tid, "Not ready");
    }
  else
    {
    if (!strlen(cur->lan_added))
      KERR("ERROR: %s %s", name, lan);
    else
      {
      if ((cur->endp_type == endp_type_phyas) ||
          (cur->endp_type == endp_type_phyav))
        item_phy_type = item_phya;
      else if ((cur->endp_type == endp_type_phyms) ||
               (cur->endp_type == endp_type_phymv))
        item_phy_type = item_phym;
      else
        KOUT("ERROR %s %s %d", cur->vhost, cur->lan_added, cur->endp_type);
      mactopo_del_req(item_phy_type, cur->name,
                      cur->num_macvlan, cur->lan_added);
      val = lan_del_name(cur->lan_added, item_phy_type, cur->vhost, 0);
      memset(cur->lan_added, 0, MAX_NAME_LEN);
      }
    if (val == 2*MAX_LAN)
      {
      memset(cur->lan, 0, MAX_NAME_LEN);
      cfg_hysteresis_send_topo_info();
      send_status_ok(llid, tid, "OK");
      }
    else
      {
      cur->llid = llid;
      cur->tid = tid;
      if (msg_send_del_lan_endp(ovsreq_del_phy_lan, cur->name,
                                cur->num_macvlan, cur->vhost, lan))
        utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *ovs_phy_translate_topo_endp(int *nb)
{
  t_ovs_phy *cur;
  int len, count = 0, nb_endp = 0;
  t_topo_endp *endp;
  cur = g_head_phy;
  while(cur)
    {
    if (strlen(cur->lan))
      count += 1;
    cur = cur->next;
    }
  endp = (t_topo_endp *) clownix_malloc(count * sizeof(t_topo_endp), 13);
  memset(endp, 0, count * sizeof(t_topo_endp));
  cur = g_head_phy;
  while(cur)
    {
    if (strlen(cur->lan))
      {
      len = sizeof(t_lan_group_item);
      endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
      memset(endp[nb_endp].lan.lan, 0, len);
      strncpy(endp[nb_endp].name, cur->vhost, MAX_NAME_LEN-1);
      endp[nb_endp].num = 0;
      endp[nb_endp].type = cur->endp_type;
      strncpy(endp[nb_endp].lan.lan[0].lan, cur->lan, MAX_NAME_LEN-1);
      endp[nb_endp].lan.nb_lan = 1;
      nb_endp += 1;
      }
    cur = cur->next;
    }
  *nb = nb_endp;
  return endp;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  ovs_phy_dyn_snf(char *name, int val)
{
  int result = -1;
  t_ovs_phy *cur = ovs_phy_exists_vhost(name);
  char phy[2*MAX_NAME_LEN];
  char *vh;

  if (val)
    {
    if (cur->endp_type == endp_type_phyas)
      KERR("ERROR %s %d", name, val);
    else
      {
      cur->del_snf_ethv_sent = 0;
      cur->endp_type = endp_type_phyas;
      ovs_dyn_snf_start_process(cur->vhost, 0, item_type_phy,
                                cur->vhost, snf_process_started);
      result = 0;
      }
    }
  else
    {
    if (cur->endp_type == endp_type_phyav)
      KERR("ERROR %s %d", name, val);
    else
      {
      cur->endp_type = endp_type_phyav;
      memset(phy, 0, 2*MAX_NAME_LEN);
      vh = cur->vhost;
      snprintf(phy, 2*MAX_NAME_LEN-1, "s%s", vh);
      if (cur->del_snf_ethv_sent == 0)
        {
        cur->del_snf_ethv_sent = 1;
        if (strlen(cur->lan))
          {
          if (ovs_snf_send_del_snf_lan(cur->name, cur->num_macvlan,
                                       cur->vhost, cur->lan))
            KERR("ERROR %s %d %s", name, val, cur->lan);
          }
        ovs_dyn_snf_stop_process(phy);
        }
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
t_ovs_phy *ovs_phy_exists_vhost(char *name)
{
  return (find_phy_vhost(name));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_phy_init(void)
{
  g_nb_phy = 0;
  g_head_phy = NULL;
}
/*--------------------------------------------------------------------------*/

