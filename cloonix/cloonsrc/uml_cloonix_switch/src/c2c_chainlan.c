/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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

static int c2c_chainlan_add_lan(t_ovs_c2c *c2c, char *lan);
static int c2c_chainlan_del_lan(t_ovs_c2c *c2c, char *lan);

typedef struct t_chainlan
{
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  char vhost[MAX_NAME_LEN];
  int must_call_snf_started;
  int endp_type;
  int cmd_mac_mangle;
  int add_ko;
  uint8_t mac_mangle[6];
  struct t_chainlan *prev;
  struct t_chainlan *next;
} t_chainlan;

static t_chainlan *g_head_chainlan;
static int g_nb_chainlan;

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
    "c2c_mac_mangle %s %02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX", cur->name,
                  mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
    rpct_send_sigdiag_msg(ovs_llid, type_hop_c2c, msg);
    }
}

void c2c_chainlan_snf_started(char *name, int num, char *vhost)
{      
  t_chainlan *cur = chainlan_find(name);
  if (cur == NULL)
    KERR("ERROR %s %d %s", name, num, vhost);
  else
    {
    if ((strlen(cur->lan) && (cur->endp_type == endp_type_c2cs)))
      ovs_snf_send_add_snf_lan(name, num, cur->vhost, cur->lan);
    else
      cur->must_call_snf_started = 1;
    }
}

void c2c_chainlan_resp_del_lan(int is_ko, char *name, int num,
                               char *vhost, char *lan)
{
  if (is_ko)
    KERR("ERROR c2c %s: ovs refused the del of lan %s, port %s may be left "
         "on the bridge", name, lan, vhost);
  mactopo_del_resp(item_c2c, name, num, lan);
  cfg_hysteresis_send_topo_info();
}

void c2c_chainlan_resp_add_lan(int is_ko, char *name, int num,
                               char *vhost, char *lan)
{
  t_chainlan *cur = chainlan_find(name);
  if (!cur)
    {
    KTRC("plug response for c2c %s (is_ko=%d): no chainlan any more",
         name, is_ko);
    mactopo_add_resp(0, item_c2c, name, num, lan);
    }
  else if (is_ko)
    {
    mactopo_add_resp(0, item_c2c, name, num, lan);
    lan_del_name(lan, item_c2c, name, 0);
    cur->add_ko += 1;
    if (cur->add_ko <= 5)
      {
      KERR("WARNING c2c %s: ovs refused the add of lan %s (%d), the "
           "heartbeat will retry", name, lan, cur->add_ko);
      ovs_c2c_attlan_clear(name);
      }
    else
      KERR("ERROR c2c %s: ovs refused the add of lan %s %d times, giving "
           "up, the c2c stays uncabled", name, lan, cur->add_ko);
    }
  else
    {
    cur->add_ko = 0;
    cur->must_call_snf_started = 0;
    mactopo_add_resp(1, item_c2c, name, num, lan);
    c2c_chainlan_snf_started(name, num, vhost);
    }
  cfg_hysteresis_send_topo_info();
}

static void action_heartbeat_del(char *name, char *lan, char *vhost)
{
  int val;
  t_chainlan *cur = chainlan_find(name);
  if (!cur)
    KERR("ERROR %s %s %s", name, lan, vhost);
  mactopo_del_req(item_c2c, name, 0, lan);
  val = lan_del_name(lan, item_c2c, name, 0);
  if (val != 2*MAX_LAN)
    msg_send_del_lan_endp(ovsreq_del_c2c_lan, name, 0, vhost, lan);
}

static void action_heartbeat_add(char *name, char *lan,
                                 char *vhost, int ovs_llid)
{
  char err[MAX_PATH_LEN];
  t_chainlan *cur = chainlan_find(name);
  if (!cur)
    KERR("ERROR %s %s %s", name, lan, vhost);
  else
    {
    lan_add_name(lan, item_c2c, name, 0);
    if (mactopo_add_req(item_c2c, name, 0, lan, NULL, NULL, err))
      {
      KERR("ERROR %s %s %s %s", name, lan, vhost, err);
      lan_del_name(lan, item_c2c, name, 0);
      }
    else if (msg_send_add_lan_endp(ovsreq_add_c2c_lan, name, 0, vhost, lan))
      {
      KERR("ERROR %s %s %s", name, lan, vhost);
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

int c2c_chainlan_action_heartbeat(t_ovs_c2c *c2c, char *lan)
{
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
          KERR("ERROR %s %s", c2c->topo.attlan, cur->lan);
        }
      else
        {
        action_heartbeat_add(c2c->name,cur->lan,c2c->vhost,c2c->ovs_llid);
        strncpy(lan, cur->lan, MAX_NAME_LEN-1);
        result = 1;
        }
      }
    }
  return result;
}

static int c2c_chainlan_del_lan(t_ovs_c2c *c2c, char *lan)
{
  int result = -1;
  t_chainlan *cur = chainlan_find(c2c->name);
  if (!cur)
    KERR("ERROR %s %s", c2c->name, lan);
  else if (!strlen(lan))
    KERR("ERROR %s %s", c2c->name, lan);
  else if (!strlen(cur->lan))
    KERR("ERROR %s %s", c2c->name, lan);
  else if (strcmp(cur->lan, lan))
    KERR("ERROR %s %s %s", c2c->name, lan, cur->lan);
  else
    {
    result = 0;
    memset(cur->lan, 0, MAX_NAME_LEN);
    cfg_hysteresis_send_topo_info();
    }
  return result;
}

static int c2c_chainlan_add_lan(t_ovs_c2c *c2c, char *lan)
{
  int result = -1;
  t_chainlan *cur = chainlan_find(c2c->name);
  if (!cur)
    KERR("ERROR %s %s", c2c->name, lan);
  else if (!strlen(lan))
    KERR("ERROR %s %s", c2c->name, lan);
  else if (strlen(cur->lan))
    KERR("ERROR %s %s", c2c->name, lan);
  else
    {
    result = 0;
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    cfg_hysteresis_send_topo_info();
    }
  return result;
}

void c2c_chainlan_update_endp_type(char *name, int endp_type)
{
  t_chainlan *cur = chainlan_find(name);
  if (!cur)
    KERR("ERROR %s", name);
  else
    cur->endp_type = endp_type;
}

int c2c_chainlan_del(t_ovs_c2c *c2c)
{
  t_chainlan *cur = chainlan_find(c2c->name);
  int result = -1;
  if (!cur)
    KERR("ERROR %s", c2c->name);
  else
    {
    result = 0;
    chainlan_free(cur);
    cfg_hysteresis_send_topo_info();
    }
  return result;
}

int c2c_chainlan_partial_del(t_ovs_c2c *c2c)
{
  t_chainlan *cur = chainlan_find(c2c->name);
  int result = -1;
  if (!cur)
    KERR("ERROR %s", c2c->name);
  else
    {
    result = 0;
    /* after an unplug cur->lan is empty one beat: a kept record would refuse
           the lan to the next c2c of that name */
    if (mactopo_test_exists(cur->name, 0))
      {
      if (strlen(c2c->topo.attlan))
        action_heartbeat_del(cur->name, c2c->topo.attlan, cur->vhost);
      else if (strlen(cur->lan))
        action_heartbeat_del(cur->name, cur->lan, cur->vhost);
      else
        {
        KERR("ERROR %s: a mac record and no lan to unplug it from", c2c->name);
        mactopo_del_req(item_c2c, cur->name, 0, "");
        }
      }
    }
  return result;
}

int c2c_chainlan_add(t_ovs_c2c *c2c)
{
  t_chainlan *cur = chainlan_find(c2c->name);
  int result = -1;
  if (cur)
    {
    /* A restart, not a duplicate */
    if (!strlen(c2c->vhost))
      KERR("ERROR %s: restarting with no vhost interface",
           c2c->name);
    else
      {
      if (strcmp(cur->vhost, c2c->vhost))
        KTRC("%s restarted: its chainlan follows the vhost from %s to %s",
             c2c->name, cur->vhost, c2c->vhost);
      memset(cur->vhost, 0, MAX_NAME_LEN);
      strncpy(cur->vhost, c2c->vhost, MAX_NAME_LEN-1);
      cur->endp_type = c2c->topo.endp_type;
      result = 0;
      cfg_hysteresis_send_topo_info();
      }
    }
  else if (!strlen(c2c->vhost))
    KERR("ERROR %s: no vhost interface, so there is nothing to cable",
         c2c->name);
  else
    {
    result = 0;
    chainlan_alloc(c2c->name, c2c->vhost, c2c->topo.endp_type);
    cfg_hysteresis_send_topo_info();
    }
  return result;
}

int c2c_chainlan_store_cmd(char *name, int cmd_mac_mangle, uint8_t *mac)
{
  t_chainlan *cur = chainlan_find(name);
  int i, result = -1;
  uint8_t *mc;
  if (!cur)
    KERR("ERROR %s", name);
  else
    {
    mc = cur->mac_mangle;
    (void) mc;
    if (cur->cmd_mac_mangle)
      KTRC("%s replacing %02X:%02X:%02X:%02X:%02X:%02X", name,
           mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
    result = 0;
    cur->cmd_mac_mangle = 1;
    for (i=0; i<6; i++)
      cur->mac_mangle[i] = mac[i];
    }
  return result;
}

/* the mac_mangle in force on a c2c, for dump commands: 1 when there is one */
int c2c_chainlan_mac_mangle(char *name, uint8_t *mac)
{
  t_chainlan *cur = chainlan_find(name);
  if ((!cur) || (!cur->cmd_mac_mangle))
    return 0;
  memcpy(mac, cur->mac_mangle, 6);
  return 1;
}

void c2c_chainlan_init(void)
{
  g_nb_chainlan = 0;
  g_head_chainlan = NULL;
}

