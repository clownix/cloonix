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
#include "utils_cmd_line_maker.h"
#include "vhost_eth.h"
#include "fmt_diag.h"
#include "phy_mngt.h"
#include "phy_evt.h"

typedef struct t_phy_vhost
{
  int llid;
  int tid;
  int ovs_tid;
  int state;
  int count;
  char name[IFNAMSIZ];
  char lan[MAX_NAME_LEN];
  struct t_phy_vhost *prev;
  struct t_phy_vhost *next;
} t_phy_vhost;

enum {
  state_min=100,
  state_wait_del_port,
  state_wait_add_port,
  state_max,
};

static t_phy_vhost *g_head_phy_vhost;

/****************************************************************************/
static t_phy_vhost *alloc_phy_vhost(char *name)
{
  t_phy_vhost *cur = (t_phy_vhost *) malloc(sizeof(t_phy_vhost));
  memset(cur, 0, sizeof(t_phy_vhost));
  strncpy(cur->name, name, IFNAMSIZ-1);
  if (g_head_phy_vhost)
    g_head_phy_vhost->prev = cur;
  cur->next = g_head_phy_vhost;
  g_head_phy_vhost = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_phy_vhost(t_phy_vhost *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_phy_vhost)
    g_head_phy_vhost = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_phy_vhost *find_phy_vhost(char *name)
{
  t_phy_vhost *cur = g_head_phy_vhost;
  while (cur)
    {
    if (!strcmp(name, cur->name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fail_resp_req(t_phy_vhost *cur)
{
   KERR("%s %s", cur->name, cur->lan);
  if (msg_exist_channel(cur->llid))
    send_status_ko(cur->llid, cur->tid, "KO");
  else
    KERR("%s %s", cur->name, cur->lan);
  free_phy_vhost(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_phy(void *data)
{
  t_phy_vhost *next, *cur = g_head_phy_vhost;
  while (cur)
    {
    next = cur->next;
    cur->count += 1;
    if (cur->count == 20)
      fail_resp_req(cur);
    cur = next;
    }
  clownix_timeout_add(20, timer_phy, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void phy_vhost_ack_add_port(int tid, int is_ok, char *lan, char *name)
{
  t_phy_vhost *cur = find_phy_vhost(name);
  if (!cur)
    {
    KERR("%s %s", name, lan);
    }
  else if (is_ok == 0)
    {
    KERR("%s %s", name, lan);
    if (msg_exist_channel(cur->llid))
      send_status_ko(cur->llid, cur->tid, "KO");
    else
      KERR("%s %s", name, lan);
    free_phy_vhost(cur);
    }
  else
    {
    if (tid != cur->ovs_tid)
      KERR("%s %s %d %d", name, lan, tid, cur->ovs_tid);
    if (cur->state != state_wait_add_port)
      KERR("%s %s %d", name, lan, cur->state);
    send_status_ok(cur->llid, cur->tid, "OK");
    free_phy_vhost(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void phy_vhost_ack_del_port(int tid, int is_ok, char *lan, char *name)
{
  t_phy_vhost *cur = find_phy_vhost(name);
  if (!cur)
    {
    KERR("%s %s", name, lan);
    }
  else if (is_ok == 0)
    {
    KERR("%s %s", name, lan);
    if (msg_exist_channel(cur->llid))
      send_status_ko(cur->llid, cur->tid, "KO");
    else
      KERR("%s %s", name, lan);
    free_phy_vhost(cur);
    }
  else
    {
    if (tid != cur->ovs_tid)
      KERR("%s %s %d %d", name, lan, tid, cur->ovs_tid);
    if (cur->state != state_wait_del_port)
      KERR("%s %s %d", name, lan, cur->state);
    else
      {
      if (msg_exist_channel(cur->llid))
        send_status_ok(cur->llid, cur->tid, "OK");
      else
        KERR("%s %s", name, lan);
      }
    free_phy_vhost(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int phy_vhost_add_port(int llid, int tid, char *name, char *lan)
{
  int result = -1;
  int ovs_tid = utils_get_next_tid();
  t_phy_vhost *cur = find_phy_vhost(name);
  if (cur)
    KERR("%s %s", name, lan);
  else if (!vhost_lan_exists(lan))
    KERR("%s %s", name, lan);
  else if (fmt_tx_add_phy_vhost_port(ovs_tid, name, lan))
    KERR("%s %s", name, lan);
  else
    {
    cur = alloc_phy_vhost(name);
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    cur->state = state_wait_add_port;
    cur->ovs_tid = ovs_tid;
    cur->llid = llid;
    cur->tid = tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int phy_vhost_del_port(char *name, char *lan)
{
  int ovs_tid, result = -1;
  t_phy_vhost *cur;
  ovs_tid = utils_get_next_tid();
  if (fmt_tx_del_phy_vhost_port(ovs_tid, name, lan))
    {
    KERR("%s %s", name, lan);
    }
  else
    {
    cur = alloc_phy_vhost(name);    
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    cur->state = state_wait_del_port;
    cur->ovs_tid = ovs_tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *phy_vhost_get_ifname(char *name, int num)
{
  char *result = NULL;
  t_vm *vm = cfg_get_vm(name);
  if (!vm)
    KERR("ERR NOT FOUND: %s", name);
  else
    {
    result = vhost_ident_get(vm->kvm.vm_id, num);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void phy_vhost_init(void)
{
  g_head_phy_vhost = NULL;
  clownix_timeout_add(200, timer_phy, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
