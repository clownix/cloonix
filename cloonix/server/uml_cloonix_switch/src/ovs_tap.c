/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include "ovs_tap.h"
#include "msg.h"
#include "lan_to_name.h"


static t_ovs_tap *g_head_tap;
static int g_nb_tap;

int get_glob_req_self_destruction(void);

/****************************************************************************/
static t_ovs_tap *find_tap(char *name)
{
  t_ovs_tap *cur = g_head_tap;
  while(cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_tap(t_ovs_tap *cur)
{
  cfg_free_obj_id(cur->tap_id);
  if (strlen(cur->lan_added))
    lan_del_name(cur->lan_added);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_tap)
    g_head_tap = cur->next;
  g_nb_tap -= 1;
  free(cur);
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_tap(int llid, int tid, char *name)
{
  uint8_t mc[6];
  int id = cfg_alloc_obj_id();
  t_ovs_tap *cur = (t_ovs_tap *) malloc(sizeof(t_ovs_tap));
  memset(cur, 0, sizeof(t_ovs_tap));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->tap_id = id;
  cur->llid = llid;
  cur->tid = tid;
  mc[0] = 0x2;
  mc[1] = 0xFF & rand();
  mc[2] = 0xFF & rand();
  mc[3] = 0xFF & rand();
  mc[4] = cur->tap_id % 100;
  mc[5] = 0xFF & rand();
  snprintf(cur->vhost, (MAX_NAME_LEN-1), "%s%d_0", OVS_BRIDGE_PORT, id);
  snprintf(cur->mac, (MAX_NAME_LEN-1), 
           "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
           mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
  if (g_head_tap)
    g_head_tap->prev = cur;
  cur->next = g_head_tap;
  g_head_tap = cur; 
  g_nb_tap += 1;
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_tap_resp_add_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan)
{
  t_ovs_tap *cur = find_tap(name);
  if (!cur)
    KERR("ERROR %d %s", is_ko, name);
  else
    {
    if (strlen(cur->lan))
      KERR("ERROR %s %s", name, cur->lan);
    memset(cur->lan, 0, MAX_NAME_LEN);
    if (is_ko)
      {
      KERR("ERROR %d %s", is_ko, name);
      utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      }
    else
      {
      cfg_hysteresis_send_topo_info();
      strncpy(cur->lan, lan, MAX_NAME_LEN);
      utils_send_status_ok(&(cur->llid), &(cur->tid));
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_tap_resp_del_lan(int is_ko, char *name, int num, 
                          char *vhost, char *lan)
{
  t_ovs_tap *cur = find_tap(name);
  if (!cur)
    KERR("ERROR %d %s", is_ko, name);
  else if (cur->del_tap_req == 1)
    {
    memset(cur->lan, 0, MAX_NAME_LEN);
    if (msg_send_del_tap(name, cur->vhost))
      KERR("ERROR %d %s", is_ko, name);
    }
  else
    {
    memset(cur->lan, 0, MAX_NAME_LEN);
    if (is_ko)
      {
      KERR("ERROR %d %s", is_ko, name);
      utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      }
    else
      {
      cfg_hysteresis_send_topo_info();
      utils_send_status_ok(&(cur->llid), &(cur->tid));
      }
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void ovs_tap_resp_msg_tap(int is_ko, int is_add, char *name)
{
  t_ovs_tap *cur = find_tap(name);
  if (!cur)
    KERR("ERROR %d %d %s", is_ko, is_add, name);
  else
    {
    if (is_ko)
      {
      KERR("ERROR %d %d %s", is_ko, is_add, name);
      utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      }
    else
      utils_send_status_ok(&(cur->llid), &(cur->tid));
    
    if (is_add == 0)
      free_tap(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_tap_exists(char *name)
{
  int result = 0;
  if (find_tap(name))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_ovs_tap *ovs_tap_get_first(int *nb_tap)
{
  t_ovs_tap *result = g_head_tap;
  *nb_tap = g_nb_tap;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_tap_add(int llid, int tid, char *name)
{
  t_ovs_tap *cur = find_tap(name);
  if (cur)
    send_status_ko(llid, tid, "Exists already");
  else
    {
    alloc_tap(llid, tid, name);
    cur = find_tap(name);
    if (!cur)
      KOUT(" ");
    if (msg_send_add_tap(name, cur->vhost, cur->mac))
      {
      KERR("ERROR %s", name);
      utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_tap_del(int llid, int tid, char *name)
{
  t_ovs_tap *cur = find_tap(name);
  int val;
  if (!cur)
    {
    KERR("ERROR %s", name);
    if (llid)
      send_status_ko(llid, tid, "Does not exist");
    }
  else if (cur->llid)
    {
    KERR("ERROR %s %d %d", name, cur->llid, cur->del_tap_req);
    if (llid)
      send_status_ko(llid, tid, "Not ready");
    }
  else if (cur->del_tap_req)
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
      val = lan_del_name(cur->lan_added);
      memset(cur->lan_added, 0, MAX_NAME_LEN);
      if (val == 2*MAX_LAN)
        {
        if (msg_send_del_tap(name, cur->vhost))
          {
          KERR("ERROR %s", name);
          utils_send_status_ko(&(cur->llid), &(cur->tid), name);
          }
        }
      else
        {
        msg_send_del_lan_endp(ovsreq_del_tap_lan, name, 0,
                              cur->vhost, cur->lan);
        cur->del_tap_req = 1;
        }
      }
    else if (msg_send_del_tap(name, cur->vhost))
      {
      KERR("ERROR %s", name);
      utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_tap_add_lan(int llid, int tid, char *name, char *lan)
{
  t_ovs_tap *cur = find_tap(name);
  if (!cur)
    {
    KERR("ERROR %s", name);
    send_status_ko(llid, tid, "Does not exist");
    }
  else if ((cur->llid) || (cur->del_tap_req))
    {
    KERR("ERROR %s %d %d", name, cur->llid, cur->del_tap_req);
    send_status_ko(llid, tid, "Not ready");
    }
  else if (strlen(cur->lan))
    {
    KERR("ERROR %s %s", name, cur->lan);
    send_status_ko(llid, tid, "Lan exists");
    }
  else
    {
    strncpy(cur->lan_added, lan, MAX_NAME_LEN);
    lan_add_name(cur->lan_added, llid);
    cur->llid = llid;
    cur->tid = tid;
    if (msg_send_add_lan_endp(ovsreq_add_tap_lan, name, 0, cur->vhost, lan))
      {
      KERR("ERROR %s", name);
      utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_tap_del_lan(int llid, int tid, char *name, char *lan)
{
  t_ovs_tap *cur = find_tap(name);
  int val = 0;
  if (!cur)
    {
    KERR("ERROR %s", name);
    send_status_ko(llid, tid, "Does not exist");
    }
  else if ((cur->llid) || (cur->del_tap_req))
    {
    KERR("ERROR %s %d %d", name, cur->llid, cur->del_tap_req);
    send_status_ko(llid, tid, "Not ready");
    }
  else
    {
    if (!strlen(cur->lan_added))
      KERR("ERROR: %s %s", name, lan);
    else
      {
      val = lan_del_name(cur->lan_added);
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
      if (msg_send_del_lan_endp(ovsreq_del_tap_lan, name, 0, cur->vhost, lan))
        utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *ovs_tap_translate_topo_endp(int *nb)
{
  t_ovs_tap *cur;
  int len, count = 0, nb_endp = 0;
  t_topo_endp *endp;
  cur = g_head_tap;
  while(cur)
    {
    if (strlen(cur->lan))
      count += 1;
    cur = cur->next;
    }
  endp = (t_topo_endp *) clownix_malloc(count * sizeof(t_topo_endp), 13);
  memset(endp, 0, count * sizeof(t_topo_endp));
  cur = g_head_tap;
  while(cur)
    {
    if (strlen(cur->lan))
      {
      len = sizeof(t_lan_group_item);
      endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
      memset(endp[nb_endp].lan.lan, 0, len);
      strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
      endp[nb_endp].num = 0;
      endp[nb_endp].type = endp_type_ethv;
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
int  ovs_tap_snf(char *name)
{
  int result = 0;
  KERR("OOOOOOOOOOOOOOOOOO %s", name);
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void ovs_tap_init(void)
{
  g_nb_tap = 0;
  g_head_tap = NULL;
}
/*--------------------------------------------------------------------------*/

