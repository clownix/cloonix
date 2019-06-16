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
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "pid_clone.h"
#include "dpdk_ovs.h"
#include "dpdk_dyn.h"
#include "dpdk_msg.h"
#include "dpdk_fmt.h"
#include "dpdk_tap.h"

enum {
     ovsreq_add_eth = 15,
     ovsreq_del_eth,
     ovsreq_add_lan_eth,
     ovsreq_del_lan_eth,
     ovsreq_add_lan_tap,
     ovsreq_del_lan_tap,
     };

typedef struct t_ovsreq
{
  int tid;
  int type;
  int time_count;
  char lan_name[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int  num;
  struct t_ovsreq *prev;
  struct t_ovsreq *next;
} t_ovsreq;

typedef struct t_ovslan
{
  char lan_name[MAX_NAME_LEN];
  int spy;
  int refcount;
  struct t_ovslan *prev;
  struct t_ovslan *next;
} t_ovslan;


#define MAX_SPY_IDX 100

static int pool_fifo_free_idx[MAX_SPY_IDX];
static int pool_read_idx;
static int pool_write_idx;

static int g_tid;
static t_ovsreq *g_head_ovsreq;
static t_ovslan *g_head_ovslan;


/****************************************************************************/
static void pool_init(void)
{
  int i;
  for(i = 0; i < MAX_SPY_IDX; i++)
    pool_fifo_free_idx[i] = i+100;
  pool_read_idx = 0;
  pool_write_idx = MAX_SPY_IDX - 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int pool_alloc(void)
{
  int idx = 0;
  if(pool_read_idx != pool_write_idx)
    {
    idx = pool_fifo_free_idx[pool_read_idx];
    pool_read_idx = (pool_read_idx + 1)%MAX_SPY_IDX;
    }
  return idx;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void pool_free(int idx)
{
  pool_fifo_free_idx[pool_write_idx] =  idx;
  pool_write_idx=(pool_write_idx + 1)%MAX_SPY_IDX;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int get_next_tid(void)
{
  g_tid += 1;
  if (g_tid == 0xFFFF)
    g_tid = 1;
  return g_tid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovslan *lan_find(char *lan_name)
{
  t_ovslan *cur = g_head_ovslan;
  while(cur)
    {
    if (!strcmp(cur->lan_name, lan_name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void lan_refcount_inc(char *lan_name)
{
  t_ovslan *cur = lan_find(lan_name);
  if (!cur)
    KERR("%s", lan_name);
  else
    cur->refcount += 1; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int lan_refcount_dec(char *lan_name)
{
  t_ovslan *cur = lan_find(lan_name);
  if (!cur)
    KERR("%s", lan_name);
  if (cur->refcount <= 0)
    KERR("%s %d", lan_name, cur->refcount);
  else
    cur->refcount -= 1; 
  return (cur->refcount);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void lan_alloc(char *lan_name, int spy)
{
  t_ovslan *cur = (t_ovslan *) clownix_malloc(sizeof(t_ovslan), 18);
  memset(cur, 0, sizeof(t_ovslan));
  strncpy(cur->lan_name, lan_name, MAX_NAME_LEN-1);
  cur->spy = spy;
  if (g_head_ovslan)
    g_head_ovslan->prev = cur;
  cur->next = g_head_ovslan;
  g_head_ovslan = cur;
KERR("ALLOC: %s %d", lan_name, spy);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void lan_free(char *lan_name)
{
  t_ovslan *cur = lan_find(lan_name);
  if (!cur)
    KERR("%s", lan_name);
  else
    {
    if (cur->refcount != 0)
      KERR("NOT FREEABLE %s %d", lan_name, cur->refcount);
    else
      {
KERR("FREE: %s %d", lan_name, cur->spy);
      pool_free(cur->spy);
      if (cur->next)
        cur->next->prev = cur->prev;
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur == g_head_ovslan)
        g_head_ovslan = cur->next;
      clownix_free(cur, __FUNCTION__);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovsreq *ovsreq_find(int tid)
{
  t_ovsreq *cur = g_head_ovsreq;
  while(cur)
    {
    if (cur->tid == tid)
      break;
    cur = cur->next;
    }
  return cur;
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ovsreq *ovsreq_alloc(int tid,int type,char *lan,char *name,int num)
{
  t_ovsreq *cur = (t_ovsreq *) clownix_malloc(sizeof(t_ovsreq), 19);
  memset(cur, 0, sizeof(t_ovsreq));
  cur->tid = tid;
  cur->type = type;
  strncpy(cur->lan_name, lan, MAX_NAME_LEN-1);
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->num = num;
  if (g_head_ovsreq)
    g_head_ovsreq->prev = cur;
  cur->next = g_head_ovsreq;
  g_head_ovsreq = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void ovsreq_free(int tid)
{
  t_ovsreq *cur = ovsreq_find(tid);
  if (!cur)
    KERR("%d", tid);
  else
    {
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur == g_head_ovsreq)
      g_head_ovsreq = cur->next;
    clownix_free(cur, __FUNCTION__);
    }
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_add_eth(t_vm *kvm, char *name, int num)
{
  int result, tid = get_next_tid();
  result = dpdk_fmt_tx_add_eth(tid, kvm, name, num);
  if (result)
    KERR("%s %d", name, num);
  else
    ovsreq_alloc(tid, ovsreq_add_eth, "none", name, num);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_msg_send_del_eth(char *name, int num)
{
  int tid = get_next_tid();
  if (dpdk_fmt_tx_del_eth(tid, name, num))
    KERR("%s %d", name, num);
  else
    ovsreq_alloc(tid, ovsreq_del_eth, "none", name, num);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delay_add_lan_endp(void *data)
{
  t_ovsreq *req = (t_ovsreq *) data;
  t_ovslan *lan;
  lan = lan_find(req->lan_name);
  if (lan && (lan->refcount > 0))
    {
    if (req->type == ovsreq_add_lan_eth)
      {
      if (dpdk_fmt_tx_add_lan_eth(req->tid, req->lan_name, req->name, req->num))
        {
        KERR("%s %s %d", req->lan_name, req->name, req->num);
        dpdk_dyn_ack_add_lan_eth_KO(req->lan_name,req->name,req->num,"delayed");
        ovsreq_free(req->tid);
        }
      }
    else if (req->type == ovsreq_add_lan_tap)
      {
      if (dpdk_fmt_tx_add_lan_tap(req->tid, req->lan_name, req->name))
        {
        KERR("%s %s", req->lan_name, req->name);
        dpdk_tap_resp_add_lan(1, req->lan_name, req->name);
        ovsreq_free(req->tid);
        }
      }
    else
      KERR("%s %s %d %d", req->lan_name, req->name, req->num, req->type);
    }
  else
    {
    KERR("DELAY %s %s %d %d", req->lan_name, req->name, req->num, req->type);
    if (req->type == ovsreq_add_lan_eth)
      {
      dpdk_dyn_ack_add_lan_eth_KO(req->lan_name,req->name,req->num,"PB delayed");
      ovsreq_free(req->tid);
      }
    else if (req->type == ovsreq_add_lan_tap)
      {
      dpdk_tap_resp_add_lan(1, req->lan_name, req->name);
      ovsreq_free(req->tid);
      }
    else
      KERR("%d", req->type);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_add_lan_tap(char *lan_name, char *name)
{ 
  int spy, result, tid = get_next_tid();
  t_ovslan *cur = lan_find(lan_name);
  t_ovsreq *ovsreq;
  if (cur == NULL)
    {
    spy = pool_alloc();
    result = dpdk_fmt_tx_add_lan(tid, lan_name, spy);
    if (result)
      {
      pool_free(spy);
      KERR("%s %s", lan_name, name);
      }
    else
      {
      lan_alloc(lan_name, spy);
      ovsreq_alloc(tid, ovsreq_add_lan_tap, lan_name, name, 0);
      }
    }
  else if (cur->refcount > 0) 
    {
    result = dpdk_fmt_tx_add_lan_tap(tid, lan_name, name);
    if (result)
      KERR("%s %s", lan_name, name);
    else
      ovsreq_alloc(tid, ovsreq_add_lan_tap, lan_name, name, 0);
    }
  else
    {
    result = 0;
    ovsreq = ovsreq_alloc(tid, ovsreq_add_lan_tap, lan_name, name, 0);
    clownix_timeout_add(10, delay_add_lan_endp,(void *) ovsreq, NULL, NULL);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_del_lan_tap(char *lan_name, char *name)
{
  int result, tid = get_next_tid();
  if (!lan_find(lan_name))
    KERR("%s %s", lan_name, name);
  result = dpdk_fmt_tx_del_lan_tap(tid, lan_name, name);
  if (result)
    KERR("%s %s", lan_name, name);
  else
    ovsreq_alloc(tid, ovsreq_del_lan_tap, lan_name, name, 0);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_add_lan_eth(char *lan_name, char *name, int num)
{
  int spy, result, tid = get_next_tid();
  t_ovslan *cur = lan_find(lan_name);
  t_ovsreq *ovsreq;
  if (cur == NULL)
    {
    spy = pool_alloc();
    result = dpdk_fmt_tx_add_lan(tid, lan_name, spy);
    if (result)
      {
      pool_free(spy);
      KERR("%s %s %d", lan_name, name, num);
      }
    else
      {
      lan_alloc(lan_name, spy);
      ovsreq_alloc(tid, ovsreq_add_lan_eth, lan_name, name, num);
      }
    }
  else if (cur->refcount > 0) 
    {
    result = dpdk_fmt_tx_add_lan_eth(tid, lan_name, name, num);
    if (result)
      KERR("%s %s %d", lan_name, name, num);
    else
      ovsreq_alloc(tid, ovsreq_add_lan_eth, lan_name, name, num);
    }
  else
    {
    result = 0;
    ovsreq = ovsreq_alloc(tid, ovsreq_add_lan_eth, lan_name, name, num);
    clownix_timeout_add(10, delay_add_lan_endp,(void *) ovsreq, NULL, NULL);
    }
  return result;
}

/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_del_lan_eth(char *lan_name, char *name, int num)
{
  int result, tid = get_next_tid();
  if (!lan_find(lan_name))
    KERR("%s %s %d", lan_name, name, num);
  result = dpdk_fmt_tx_del_lan_eth(tid, lan_name, name, num);
  if (result)
    KERR("%s %s %d", lan_name, name, num);
  else
    ovsreq_alloc(tid, ovsreq_del_lan_eth, lan_name, name, num);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_msg_ack_eth(int tid, char *name, int num,
                      int is_add, int is_ko,  char *lab)
{
  t_ovsreq *cur = ovsreq_find(tid);
  if (!cur)
    {
    KERR("%d %s %d is_add:%d is_ko:%d", tid, name, num, is_add, is_ko);
    }
  else
    {
    if (strcmp(name, cur->name))
      KERR("%s %s is_add:%d is_ko:%d", name, cur->name, is_add, is_ko);
    else if (num != cur->num)
      KERR("%d %d is_add:%d is_ko:%d", num, cur->num, is_add, is_ko);
    else if (is_add)
      {
      if (cur->type != ovsreq_add_eth)
        KERR("%s %d %d", cur->name, cur->num, cur->type);
      if (is_ko)
        dpdk_dyn_ack_add_eth_KO(name, num, lab);
      else
        dpdk_dyn_ack_add_eth_OK(name, num, lab);
      ovsreq_free(tid);
      }
    else
      {
      if (cur->type != ovsreq_del_eth)
        KERR("%s %d %d", cur->name, cur->num, cur->type);
      if (is_ko)
        KERR("%s %d %d", cur->name, cur->num, cur->type);
      ovsreq_free(tid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_msg_ack_lan_endp(int tid, char *lan_name, char *name, int num,
                           int is_add, int is_ko, char *lab)
{
  int ntid;
  t_ovsreq *cur = ovsreq_find(tid);
  t_ovslan *lan = lan_find(lan_name);
  int spy = 0;
  if (lan)
    spy = lan->spy;
  if (!cur)
    {
    KERR("%d %s %s %d is_add:%d is_ko:%d",tid,lan_name,name,num,is_add,is_ko);
    }
  else
    {
    if (strcmp(lan_name, cur->lan_name))
      KERR("%s %s is_add:%d is_ko:%d", lan_name, cur->lan_name, is_add, is_ko);
    else if (strcmp(name, cur->name))
      KERR("%s %s is_add:%d is_ko:%d", name, cur->name, is_add, is_ko);
    else if (num != cur->num)
      KERR("%d %d is_add:%d is_ko:%d", num, cur->num, is_add, is_ko);
    else if (is_add)
      {
      if (!lan_find(cur->lan_name))
        KERR("%s %s %d", cur->lan_name, cur->name, cur->num);
      if (is_ko == 0)
        lan_refcount_inc(cur->lan_name);
      if (cur->type == ovsreq_add_lan_eth)
        {
        if (is_ko)
          dpdk_dyn_ack_add_lan_eth_KO(lan_name, name, num, lab);
        else
          dpdk_dyn_ack_add_lan_eth_OK(lan_name, name, num, lab);
        }
      else if (cur->type == ovsreq_add_lan_tap)
        {
        dpdk_tap_resp_add_lan(is_ko, lan_name, name);
        }
      else
        KERR("%s %s %d %d",cur->lan_name,cur->name,cur->num,cur->type);
      ovsreq_free(tid);
      }
    else
      {
      if (lan_refcount_dec(lan_name) == 1)
        {
        ntid = get_next_tid();
        ovsreq_alloc(ntid, cur->type, cur->lan_name, cur->name, cur->num);
        if (dpdk_fmt_tx_del_lan(ntid, lan_name, spy))
          {
          KERR("%s %d", lan_name, spy);
          if (cur->type == ovsreq_del_lan_eth)
            dpdk_dyn_ack_del_lan_eth_KO(lan_name, name, num, "badlandel");
          else if (cur->type == ovsreq_del_lan_tap)
            dpdk_tap_resp_del_lan(1, lan_name, name);
          else
            KERR("%s %s %d %d",cur->lan_name,cur->name,cur->num,cur->type);
          ovsreq_free(ntid);
          lan_refcount_dec(lan_name);
          }
        }
      else
        {
        if (cur->type == ovsreq_del_lan_eth)
          {
          if (is_ko)
            dpdk_dyn_ack_del_lan_eth_KO(lan_name, name, num, lab);
          else
            dpdk_dyn_ack_del_lan_eth_OK(lan_name, name, num, lab);
          }
        else if (cur->type == ovsreq_del_lan_tap)
          dpdk_tap_resp_del_lan(is_ko, lan_name, name);
        else
          KERR("%s %s %d %d",cur->lan_name,cur->name,cur->num,cur->type);
        }
      ovsreq_free(tid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_msg_ack_lan(int tid, char *lan_name,
                      int is_add, int is_ko, char *lab)
{
  int ntid;
  t_ovsreq *ovsreq, *cur = ovsreq_find(tid);
  if (!cur)
    {
    KERR("%d %s is_add:%d is_ko:%d", tid, lan_name, is_add, is_ko);
    }
  else
    {
    if (strcmp(lan_name, cur->lan_name))
      KERR("%s %s is_add:%d is_ko:%d", lan_name, cur->lan_name,is_add,is_ko);
    if (is_add)
      {
      if (is_ko)
        {
        KERR("%s %s %d", cur->lan_name, cur->name, cur->num);
        if (cur->type == ovsreq_add_lan_eth)
          dpdk_dyn_ack_add_lan_eth_KO(lan_name, cur->name, cur->num, lab);
        else if (cur->type == ovsreq_add_lan_tap)
          dpdk_tap_resp_add_lan(is_ko, lan_name, cur->name);
        else
          KERR("%s %s %d %d",cur->lan_name,cur->name,cur->num,cur->type);
        ovsreq_free(tid);
        }
      else
        {
        lan_refcount_inc(cur->lan_name);
        ntid = get_next_tid();
        ovsreq=ovsreq_alloc(ntid,cur->type,cur->lan_name,cur->name,cur->num);
        ovsreq_free(tid);
        clownix_timeout_add(10,delay_add_lan_endp,(void *)ovsreq,NULL,NULL);
        }
      }
    else
      {
      lan_refcount_dec(cur->lan_name);
      lan_free(cur->lan_name);
      if (cur->type == ovsreq_del_lan_eth)
        {
        if (is_ko)
          dpdk_dyn_ack_del_lan_eth_KO(lan_name, cur->name, cur->num, lab);
        else
          dpdk_dyn_ack_del_lan_eth_OK(lan_name, cur->name, cur->num, lab);
        }
      else if (cur->type == ovsreq_add_lan_tap)
        dpdk_tap_resp_del_lan(is_ko, lan_name, cur->name);
      else
        KERR("%s %s %d %d",cur->lan_name,cur->name,cur->num,cur->type);
      ovsreq_free(tid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_msg_beat(void *data)
{
  t_ovsreq *next, *cur = g_head_ovsreq;
  while(cur)
    {
    next = cur->next;
    cur->time_count += 1;
    if (cur->time_count > 6)
      {
      switch(cur->type)
        {

        case ovsreq_add_lan_tap:
          KERR("%d %s %s", cur->tid, cur->lan_name, cur->name);
          dpdk_tap_resp_add_lan(1, cur->lan_name, cur->name);
          ovsreq_free(cur->tid);
        break;
  
        case ovsreq_del_lan_tap:
          KERR("%d %s %s", cur->tid, cur->lan_name, cur->name);
          dpdk_tap_resp_del_lan(1, cur->lan_name, cur->name);
          ovsreq_free(cur->tid);
        break;

        case ovsreq_add_eth:
          KERR("%d %s %d", cur->tid, cur->name, cur->num);
          dpdk_dyn_ack_add_eth_KO(cur->name, cur->num, "timeout");
          ovsreq_free(cur->tid);
        break;

        case ovsreq_del_eth:
          KERR("%d %s %d", cur->tid, cur->name, cur->num);
          ovsreq_free(cur->tid);
        break;

        case ovsreq_add_lan_eth:
          KERR("%d %s %s %d", cur->tid, cur->lan_name, cur->name, cur->num);
          dpdk_dyn_ack_add_lan_eth_KO(cur->lan_name, cur->name,
                                      cur->num, "timeout");
          ovsreq_free(cur->tid);
        break;

        case ovsreq_del_lan_eth:
          KERR("%d %s %s %d", cur->tid, cur->lan_name, cur->name, cur->num);
          dpdk_dyn_ack_del_lan_eth_KO(cur->lan_name, cur->name,
                                      cur->num, "timeout");
          ovsreq_free(cur->tid);
        break;

        default:
          KOUT("%d", cur->type);
        }
      }
    cur = next;
    }
  clownix_timeout_add(50, timer_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_msg_init(void)
{
  g_tid = 0;
  g_head_ovsreq = NULL;
  g_head_ovslan = NULL;
  pool_init();
  clownix_timeout_add(50, timer_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

