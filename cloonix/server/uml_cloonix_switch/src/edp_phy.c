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
#include "fmt_diag.h"
#include "edp_mngt.h"
#include "edp_evt.h"
#include "dpdk_dyn.h"

typedef struct t_edp_phy
{
  int llid;
  int tid;
  int ovs_tid;
  int state;
  int count;
  char name[IFNAMSIZ];
  char lan[MAX_NAME_LEN];
  struct t_edp_phy *prev;
  struct t_edp_phy *next;
} t_edp_phy;

enum {
  state_min=100,
  state_wait_del_port,
  state_wait_add_port,
  state_max,
};

static t_edp_phy *g_head_edp_phy;

/****************************************************************************/
static t_edp_phy *alloc_edp_phy(char *name)
{
  t_edp_phy *cur = (t_edp_phy *) malloc(sizeof(t_edp_phy));
  memset(cur, 0, sizeof(t_edp_phy));
  strncpy(cur->name, name, IFNAMSIZ-1);
  if (g_head_edp_phy)
    g_head_edp_phy->prev = cur;
  cur->next = g_head_edp_phy;
  g_head_edp_phy = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_edp_phy(t_edp_phy *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_edp_phy)
    g_head_edp_phy = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_edp_phy *find_edp_phy(char *name)
{
  t_edp_phy *cur = g_head_edp_phy;
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
  t_edp_phy *next, *cur = g_head_edp_phy;
  while (cur)
    {
    next = cur->next;
    cur->count += 1;
    if (cur->count == 20)
      {
      KERR("%s %s", cur->name, cur->lan);
      utils_send_status_ko(&(cur->llid), &(cur->tid), "timeout");
      free_edp_phy(cur);
      }
    cur = next;
    }
  clownix_timeout_add(20, timer_edp, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_phy_ack_add(int tid, int is_ok, char *lan, char *name)
{
  t_edp_phy *cur = find_edp_phy(name);
  if (!cur)
    {
    KERR("%s %s", name, lan);
    }
  else if (is_ok == 0)
    {
    KERR("%s %s", name, lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "openvswitch fail");
    free_edp_phy(cur);
    }
  else
    {
    if (tid != cur->ovs_tid)
      KERR("%s %s %d %d", name, lan, tid, cur->ovs_tid);
    if (cur->state != state_wait_add_port)
      KERR("%s %s %d", name, lan, cur->state);
    edp_evt_update_non_fix_add(eth_type_dpdk, name, lan);
    utils_send_status_ok(&(cur->llid), &(cur->tid));
    free_edp_phy(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_phy_ack_del(int tid, int is_ok, char *lan, char *name)
{
  t_edp_phy *cur = find_edp_phy(name);
  if (!cur)
    {
    KERR("%s %s", name, lan);
    }
  else if (is_ok == 0)
    {
    KERR("%s %s", name, lan);
    utils_send_status_ko(&(cur->llid), &(cur->tid), "openvswitch fail");
    free_edp_phy(cur);
    }
  else
    {
    if (tid != cur->ovs_tid)
      KERR("%s %s %d %d", name, lan, tid, cur->ovs_tid);
    if (cur->state != state_wait_del_port)
      KERR("%s %s %d", name, lan, cur->state);
    else
      {
      edp_evt_update_non_fix_del(eth_type_dpdk, name, lan);
      utils_send_status_ok(&(cur->llid), &(cur->tid));
      }
    free_edp_phy(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int edp_phy_add_dpdk(int llid, int tid, char *lan, char *name)
{
  int result = -1;
  int ovs_tid = utils_get_next_tid();
  t_edp_phy *cur = find_edp_phy(name);
  if (cur)
    KERR("%s %s", name, lan);
  else if (!dpdk_dyn_lan_exists(lan))
    KERR("%s %s", name, lan);
  else if (fmt_tx_add_lan_phy_dpdk(ovs_tid, lan, name))
    KERR("%s %s", name, lan);
  else
    {
    cur = alloc_edp_phy(name);
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
int edp_phy_del_dpdk(char *lan, char *name)
{
  int ovs_tid, result = -1;
  t_edp_phy *cur;
  ovs_tid = utils_get_next_tid();
  if (fmt_tx_del_lan_phy_dpdk(ovs_tid, lan, name))
    {
    KERR("%s %s", name, lan);
    }
  else
    {
    cur = alloc_edp_phy(name);    
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    cur->state = state_wait_del_port;
    cur->ovs_tid = ovs_tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void edp_phy_init(void)
{
  g_head_edp_phy = NULL;
  clownix_timeout_add(200, timer_edp, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
