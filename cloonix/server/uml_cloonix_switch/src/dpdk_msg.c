/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include "dpdk_msg.h"
#include "fmt_diag.h"
#include "dpdk_nat.h"
#include "dpdk_tap.h"
#include "dpdk_kvm.h"
#include "dpdk_phy.h"
#include "dpdk_a2b.h"
#include "dpdk_d2d.h"
#include "dpdk_xyx.h"
#include "utils_cmd_line_maker.h"

enum {
     ovsreq_add_lan_nat = 15,
     ovsreq_del_lan_nat,
     ovsreq_add_lan_a2b,
     ovsreq_del_lan_a2b,
     ovsreq_add_lan_d2d,
     ovsreq_del_lan_d2d,
     ovsreq_add_lan_ethd,
     ovsreq_add_lan_eths,
     ovsreq_del_lan_ethd,
     ovsreq_del_lan_eths,
     ovsreq_add_lan_tap,
     ovsreq_del_lan_tap,
     ovsreq_add_lan_phy,
     ovsreq_del_lan_phy,
     };

typedef struct t_ovsreq
{
  int tid;
  int type;
  int time_count;
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int  num;
  struct t_ovsreq *prev;
  struct t_ovsreq *next;
} t_ovsreq;

typedef struct t_ovslan
{
  char lan[MAX_NAME_LEN];
  int refcount;
  int must_send_del;
  struct t_ovslan *prev;
  struct t_ovslan *next;
} t_ovslan;


static t_ovsreq *g_head_ovsreq;
static t_ovslan *g_head_ovslan;

/****************************************************************************/
static t_ovslan *lan_find(char *lan)
{
  t_ovslan *cur = g_head_ovslan;
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
static void lan_refcount_inc(char *lan, int line)
{
  t_ovslan *cur = lan_find(lan);
  if (!cur)
    KERR("ERROR %s", lan);
  else
    {
    cur->refcount += 1; 
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int lan_refcount_dec(char *lan, int line)
{
  int result = 0;
  t_ovslan *cur = lan_find(lan);
  if (cur)
    {
    if (cur->refcount <= 0)
      KERR("ERROR %s %d", lan, cur->refcount);
    else
      {
      cur->refcount -= 1; 
      result = cur->refcount;
      }
    }
  return (result);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void lan_alloc(char *lan)
{
  t_ovslan *cur = (t_ovslan *) clownix_malloc(sizeof(t_ovslan), 18);
  memset(cur, 0, sizeof(t_ovslan));
  strncpy(cur->lan, lan, MAX_NAME_LEN-1);
  if (g_head_ovslan)
    g_head_ovslan->prev = cur;
  cur->next = g_head_ovslan;
  g_head_ovslan = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void lan_free_final(char *lan)
{
  t_ovslan *cur = lan_find(lan);
  if (cur)
    {
    if (cur->must_send_del == 0)
      {
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
static void lan_free_try(char *name, int num, char *lan, int type)
{
  t_ovslan *cur = lan_find(lan);
  if (cur)
    {
    if ((!dpdk_nat_lan_exists(lan)) &&
        (!dpdk_xyx_lan_exists(lan)) &&
        (!dpdk_a2b_lan_exists(lan)) &&
        (!dpdk_d2d_lan_exists(lan)))
      {
      lan_free_final(lan);
      }
    if ((dpdk_xyx_lan_exists(lan)) &&
        ((type == ovsreq_del_lan_tap) ||
         (type == ovsreq_del_lan_phy) ||
         (type == ovsreq_del_lan_ethd) ||
         (type == ovsreq_del_lan_eths)))
      {
      if (name == NULL)
        KOUT(" ");
      else
        {
        if (dpdk_xyx_lan_exists_in_xyx(name, 0, lan))
          {
          if (cur->refcount <= 0)
            {
            lan_free_final(lan);
            }
          }
        }
      }
    else if ((dpdk_nat_lan_exists(lan)) &&
        (type == ovsreq_del_lan_nat))
      {
      if (name == NULL)
        KOUT(" ");
      else
        {
        if (dpdk_nat_lan_exists_in_nat(name, lan))
          {
          if (cur->refcount <= 0)
            {
            lan_free_final(lan);
            }
          }
        }
      }
    else if ((dpdk_a2b_lan_exists(lan)) &&
             (type == ovsreq_del_lan_a2b))
      {
      if (name == NULL)
        KOUT(" ");
      else
        {
        if (dpdk_a2b_lan_exists_in_a2b(name, num, lan))
          {
          if (cur->refcount <= 0)
            {
            lan_free_final(lan);
            }
          }
        }
      }
    else if ((dpdk_d2d_lan_exists(lan)) &&
             (type == ovsreq_del_lan_d2d))
      {
      if (name == NULL)
        KOUT(" ");
      else
        {
        if (dpdk_d2d_lan_exists_in_d2d(name, lan, 1))
          {
          if (cur->refcount <= 0)
            {
            lan_free_final(lan);
            }
          }
        }
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
static t_ovsreq *ovsreq_alloc(int tid, int type, char *lan,
                              char *name, int num)
{
  t_ovsreq *cur = (t_ovsreq *) clownix_malloc(sizeof(t_ovsreq), 19);
  memset(cur, 0, sizeof(t_ovsreq));
  cur->tid = tid;
  cur->type = type;
  strncpy(cur->lan, lan, MAX_NAME_LEN-1);
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
    KERR("ERROR %d", tid);
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
static void delay_add_lan_endp(void *data)
{
  t_ovsreq *req = (t_ovsreq *) data;
  t_ovslan *lan;
  int result;
  lan = lan_find(req->lan);
  if (lan && (lan->refcount > 0))
    {
    if (req->type == ovsreq_add_lan_ethd)
      {
      result = fmt_tx_add_lan_ethd(req->tid,req->lan,req->name,req->num);
      if (result)
        {
        KERR("ERROR %s %s %d", req->lan, req->name, req->num);
        dpdk_kvm_resp_add_lan(1, req->lan, req->name, req->num);
        ovsreq_free(req->tid);
        }
      }
    else if (req->type == ovsreq_add_lan_eths)
      {
      result = fmt_tx_add_lan_eths(req->tid,req->lan,req->name,req->num);
      if (result)
        {
        KERR("ERROR %s %s %d", req->lan, req->name, req->num);
        dpdk_kvm_resp_add_lan(1, req->lan, req->name, req->num);
        ovsreq_free(req->tid);
        }
      }
   else if (req->type == ovsreq_add_lan_nat)
      {
      if (fmt_tx_add_lan_nat(req->tid, req->lan, req->name))
        {
        KERR("ERROR %s %s", req->lan, req->name);
        dpdk_nat_resp_add_lan(1, req->lan, req->name);
        ovsreq_free(req->tid);
        }
      }
   else if (req->type == ovsreq_add_lan_d2d)
      {
      if (fmt_tx_add_lan_d2d(req->tid, req->lan, req->name))
        {
        KERR("ERROR %s %s", req->lan, req->name);
        dpdk_d2d_resp_add_lan(1, req->lan, req->name);
        ovsreq_free(req->tid);
        }
      }
    else if (req->type == ovsreq_add_lan_tap)
      {
      if (fmt_tx_add_lan_tap(req->tid,req->lan,req->name))
        {
        KERR("ERROR %s %s", req->lan, req->name);
        dpdk_tap_resp_add_lan(1, req->lan, req->name);
        ovsreq_free(req->tid);
        }
      }
    else if (req->type == ovsreq_add_lan_phy)
      {
      if (fmt_tx_add_lan_phy(req->tid,req->lan,req->name))
        {
        KERR("ERROR %s %s", req->lan, req->name);
        dpdk_phy_resp_add_lan(1, req->lan, req->name);
        ovsreq_free(req->tid);
        }
      }
    else if (req->type == ovsreq_add_lan_a2b)
      {
      if (fmt_tx_add_lan_a2b(req->tid, req->lan, req->name, req->num))
        {
        KERR("ERROR %s %s %d", req->lan, req->name, req->num);
        dpdk_a2b_resp_add_lan(1, req->lan, req->name, req->num);
        ovsreq_free(req->tid);
        }
      }
    else
      KERR("ERROR %s %s %d %d", req->lan, req->name, req->num, req->type);
    }
  else
    {
    if (req->time_count < 5)
      {
      clownix_timeout_add(5, delay_add_lan_endp, data, NULL, NULL);
      }
    else
      {
      KERR("ERROR DELAY %s %s %d %d", req->lan, req->name, req->num, req->type);
      if ((req->type == ovsreq_add_lan_ethd) ||
          (req->type == ovsreq_add_lan_eths))
        dpdk_kvm_resp_add_lan(1, req->lan, req->name, req->num);
      else if (req->type == ovsreq_add_lan_nat)
        dpdk_nat_resp_add_lan(1, req->lan, req->name);
      else if (req->type == ovsreq_add_lan_tap)
        dpdk_tap_resp_add_lan(1, req->lan, req->name);
      else if (req->type == ovsreq_add_lan_phy)
        dpdk_phy_resp_add_lan(1, req->lan, req->name);
      else if (req->type == ovsreq_add_lan_a2b)
        dpdk_a2b_resp_add_lan(1, req->lan, req->name, req->num);
      else if (req->type == ovsreq_add_lan_d2d)
        dpdk_d2d_resp_add_lan(1, req->lan, req->name);
      else
        KERR("ERROR %d", req->type);
      ovsreq_free(req->tid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_add_lan_tap(char *lan, char *name)
{ 
  int result = -1, tid = utils_get_next_tid();
  t_ovslan *cur = lan_find(lan);
  t_ovsreq *ovsreq;
  if (cur == NULL)
    {
    result = fmt_tx_add_lan(tid, lan);
    if (result)
      {
      KERR("ERROR %s %s", lan, name);
      }
    else
      {
      lan_alloc(lan);
      ovsreq_alloc(tid, ovsreq_add_lan_tap, lan, name, 0);
      }
    }
  else if (cur->refcount > 0) 
    {
    result = fmt_tx_add_lan_tap(tid, lan, name);
    if (result)
      KERR("ERROR %s %s", lan, name);
    else
      ovsreq_alloc(tid, ovsreq_add_lan_tap, lan, name, 0);
    }
  else
    {
    result = 0;
    ovsreq=ovsreq_alloc(tid, ovsreq_add_lan_tap, lan, name, 0);
    clownix_timeout_add(5, delay_add_lan_endp,(void *) ovsreq, NULL, NULL);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_add_lan_phy(char *lan, char *name)
{
  int result = -1, tid = utils_get_next_tid();
  t_ovslan *cur = lan_find(lan);
  t_ovsreq *ovsreq;
  if (cur == NULL)
    {
    result = fmt_tx_add_lan(tid, lan);
    if (result)
      {
      KERR("ERROR %s %s", lan, name);
      }
    else
      {
      lan_alloc(lan);
      ovsreq_alloc(tid, ovsreq_add_lan_phy, lan, name, 0);
      }
    }
  else if (cur->refcount > 0)
    {
    result = fmt_tx_add_lan_phy(tid, lan, name);
    if (result)
      KERR("ERROR %s %s", lan, name);
    else
      ovsreq_alloc(tid, ovsreq_add_lan_phy, lan, name, 0);
    }
  else
    {
    result = 0;
    ovsreq=ovsreq_alloc(tid, ovsreq_add_lan_phy, lan, name, 0);
    clownix_timeout_add(5, delay_add_lan_endp,(void *) ovsreq, NULL, NULL);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_add_lan_ethd(char *lan, char *name, int num)
{
  int result = -1, tid = utils_get_next_tid();
  t_ovslan *cur = lan_find(lan);
  t_ovsreq *ovsreq;
  if (cur == NULL)
    {
    result = fmt_tx_add_lan(tid, lan);
    if (result)
      {
      KERR("ERROR %s %s %d", lan, name, num);
      }
    else
      {
      lan_alloc(lan);
      ovsreq_alloc(tid, ovsreq_add_lan_ethd, lan, name, num);
      }
    }
  else if (cur->refcount > 0) 
    {
    result = fmt_tx_add_lan_ethd(tid, lan, name, num);
    if (result)
      KERR("ERROR %s %s %d", lan, name, num);
    else
      ovsreq_alloc(tid, ovsreq_add_lan_ethd, lan, name, num);
    }
  else
    {
    result = 0;
    ovsreq = ovsreq_alloc(tid, ovsreq_add_lan_ethd, lan, name, num);
    clownix_timeout_add(5, delay_add_lan_endp,(void *) ovsreq, NULL, NULL);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_add_lan_eths(char *lan, char *name, int num)
{
  int result = -1, tid = utils_get_next_tid();
  t_ovslan *cur = lan_find(lan);
  t_ovsreq *ovsreq;
  if (cur == NULL)
    {
    result = fmt_tx_add_lan(tid, lan);
    if (result)
      {
      KERR("ERROR %s %s %d", lan, name, num);
      }
    else
      {
      lan_alloc(lan);
      ovsreq_alloc(tid, ovsreq_add_lan_eths, lan, name, num);
      }
    }
  else if (cur->refcount > 0)
    {
    result = fmt_tx_add_lan_eths(tid, lan, name, num);
    if (result)
      KERR("ERROR %s %s %d", lan, name, num);
    else
      ovsreq_alloc(tid, ovsreq_add_lan_eths, lan, name, num);
    }
  else
    {
    result = 0;
    ovsreq = ovsreq_alloc(tid, ovsreq_add_lan_eths, lan, name, num);
    clownix_timeout_add(5, delay_add_lan_endp,(void *) ovsreq, NULL, NULL);
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int dpdk_msg_send_add_lan_nat(char *lan, char *name)
{
  int result = -1, tid = utils_get_next_tid();
  t_ovslan *cur = lan_find(lan);
  t_ovsreq *ovsreq;
  if (cur == NULL)
    {
    result = fmt_tx_add_lan(tid, lan);
    if (result)
      {
      KERR("ERROR %s %s", lan, name);
      }
    else
      {
      lan_alloc(lan);
      ovsreq_alloc(tid, ovsreq_add_lan_nat, lan, name,0);
      }
    }
  else if (cur->refcount > 0)
    {
    result = fmt_tx_add_lan_nat(tid, lan, name);
    if (result)
      KERR("ERROR %s %s", lan, name);
    else
      ovsreq_alloc(tid, ovsreq_add_lan_nat,lan,name,0);
    }
  else
    {
    result = 0;
    ovsreq = ovsreq_alloc(tid,ovsreq_add_lan_nat,lan,name,0);
    clownix_timeout_add(5, delay_add_lan_endp,(void *) ovsreq, NULL, NULL);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_add_lan_a2b(char *lan, char *name, int num)
{
  int result = -1, tid = utils_get_next_tid();
  t_ovslan *cur = lan_find(lan);
  t_ovsreq *ovsreq;
  if (cur == NULL)
    {
    result = fmt_tx_add_lan(tid, lan);
    if (result)
      {
      KERR("ERROR %s %s", lan, name);
      }
    else
      {
      lan_alloc(lan);
      ovsreq_alloc(tid, ovsreq_add_lan_a2b, lan, name, num);
      }
    }
  else if (cur->refcount > 0)
    {
    result = fmt_tx_add_lan_a2b(tid, lan, name, num);
    if (result)
      KERR("ERROR %s %s %d", lan, name, num);
    else
      ovsreq_alloc(tid, ovsreq_add_lan_a2b, lan, name, num);
    }
  else
    {
    result = 0;
    ovsreq = ovsreq_alloc(tid, ovsreq_add_lan_a2b, lan, name, num);
    clownix_timeout_add(5, delay_add_lan_endp,(void *) ovsreq, NULL, NULL);
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int dpdk_msg_send_add_lan_d2d(char *lan, char *name)
{   
  int result = -1, tid = utils_get_next_tid(); 
  t_ovslan *cur = lan_find(lan);
  t_ovsreq *ovsreq;
  if (cur == NULL)
    {
    result = fmt_tx_add_lan(tid, lan);
    if (result)
      {
      KERR("ERROR %s %s", lan, name);
      }
    else
      {
      lan_alloc(lan);
      ovsreq_alloc(tid, ovsreq_add_lan_d2d, lan, name, 0);
      }
    }
  else if (cur->refcount > 0)
    {
    result = fmt_tx_add_lan_d2d(tid, lan, name);
    if (result)
      KERR("ERROR %s %s", lan, name);
    else
      ovsreq_alloc(tid, ovsreq_add_lan_d2d, lan, name, 0);
    }
  else
    {
    result = 0;
    ovsreq = ovsreq_alloc(tid,ovsreq_add_lan_d2d,lan,name,0);
    clownix_timeout_add(5, delay_add_lan_endp,(void *) ovsreq, NULL, NULL);
    }
  return result;
}

/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_del_lan_tap(char *lan, char *name)
{
  int result, tid = utils_get_next_tid();
  if (!lan_find(lan))
    KERR("ERROR %s %s", lan, name);
  result = fmt_tx_del_lan_tap(tid, lan, name);
  if (result)
    KERR("ERROR %s %s", lan, name);
  else
    ovsreq_alloc(tid, ovsreq_del_lan_tap, lan, name, 0);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_del_lan_phy(char *lan, char *name)
{
  int result, tid = utils_get_next_tid();
  if (!lan_find(lan))
    KERR("ERROR %s %s", lan, name);
  result = fmt_tx_del_lan_phy(tid, lan, name);
  if (result)
    KERR("ERROR %s %s", lan, name);
  else
    ovsreq_alloc(tid, ovsreq_del_lan_phy, lan, name, 0);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_del_lan_ethd(char *lan, char *name, int num)
{
  int result, tid = utils_get_next_tid();
  if (!lan_find(lan))
    KERR("ERROR %s %s %d", lan, name, num);
  result = fmt_tx_del_lan_ethd(tid, lan, name, num);
  if (result)
    KERR("ERROR %s %s %d", lan, name, num);
  else
    ovsreq_alloc(tid, ovsreq_del_lan_ethd, lan, name, num);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_del_lan_eths(char *lan, char *name, int num)
{
  int result, tid = utils_get_next_tid();
  if (!lan_find(lan))
    KERR("ERROR %s %s %d", lan, name, num);
  result = fmt_tx_del_lan_eths(tid, lan, name, num);
  if (result)
    KERR("ERROR %s %s %d", lan, name, num);
  else
    ovsreq_alloc(tid, ovsreq_del_lan_eths, lan, name, num);
  return result;
}
/*--------------------------------------------------------------------------*/



/****************************************************************************/
int dpdk_msg_send_del_lan_nat(char *lan, char *name)
{
  int result, tid = utils_get_next_tid();
  if (!lan_find(lan))
    KERR("ERROR %s %s", lan, name);
  result = fmt_tx_del_lan_nat(tid, lan, name);
  if (result)
    KERR("ERROR %s %s", lan, name);
  else
    ovsreq_alloc(tid, ovsreq_del_lan_nat, lan, name, 0);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_del_lan_a2b(char *lan, char *name, int num)
{
  int result, tid = utils_get_next_tid();
  if (!lan_find(lan))
    KERR("ERROR %s %s %d", lan, name, num);
  result = fmt_tx_del_lan_a2b(tid, lan, name, num);
  if (result)
    KERR("ERROR %s %s %d", lan, name, num);
  else
    ovsreq_alloc(tid, ovsreq_del_lan_a2b, lan, name, num);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_msg_send_del_lan_d2d(char *lan, char *name)
{
  int result, tid = utils_get_next_tid();
  if (!lan_find(lan))
    KERR("ERROR %s %s", lan, name);
  result = fmt_tx_del_lan_d2d(tid, lan, name);
  if (result)
    KERR("ERROR %s %s", lan, name);
  else
    ovsreq_alloc(tid, ovsreq_del_lan_d2d, lan, name, 0);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void transmit_add_ack(int tid, t_ovsreq *cur, int is_ko)
{
  if (!lan_find(cur->lan))
    KERR("ERROR %s %s %d", cur->lan, cur->name, cur->num);
  if (is_ko == 0)
    lan_refcount_inc(cur->lan, __LINE__);
  if ((cur->type == ovsreq_add_lan_ethd) ||
      (cur->type == ovsreq_add_lan_eths))
    {
    dpdk_kvm_resp_add_lan(is_ko, cur->lan, cur->name, cur->num);
    }
  else if (cur->type == ovsreq_add_lan_nat)
    {
    dpdk_nat_resp_add_lan(is_ko, cur->lan, cur->name);
    }
  else if (cur->type == ovsreq_add_lan_tap)
    {
    dpdk_tap_resp_add_lan(is_ko, cur->lan, cur->name);
    }
  else if (cur->type == ovsreq_add_lan_phy)
    {
    dpdk_phy_resp_add_lan(is_ko, cur->lan, cur->name);
    }
  else if (cur->type == ovsreq_add_lan_a2b)
    {
    dpdk_a2b_resp_add_lan(is_ko, cur->lan, cur->name, cur->num);
    }
  else if (cur->type == ovsreq_add_lan_d2d)
    {
    dpdk_d2d_resp_add_lan(is_ko, cur->lan, cur->name);
    }
  else
    KERR("ERROR %s %s %d %d", cur->lan, cur->name, cur->num, cur->type);
  ovsreq_free(tid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void transmit_del_ack(int tid, t_ovsreq *cur, int is_ko)
{
  int ntid;
  t_ovslan *lanreq = lan_find(cur->lan);
  if (lan_refcount_dec(cur->lan, __LINE__) == 1)
    {
    ntid = utils_get_next_tid();
    ovsreq_alloc(ntid, cur->type, cur->lan, cur->name, cur->num);
    if (fmt_tx_del_lan(ntid, cur->lan))
      {
      KERR("ERROR %s", cur->lan);
      ovsreq_free(ntid);
      lan_refcount_dec(cur->lan, __LINE__);
      }
    if (lanreq == NULL)
      KERR("ERROR LAN %s", cur->lan);
    else
      {
      if (lanreq->must_send_del == 1)
        lanreq->must_send_del = 0;
      else
        KERR("ERROR LAN %s", cur->lan);
      }
    }
  if ((cur->type == ovsreq_del_lan_ethd) ||
      (cur->type == ovsreq_del_lan_eths))
    {
    dpdk_kvm_resp_del_lan(is_ko, cur->lan, cur->name, cur->num);
    }
  else if (cur->type == ovsreq_del_lan_nat)
    {
    dpdk_nat_resp_del_lan(is_ko, cur->lan, cur->name);
    }
  else if (cur->type == ovsreq_del_lan_tap)
    {
    dpdk_tap_resp_del_lan(is_ko, cur->lan, cur->name);
    }
  else if (cur->type == ovsreq_del_lan_phy)
    {
    dpdk_phy_resp_del_lan(is_ko, cur->lan, cur->name);
    }
  else if (cur->type == ovsreq_del_lan_a2b)
    {
    dpdk_a2b_resp_del_lan(is_ko, cur->lan, cur->name, cur->num);
    }
  else if (cur->type == ovsreq_del_lan_d2d)
    {
    dpdk_d2d_resp_del_lan(is_ko, cur->lan, cur->name);
    }
  else
    KERR("ERROR %s %s %d %d", cur->lan, cur->name, cur->num, cur->type);
  lan_free_try(cur->name, cur->num, cur->lan, cur->type);
  ovsreq_free(tid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_msg_ack_lan_endp(int tid, char *lan, char *name, int num,
                           int is_add, int is_ko, char *lab)
{
  t_ovsreq *cur = ovsreq_find(tid);
  if (!cur)
    {
    KERR("ERROR %d %s %s %d is_add:%d is_ko:%d",tid,lan,name,num,is_add,is_ko);
    }
  else
    {
    if (strcmp(lan, cur->lan))
      KERR("ERROR %s %s is_add:%d is_ko:%d", lan, cur->lan, is_add, is_ko);
    else if (strcmp(name, cur->name))
      KERR("ERROR %s %s is_add:%d is_ko:%d", name, cur->name, is_add, is_ko);
    else if (num != cur->num)
      KERR("ERROR %d %d is_add:%d is_ko:%d", num, cur->num, is_add, is_ko);
    else if (is_add)
      {
      transmit_add_ack(tid, cur, is_ko);
      }
    else
      {
      transmit_del_ack(tid, cur, is_ko);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_msg_ack_lan(int tid, char *lan, int is_add, int is_ko, char *lab)
{
  int ntid;
  t_ovslan *lanreq = lan_find(lan);
  t_ovsreq *ovsreq, *cur = ovsreq_find(tid);
  if (!cur)
    {
    KERR("ERROR %d %s is_add:%d is_ko:%d", tid, lan, is_add, is_ko);
    }
  else
    {
    if (strcmp(lan, cur->lan))
      KERR("ERROR %s %s is_add:%d is_ko:%d", lan, cur->lan, is_add, is_ko);
    if (is_add)
      {
      if (is_ko)
        {
        KERR("ERROR ADD LAN KO %s", lan);
        transmit_add_ack(tid, cur, is_ko);
        ovsreq_free(tid);
        }
      else
        {
        if (lanreq == NULL)
          KERR("ERROR LAN %s", lan);
        else
          lanreq->must_send_del = 1;
        lan_refcount_inc(cur->lan, __LINE__);
        ntid = utils_get_next_tid();
        ovsreq=ovsreq_alloc(ntid, cur->type, cur->lan, cur->name, cur->num);
        ovsreq_free(tid);
        clownix_timeout_add(1, delay_add_lan_endp, (void *)ovsreq, NULL, NULL);
        }
      }
    else
      {
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
    if (cur->time_count > 25)
      {
      switch(cur->type)
        {

       case ovsreq_add_lan_nat:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->lan, cur->name);
          dpdk_nat_resp_add_lan(1, cur->lan, cur->name);
          ovsreq_free(cur->tid);
        break;

       case ovsreq_add_lan_a2b:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                      cur->name, cur->num);
          dpdk_a2b_resp_add_lan(1, cur->lan, cur->name, cur->num);
          ovsreq_free(cur->tid);
        break;

        case ovsreq_del_lan_nat:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->lan, cur->name);
          dpdk_nat_resp_del_lan(1, cur->lan, cur->name);
          ovsreq_free(cur->tid);
        break;

        case ovsreq_add_lan_tap:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->lan, cur->name);
          dpdk_tap_resp_add_lan(1, cur->lan, cur->name);
          ovsreq_free(cur->tid);
        break;

        case ovsreq_del_lan_tap:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->lan, cur->name);
          dpdk_tap_resp_del_lan(1, cur->lan, cur->name);
          ovsreq_free(cur->tid);
        break;

        case ovsreq_add_lan_phy:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->lan, cur->name);
          dpdk_phy_resp_add_lan(1, cur->lan, cur->name);
          ovsreq_free(cur->tid);
        break;

        case ovsreq_del_lan_phy:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->lan, cur->name);
          dpdk_phy_resp_del_lan(1, cur->lan, cur->name);
          ovsreq_free(cur->tid);
        break;

        case ovsreq_del_lan_a2b:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                      cur->name, cur->num);
          dpdk_a2b_resp_del_lan(1, cur->lan, cur->name, cur->num);
          ovsreq_free(cur->tid);
        break;

       case ovsreq_add_lan_d2d:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->lan, cur->name);
          dpdk_d2d_resp_add_lan(1, cur->lan, cur->name);
          ovsreq_free(cur->tid);
        break;

        case ovsreq_del_lan_d2d:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->lan, cur->name);
          dpdk_d2d_resp_del_lan(1, cur->lan, cur->name);
          ovsreq_free(cur->tid);
        break;

        case ovsreq_add_lan_ethd:
        case ovsreq_add_lan_eths:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                            cur->name, cur->num);
          dpdk_kvm_resp_add_lan(1, cur->lan, cur->name, cur->num);
          ovsreq_free(cur->tid);
        break;

        case ovsreq_del_lan_ethd:
        case ovsreq_del_lan_eths:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                      cur->name, cur->num);
          dpdk_kvm_resp_del_lan(1, cur->lan, cur->name, cur->num);
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
void dpdk_msg_vlan_exist_no_more(char *lan)
{
  if (lan && (strlen(lan)))
    {
    lan_free_try(NULL, 0, lan, 0);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_msg_init(void)
{
  g_head_ovsreq = NULL;
  g_head_ovslan = NULL;
  clownix_timeout_add(50, timer_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

