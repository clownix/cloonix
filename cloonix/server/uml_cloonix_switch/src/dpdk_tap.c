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
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "endp_mngt.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "llid_trace.h"
#include "machine_create.h"
#include "file_read_write.h"
#include "dpdk_ovs.h"
#include "dpdk_dyn.h"
#include "dpdk_tap.h"
#include "dpdk_msg.h"
#include "dpdk_fmt.h"
#include "qmp.h"
#include "system_callers.h"
#include "stats_counters.h"
#include "dpdk_msg.h"
#include "layout_rpc.h"
#include "layout_topo.h"

/****************************************************************************/
typedef struct t_dlan
{
  char lan[MAX_NAME_LEN];
  int  waiting_ack_add;
  int  waiting_ack_del;
  int  timer_count;
  int  llid;
  int  tid;
  struct t_dlan *prev;
  struct t_dlan *next;
} t_dlan;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_dtap
{
  char name[MAX_NAME_LEN];
  int waiting_ack_add;
  int waiting_ack_del;
  int timer_count;
  int llid;
  int tid;
  int ms;
  int pkt_tx;
  int pkt_rx;
  int byte_tx;
  int byte_rx;
  t_dlan *head_lan;
  struct t_dtap *prev;
  struct t_dtap *next;
} t_dtap;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_toadd
{
  char name[MAX_NAME_LEN];
  int llid;
  int tid;
  int coherency_idx;
  int count;
} t_toadd;
/*--------------------------------------------------------------------------*/

static t_dtap *g_head_dtap;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dtap *get_dtap(char *name)
{ 
  t_dtap *dtap = g_head_dtap;
  while(dtap)
    {
    if (!strcmp(name, dtap->name))
      break;
    dtap = dtap->next;
    }
  return dtap;
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dlan *get_dlan(t_dtap *dtap, char *lan)
{
  t_dlan *cur = dtap->head_lan;
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
static t_dlan *alloc_dlan(t_dtap *dtap, char *lan)
{
  t_dlan *dlan = (t_dlan *) clownix_malloc(sizeof(t_dlan), 5);
  memset(dlan, 0, sizeof(t_dlan));
  memcpy(dlan->lan, lan, MAX_NAME_LEN-1);
  if (dtap->head_lan)
    dtap->head_lan->prev = dlan;
  dlan->next = dtap->head_lan;
  dtap->head_lan = dlan;
  return dlan;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_dlan(t_dtap *dtap, char *lan)
{
  t_dlan *cur;
  char lan_name[MAX_NAME_LEN];
  memset (lan_name, 0, MAX_NAME_LEN);
  strncpy(lan_name, lan, MAX_NAME_LEN-1);
  if (dtap == NULL)
    KOUT(" ");
  cur = dtap->head_lan;
  while (cur)
    {
    if (!strcmp(cur->lan, lan_name))
      break;
    }
  if (!cur)
    KERR("%s %s", dtap->name, lan_name);
  else
    {
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == dtap->head_lan)
      dtap->head_lan = cur->next;
    clownix_free(cur, __FUNCTION__);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    if ((!dpdk_dyn_lan_exists(lan_name)) &&
        (!dpdk_tap_lan_exists(lan_name)))
      dpdk_msg_vlan_exist_no_more(lan_name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_all_dlan(t_dtap *dtap)
{
  t_dlan *cur, *next;
  if (dtap == NULL)
    KOUT(" "); 
  cur = dtap->head_lan;
  while (cur)
    {
    next = cur->next;
    free_dlan(dtap, cur->lan);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_dtap *alloc_dtap(char *name)
{
  t_dtap *dtap = (t_dtap *)clownix_malloc(sizeof(t_dtap), 5);
  memset(dtap, 0, sizeof(t_dtap));
  memcpy(dtap->name, name, MAX_NAME_LEN-1);
  if (g_head_dtap)
    g_head_dtap->prev = dtap;
  dtap->next = g_head_dtap;
  g_head_dtap = dtap;
  return dtap;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_dtap(char *name)
{
  t_dtap *dtap = get_dtap(name);
  if (!dtap)
    KERR("%s", name);
  else
    {
    layout_del_sat(name);
    free_all_dlan(dtap);
    if (dtap->prev)
      dtap->prev->next = dtap->next;
    if (dtap->next)
      dtap->next->prev = dtap->prev;
    if (g_head_dtap == dtap)
      g_head_dtap = dtap->next;
    clownix_free(dtap, __FUNCTION__);
    }
  event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_tap_msg_beat(void *data)
{
  t_dtap *next, *dtap = g_head_dtap;
  t_dlan *cur;
  while(dtap)
    {
    next = dtap->next;
    cur = dtap->head_lan;
    while (cur)
      {
      if ((cur->waiting_ack_add != 0) || (cur->waiting_ack_del != 0))
        {
        cur->timer_count += 1;
        }
      cur = cur->next;
      }
    if ((dtap->waiting_ack_add != 0) || (dtap->waiting_ack_del != 0))
      {
      dtap->timer_count += 1;
      if (dtap->timer_count > 5)
        {
        if (!msg_exist_channel(dtap->llid))
          KERR("Time %s", dtap->name);
        else
          {
          KERR("Timeout %s", dtap->name);
          send_status_ko(dtap->llid, dtap->tid, "Timeout");
          }
        free_dtap(dtap->name);
        }
      }
    dtap = next;
    }
  clownix_timeout_add(50, timer_tap_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_get_qty(void)
{
  int result = 0;
  t_dtap *cur = g_head_dtap; 
  while(cur)
    {
    result += 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_topo_endp(int nb_max, t_topo_endp *endp_tab)
{
  int i, nb, len, result, nb_lan;
  t_dtap *dtap = g_head_dtap; 
  t_dlan *cur;
  t_topo_endp *endp;
  nb = dpdk_tap_get_qty();
  if (nb > nb_max)
    KOUT("%d %d", nb, nb_max);
  memset(endp_tab, 0, nb * sizeof(t_topo_endp));
  nb = 0;
  while(dtap)
    {
    if ((dtap->waiting_ack_add == 0) &&
        (dtap->waiting_ack_del == 0))
      {
      result = 0;
      nb_lan = 0;
      endp = &(endp_tab[nb]); 
      nb += 1;
      strncpy(endp->name, dtap->name, MAX_NAME_LEN-1);
      endp->type = endp_type_dpdk_tap;
      cur = dtap->head_lan;
      while (cur)
        {
        if ((cur->waiting_ack_add == 0) &&
            (cur->waiting_ack_del == 0))
          {
          result = 1;
          nb_lan += 1;
          }
        cur = cur->next;
        }
      if (result)
        {
        i = 0;
        len = nb_lan * sizeof(t_lan_group_item);
        endp->lan.lan = (t_lan_group_item *) clownix_malloc(len, 2);
        memset(endp->lan.lan, 0, len);
        endp->lan.nb_lan = nb_lan;
        cur = dtap->head_lan;
        while (cur)
          {
          if ((cur->waiting_ack_add == 0) &&
              (cur->waiting_ack_del == 0))
            {
            strncpy(endp->lan.lan[i].lan, cur->lan, MAX_NAME_LEN-1);
            i += 1;
            }
          cur = cur->next;
          }
        }
      }
    dtap = dtap->next;
    }
  return nb;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void resp_lan(int is_add, int is_ko, char *lan, char *name)
{
  t_dtap *dtap = get_dtap(name);
  t_dlan *dlan;
  if (dtap)
    {
    dlan = get_dlan(dtap, lan);;
    if (!dlan)
      KERR("%d %s %s", is_ko, lan, name);
    else if (!msg_exist_channel(dlan->llid))
      KERR("%d %s %s", is_ko, lan, name);
    else
      {
      if (is_add)
        {
        if ((dlan->waiting_ack_add == 0) ||
            (dlan->waiting_ack_del == 1))
          KERR("%d %s %s %d", is_ko, lan, name, dlan->waiting_ack_add);
        }
      else
        {
        if ((dlan->waiting_ack_add == 1) ||
            (dlan->waiting_ack_del == 0))
          KERR("%d %s %s %d", is_ko, lan, name, dlan->waiting_ack_del);
        }
      if (is_ko)
        {
        KERR("Resp KO %s %s", lan, dtap->name);
        send_status_ko(dlan->llid, dlan->tid, "Resp KO");
        free_dlan(dtap, lan);
        dlan = NULL;
        }
      else
        {
        dlan->waiting_ack_add = 0;
        dlan->waiting_ack_del = 0;
        send_status_ok(dlan->llid, dlan->tid, "OK");
        if (is_add == 0)
          {
          free_dlan(dtap, lan);
          dlan = NULL;
          }
        else
          event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
        }
      }
    if (dlan != NULL)
      {
      dlan->waiting_ack_add = 0;
      dlan->waiting_ack_del = 0;
      dlan->timer_count = 0;
      dlan->llid = 0;
      dlan->tid = 0;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_resp_add_lan(int is_ko, char *lan, char *name)
{
  resp_lan(1, is_ko, lan, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_resp_del_lan(int is_ko, char *lan, char *name)
{
  resp_lan(0, is_ko, lan, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_resp_add(int is_ko, char *name)
{
  t_dtap *dtap = get_dtap(name);
  if (dtap == NULL)
    KERR("%s", name);
  else if (!dtap->llid)
    KERR("%s", name);
  else if (!msg_exist_channel(dtap->llid))
    KERR("%s", name);
  else if (is_ko)
    {
    KERR("Resp KO %s", dtap->name);
    send_status_ko(dtap->llid, dtap->tid, "ovs req ko");
    free_dtap(name);
    }
  else
    {
    send_status_ok(dtap->llid, dtap->tid, "OK");
    if (dtap->waiting_ack_add == 0)
      KERR("%s", name);
    if (dtap->waiting_ack_del != 0)
      KERR("%s", name);
    dtap->waiting_ack_add = 0;
    dtap->waiting_ack_del = 0;
    dtap->timer_count = 0;
    dtap->llid = 0;
    dtap->tid = 0;
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    } 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_resp_del(char *name)
{
  t_dtap *dtap = get_dtap(name);
  if (dtap == NULL)
    KERR("%s", name);
  else
    {
    if ((dtap->llid) && (msg_exist_channel(dtap->llid)))
      send_status_ok(dtap->llid, dtap->tid, "OK");
    if (dtap->waiting_ack_add != 0)
      KERR("%s", name);
    free_dtap(name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_add_lan(int llid, int tid, char *lan, char *name)
{
  int result = -1;
  t_dtap *dtap = get_dtap(name);
  t_dlan *dlan;
  if (dtap == NULL)
    KERR("%s %s", lan, name);
  else if ((dtap->waiting_ack_add != 0) || (dtap->waiting_ack_del != 0))
    KERR("%s %s", lan, name);
  else
    {
    dlan = get_dlan(dtap, lan);
    if (dlan != NULL)
      KERR("%s %s", lan, name);
    else
      {
      if (dpdk_msg_send_add_lan_tap(lan, name))
        KERR("%s %s", lan, name);
      else
        {
        dlan = alloc_dlan(dtap, lan);
        dlan->waiting_ack_add = 1;
        dlan->timer_count = 0;
        dlan->llid = llid;
        dlan->tid = tid;
        result = 0;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_del_lan(int llid, int tid, char *lan, char *name)
{

  int result = -1;
  t_dtap *dtap = get_dtap(name);
  t_dlan *dlan;
  if (dtap == NULL)
    KERR("%s %s", lan, name);
  else if ((dtap->waiting_ack_add != 0) || (dtap->waiting_ack_del != 0))
    KERR("%s %s", lan, name);
  else
    {
    dlan = get_dlan(dtap, lan);
    if (dlan == NULL)
      KERR("%s %s", lan, name);
    else
      {
      if (dpdk_msg_send_del_lan_tap(lan, name))
        KERR("%s %s", lan, name);
      else
        {
        dlan->waiting_ack_del = 1;
        dlan->timer_count = 0;
        dlan->llid = llid;
        dlan->tid = tid;
        result = 0;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int tap_add(int llid, int tid, char *name)
{
  int result = -1;
  t_dtap *dtap = get_dtap(name);
  if (dtap)
    KERR("%s", name);
  else
    {
    if (dpdk_fmt_tx_add_tap(0, name))
      KERR("%s", name);
    else
      {
      dtap = alloc_dtap(name);
      dtap->waiting_ack_add = 1;
      dtap->timer_count = 0;
      dtap->llid = llid;
      dtap->tid = tid;
      layout_add_sat(name, llid);
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_tap_add(void *data)
{
  t_toadd *toadd = (t_toadd *) data;
  if (dpdk_ovs_muovs_ready())
    {
    if (tap_add(toadd->llid, toadd->tid, toadd->name))
      {
      KERR("Resp KO %s", toadd->name);
      if (!msg_exist_channel(toadd->llid))
        KERR("%s", toadd->name);
      else
        send_status_ko(toadd->llid, toadd->tid, "ovs req ko");
      }
    recv_coherency_unlock(toadd->coherency_idx);
    clownix_free(data, __FUNCTION__);
    }
  else
    {
    toadd->count += 1;
    if (toadd->count > 100)
      {
      KERR("Resp KO %s", toadd->name);
      if (!msg_exist_channel(toadd->llid))
        KERR("%s", toadd->name);
      else
        send_status_ko(toadd->llid, toadd->tid, "ovs req ko");
      recv_coherency_unlock(toadd->coherency_idx);
      clownix_free(data, __FUNCTION__);
      }
    else
      {
      clownix_timeout_add(10, timer_tap_add, (void *) toadd, NULL, NULL);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_add(int llid, int tid, char *name, int coherency_lock, int idx)
{
  int result = -1;
  t_toadd *toadd;
  if (coherency_lock)
    {
    toadd = (t_toadd *) clownix_malloc(sizeof(t_toadd), 5);
    memset(toadd, 0, sizeof(t_toadd));
    toadd->llid = llid;
    toadd->tid = tid;
    toadd->coherency_idx = idx;
    strncpy(toadd->name, name, MAX_NAME_LEN-1); 
    clownix_timeout_add(10, timer_tap_add, (void *) toadd, NULL, NULL);
    result = 0;
    }
  else
    {
    result = tap_add(llid, tid, name);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_del(int llid, int tid, char *name)
{
  int result = -1;
  t_dtap *dtap = get_dtap(name);
  if (!dtap)
    KERR("%s", name);
  else if ((dtap->waiting_ack_add != 0) || (dtap->waiting_ack_del != 0))
    {
    KERR("%s", name);
    free_dtap(name);
    }
  else if (dpdk_fmt_tx_del_tap(0, name))
    {
    KERR("%s", name);
    free_dtap(name);
    }
  else
    {
    dtap->waiting_ack_del = 1;
    dtap->timer_count = 0;
    dtap->llid = llid;
    dtap->tid = tid;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_exist(char *name)
{
  int result = 0;
  if (get_dtap(name))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_tap_lan_exists(char *lan)
{
  t_dtap *dtap = g_head_dtap;
  int result = 0;
  while(dtap)
    {
    if (get_dlan(dtap, lan) != NULL)
      result = 1;
    dtap = dtap->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dpdk_dyn_lan_exists_in_tap(char *name, char *lan)
{
  int result = 0;
  t_dtap *dtap = get_dtap(name);
  t_dlan *dlan;
  if(dtap)
    {
    dlan = get_dlan(dtap, lan);
    if (dlan != NULL)
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_end_ovs(void)
{
  t_dtap *ndtap, *dtap = g_head_dtap;
  t_dlan *cur;
  while(dtap)
    {
    ndtap = dtap->next;
    cur = dtap->head_lan;
    while(cur)
      {
      if (dpdk_msg_send_del_lan_tap(cur->lan, dtap->name))
        KERR("%s %s", cur->lan, dtap->name);
      cur = cur->next;
      }
    if (dpdk_fmt_tx_del_tap(0, dtap->name))
      {
      KERR("%s", dtap->name);
      free_dtap(dtap->name);
      }
    dtap = ndtap;
    }
  
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void dpdk_tap_init(void)
{
  g_head_dtap = NULL;
  clownix_timeout_add(50, timer_tap_msg_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
