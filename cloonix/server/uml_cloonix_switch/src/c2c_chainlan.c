/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include "ovs_c2c.h"
#include "lan_to_name.h"
#include "ovs_snf.h"
#include "mactopo.h"
#include "proxycrun.h"
#include "c2c_chainlan.h"
#include "msg.h"
#include "hop_event.h"

typedef struct t_chainlan
{
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];
  int must_call_snf_started;
  int endp_type;
  int cmd_mac_mangle;
  uint8_t mac_mangle[6];
  struct t_chainlan *prev;
  struct t_chainlan *next;
} t_chainlan;

static t_chainlan *g_head_chainlan;
static int g_nb_chainlan;
int shadown_lan_exists(char *name, int num, char *lan);

/****************************************************************************/
static void chainlan_alloc(char *name, char *vhost, int endp_type)
{
  t_chainlan *cur = (t_chainlan *) malloc(sizeof(t_chainlan));
  memset(cur, 0, sizeof(t_chainlan));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->vhost, vhost, MAX_NAME_LEN-1);
  cur->endp_type = endp_type;
  if (g_head_chainlan)
    g_head_chainlan->prev = cur;
  cur->next = g_head_chainlan;
  g_head_chainlan = cur;
  g_nb_chainlan += 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void chainlan_free(t_chainlan *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_head_chainlan == cur)
    g_head_chainlan = g_head_chainlan->next;
  g_nb_chainlan -= 1;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_chainlan *chainlan_find(char *name)
{
  t_chainlan *cur = g_head_chainlan;
  while (cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    } 
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void mac_mangle(t_chainlan *cur, int ovs_llid)
{
  char msg[MAX_PATH_LEN];
  uint8_t *mc = cur->mac_mangle;
  if ((cur->cmd_mac_mangle) &&
      (ovs_llid) &&
      (msg_exist_channel(ovs_llid)))
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,
    "c2c_mac_mangle %s %hhX:%hhX:%hhX:%hhX:%hhX:%hhX", cur->name,
                  mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
    rpct_send_sigdiag_msg(ovs_llid, type_hop_c2c, msg);
    hop_event_hook(ovs_llid, FLAG_HOP_SIGDIAG, msg);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void c2c_chainlan_snf_started(char *name, int num, char *vhost)
{      
  char *locnet = cfg_get_cloonix_name();
  t_chainlan *cur = chainlan_find(name);
  if (cur == NULL)
    KERR("ERROR %s %s %d %s", locnet, name, num, vhost);
  else
    {
    if ((strlen(cur->lan) && (cur->endp_type == endp_type_c2cs)))
      ovs_snf_send_add_snf_lan(name, num, cur->vhost, cur->lan);
    else
      cur->must_call_snf_started = 1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_chainlan_resp_del_lan(int is_ko, char *name, int num,
                               char *vhost, char *lan)
{
  mactopo_del_resp(item_c2c, name, num, lan);
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_chainlan_resp_add_lan(int is_ko, char *name, int num,
                               char *vhost, char *lan)
{
  char *locnet = cfg_get_cloonix_name();
  t_chainlan *cur = chainlan_find(name);
  if (!cur)
    KERR("ERROR %s %d %s %d %s %s", locnet, is_ko, name, num, vhost, lan);
  else
    cur->must_call_snf_started = 0;
  mactopo_add_resp(item_c2c, name, num, lan);
  c2c_chainlan_snf_started(name, num, vhost);
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void action_heartbeat_del(char *name, char *lan, char *vhost)
{
  int val;
  char *locnet = cfg_get_cloonix_name();
  t_chainlan *cur = chainlan_find(name);
  if (!cur)
    KERR("ERROR %s %s %s %s", locnet, name, lan, vhost);
  mactopo_del_req(item_c2c, name, 0, lan);
  val = lan_del_name(lan, item_c2c, name, 0);
  if (val != 2*MAX_LAN)
    msg_send_del_lan_endp(ovsreq_del_c2c_lan, name, 0, vhost, lan);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void action_heartbeat_add(char *name, char *lan,
                                 char *vhost, int ovs_llid)
{
  char *locnet = cfg_get_cloonix_name();
  char err[MAX_PATH_LEN];
  t_chainlan *cur = chainlan_find(name);
  if (!cur)
    KERR("ERROR %s %s %s %s", locnet, name, lan, vhost);
  else
    {
    lan_add_name(lan, item_c2c, name, 0);
    if (mactopo_add_req(item_c2c, name, 0, lan, NULL, NULL, err))
      {
      KERR("ERROR %s %s %s %s %s", locnet, name, lan, vhost, err);
      lan_del_name(lan, item_c2c, name, 0);
      }
    else if (msg_send_add_lan_endp(ovsreq_add_c2c_lan, name, 0, vhost, lan))
      {
      KERR("ERROR %s %s %s %s", locnet, name, lan, vhost);
      lan_del_name(lan, item_c2c, name, 0);
      }
    else if (cur->must_call_snf_started)
      {
      if ((strlen(cur->lan) && (cur->endp_type == endp_type_c2cs)))
        {
        ovs_snf_send_add_snf_lan(name, 0, vhost, lan);
        cur->must_call_snf_started = 0;
        }
      }
    mac_mangle(cur, ovs_llid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int c2c_chainlan_action_heartbeat(t_ovs_c2c *c2c, char *lan)
{
  char *locnet = cfg_get_cloonix_name();
  t_chainlan *cur = chainlan_find(c2c->name);
  int result = 0;
  char shadow_lan[MAX_NAME_LEN];
  memset(lan, 0, MAX_NAME_LEN);
  if (cur)
    {
    if (strlen(cur->lan) == 0)
      {
      if (strlen(c2c->topo.attlan))
        {
        action_heartbeat_del(c2c->name, c2c->topo.attlan, c2c->vhost);
        result = 1;
        }
      else
        {
        if (shadown_lan_exists(c2c->name, 0, shadow_lan))
          c2c_chainlan_add_lan(c2c, shadow_lan);
        }
      }
    else if (!shadown_lan_exists(c2c->name, 0, shadow_lan))
      {
      c2c_chainlan_del_lan(c2c, cur->lan);
      }
    else if ((c2c->topo.tcp_connection_peered == 1) &&
             (c2c->topo.udp_connection_peered == 1))
      {
      if (strlen(c2c->topo.attlan))
        {
        if (strcmp(c2c->topo.attlan, cur->lan))
          KERR("ERROR %s %s %s", locnet, c2c->topo.attlan, cur->lan);
        }
      else
        {
        action_heartbeat_add(c2c->name,cur->lan,c2c->vhost,c2c->ovs_llid);
        strncpy(lan, cur->lan, MAX_NAME_LEN);
        result = 1;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int c2c_chainlan_exists_lan(char *name)
{
  int result = 0;
  t_chainlan *cur = chainlan_find(name);
  if (cur)
    {
    if (strlen(cur->lan))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int c2c_chainlan_del_lan(t_ovs_c2c *c2c, char *lan)
{
  char *locnet = cfg_get_cloonix_name();
  int result = -1;
  t_chainlan *cur = chainlan_find(c2c->name);
  if (!cur)
    KERR("ERROR %s %s %s", locnet, c2c->name, lan);
  else if (!strlen(lan))
    KERR("ERROR %s %s %s", locnet, c2c->name, lan);
  else if (!strlen(cur->lan))
    KERR("ERROR %s %s %s", locnet, c2c->name, lan);
  else if (strcmp(cur->lan, lan))
    KERR("ERROR %s %s %s %s", locnet, c2c->name, lan, cur->lan);
  else
    {
    result = 0;
    memset(cur->lan, 0, MAX_NAME_LEN);
    cfg_hysteresis_send_topo_info();
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int c2c_chainlan_add_lan(t_ovs_c2c *c2c, char *lan)
{
  char *locnet = cfg_get_cloonix_name();
  int result = -1;
  t_chainlan *cur = chainlan_find(c2c->name);
  if (!cur)
    KERR("ERROR %s %s %s", locnet, c2c->name, lan);
  else if (!strlen(lan))
    KERR("ERROR %s %s %s", locnet, c2c->name, lan);
  else if (!strlen(lan))
    KERR("ERROR %s %s %s", locnet, c2c->name, lan);
  else if (strlen(cur->lan))
    KERR("ERROR %s %s %s", locnet, c2c->name, lan);
  else
    {
    result = 0;
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    cfg_hysteresis_send_topo_info();
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_chainlan_update_endp_type(char *name, int endp_type)
{
  char *locnet = cfg_get_cloonix_name();
  t_chainlan *cur = chainlan_find(name);
  if (!cur)
    KERR("ERROR %s %s", locnet, name);
  else
    cur->endp_type = endp_type;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int c2c_chainlan_del(t_ovs_c2c *c2c)
{
  char *locnet = cfg_get_cloonix_name();
  t_chainlan *cur = chainlan_find(c2c->name);
  int result = -1;
  if (!cur)
    KERR("ERROR %s %s", locnet, c2c->name);
  else
    {
    result = 0;
    chainlan_free(cur);
    cfg_hysteresis_send_topo_info();
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int c2c_chainlan_partial_del(t_ovs_c2c *c2c)
{
  char *locnet = cfg_get_cloonix_name();
  t_chainlan *cur = chainlan_find(c2c->name);
  int result = -1;
  if (!cur)
    KERR("ERROR %s %s", locnet, c2c->name);
  else
    {
    result = 0;
    if ((mactopo_test_exists(cur->name, 0)) &&
        (strlen(cur->lan)))
    action_heartbeat_del(cur->name, cur->lan, cur->vhost);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int c2c_chainlan_add(t_ovs_c2c *c2c)
{
  char *locnet = cfg_get_cloonix_name();
  t_chainlan *cur = chainlan_find(c2c->name);
  int result = -1;
  if (cur)
    KERR("ERROR %s %s", locnet, c2c->name);
  else if (!strlen(c2c->vhost))
    KERR("ERROR %s %s", locnet, c2c->name);
  else
    {
    result = 0;
    chainlan_alloc(c2c->name, c2c->vhost, c2c->topo.endp_type);
    cfg_hysteresis_send_topo_info();
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int c2c_chainlan_store_cmd(char *name, int cmd_mac_mangle, uint8_t *mac)
{
  char *locnet = cfg_get_cloonix_name();
  t_chainlan *cur = chainlan_find(name);
  int i, result = -1;
  uint8_t *mc;
  if (!cur)
    KERR("ERROR %s %s", locnet, name);
  else if (cur->cmd_mac_mangle)
    {
    mc = cur->mac_mangle;
    KERR("ERROR %s %s %02X:%02X:%02X:%02X:%02X:%02X", locnet, name,
         mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
    }
  else
    {
    result = 0;
    cur->cmd_mac_mangle = 1;
    for (i=0; i<6; i++)
      cur->mac_mangle[i] = mac[i];
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_chainlan_init(void)
{
  g_nb_chainlan = 0;
  g_head_chainlan = NULL;
}
/*--------------------------------------------------------------------------*/

