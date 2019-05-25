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
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "cfg_store.h"
#include "endp_evt.h"
#include "utils_cmd_line_maker.h"
#include "lan_to_name.h"
#include "mulan_mngt.h"
#include "endp_mngt.h"


/****************************************************************************/
typedef struct t_time_delay
{
  char name[MAX_NAME_LEN];
  int num;
} t_time_delay;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_attached
{
  char attached_lan[MAX_NAME_LEN];
  char waiting_lan[MAX_NAME_LEN];
  int llid;
  int tid;
  int timer_lan_ko_resp;
  int rank;
} t_attached;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_evendp
{
  char name[MAX_NAME_LEN];
  int  num;
  t_attached atlan[MAX_TRAF_ENDPOINT];
  struct t_evendp *prev;
  struct t_evendp *next;
} t_evendp;
/*--------------------------------------------------------------------------*/
typedef struct t_timer_evt
{
  int  timer_lan_ko_resp;
  char name[MAX_NAME_LEN];
  int  num;
  char lan[MAX_NAME_LEN];
  char label[MAX_PATH_LEN];
  int tidx;
} t_timer_evt;
/*--------------------------------------------------------------------------*/

static t_evendp *g_head_endp;


/*****************************************************************************/
int endp_is_wlan(char *name, int num, int *nb_eth)
{
  int result = 0;
  t_vm *vm = cfg_get_vm(name);
  if (vm)
    {
    *nb_eth = vm->kvm.nb_dpdk + vm->kvm.nb_eth;
    if (num >= vm->kvm.nb_dpdk + vm->kvm.nb_eth)
      result = 1;
    }
  else
    *nb_eth = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_evendp *endp_find(char *name, int num)
{
  t_evendp *cur = NULL;
  if ((!name) || (!(name[0])))
    KERR(" ");
  else
    {
    cur = g_head_endp;
    while(cur)
      {
      if ((!strcmp(cur->name, name)) && (cur->num == num))
        break;
      cur = cur->next;
      }
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int endp_evt_exists(char *name, int num)
{
  int result = 0;
  if (endp_find(name, num))
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_attached *endp_atlan_find(char *name, int num, int tidx)
{
  t_evendp *evendp = endp_find(name, num);
  t_attached *atlan = NULL;
  if (evendp)
    atlan = &(evendp->atlan[tidx]);
  return atlan;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void init_waiting_lan(char *name, int num, int tidx, 
                             char *lan, int llid, int tid)
{
  t_attached *atlan = endp_atlan_find(name, num, tidx);
  if (!atlan)
    KERR("%s %d %d", name, num, tidx);
  else
    {
    if (lan == NULL)
      {
      memset(atlan->waiting_lan, 0, MAX_NAME_LEN);
      atlan->llid = 0;
      atlan->tid = 0;
      }
    else 
      {
      if (strlen(lan) == 0)
        KERR("%s %d %d", name, num, tidx);
      else if (strlen(atlan->waiting_lan))
        KERR("%s %d %d %s", name, num, tidx, atlan->waiting_lan);
      memset(atlan->waiting_lan, 0, MAX_NAME_LEN);
      strncpy(atlan->waiting_lan, lan, MAX_NAME_LEN-1);
      atlan->llid = llid;
      atlan->tid = tid;
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_evendp *endp_find_next_with_lan(t_evendp *start, 
                                             char *lan, int *tidx)
{
  int i, found = 0;
  t_evendp *cur = start;
  if (!lan[0])
    KOUT(" ");
  if (!cur)
    cur = g_head_endp;
  else 
    cur = cur->next;
  while(cur)
    {
    for (i=0; i<MAX_TRAF_ENDPOINT; i++)
      {
      if (!(strcmp(cur->atlan[i].waiting_lan, lan)))
        {
        *tidx = i;
        found = 1;
        break;
        }
      }
    if (found)
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_evendp *endp_find_next_with_attached_lan(t_evendp *start, 
                                                      char *lan, int *tidx)
{
  int i, found = 0;
  t_evendp *cur = start;
  if (!lan[0])
    KOUT(" ");
  if (!cur)
    cur = g_head_endp;
  else
    cur = cur->next;

  while(cur) 
    {
    for (i=0; i<MAX_TRAF_ENDPOINT; i++)
      {
      if (!(strcmp(cur->atlan[i].attached_lan, lan)))
        {
        *tidx = i;
        found = 1;
        break;
        }
      } 
    if (found)
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_evendp *endp_alloc(char *name, int num)
{
  t_evendp *evendp = endp_find(name, num);
  int i;
  if (evendp)
    KERR("%s %d", name, num);
  else
    {
    evendp = (t_evendp *)clownix_malloc(sizeof(t_evendp), 4);
    memset(evendp, 0, sizeof(t_evendp));
    strncpy(evendp->name, name, MAX_NAME_LEN-1);
    evendp->num = num;
    if (g_head_endp)
      g_head_endp->prev = evendp;
    evendp->next = g_head_endp;
    g_head_endp = evendp;
    for (i=0; i<MAX_TRAF_ENDPOINT; i++)
      init_waiting_lan(name, num, i, NULL, 0, 0);
    }
  return evendp;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void endp_free(char *name, int num)
{
  t_evendp *evendp = endp_find(name, num);
  if (evendp)
    {
    if (evendp->prev)
      evendp->prev->next = evendp->next;
    if (evendp->next)
      evendp->next->prev = evendp->prev;
    if (evendp == g_head_endp)
      g_head_endp = evendp->next;
    clownix_free(evendp, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int endp_evt_lan_full(char *name, int num, int *tidx)
{
  int i, result = 1;
  t_evendp *evendp = endp_find(name, num);
  if (!evendp)
    KERR("%s", name);
  else
    {
    for (i=0; i<MAX_TRAF_ENDPOINT; i++)
      {
      if ((evendp->atlan[i].attached_lan[0] == 0) &&
          (evendp->atlan[i].waiting_lan[0] == 0))  
        {
        *tidx = i;
        result = 0;
        break;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int endp_evt_lan_find(char *name, int num, char *lan, int *tidx)
{
  int i, result = 0;
  t_evendp *evendp = endp_find(name, num);
  if (!evendp)
    KERR("%s %d", name, num);
  else
    {
    for (i=0; i<MAX_TRAF_ENDPOINT; i++)
      {
      if (!strcmp(lan, evendp->atlan[i].attached_lan))
        {
        *tidx = i;
        result = 1;
        break;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void resp_to_cli(char *name, int num, int llid, int tid, 
                        int ok, char *lan)
{
  char info[MAX_PATH_LEN];
  if (llid && msg_exist_channel(llid))
    {
    if (ok)
      {
      sprintf( info, "ethvadd %s %d %s", name, num, lan);
      send_status_ok(llid, tid, info);
      }
    else
      {
      sprintf( info, "ethvadd %s %d %s", name, num, lan);
      send_status_ko(llid, tid, info);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_evt(void *data)
{
  t_attached *atlan;
  t_timer_evt *te = (t_timer_evt *) data;
  atlan = endp_atlan_find(te->name, te->num, te->tidx);
  if ((atlan) && (atlan->llid) && 
      (strlen(atlan->waiting_lan)) && 
      (te->timer_lan_ko_resp == atlan->timer_lan_ko_resp))
    {
    if (strcmp(atlan->waiting_lan, te->lan))
      KERR("%s %s", atlan->waiting_lan, te->lan);
    else 
      {
      if (strlen(atlan->attached_lan) == 0)
        {
        if (msg_exist_channel(atlan->llid))
          send_status_ko(atlan->llid, atlan->tid, te->label);
        init_waiting_lan(te->name, te->num, te->tidx, NULL, 0, 0);
        KERR("%s", te->label);
        }
      }
    }
  clownix_free(te, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void endp_timer_ko_resp(int delay, char *name, int num, int tidx,
                               char *lan, char *reason)
{
  t_attached *atlan = endp_atlan_find(name, num, tidx);
  t_timer_evt *te;
  if ((atlan) && (atlan->llid))
    {
    atlan->timer_lan_ko_resp += 1;
    te = (t_timer_evt *) clownix_malloc(sizeof(t_timer_evt), 5);
    memset(te, 0, sizeof(t_timer_evt));
    te->timer_lan_ko_resp = atlan->timer_lan_ko_resp;
    strncpy(te->name, name, MAX_NAME_LEN-1);
    te->num = num;
    te->tidx = tidx;
    strncpy(te->lan, lan, MAX_NAME_LEN-1);
    strncpy(te->label, reason, MAX_PATH_LEN-1);
    clownix_timeout_add(delay, timer_evt, (void *) te, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int wlan_req_mulan(char *lan, char *name, int num, int tidx)
{
  int result = -1; 
  char *sock;
  int nb_eth;
  t_vm *vm = cfg_get_vm(name);
  if (vm)
    {
    if (endp_is_wlan(name, num, &nb_eth))
      {
      sock = utils_get_qbackdoor_wlan_path(vm->kvm.vm_id, num-nb_eth);
      if (mulan_send_wlan_req_connect(sock, lan, name, num, tidx))
        KERR("%s %s %s %d", sock, lan, name, num);
      else
        result = 0;
      }
    else
      KERR("%s %s %d", lan, name, num);
    }
  else
    KERR("%s %s %d", lan, name, num);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void do_add_lan(int llid, int tid, int is_wlan, 
                       char *name, int num, int tidx, 
                       t_evendp *evendp, char *lan, int nb_eth)
{
  int mulan_is_wlan;
  if (!(mulan_exists(lan, &mulan_is_wlan)))
    {
    if (mulan_start(lan, is_wlan))
      {
      send_status_ko(llid, tid, "bad mulan start");
      }
    else
      {
      init_waiting_lan(name, num, tidx, lan, llid, tid); 
      endp_timer_ko_resp(500, name, num, tidx, lan, "timeout start mulan");
      }
    } 
  else
    {   
    if (mulan_is_wlan != is_wlan)
      {
      if (msg_exist_channel(llid))
        send_status_ko(llid, tid, "mulan incoherency wlan non-wlan");
      }
    else if (mulan_is_zombie(lan))
      {
      KERR("%s %s", name, lan);
      if (msg_exist_channel(llid))
        send_status_ko(llid, tid, "mulan zombie");
      }
    else
      {
      init_waiting_lan(name, num, tidx, lan, llid, tid); 
      if (mulan_is_oper(lan))
        {
        if (is_wlan)
          {
          if (wlan_req_mulan(lan, name, num, tidx))
            KERR("%s %d %s", name, num, lan);
          }
        else
          {
          if (endp_mngt_lan_connect(1, name, num, tidx, lan))
            KERR("%s %s", name, lan);
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void endp_evt_add_lan(int llid, int tid, char *name, int num, 
                            char *lan, int tidx)
{
  t_evendp *evendp = endp_find(name, num);
  t_attached *atlan = endp_atlan_find(name, num, tidx);
  int nb_eth, endp_type, is_wlan;
  if ((!lan) || (!lan[0]))
    KOUT("%s", name);
  if (!endp_mngt_exists(name, num, &endp_type))
    {
    KERR("%s %s", name, lan);
    if (msg_exist_channel(llid))
      send_status_ko(llid, tid, "evendp record should be present");
    }
  else if (!atlan)
    {
    if (msg_exist_channel(llid))
      {
      if (endp_type == endp_type_c2c)
        send_status_ko(llid, tid, "c2c exists only when connected to peer");
      else
        send_status_ko(llid, tid, "evendp not found");
      }
    }
  else if ((endp_type != endp_type_tap) &&
           (endp_type != endp_type_dpdk_tap) &&
           (endp_type != endp_type_snf) &&
           (endp_type != endp_type_c2c) &&
           (endp_type != endp_type_nat) &&
           (endp_type != endp_type_a2b) &&
           (endp_type != endp_type_raw) &&
           (endp_type != endp_type_kvm_eth)  &&
           (endp_type != endp_type_kvm_wlan) &&
           (endp_type != endp_type_wif))
    KOUT("%d", endp_type);
  else if (!endp_mngt_connection_state_is_restfull(name, num))
    {
    if (msg_exist_channel(llid))
      send_status_ko(llid, tid, "evendp is not restfull");
    }
  else if (strlen((atlan->waiting_lan)))
    {
    if (msg_exist_channel(llid))
      send_status_ko(llid, tid, "evendp connecting");
    }
  else if (strlen((atlan->attached_lan)))
    {
    if (msg_exist_channel(llid))
      send_status_ko(llid, tid, "evendp connected");
    }
  else
    {
    is_wlan = endp_is_wlan(name, num, &nb_eth);
    do_add_lan(llid, tid, is_wlan, name, num, tidx, evendp, lan, nb_eth);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int endp_evt_lan_is_in_use(char *lan)
{
  int tidx, result = 0;
  if ((!lan) || (!lan[0]))
    KOUT(" ");
  if ((endp_find_next_with_lan(NULL, lan, &tidx)) || 
      (endp_find_next_with_attached_lan(NULL, lan, &tidx)))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int endp_evt_del_lan(char *name, int num, int tidx, char *lan)
{
  t_evendp *evendp = endp_find(name, num);
  t_attached *atlan = endp_atlan_find(name, num, tidx);
  int nb_eth, lan_num, result = -1;
  if ((!lan) || (!lan[0]))
    KOUT("%s", name);
  lan_num = lan_get_with_name(lan);
  if ((lan_num <= 0) || (lan_num >= MAX_LAN))
    KERR("%s %d %s %d", name, tidx, lan, lan_num);
  else
    {
    if ((!evendp) || (!atlan))
      KERR("%s %s", name, lan);
    else if (strcmp(lan, atlan->attached_lan))
      KERR("%s %s", lan, atlan->attached_lan);
    else
      {
      if (endp_is_wlan(name, num, &nb_eth))
        {
        if (mulan_send_wlan_req_disconnect(lan, name, num, tidx))
          KERR("%s %s %d", lan, name, num);
        }
      else
        endp_mngt_lan_disconnect(name, num, tidx, lan);
      mulan_test_stop(lan);
      endp_mngt_del_attached_lan(name, num, tidx, lan); 
      init_waiting_lan(name, num, tidx, NULL, 0, 0);
      memset(atlan->attached_lan, 0, MAX_NAME_LEN);
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_evt_connect_OK(char *name, int num, char *lan, int tidx, int rank)
{
  t_evendp *evendp = endp_find(name, num);
  t_attached *atlan = endp_atlan_find(name, num, tidx);
  int llid, tid;
  if (!lan[0])
    KERR(" ");
  else if (!evendp)
    KERR("%s %d %s", name, num, lan);
  else if (!atlan)
    KERR("%s %s", name, lan);
  else
    {
    if (!strcmp(atlan->waiting_lan, lan))
      {
      llid = atlan->llid;
      tid = atlan->tid;
      atlan->rank = rank;
      strncpy(atlan->attached_lan, lan, MAX_NAME_LEN-1);
      endp_mngt_add_attached_lan(llid, name, num, tidx, lan); 
      init_waiting_lan(name, num, tidx, NULL, 0, 0);
      resp_to_cli(name, num, llid, tid, 1, lan);
      }
    else
      KERR("%s %s", name, lan);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_evt_connect_KO(char *name, int num, char *lan, int tidx)
{
  t_evendp *evendp = endp_find(name, num);
  t_attached *atlan = endp_atlan_find(name, num, tidx);
  int llid, tid;
  if (!lan[0])
    KERR("%s %s", name, lan);
  else if ((!evendp) || (!atlan))
    KERR("%s %s", name, lan);
  else
    {
   if (!strcmp(atlan->waiting_lan, lan))
      {
      llid = atlan->llid;
      tid = atlan->tid;
      init_waiting_lan(name, num, tidx, NULL, 0, 0);
      }
    else
      KERR("%s %s", name, lan);
    resp_to_cli(name, num, llid, tid, 0, lan);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_evt_birth(char *name, int num, int endp_type)
{
  t_evendp *evendp = endp_find(name, num);
  if (evendp)
    KERR("%s %d", name, num);
  else
    {
    endp_alloc(name, num);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_evt_quick_death(char *name, int num)
{
  int i;
  t_attached *atlan;
  for (i=0; i<MAX_TRAF_ENDPOINT; i++)
    {
    atlan = endp_atlan_find(name, num, i);
    if (atlan)
      {
      if (strlen(atlan->attached_lan))
        {
        endp_evt_del_lan(name, num, i, atlan->attached_lan);
        }
      }
    }
  endp_free(name, num);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_endp_death(void *data)
{
  t_time_delay *td = (t_time_delay *) data;
  t_evendp *evendp = endp_find(td->name, td->num);
  endp_mngt_send_quit(td->name, td->num);
  if (evendp)
    {
    endp_evt_quick_death(td->name, td->num);
    }
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int endp_evt_death(char *name, int num)
{
  t_evendp *evendp = endp_find(name, num);
  t_attached *atlan;
  t_time_delay *td;
  int nb_eth, i, result = -1;
  if (evendp)
    {
    for (i=0; i<MAX_TRAF_ENDPOINT; i++)
      {
      atlan = endp_atlan_find(name, num, i);
      if (strlen(atlan->attached_lan))
        {
        if (endp_is_wlan(name, num, &nb_eth))
          {
          if (mulan_send_wlan_req_disconnect(atlan->attached_lan, name, num, i))
            KERR("%s %s %d", atlan->attached_lan, name, num);
          }
        else
          endp_mngt_lan_disconnect(name, num, i, atlan->attached_lan);
        }
      if (strlen(atlan->waiting_lan))
        {
        if (endp_is_wlan(name, num, &nb_eth))
          {
          if (mulan_send_wlan_req_disconnect(atlan->waiting_lan, name, num, i))
            KERR("%s %s %d", atlan->waiting_lan, name, num);
          }
        else
          endp_mngt_lan_disconnect(name, num, i, atlan->waiting_lan);
        }
      }
    td = (t_time_delay *) clownix_malloc(sizeof(t_time_delay), 4);
    memset(td, 0, sizeof(t_time_delay));
    strncpy(td->name, name, MAX_NAME_LEN-1);
    td->num = num;
    clownix_timeout_add(50, timer_endp_death, (void *) td, NULL, NULL);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void death_mulan(char *lan)
{
  int tidx, ntidx;
  t_evendp *cur, *next;
  cur = endp_find_next_with_lan(NULL, lan, &tidx);
  while (cur)
    {
    next = endp_find_next_with_lan(cur, lan, &ntidx);
    if (cur->atlan[tidx].llid)
      {
      if (msg_exist_channel(cur->atlan[tidx].llid))
        send_status_ko(cur->atlan[tidx].llid,
                       cur->atlan[tidx].tid, "mulan death");
      init_waiting_lan(cur->name, cur->num, tidx, NULL, 0, 0);
      }
    tidx = ntidx;
    cur = next;
    }
  cur = endp_find_next_with_attached_lan(NULL, lan, &tidx);
  while (cur)
    {
    next = endp_find_next_with_attached_lan(cur, lan, &ntidx);
    endp_evt_del_lan(cur->name, cur->num, tidx, lan);
    tidx = ntidx;
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_evt_mulan_death(char *lan)
{
  if ((!lan) || (lan[0] == 0))
    KOUT(" ");
  death_mulan(lan);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_evt_mulan_birth(char *lan)
{
  int tidx, is_wlan;
  t_evendp *cur;
  if ((!lan) || (!lan[0]))
    KOUT(" ");
  cur = endp_find_next_with_lan(NULL, lan, &tidx);
  while (cur)
    {
    is_wlan = mulan_get_is_wlan(lan);
    if (is_wlan)
      {
      if (wlan_req_mulan(lan, cur->name, cur->num, tidx))
        KERR("%s %d %s", cur->name, cur->num, lan);
      }
    else
      {
      if (endp_mngt_lan_connect(10, cur->name, cur->num, tidx, lan)) 
        KERR("%s %d %s %d", cur->name, cur->num, lan, tidx);
      }
    cur = endp_find_next_with_lan(cur, lan, &tidx);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void endp_evt_init(void)
{
  g_head_endp = NULL;
}
/*--------------------------------------------------------------------------*/



