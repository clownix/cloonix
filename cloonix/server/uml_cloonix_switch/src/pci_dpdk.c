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
#include "phy_mngt.h"
#include "phy_evt.h"
#include "pci_dpdk.h"
#include "dpdk_dyn.h"
#include "suid_power.h"


/****************************************************************************/
typedef struct t_pci_dpdk
{
  int llid;
  int tid;
  int ovs_tid;
  int state;
  int count;
  char pci[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  struct t_pci_dpdk *prev;
  struct t_pci_dpdk *next;
} t_pci_dpdk;

enum {
  state_min=100,
  state_wait_del_port,
  state_wait_vfio_attach,
  state_wait_add_port,
  state_max,
};

static t_pci_dpdk *g_head_pci_dpdk;

/****************************************************************************/
static t_pci_dpdk *alloc_pci_dpdk(char *pci)
{
  t_pci_dpdk *cur = (t_pci_dpdk *) malloc(sizeof(t_pci_dpdk));
  memset(cur, 0, sizeof(t_pci_dpdk));
  strncpy(cur->pci, pci, IFNAMSIZ-1);
  if (g_head_pci_dpdk)
    g_head_pci_dpdk->prev = cur;
  cur->next = g_head_pci_dpdk;
  g_head_pci_dpdk = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_pci_dpdk(t_pci_dpdk *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_pci_dpdk)
    g_head_pci_dpdk = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_pci_dpdk *find_pci_dpdk(char *pci)
{
  t_pci_dpdk *cur = g_head_pci_dpdk;
  while (cur)
    {
    if (!strcmp(pci, cur->pci))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fail_resp_req(t_pci_dpdk *cur)
{
  KERR("TIMEOUT FAIL %s %s", cur->lan, cur->pci);
  if (msg_exist_channel(cur->llid))
    send_status_ko(cur->llid, cur->tid, "KO");
  else
    KERR("%s %s", cur->lan, cur->pci);
  phy_evt_end_eth_type_dpdk(cur->lan, 0);
  free_pci_dpdk(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_pci(void *data)
{
  t_pci_dpdk *next, *cur = g_head_pci_dpdk;
  while (cur)
    {
    next = cur->next;
    cur->count += 1;
    if (cur->count == 10)
      fail_resp_req(cur);
    cur = next;
    }
  clownix_timeout_add(20, timer_pci, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pci_dpdk_ack_add(int tid, int is_ok, char *lan, char *pci)
{
  t_pci_dpdk *cur = find_pci_dpdk(pci);
  if (!cur)
    {
    KERR("%s %s", lan, pci);
    }
  else if (is_ok == 0)
    {
    KERR("%s %s", lan, pci);
    if (msg_exist_channel(cur->llid))
      send_status_ko(cur->llid, cur->tid, "KO");
    else
      KERR("%s %s", lan, pci);
    free_pci_dpdk(cur);
    }
  else
    {
    if (tid != cur->ovs_tid)
      KERR("%s %s %d %d", pci, lan, tid, cur->ovs_tid);
    if (cur->state != state_wait_add_port)
      KERR("%s %s %d", pci, lan, cur->state);
    send_status_ok(cur->llid, cur->tid, "OK");
    free_pci_dpdk(cur);
    phy_evt_end_eth_type_dpdk(lan, 1);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pci_dpdk_ack_del(int tid, int is_ok, char *lan, char *pci)
{
  t_pci_dpdk *cur = find_pci_dpdk(pci);
  if (!cur)
    {
    KERR("%s %s", pci, lan);
    }
  else if (is_ok == 0)
    {
    KERR("%s %s", pci, lan);
    if (msg_exist_channel(cur->llid))
      send_status_ko(cur->llid, cur->tid, "KO");
    else
      KERR("%s %s", pci, lan);
    free_pci_dpdk(cur);
    }
  else
    {
    if (tid != cur->ovs_tid)
      KERR("%s %s %d %d", pci, lan, tid, cur->ovs_tid);
    if (cur->state != state_wait_del_port)
      KERR("%s %s %d", pci, lan, cur->state);
    else
      {
      if (msg_exist_channel(cur->llid))
        send_status_ok(cur->llid, cur->tid, "OK");
      }
    free_pci_dpdk(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pci_dpdk_ack_vfio_attach(int is_ok, char *pci)
{
  t_pci_dpdk *cur = find_pci_dpdk(pci);
  int ovs_tid = utils_get_next_tid();
  if (!cur)
    KERR("%s", pci);
  else
    {
    if (cur->state != state_wait_vfio_attach)
      KERR("%s %s %d %d", pci, cur->lan, cur->state, state_wait_vfio_attach);
    if (is_ok)
      {
      cur->ovs_tid = ovs_tid;
      cur->state = state_wait_add_port;
      if (fmt_tx_add_lan_pci_dpdk(ovs_tid, cur->lan, pci))
        KERR("%s %s", pci, cur->lan);
      }
    else
      KERR("Bad driver vfio-pci attach %s %s", pci, cur->lan);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int pci_dpdk_add(int llid, int tid, char *lan, char *pci)
{
  int result = -1;
  int ovs_tid = utils_get_next_tid();
  t_pci_dpdk *cur = find_pci_dpdk(pci);
  char *drv;
  if (cur)
    KERR("%s %s", pci, lan);
  else if (!dpdk_dyn_lan_exists(lan))
    KERR("%s %s", pci, lan);
  else
    {
    cur = alloc_pci_dpdk(pci);
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    strncpy(cur->pci, pci, MAX_NAME_LEN-1);
    drv = suid_power_get_drv(pci);
    if (strcmp(drv, "vfio-pci"))
      {
      if (suid_power_req_vfio_attach(pci))
        KERR("%s %s", lan, pci);
      cur->state = state_wait_vfio_attach;
      }
    else
      {
      cur->ovs_tid = ovs_tid;
      cur->state = state_wait_add_port;
      if (fmt_tx_add_lan_pci_dpdk(ovs_tid, lan, pci))
        KERR("%s %s", pci, lan);
      }
    cur->llid = llid;
    cur->tid = tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int pci_dpdk_del(char *lan, char *pci)
{
  int ovs_tid, result = -1;
  t_pci_dpdk *cur;
  ovs_tid = utils_get_next_tid();
  if (fmt_tx_del_lan_pci_dpdk(ovs_tid, lan, pci))
    {
    KERR("%s %s", pci, lan);
    }
  else
    {
    cur = alloc_pci_dpdk(pci);    
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
    cur->state = state_wait_del_port;
    cur->ovs_tid = ovs_tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pci_dpdk_init(void)
{
  g_head_pci_dpdk = NULL;
  clownix_timeout_add(200, timer_pci, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
