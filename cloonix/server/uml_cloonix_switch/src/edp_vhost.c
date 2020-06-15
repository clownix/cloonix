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
#include "edp_mngt.h"
#include "edp_evt.h"

typedef struct t_edp_vhost
{
  int llid;
  int tid;
  int ovs_tid;
  int state;
  int count;
  char name[IFNAMSIZ];
  char lan[MAX_NAME_LEN];
  struct t_edp_vhost *prev;
  struct t_edp_vhost *next;
} t_edp_vhost;

enum {
  state_min=100,
  state_wait_del_port,
  state_wait_add_port,
  state_max,
};

static t_edp_vhost *g_head_edp_vhost;

/****************************************************************************/
static t_edp_vhost *alloc_edp_vhost(char *name)
{
  t_edp_vhost *cur = (t_edp_vhost *) malloc(sizeof(t_edp_vhost));
  memset(cur, 0, sizeof(t_edp_vhost));
  strncpy(cur->name, name, IFNAMSIZ-1);
  if (g_head_edp_vhost)
    g_head_edp_vhost->prev = cur;
  cur->next = g_head_edp_vhost;
  g_head_edp_vhost = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_edp_vhost(t_edp_vhost *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_edp_vhost)
    g_head_edp_vhost = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_edp_vhost *find_edp_vhost(char *name)
{
  t_edp_vhost *cur = g_head_edp_vhost;
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
static void timer_edp(void *data)
{
  t_edp_vhost *next, *cur = g_head_edp_vhost;
  while (cur)
    {
    next = cur->next;
    cur->count += 1;
    if (cur->count == 20)
      {
      KERR("%s %s", cur->name, cur->lan);
      utils_send_status_ko(&(cur->llid), &(cur->tid), "timeout");
      free_edp_vhost(cur);
      }
    cur = next;
    }
  clownix_timeout_add(20, timer_edp, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_vhost_ack_add_port(int tid, int is_ok, char *lan, char *name)
{
  t_edp_vhost *cur = find_edp_vhost(name);
  if (!cur)
    {
    KERR("%s %s", name, lan);
    }
  else if (is_ok == 0)
    {
    KERR("%s %s", name, lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "openvswitch fail");
    free_edp_vhost(cur);
    }
  else
    {
    if (tid != cur->ovs_tid)
      KERR("%s %s %d %d", name, lan, tid, cur->ovs_tid);
    if (cur->state != state_wait_add_port)
      KERR("%s %s %d", name, lan, cur->state);
    edp_evt_update_non_fix_add(eth_type_vhost, name, lan);
    utils_send_status_ok(&(cur->llid), &(cur->tid));
    free_edp_vhost(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_vhost_ack_del_port(int tid, int is_ok, char *lan, char *name)
{
  t_edp_vhost *cur = find_edp_vhost(name);
  if (!cur)
    {
    KERR("%s %s", name, lan);
    }
  else if (is_ok == 0)
    {
    KERR("%s %s", name, lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "openvswitch fail");
    free_edp_vhost(cur);
    }
  else
    {
    if (tid != cur->ovs_tid)
      KERR("%s %s %d %d", name, lan, tid, cur->ovs_tid);
    if (cur->state != state_wait_del_port)
      KERR("%s %s %d", name, lan, cur->state);
    else
      {
      edp_evt_update_non_fix_del(eth_type_vhost, name, lan);
      utils_send_status_ok(&(cur->llid), &(cur->tid));
      }
    free_edp_vhost(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_vhost_add_port(int llid, int tid, char *name, char *lan)
{
  int result = -1;
  int ovs_tid = utils_get_next_tid();
  t_edp_vhost *cur = find_edp_vhost(name);
  if (cur)
    KERR("%s %s", name, lan);
  else if (!vhost_lan_exists(lan))
    KERR("%s %s", name, lan);
  else if (fmt_tx_add_phy_vhost_port(ovs_tid, name, lan))
    KERR("%s %s", name, lan);
  else
    {
    cur = alloc_edp_vhost(name);
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
int edp_vhost_del_port(char *name, char *lan)
{
  int ovs_tid, result = -1;
  t_edp_vhost *cur;
  ovs_tid = utils_get_next_tid();
  if (fmt_tx_del_phy_vhost_port(ovs_tid, name, lan))
    {
    KERR("%s %s", name, lan);
    }
  else
    {
    cur = alloc_edp_vhost(name);    
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    cur->state = state_wait_del_port;
    cur->ovs_tid = ovs_tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *edp_vhost_get_ifname(char *name, int num)
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
void edp_vhost_init(void)
{
  g_head_edp_vhost = NULL;
  clownix_timeout_add(200, timer_edp, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
