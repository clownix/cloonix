/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>


#include "ioc.h"
#include "cirspy.h"

/****************************************************************************/
typedef struct t_evt_eth
{
  int idx;
  char name[MAX_NAME_LEN];
  int vm_id;
  int eth;
  t_eth_params eth_params;
  int pkt_tx;
  int bytes_tx;
  int pkt_rx;
  int bytes_rx;
  struct t_evt_eth *prev;
  struct t_evt_eth *next;
  struct t_evt_eth *lanprev;
  struct t_evt_eth *lannext;
} t_evt_eth;
/*--------------------------------------------------------------------------*/
typedef struct t_evt_vm
{
  char name[MAX_NAME_LEN];
  int vm_id;
  int num;
  t_evt_eth *head_evt_eth;
  struct t_evt_vm *prev;
  struct t_evt_vm *next;
} t_evt_vm;  
/*--------------------------------------------------------------------------*/
typedef struct t_evt_lan
{
  int idx;
  char lan[MAX_NAME_LEN];
  struct t_evt_eth *head_lan_eth;
  struct t_evt_lan *prev;
  struct t_evt_lan *next;
} t_evt_lan;

/*---------------------------------------------------------------------------*/
static t_evt_lan *g_lan_head_tab[CIRC_MAX_TAB + 1];
static t_evt_vm *g_vm_head;
static t_evt_lan *g_lan_head;
static uint32_t volatile g_sync_lock;


/****************************************************************************/
static t_evt_lan *find_lan(char *lan)
{
  t_evt_lan *cur = g_lan_head;
  while(cur)
    {
    if (!strcmp(cur->lan, lan))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_evt_vm *find_evt_vm(char *name)
{
  t_evt_vm *cur = g_vm_head;
  while(cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_evt_eth *find_evt_eth(t_evt_vm *vme, int eth)
{
  t_evt_eth *cur = vme->head_evt_eth;
  while(cur)
    {
    if (cur->eth == eth)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void mutex_lock(void)
{
   while (__sync_lock_test_and_set(&g_sync_lock, 1));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void mutex_unlock(void)
{
  __sync_lock_release(&g_sync_lock);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void mutex_init(void)
{
  g_sync_lock = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_evt_eth *get_eth_with_mac(t_evt_lan *lane, char *mac)
{
  t_evt_eth *cur = lane->head_lan_eth;
  char *mc;
  while (cur)
    {
    mc = cur->eth_params.mac_addr;
    if (!memcmp(mc, mac, 6))
      {
      break;
      }
    cur = cur->lannext;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_stats(t_evt_lan *lane, int flag_allcast, 
                         t_evt_eth *src, t_evt_eth *dst, int len)
{
  t_evt_eth *cur = lane->head_lan_eth;
  src->pkt_tx += 1;
  src->bytes_tx += len;
  if (flag_allcast)
    {
    while (cur)
      {
      if (cur != src)
        {
        cur->pkt_rx += 1;
        cur->bytes_rx += len;
        } 
      cur = cur->next;
      }
    }
  else
    {
    dst->pkt_rx += 1;
    dst->bytes_rx += len;
    } 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_chain_eth(t_evt_vm *vme, char *name, int num, int vm_id,
                             t_eth_params eth_params[MAX_DPDK_VM])
{
  int i;
  t_evt_eth *cur;
  for (i=0; i<num; i++)
    {
    cur = (t_evt_eth *) malloc(sizeof(t_evt_eth));
    memset(cur, 0, sizeof(t_evt_eth));
    memcpy(cur->name, name, MAX_NAME_LEN-1);
    cur->vm_id = vm_id;
    cur->eth = i;
    memcpy(&(cur->eth_params), &(eth_params[i]), sizeof(t_eth_params)); 
    if (vme->head_evt_eth)
      vme->head_evt_eth->prev = cur;
    cur->next = vme->head_evt_eth;
    vme->head_evt_eth = cur;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void unchain_eth_lan(t_evt_lan *lane, t_evt_eth *cur)
{
  if (cur->lanprev)
    cur->lanprev->lannext = cur->lannext;
  if (cur->lannext)
    cur->lannext->lanprev = cur->lanprev;
  if (lane->head_lan_eth==cur)
    lane->head_lan_eth = cur->lannext;
  cur->idx = 0;
  cur->lanprev = NULL;
  cur->lannext = NULL;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delete_chain_eth(t_evt_vm *vme)
{
  int i;
  t_evt_eth *cur;
  t_evt_lan *lane;
  for (i=0; i<vme->num; i++)
    {
    cur = vme->head_evt_eth;
    if (cur->idx != 0)
      {
      lane = g_lan_head_tab[cur->idx];
      if ((lane == NULL) || (lane->head_lan_eth == NULL))
        KERR(" ");
      else
        unchain_eth_lan(lane, cur);
      }
    if (cur->prev) 
      cur->prev->next = cur->next;
    if (cur->next) 
      cur->next->prev = cur->prev;
    if (cur==vme->head_evt_eth)
      vme->head_evt_eth = cur->next;
    }
  if (vme->head_evt_eth != NULL)
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_vm_add(char *name, int num, int vm_id,
                      t_eth_params eth_params[MAX_DPDK_VM])
{
  t_evt_vm *cur = find_evt_vm(name);
  if (cur)
    KERR("%s", name);
  else
    {
    cur = malloc(sizeof(t_evt_vm));
    memset(cur, 0, sizeof(t_evt_vm));
    memcpy(cur->name, name, MAX_NAME_LEN-1);
    cur->num = num;
    cur->vm_id = vm_id;
    create_chain_eth(cur, name, num, vm_id, eth_params);
    mutex_lock();
    if (g_vm_head) 
      g_vm_head->prev = cur;
    cur->next = g_vm_head;
    g_vm_head = cur;
    mutex_unlock();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_vm_del(char *name)
{
  t_evt_vm *cur = find_evt_vm(name);
  if (!cur)
    KERR("%s", name);
  else
    {
    mutex_lock();
    delete_chain_eth(cur);
    if (cur->prev) 
      cur->prev->next = cur->next;
    if (cur->next) 
      cur->next->prev = cur->prev;
    if (cur==g_vm_head) 
      g_vm_head = cur->next;
    mutex_unlock();
    free(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int eventfull_lan_add_eth(char *lan, char *name, int num)
{
  int result = -1;
  t_evt_lan *lane = find_lan(lan);
  t_evt_vm *vme = find_evt_vm(name);
  t_evt_eth *ethe;
  if (!lane)
    KERR("%s %s %d", lan, name, num);
  else if ((lane->idx <= 0) || (lane->idx > CIRC_MAX_TAB))
    KERR("ERROR idx=%d", lane->idx);
  else if (!vme)
    KERR("%s %s %d", lan, name, num);
  else
    {
    ethe = find_evt_eth(vme, num);
    if ((ethe) && (ethe->idx == 0) && 
        (ethe->lanprev == NULL) &&
        (ethe->lannext == NULL))
      {
      mutex_lock();
      ethe->idx = lane->idx;
      if (lane->head_lan_eth)
        lane->head_lan_eth->lanprev = ethe;
      ethe->lannext = lane->head_lan_eth;
      lane->head_lan_eth = ethe;
      mutex_unlock();
      result = 0;
      } 
    else
      {
      if (ethe == NULL)
        KERR("%s %s %d", lan, name, num);
      else
        KERR("%s %s %d %d %p %p", lan, name, num, ethe->idx,
                                  ethe->lanprev, ethe->lannext);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int eventfull_lan_del_eth(char *lan, char *name, int num)
{
  int result = -1;
  t_evt_lan *lane = find_lan(lan);
  t_evt_vm *vme = find_evt_vm(name);
  t_evt_eth *ethe;
  if (!lane)
    KERR("%s", lan);
  else if ((lane->idx <= 0) || (lane->idx > CIRC_MAX_TAB))
    KERR("ERROR idx=%d", lane->idx);
  else if (!vme)
    KERR("%s", name);
  else
    {
    ethe = find_evt_eth(vme, num);
    if ((ethe) && (ethe->idx == lane->idx))
      {
      mutex_lock();
      unchain_eth_lan(lane, ethe);
      result = 0;
      mutex_unlock();
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_lan_open(int idx, char *lan)
{
  t_evt_lan *lane = g_lan_head_tab[idx];
  if (lane)
    KERR("%d %s", idx, lan);
  else
    {
    lane = (t_evt_lan *) malloc(sizeof(t_evt_lan));
    memset(lane, 0, sizeof(t_evt_lan));
    lane->idx = idx;
    memcpy(lane->lan, lan, MAX_NAME_LEN - 1);  
    mutex_lock();
    g_lan_head_tab[idx] = lane;
    if (g_lan_head)
        g_lan_head->prev = lane;
    lane->next = g_lan_head;
    g_lan_head = lane;
    mutex_unlock();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_lan_close(int idx)
{
  t_evt_lan *lane = g_lan_head_tab[idx];
  t_evt_eth *cur, *next;
  if (!lane)
    KERR("%d", idx);
  else
    {
    mutex_lock();
    cur = lane->head_lan_eth;
    while(cur)
      {
      next = cur->lannext;
      unchain_eth_lan(lane, cur);
      cur = next;
      }
    if (lane->head_lan_eth)
      KOUT(" ");
    g_lan_head_tab[idx] = NULL;
    if (lane->prev)
      lane->prev->next = lane->next;
    if (lane->next)
      lane->next->prev = lane->prev;
    if (lane==g_lan_head)
      g_lan_head = lane->next;
    free(lane);
    mutex_unlock();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_collect_send(t_all_ctx *all_ctx, int cloonix_llid)
{
  t_evt_vm *curvm = g_vm_head;
  t_evt_eth *cureth;
  int tidx;
  unsigned int ms = (unsigned int) cloonix_get_msec();
  char txt[2*MAX_NAME_LEN];
  while(curvm)
    {
    cureth = curvm->head_evt_eth;
    while(cureth)
      {
      tidx = MAX_VM * cureth->vm_id + cureth->eth;
      memset(txt, 0, 2*MAX_NAME_LEN);
      snprintf(txt, (2*MAX_NAME_LEN) - 1,
               "endp_eventfull_tx_rx %u %d %d %d %d %d",
               ms, tidx, cureth->pkt_tx, cureth->bytes_tx,
               cureth->pkt_rx, cureth->bytes_rx);
      cureth->pkt_tx = 0;
      cureth->bytes_tx = 0;
      cureth->pkt_rx = 0;
      cureth->bytes_rx = 0;
      rpct_send_evt_msg(all_ctx, cloonix_llid, 0, txt);
      cureth = cureth->next;
      }
    curvm = curvm->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_hook_spy(int idx, int len, char *buf)
{
  int flag_allcast = buf[0] & 0x01;
  t_evt_eth *src, *dst = NULL;
  t_evt_lan *lane;
  mutex_lock();
  lane = g_lan_head_tab[idx];
  if (lane)
    {
    if (!flag_allcast)
      {
      dst = get_eth_with_mac(lane, &(buf[0]));
      }
    src = get_eth_with_mac(lane, &(buf[6]));
    if ((src != NULL) && ( (dst != NULL) || flag_allcast != 0)) 
      update_stats(lane, flag_allcast, src, dst, len);
    }
  mutex_unlock();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void eventfull_init(void)
{
  g_vm_head = NULL;
  mutex_init();
  memset(g_lan_head_tab, 0, (CIRC_MAX_TAB + 1) * sizeof(t_evt_lan *));
}
/*--------------------------------------------------------------------------*/

