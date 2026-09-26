/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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
#include "ovs.h"
#include "msg.h"
#include "fmt_diag.h"
#include "utils_cmd_line_maker.h"
#include "cnt.h"
#include "kvm.h"
#include "ovs_snf.h"
#include "ovs_tap.h"
#include "ovs_phy.h"
#include "ovs_nat_main.h"
#include "ovs_a2b.h"
#include "ovs_c2c.h"
#include "lan_to_name.h"
#include "c2c_chainlan.h"

typedef struct t_ovsreq
{
  int tid;
  int type;
  int time_count;
  char lan[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int  num;
  char vhost[MAX_NAME_LEN];
  struct t_ovsreq *prev;
  struct t_ovsreq *next;
} t_ovsreq;

static t_ovsreq *g_head_ovsreq;
static int g_qty_ovsreq;

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

static t_ovsreq *ovsreq_find_with_data(int type, char *name, int num,
                                       char *vhost, char *lan)
{
  t_ovsreq *cur = g_head_ovsreq;
  while(cur)
    {
    if ((cur->type == type) && (cur->num == num) &&
        (!strcmp(cur->name,name)) && (!strcmp(cur->vhost,vhost)) &&
        (!strcmp(cur->lan,lan))) 
      break;
    cur = cur->next;
    }
  return cur;
}

static t_ovsreq *ovsreq_alloc(int tid, int type, char *name, int num,
                              char *vhost, char *lan)
{
  t_ovsreq *cur = (t_ovsreq *) clownix_malloc(sizeof(t_ovsreq), 32);
  memset(cur, 0, sizeof(t_ovsreq));
  cur->tid = tid;
  cur->type = type;
  if (lan)
    strncpy(cur->lan, lan, MAX_NAME_LEN-1);
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->vhost, vhost, MAX_NAME_LEN-1);
  cur->num = num;
  if (g_head_ovsreq)
    g_head_ovsreq->prev = cur;
  cur->next = g_head_ovsreq;
  g_head_ovsreq = cur;
  g_qty_ovsreq += 1;
  return cur;
}

static void ovsreq_free(t_ovsreq *cur)
{
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur == g_head_ovsreq)
    g_head_ovsreq = cur->next;
  clownix_free(cur, __FUNCTION__);
  g_qty_ovsreq -= 1;
} 

int msg_send_add_lan_endp(int ovsreq_tag, char *name, int num,
                          char *vhost, char *lan)
{
  int result = -1, tid = utils_get_next_tid();
  if ((!name) || (!vhost) || (!lan))
    KERR("ERROR");
  else if ((strlen(lan) == 0) || (strlen(name) == 0) || (strlen(vhost) == 0))  
    KERR("ERROR %s %s %s", lan, name, vhost);
  else if ((ovsreq_tag != ovsreq_add_kvm_lan) && 
           (ovsreq_tag != ovsreq_add_cnt_lan) &&
           (ovsreq_tag != ovsreq_add_tap_lan) &&
           (ovsreq_tag != ovsreq_add_phy_lan) &&
           (ovsreq_tag != ovsreq_add_nat_lan) &&
           (ovsreq_tag != ovsreq_add_a2b_lan) &&
           (ovsreq_tag != ovsreq_add_c2c_lan))
    KERR("ERROR %s %d %s %s %d", name, num, vhost, lan, ovsreq_tag);
  else if (!lan_get_with_name(lan))
    KERR("ERROR %s %d %d %s", name, num, ovsreq_tag, lan);
  else if (fmt_tx_add_lan_endp(tid, name, num, vhost, lan))
    KERR("ERROR %s %d %s %s %d", name, num, vhost, lan, ovsreq_tag);
  else
    {
    ovsreq_alloc(tid, ovsreq_tag, name, num, vhost, lan);
    result = 0;
    }
  return result;
}

int msg_send_del_lan_endp(int ovsreq_tag, char *name, int num,
                          char *vhost,  char *lan)
{
  int result = -1, tid = utils_get_next_tid();
  if ((!name) || (!vhost) || (!lan))
    KERR("ERROR");
  else if ((strlen(lan) == 0) || (strlen(name) == 0) || (strlen(vhost) == 0))
    KERR("ERROR %s %s %s", lan, name, vhost);
  else if ((ovsreq_tag != ovsreq_del_kvm_lan) &&
           (ovsreq_tag != ovsreq_del_cnt_lan) &&
           (ovsreq_tag != ovsreq_del_tap_lan) &&
           (ovsreq_tag != ovsreq_del_phy_lan) &&
           (ovsreq_tag != ovsreq_del_nat_lan) &&
           (ovsreq_tag != ovsreq_del_a2b_lan) &&
           (ovsreq_tag != ovsreq_del_c2c_lan))
    KERR("ERROR %s %d %s %s %d", name, num, vhost, lan, ovsreq_tag);
  else if (!lan_get_with_name(lan))
    KTRC("del of lan %s for %s %d (req %d): the lan is already gone",
         lan, name, num, ovsreq_tag);
  else if (ovsreq_find_with_data(ovsreq_tag, name, num, vhost, lan))
    KERR("ERROR %s %d: a del of lan %s (req %d) is already in flight",
         name, num, lan, ovsreq_tag);
  else  if (fmt_tx_del_lan_endp(tid, name, num, vhost, lan))
    KERR("ERROR %s %d %s %s %d", name, num, vhost, lan, ovsreq_tag);
  else
    {
    ovsreq_alloc(tid, ovsreq_tag, name, num, vhost, lan);
    result = 0;
    }
  return result;
}

int msg_send_vhost_up(char *name, int num, char *vhost)
{ 
  int result = -1, tid = utils_get_next_tid();
  if ((!name) || (!vhost))
    KERR("ERROR");
  else if ((!strlen(name)) || (!strlen(vhost)))
    KERR("ERROR %s %s", name, vhost);
  else if (fmt_tx_vhost_up(tid, name, num, vhost))
    KERR("ERROR %s %d", name, num);
  else
    {
    ovsreq_alloc(tid, ovsreq_vhost_up, name, num, vhost, NULL);
    result = 0;
    }
  return result;
}

int msg_send_add_tap(char *name, char *vhost, char *mac)
{
  int result = -1, tid = utils_get_next_tid();
  if ((!name) || (!vhost) || (!mac))
    KERR("ERROR");
  else if (!(strlen(name)) || (!strlen(vhost)) || (!strlen(mac)))
    KERR("ERROR %s %s", name, mac);
  else if (fmt_tx_add_tap(tid, name, vhost, mac))
    KERR("ERROR %s %s %s", name, vhost, mac);
  else
    {
    ovsreq_alloc(tid, ovsreq_add_tap, name, 0, vhost, NULL);
    result = 0;
    }
  return result;
}

int msg_send_del_tap(char *name, char *vhost)
{
  int result = -1, tid = utils_get_next_tid();
  if ((!name) || (!vhost))
    KERR("ERROR");
  else if ((!strlen(name)) || (!strlen(vhost)))
    KERR("ERROR %s %s", name, vhost);
  else if (fmt_tx_del_tap(tid, name, vhost))
    KERR("ERROR %s %s", name, vhost);
  else
    {
    ovsreq_alloc(tid, ovsreq_del_tap, name, 0, vhost, NULL);
    result = 0;
    }
  return result;
}

int msg_send_add_phy(char *name, char *vhost, char *mac, int num_macvlan)
{
  int result = -1, tid = utils_get_next_tid();
  if ((!name) || (!vhost) || (!mac))
    KERR("ERROR");
  else if (!(strlen(name)) || (!strlen(vhost)) || (!strlen(mac)))
    KERR("ERROR %s %s", name, mac);
  else if (fmt_tx_add_phy(tid, name, vhost, mac, num_macvlan))
    KERR("ERROR %s %s %s", name, vhost, mac);
  else
    {
    ovsreq_alloc(tid, ovsreq_add_phy, name, num_macvlan, vhost, NULL);
    result = 0;
    }
  return result;
}

int msg_send_del_phy(char *name, char *vhost, int num_macvlan)
{
  int result = -1, tid = utils_get_next_tid();
  if ((!name) || (!vhost))
    KERR("ERROR");
  else if ((!strlen(name)) || (!strlen(vhost)))
    KERR("ERROR %s %s", name, vhost);
  else if (fmt_tx_del_phy(tid, name, vhost, num_macvlan))
    KERR("ERROR %s %s", name, vhost);
  else
    {
    ovsreq_alloc(tid, ovsreq_del_phy, name, num_macvlan, vhost, NULL);
    result = 0;
    }
  return result;
}

int msg_send_add_snf_lan(char *name, int num, char *vhost, char *lan)
{
  int result = -1, tid = utils_get_next_tid();
  if ((!name) || (!vhost) || (!lan))
    KERR("ERROR");
  else if (!(strlen(name)) || (!strlen(vhost)) || (!strlen(lan)))
    KERR("ERROR %s %s %s", name, vhost, lan);
  else if (fmt_tx_add_snf_lan(tid, name, num, vhost, lan))
    KERR("ERROR %s %d", name, num);
  else
    {
    ovsreq_alloc(tid, ovsreq_add_snf_lan, name, num, vhost, lan);
    result = 0;
    }
  return result;
}

int msg_send_del_snf_lan(char *name, int num, char *vhost, char *lan)
{
  int result = -1, tid = utils_get_next_tid();
  if ((!name) || (!vhost) || (!lan))
    KERR("ERROR");
  else if (!(strlen(name)) || (!strlen(vhost)) || (!strlen(lan)))
    KERR("ERROR %s %s %s", name, vhost, lan);
  else if (fmt_tx_del_snf_lan(tid, name, num, vhost, lan))
    KERR("ERROR %s %d", name, num);
  else
    {
    ovsreq_alloc(tid, ovsreq_del_snf_lan, name, num, vhost, lan);
    result = 0;
    }
  return result;
}

static void transmit_add_ack(int tid, t_ovsreq *cur, int is_ko)
{
  if (!lan_get_with_name(cur->lan))
    {
    KERR("WARNING %s %s %d: the lan went away while the add was in flight, "
         "refusing the add instead of recreating it",
         cur->lan, cur->name, cur->num);
    lan_history_dump(cur->lan);
    int del_type = 0;
    is_ko = 1;
    if (cur->type == ovsreq_add_cnt_lan)      del_type = ovsreq_del_cnt_lan;
    else if (cur->type == ovsreq_add_kvm_lan) del_type = ovsreq_del_kvm_lan;
    else if (cur->type == ovsreq_add_tap_lan) del_type = ovsreq_del_tap_lan;
    else if (cur->type == ovsreq_add_phy_lan) del_type = ovsreq_del_phy_lan;
    else if (cur->type == ovsreq_add_nat_lan) del_type = ovsreq_del_nat_lan;
    else if (cur->type == ovsreq_add_c2c_lan) del_type = ovsreq_del_c2c_lan;
    else if (cur->type == ovsreq_add_a2b_lan) del_type = ovsreq_del_a2b_lan;
    if (del_type)
      msg_send_del_lan_endp(del_type, cur->name, cur->num,
                            cur->vhost, cur->lan);
    else
      KERR("ERROR no del request known for add type %d", cur->type);
    }
  if (cur->type == ovsreq_add_cnt_lan)
    cnt_resp_add_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_add_kvm_lan)
    kvm_resp_add_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_add_tap_lan)
    ovs_tap_resp_add_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_add_phy_lan)
    ovs_phy_resp_add_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_add_nat_lan)
    ovs_nat_main_resp_add_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_add_a2b_lan)
    ovs_a2b_resp_add_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_add_c2c_lan)
    c2c_chainlan_resp_add_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else
    KERR("ERROR %s %d %s %s %d", cur->name, cur->num,
                                 cur->vhost, cur->lan, cur->type);
}

static void transmit_del_ack(int tid, t_ovsreq *cur, int is_ko)
{

  if (cur->type == ovsreq_del_cnt_lan)
    cnt_resp_del_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_del_kvm_lan)
    kvm_resp_del_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_del_tap_lan)
    ovs_tap_resp_del_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_del_phy_lan)
    ovs_phy_resp_del_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_del_nat_lan)
    ovs_nat_main_resp_del_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_del_a2b_lan)
    ovs_a2b_resp_del_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else if (cur->type == ovsreq_del_c2c_lan)
    c2c_chainlan_resp_del_lan(is_ko, cur->name, cur->num, cur->vhost, cur->lan);
  else
    KERR("ERROR %s %d %s %s %d", cur->name, cur->num,
                                 cur->vhost, cur->lan, cur->type);

}

void msg_ack_lan_endp(int tid, int is_ko, int is_add, char *name, int num,
                      char *vhost, char *lan)
{
  t_ovsreq *cur = ovsreq_find(tid);
  if (cur)
    {
    if (strcmp(lan, cur->lan))
      KERR("ERROR %s %s is_add:%d is_ko:%d %d",lan,cur->lan,is_add,is_ko,tid);
    else if (strcmp(name, cur->name))
      KERR("ERROR %s %s is_add:%d is_ko:%d", name, cur->name, is_add,is_ko);
    else if (strcmp(vhost, cur->vhost))
      KERR("ERROR %s %s is_add:%d is_ko:%d", vhost, cur->vhost,is_add,is_ko);
    else if (num != cur->num)
      KERR("ERROR %d %d is_add:%d is_ko:%d", num, cur->num, is_add, is_ko);
    else 
      {
      if (is_add)
        transmit_add_ack(tid, cur, is_ko);
      else
        transmit_del_ack(tid, cur, is_ko);
      }
    ovsreq_free(cur);
    }
}

void msg_ack_lan(int tid, int is_ko, int is_add, char *lan)
{
  /* Report only; transmit_add_ack above refuses a late add. */
  if (is_add)
    {
    if (!lan_get_with_name(lan))
      {
      int st = lan_history_state(lan);
      if (st == LAN_HISTORY_REUSED)
        KTRC("the add ack for lan %s arrived after the name was reused",
             lan);
      else if (st == LAN_HISTORY_DIED)
        KTRC("the add ack for lan %s arrived after it was deleted", lan);
      else
        {
        KERR("WARNING the add ack for lan %s arrived after the lan was "
             "freed, and the ring has no history of that name at all",
             lan);
        lan_history_dump(lan);
        }
      }
    }
  else
    {
    if (lan_get_with_name(lan))
      {
      if (lan_history_state(lan) == LAN_HISTORY_REUSED)
        KTRC("the del ack for lan %s arrived after the name was reused",
             lan);
      else
        {
        /* A lan of that name is present and the ring shows no rebirth, so the
           delete really did not take. */
        KERR("ERROR the del ack for lan %s arrived and a lan of that name "
             "is here, and the name was NOT reused: the delete did not "
             "take", lan);
        lan_history_dump(lan);
        }
      }
    }
  if (is_ko)
    {
    if (is_add)
      KERR("ERROR ADD LAN %s", lan);
    else 
      KERR("ERROR DEL LAN %s", lan);
    }
}

void msg_ack_vhost_up(int tid, int is_ko, char *name, int num, char *vhost)
{
  t_ovsreq *cur = ovsreq_find(tid);
  if (!cur)
    {
    KERR("ERROR is_ko:%d name:%s num:%d vhost:%s",
         is_ko, name, num, vhost);
    }
  else
    {
    ovs_snf_resp_msg_vhost_up(is_ko, vhost, name, num);
    ovsreq_free(cur);
    }
}

void msg_ack_tap(int tid, int is_ko, int is_add, char *name,
                 char *vhost, char *mac)
{
  t_ovsreq *cur = ovsreq_find(tid);
  if (!cur)
    {
    KERR("ERROR is_ko:%d is_add:%d name:%s vhost:%s mac:%s",
         is_ko, is_add, name, vhost, mac);
    }
  else
    {
    ovs_tap_resp_msg_tap(is_ko, is_add, name);
    ovsreq_free(cur);
    }
}

void msg_ack_phy(int tid, int is_ko, int is_add, char *name,
                 int num_macvlan, char *vhost)
{
  t_ovsreq *cur = ovsreq_find(tid);
  if (!cur)
    {
    KERR("ERROR is_ko:%d is_add:%d name:%s vhost:%s",
         is_ko, is_add, name, vhost);
    }
  else
    {
    if (strcmp(vhost, cur->vhost))
      KERR("ERROR name:%s %s %s", name, vhost, cur->vhost);
    ovs_phy_resp_msg_phy(is_ko, is_add, name, cur->num, vhost);
    ovsreq_free(cur);
    }
}

void msg_ack_snf_ethv(int tid, int is_ko, int is_add, char *name, int num,
                      char *vhost, char *lan)
{
  t_ovsreq *cur = ovsreq_find(tid);
  if (!cur)
    {
    KERR("ERROR is_ko:%d is_add:%d name:%s num:%d vhost:%s",
         is_ko, is_add, name, num, vhost);
    }
  else
    {
    if (is_add == 1)
      ovs_snf_resp_msg_add_lan(is_ko, vhost, name, num, lan);
    else
      ovs_snf_resp_msg_del_lan(is_ko, vhost, name, num, lan);
    ovsreq_free(cur);
    }
}

static void timer_msg_beat(void *data)
{
  t_ovsreq *next, *cur = g_head_ovsreq;
  /* One line every few seconds while the queue is deep: head and its wait. */
  static int trace_beat;
  trace_beat += 1;
  if ((cur) && (g_qty_ovsreq > 20) && ((trace_beat % 50) == 0))
    KERR("WARNING ovs queue %d deep, head: type %d tid %d %s %s waited %d "
         "beats", g_qty_ovsreq, cur->type, cur->tid, cur->name, cur->lan,
         cur->time_count);
  while(cur)
    {
    next = cur->next;
    cur->time_count += 1;
    if (cur->time_count > 100)
      {
      switch(cur->type)
        {

        case ovsreq_add_kvm_lan:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                            cur->name, cur->num);
          kvm_resp_add_lan(1, cur->name, cur->num,
                           cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_del_kvm_lan:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                            cur->name, cur->num);
          kvm_resp_del_lan(1, cur->name, cur->num,
                           cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_add_cnt_lan:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                            cur->name, cur->num);
          cnt_resp_add_lan(1, cur->name, cur->num,
                           cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_del_cnt_lan:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                            cur->name, cur->num);
          cnt_resp_del_lan(1, cur->name, cur->num,
                           cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_vhost_up:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->vhost,
                                      cur->name, cur->num);
          ovs_snf_resp_msg_vhost_up(1, cur->vhost, cur->name, cur->num);
          ovsreq_free(cur);
        break;

        case ovsreq_add_tap:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->vhost, cur->name );
          ovs_tap_resp_msg_tap(1, 1, cur->name);
          ovsreq_free(cur);
        break;

        case ovsreq_del_tap:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->vhost, cur->name );
          ovs_tap_resp_msg_tap(1, 0, cur->name);
          ovsreq_free(cur);
        break;

        case ovsreq_add_phy:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->vhost, cur->name );
          ovs_phy_resp_msg_phy(1, 1, cur->name, cur->num, cur->vhost);
          ovsreq_free(cur);
        break;

        case ovsreq_del_phy:
          KERR("ERROR TIMEOUT %d %s %s", cur->tid, cur->vhost, cur->name );
          ovs_phy_resp_msg_phy(1, 0, cur->name, cur->num, cur->vhost);
          ovsreq_free(cur);
        break;

        case ovsreq_add_snf_lan:
          KERR("ERROR TIMEOUT %d %s %s %d %s", cur->tid, cur->vhost,
                                      cur->name, cur->num, cur->lan);
          ovs_snf_resp_msg_add_lan(1,cur->vhost,cur->name,cur->num,cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_del_snf_lan:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                            cur->name, cur->num);
          ovs_snf_resp_msg_del_lan(1,cur->vhost,cur->name,cur->num,cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_add_tap_lan:
          KERR("ERROR TIMEOUT %d %s %s %s", cur->tid, cur->vhost,
                                            cur->name, cur->lan);
          ovs_tap_resp_add_lan(1, cur->name, 0, cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_del_tap_lan:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                            cur->name, cur->num);
          ovs_tap_resp_del_lan(1, cur->name, 0, cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_add_phy_lan:
          KERR("ERROR TIMEOUT %d %s %s %s", cur->tid, cur->vhost,
                                            cur->name, cur->lan);
          ovs_phy_resp_add_lan(1, cur->name, 0, cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_del_phy_lan:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                            cur->name, cur->num);
          ovs_phy_resp_del_lan(1, cur->name, 0, cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

       case ovsreq_add_nat_lan:
          KERR("ERROR TIMEOUT %d %s %s %s", cur->tid, cur->vhost,
                                            cur->name, cur->lan);
          ovs_nat_main_resp_add_lan(1, cur->name, 0, cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_del_nat_lan:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                            cur->name, cur->num);
          ovs_nat_main_resp_del_lan(1, cur->name, 0, cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

       case ovsreq_add_a2b_lan:
          KERR("ERROR TIMEOUT %d %s %s %s", cur->tid, cur->vhost,
                                            cur->name, cur->lan);
          ovs_a2b_resp_add_lan(1, cur->name, cur->num, cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_del_a2b_lan:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                            cur->name, cur->num);
          ovs_a2b_resp_del_lan(1, cur->name, cur->num, cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

       case ovsreq_add_c2c_lan:
          KERR("ERROR TIMEOUT %d %s %s %s", cur->tid, cur->vhost,
                                            cur->name, cur->lan);
          c2c_chainlan_resp_add_lan(1, cur->name, 0, cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

        case ovsreq_del_c2c_lan:
          KERR("ERROR TIMEOUT %d %s %s %d", cur->tid, cur->lan,
                                            cur->name, cur->num);
          c2c_chainlan_resp_del_lan(1, cur->name, 0, cur->vhost, cur->lan);
          ovsreq_free(cur);
        break;

        default:
          KOUT("%d", cur->type);
        }
      }
    cur = next;
    }
  clownix_timeout_add(200, timer_msg_beat, NULL, NULL, NULL);
}

void msg_lan_add_name(char *lan)
{
  int tid = utils_get_next_tid();
  if (fmt_tx_add_lan(tid, lan))
    KERR("ERROR %s", lan);
}

/* rstp: the client is answered on the OK or KO of ovs_drv, never before.  */
typedef struct t_rstp_wait
{
  int llid;
  int tid;
  int fmt_tid;
  char lan[MAX_NAME_LEN];
  struct t_rstp_wait *next;
} t_rstp_wait;
static t_rstp_wait *g_rstp_wait;

static t_rstp_wait *rstp_wait_take(int fmt_tid)
{
  t_rstp_wait *cur = g_rstp_wait, *prev = NULL;
  while (cur && (cur->fmt_tid != fmt_tid))
    {
    prev = cur;
    cur = cur->next;
    }
  if (cur)
    {
    if (prev)
      prev->next = cur->next;
    else
      g_rstp_wait = cur->next;
    }
  return cur;
}

static void timer_rstp_wait(void *data)
{
  t_rstp_wait *rw = rstp_wait_take(((t_rstp_wait *) data)->fmt_tid);
  if (rw)
    {
    KERR("ERROR rstp %s: no answer from ovs_drv in 5 s", rw->lan);
    send_status_ko(rw->llid, rw->tid, "rstp: no answer from ovs_drv");
    clownix_free(rw, __FILE__);
    }
}

void msg_lan_rstp(int llid, int tid, char *lan, int val)
{
  t_rstp_wait *rw;
  int fmt_tid = utils_get_next_tid();
  if (fmt_tx_add_lan_rstp(fmt_tid, lan, val))
    {
    KERR("ERROR rstp %s val=%d: not sent to ovs_drv", lan, val);
    send_status_ko(llid, tid, "rstp: not sent to ovs_drv");
    return;
    }
  rw = (t_rstp_wait *) clownix_malloc(sizeof(t_rstp_wait), 3);
  memset(rw, 0, sizeof(t_rstp_wait));
  rw->llid = llid;
  rw->tid = tid;
  rw->fmt_tid = fmt_tid;
  strncpy(rw->lan, lan, MAX_NAME_LEN-1);
  rw->next = g_rstp_wait;
  g_rstp_wait = rw;
  clownix_timeout_add(500, timer_rstp_wait, (void *) rw, NULL, NULL);
}

void msg_lan_rstp_resp(int fmt_tid, int ok, char *lan, int val)
{
  t_rstp_wait *rw = rstp_wait_take(fmt_tid);
  if (rw == NULL)
    KERR("ERROR rstp %s val=%d: %s came after the 5 s wait, nobody told",
         lan, val, ok ? "OK" : "KO");
  else
    {
    if (ok)
      {
      lan_rstp_set(lan, val);
      send_status_ok(rw->llid, rw->tid, "ok");
      }
    else
      {
      KERR("ERROR rstp %s val=%d: KO from ovs_drv", lan, val);
      send_status_ko(rw->llid, rw->tid, "rstp: KO from ovs_drv");
      }
    clownix_free(rw, __FILE__);
    }
}

void msg_lan_del_name(char *lan)
{
  int tid = utils_get_next_tid();
  if (fmt_tx_del_lan(tid, lan))
    KERR("ERROR %s", lan);
}

int msg_ovsreq_exists_name(char *name)
{
  int result = 0;
  t_ovsreq *cur = g_head_ovsreq;
  while(cur)
    {
    if (!strcmp(cur->name,name))
      {
      result = 1;
      break;
      }
    cur = cur->next;
    }
  return result;
}

int msg_ovsreq_get_qty(void)
{
  return g_qty_ovsreq;
}

void msg_init(void)
{
  g_qty_ovsreq = 0;
  g_head_ovsreq = NULL;
  clownix_timeout_add(100, timer_msg_beat, NULL, NULL, NULL);
}

