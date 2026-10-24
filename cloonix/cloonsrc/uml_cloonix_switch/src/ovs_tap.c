/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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
#include "ovs_snf.h"
#include "mactopo.h"
#include "pthexec.h"
#include "ack_pending.h"

static t_ovs_tap *g_head_tap;
static int g_nb_tap;

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

static void free_tap(t_ovs_tap *cur)
{
  cfg_free_obj_id(cur->tap_id);
  if (strlen(cur->lan_added))
    {
    mactopo_del_req(item_tap, cur->name, 0, cur->lan_added);
    lan_del_name(cur->lan_added, item_tap, cur->name, 0);
    }
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

static void alloc_tap(int llid, int tid, char *name)
{
  int id = cfg_alloc_obj_id();
  t_ovs_tap *cur = (t_ovs_tap *) malloc(sizeof(t_ovs_tap));
  memset(cur, 0, sizeof(t_ovs_tap));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->tap_id = id;
  cur->llid = llid;
  cur->tid = tid;
  cur->endp_type = endp_type_tapv;
  cur->mac_addr[0] = 0x2;
  cur->mac_addr[1] = 0xFF & rand();
  cur->mac_addr[2] = 0xFF & rand();
  cur->mac_addr[3] = 0xFF & rand();
  cur->mac_addr[4] = cur->tap_id % 100;
  cur->mac_addr[5] = 0xFF & rand();
  snprintf(cur->vhost, (MAX_NAME_LEN-1), "%s", pthexec_ovs_port(id, 0));
  if (g_head_tap)
    g_head_tap->prev = cur;
  cur->next = g_head_tap;
  g_head_tap = cur; 
  g_nb_tap += 1;
  cfg_hysteresis_send_topo_info();
}

void ovs_tap_resp_add_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan)
{
  t_ovs_tap *cur = find_tap(name);
  if (!cur)
    {
    mactopo_add_resp(0, item_tap, name, num, lan);
    KERR("ERROR %d %s", is_ko, name);
    }
  else
    {
    if (strlen(cur->lan))
      KERR("ERROR %s %s", name, cur->lan);
    memset(cur->lan, 0, MAX_NAME_LEN);
    if (is_ko)
      {
      mactopo_add_resp(0, item_tap, name, num, lan);
      KERR("ERROR tap %s: the add of lan %s was refused", name, lan);
      utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      if (strlen(cur->lan_added))
        {
        mactopo_del_req(item_tap, name, 0, cur->lan_added);
        lan_del_name(cur->lan_added, item_tap, name, 0);
        memset(cur->lan_added, 0, MAX_NAME_LEN);
        }
      }
    else
      {
      mactopo_add_resp(1, item_tap, name, num, lan);
      cfg_hysteresis_send_topo_info();
      strncpy(cur->lan, lan, MAX_NAME_LEN-1);
      utils_send_status_ok(&(cur->llid), &(cur->tid));
      }
    }
}

void ovs_tap_resp_del_lan(int is_ko, char *name, int num, 
                          char *vhost, char *lan)
{
  t_ovs_tap *cur = find_tap(name);
  if (!cur)
    {
    mactopo_del_resp(0, name, num, lan);
    KERR("ERROR %d %s %s", is_ko, name, lan);
    }
  else if (cur->del_tap_req == 1)
    {
    mactopo_del_resp(item_tap, name, num, lan);
    memset(cur->lan, 0, MAX_NAME_LEN);
    if (msg_send_del_tap(name, cur->vhost))
      {
      KERR("ERROR tap %s: the del could not be sent to ovs, dropping the "
           "record", name);
      utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      free_tap(cur);
      }
    }
  else
    {
    mactopo_del_resp(item_tap, name, num, lan);
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

void ovs_tap_resp_msg_tap(int is_ko, int is_add, char *name)
{
  char err[MAX_PATH_LEN];
  t_ovs_tap *cur = find_tap(name);
  if (!cur)
    KERR("ERROR tap %s: ovs answered its %s (ko=%d) but it has no record",
         name, is_add ? "add" : "del", is_ko);
  else
    {
    if (is_ko)
      {
      KERR("ERROR tap %s: ovs refused its %s", name, is_add ? "add" : "del");
      snprintf(err, MAX_PATH_LEN-1, "tap %s: cloonix-ovs-drv could not %s "
               "it, its ERROR line in syslog says which step", name,
               is_add ? "make" : "remove");
      utils_send_status_ko(&(cur->llid), &(cur->tid), err);
      }
    else
      {
      utils_send_status_ok(&(cur->llid), &(cur->tid));
      }
    /* A refused add left no tap: keep no record of it. */
    if ((is_add == 0) || (is_ko))
      free_tap(cur);
    }
}

t_ovs_tap *ovs_tap_exists(char *name)
{
  return (find_tap(name));
}

t_ovs_tap *ovs_tap_get_first(int *nb_tap)
{
  t_ovs_tap *result = g_head_tap;
  *nb_tap = g_nb_tap;
  return result;
}

void ovs_tap_add(int llid, int tid, char *name)
{
  t_ovs_tap *cur = find_tap(name);
  char mac[MAX_NAME_LEN];
  unsigned char *mc;
  if (cur)
    {
    KERR("WARNING add tap %s refused: a tap of that name already exists",
         name);
    send_status_ko(llid, tid, "Exists already");
    }
  else
    {
    alloc_tap(llid, tid, name);
    cur = find_tap(name);
    if (!cur)
      KOUT(" ");
    mc = cur->mac_addr;
    memset(mac, 0, MAX_NAME_LEN);
    snprintf(mac, (MAX_NAME_LEN-1), 
             "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
             mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
    if (msg_send_add_tap(name, cur->vhost, mac))
      {
      KERR("ERROR tap %s: the add could not be sent to ovs", name);
      utils_send_status_ko(&(cur->llid), &(cur->tid), name);
      free_tap(cur);
      }
    }
}

static int tap_del_busy(char *name)
{
  t_ovs_tap *cur = find_tap(name);
  if (!cur)
    return -1;
  return ((cur->llid) || (cur->del_tap_req));
}

void ovs_tap_del(int llid, int tid, char *name)
{
  t_ovs_tap *cur = find_tap(name);
  int val;
  if (!cur)
    {
    KERR("ERROR del tap %s refused: it does not exist", name);
    if (llid)
      send_status_ko(llid, tid, "Does not exist");
    }
  else if ((cur->llid) || (cur->del_tap_req))
    {
    /* no client: the del all pass asking again while ovs answers */
    if (!llid)
      KTRC("tap %s: a request on it is in flight, the del all pass waits",
           name);
    else
      del_when_free(llid, tid, name, "tap", tap_del_busy, ovs_tap_del);
    }
  else
    {
    cur->llid = llid;
    cur->tid = tid;
    if (strlen(cur->lan_added))
      {
      if (!strlen(cur->lan))
        KERR("ERROR %s %s", name, cur->lan_added);
      mactopo_del_req(item_tap, cur->name, 0, cur->lan_added);
      val = lan_del_name(cur->lan_added, item_tap, cur->name, 0);
      memset(cur->lan_added, 0, MAX_NAME_LEN);
      if (val == 2*MAX_LAN)
        {
        cur->del_tap_req = 2;
        if (msg_send_del_tap(name, cur->vhost))
          {
          KERR("ERROR tap %s: the del could not be sent to ovs", name);
          utils_send_status_ko(&(cur->llid), &(cur->tid), name);
          free_tap(cur);
          }
        }
      else
        {
        cur->del_tap_req = 1;
        if (msg_send_del_lan_endp(ovsreq_del_tap_lan, name, 0,
                                  cur->vhost, cur->lan))
          {
          KERR("ERROR tap %s: the del of lan %s could not be sent, deleting "
               "the tap anyway", name, cur->lan);
          memset(cur->lan, 0, MAX_NAME_LEN);
          if (msg_send_del_tap(name, cur->vhost))
            {
            KERR("ERROR tap %s: the del could not be sent to ovs", name);
            utils_send_status_ko(&(cur->llid), &(cur->tid), name);
            free_tap(cur);
            }
          }
        }
      }
    else
      {
      cur->del_tap_req = 2;
      if (msg_send_del_tap(name, cur->vhost))
        {
        KERR("ERROR tap %s: the del could not be sent to ovs", name);
        utils_send_status_ko(&(cur->llid), &(cur->tid), name);
        free_tap(cur);
        }
      }
    }
}

void ovs_tap_add_lan(int llid, int tid, char *name, char *lan)
{
  t_ovs_tap *cur = find_tap(name);
  char err[MAX_PATH_LEN]; 
  if (!cur)
    {
    KERR("ERROR plug %s to tap %s: no tap of that name", lan, name);
    send_status_ko(llid, tid, "Does not exist");
    }
  else if ((cur->llid) || (cur->del_tap_req))
    {
    KERR("ERROR plug %s %s refused: %s", name, lan, cur->del_tap_req ?
         "its delete is running" : "a request of its own is in flight");
    send_status_ko(llid, tid, "Not ready");
    }
  else if (strlen(cur->lan))
    {
    KERR("ERROR tap %s already has lan %s, refusing %s",
         name, cur->lan, lan);
    send_status_ko(llid, tid, "Lan exists");
    }
  else
    {
    lan_add_name(lan, item_tap, name, 0);
    if (mactopo_add_req(item_tap, name, 0, lan, 
                        cur->vhost, cur->mac_addr, err))
      {
      KERR("ERROR %s %d %s %s", name, 0, lan, err);
      send_status_ko(llid, tid, err);
      lan_del_name(lan, item_tap, name, 0);
      }
    else if (msg_send_add_lan_endp(ovsreq_add_tap_lan, name, 0,
                                   cur->vhost, lan))
      {
      KERR("ERROR %s %s", name, lan);
      send_status_ko(llid, tid, "msg_send_add_lan_endp error");
      lan_del_name(lan, item_tap, name, 0);
      }
    else
      {
      strncpy(cur->lan_added, lan, MAX_NAME_LEN-1);
      cur->llid = llid;
      cur->tid = tid;
      }
    }
}

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
    KERR("ERROR unplug %s %s refused: %s", name, lan, cur->del_tap_req ?
         "its delete is running" : "a request of its own is in flight");
    send_status_ko(llid, tid, "Not ready");
    }
  else
    {
    if (!strlen(cur->lan_added))
      KERR("ERROR: %s %s", name, lan);
    else
      {
      mactopo_del_req(item_tap, name, 0, cur->lan_added);
      val = lan_del_name(cur->lan_added, item_tap, cur->name, 0);
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
        {
        KERR("ERROR tap %s: the del of lan %s could not be sent to ovs",
             name, lan);
        utils_send_status_ko(&(cur->llid), &(cur->tid), name);
        }
      }
    }
}

void ovs_tap_topo_endp_push(void)
{
  t_ovs_tap *cur = g_head_tap;
  while(cur)
    {
    if (strlen(cur->lan))
      cfg_topo_endp_push(cur->name, 0, cur->endp_type, cur->lan);
    cur = cur->next;
    }
}

static void snf_process_started(char *name, int num, char *vhost)
{
  t_ovs_tap *cur = find_tap(name);

  if (cur == NULL)
    KERR("ERROR %s %d", name, num);
  else
    {
    if (strlen(cur->lan))
      ovs_snf_send_add_snf_lan(name, num, cur->vhost, cur->lan);
    else
      KERR("ERROR %s %d %s", name, num, cur->lan);
    }
}

int  ovs_tap_dyn_snf(char *name, int val)
{
  int result = -1;
  t_ovs_tap *cur = find_tap(name);
  char tap[MAX_NAME_LEN];
  char *vh;

  if (!cur)
    KERR("ERROR snf %s on tap %s: no tap of that name", val ? "on" : "off",
         name);
  else if (val)
    {
    if (cur->endp_type == endp_type_taps)
      result = 0;
    else
      {
      cur->del_snf_ethv_sent = 0;
      cur->endp_type = endp_type_taps;
      ovs_dyn_snf_start_process(name, 0, item_type_tap,
                                cur->vhost, snf_process_started);
      result = 0;
      }
    }
  else
    {
    if (cur->endp_type == endp_type_tapv)
      result = 0;
    else
      {
      cur->endp_type = endp_type_tapv;
      memset(tap, 0, MAX_NAME_LEN);
      vh = cur->vhost;
      utils_get_snf_tap_name(tap, vh);
      if (cur->del_snf_ethv_sent == 0)
        {
        cur->del_snf_ethv_sent = 1;
        if (strlen(cur->lan))
          {
          if (ovs_snf_send_del_snf_lan(name, 0, cur->vhost, cur->lan))
            KERR("ERROR DEL KVMETH %s %s", name, cur->lan);
          }
        ovs_dyn_snf_stop_process(tap);
        }
      result = 0;
      }
    }
  return result;
}

/* A request whose answer never comes must not block the object for ever. */
static void timer_stuck_request(void *data)
{
  t_ovs_tap *cur = g_head_tap;
  while (cur)
    {
    if ((cur->llid) || (cur->del_tap_req))
      {
      cur->stuck_count += 1;
      if (cur->stuck_count > 60)
        {
        /* Both flags: the delete guard also refuses on del_tap_req, set while
           waiting for an ovs answer that may never come. */
        KERR("ERROR tap %s: no answer for %d seconds (llid=%d "
             "del_tap_req=%d), forgetting it so the tap can be deleted",
             cur->name, cur->stuck_count, cur->llid, cur->del_tap_req);
        cur->llid = 0;
        cur->tid = 0;
        cur->del_tap_req = 0;
        cur->stuck_count = 0;
        }
      }
    else
      cur->stuck_count = 0;
    cur = cur->next;
    }
  clownix_timeout_add(100, timer_stuck_request, NULL, NULL, NULL);
}

void ovs_tap_init(void)
{
  g_nb_tap = 0;
  g_head_tap = NULL;
  clownix_timeout_add(100, timer_stuck_request, NULL, NULL, NULL);
}

