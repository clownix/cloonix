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
#include "machine_create.h"
#include "utils_cmd_line_maker.h"
#include "event_subscriber.h"
#include "ovs.h"
#include "fmt_diag.h"
#include "msg.h"
#include "suid_power.h"
#include "ovs_snf.h"
#include "lan_to_name.h"
#include "kvm.h"
#include "ovs_phy.h"
#include "mactopo.h"

typedef struct t_alloc_delay
{
  char name[MAX_NAME_LEN];
  int num;
  int count;
} t_alloc_delay;

static t_ethv_cnx *g_head_ethv;
static int g_nb_ethv;

static t_ethv_cnx *ethv_find(char *name, int num)
{
  t_ethv_cnx *cur = g_head_ethv;
  if (strlen(name))
    {
    while(cur && (strcmp(cur->name, name) || (cur->num != num)))
      {
      cur = cur->next;
      }
    }
  return cur;
}

static void ethv_alloc(char *name, int num, int endp_type, char *vhost)
{
  t_ethv_cnx *cur  = ethv_find(name, num);
  if (cur)
    KERR("ERROR KVMETH ALLOC ETHD %s %d", name, num);
  else
    {
    cur  = (t_ethv_cnx *) clownix_malloc(sizeof(t_ethv_cnx), 6);
    memset(cur, 0, sizeof(t_ethv_cnx));
    strncpy(cur->name, name, MAX_NAME_LEN-1);
    if (vhost)
      strncpy(cur->vhost, vhost, MAX_NAME_LEN-1);
    cur->num = num;
    cur->endp_type = endp_type;
    if (g_head_ethv)
      g_head_ethv->prev = cur;
    cur->next = g_head_ethv;
    g_head_ethv = cur;
    g_nb_ethv += 1;
    }
}

static void ethv_free(char *name, int num)
{
  t_ethv_cnx *cur  = ethv_find(name, num);
  if (!cur)
    KERR("ERROR %s %d", name, num);
  else
    {
    if (strlen(cur->lan_added))
      {
      KERR("ERROR %s %d %s", name, num, cur->lan_added);
      mactopo_del_req(item_kvm, name, num, cur->lan_added);
      lan_del_name(cur->lan_added, item_kvm, name, num);
      }
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == g_head_ethv)
      g_head_ethv = cur->next;
    clownix_free(cur, __FUNCTION__);
    g_nb_ethv -= 1;
    }
}

#define KVM_ETH_READY_POLL 4
#define KVM_ETH_READY_FALLBACK_MS 250
#define KVM_ETH_READY_MAX_TRIES 1000

static int g_said_fallback;
static unsigned long g_tap_nb;
static unsigned long g_tap_total_ms;
static int g_tap_min_ms;
static int g_tap_max_ms;

static int tap_is_in_the_netns(char *vhost)
{
  char path[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  FILE *fp;
  int pid, len, result = -1;
  char *ptr;
  pid = ovs_get_a_pid_in_the_netns();
  if (pid <= 0)
    return -1;
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/net/dev", pid);
  fp = fopen(path, "r");
  if (!fp)
    return -1;
  result = 0;
  len = strlen(vhost);
  while (fgets(line, MAX_PATH_LEN, fp))
    {
    ptr = line;
    while ((*ptr == ' ') || (*ptr == '\t'))
      ptr++;
    if ((!strncmp(ptr, vhost, len)) && (ptr[len] == ':'))
      {
      result = 1;
      break;
      }
    }
  fclose(fp);
  return result;
}

static void timer_alloc_delay(void *data)
{
  t_alloc_delay *ald = (t_alloc_delay *) data;
  t_ethv_cnx *cur  = ethv_find(ald->name, ald->num);
  int there;
  if (!cur)
    {
    KTRC("%s %d: the eth record went while waiting for its tap", ald->name,
         ald->num);
    clownix_free(ald, __FUNCTION__);
    return;
    }
  ald->count += 1;
  there = cur->vhost[0] ? tap_is_in_the_netns(cur->vhost) : -1;
  if (there == 0)
    {
    /* Not there yet, and we can see that it is not. */
    if (ald->count < KVM_ETH_READY_MAX_TRIES)
      {
      clownix_timeout_add(KVM_ETH_READY_POLL, timer_alloc_delay, ald,
                          NULL, NULL);
      return;
      }
    KERR("ERROR %s %d: its tap %s never appeared in the net namespace, the "
         "eth is left unusable", ald->name, ald->num, cur->vhost);
    clownix_free(ald, __FUNCTION__);
    return;
    }
  if (there == -1)
    {
    if ((ald->count * KVM_ETH_READY_POLL * 5) < KVM_ETH_READY_FALLBACK_MS)
      {
      clownix_timeout_add(KVM_ETH_READY_POLL, timer_alloc_delay, ald,
                          NULL, NULL);
      return;
      }
    if (!g_said_fallback)
      {
      g_said_fallback = 1;
      KERR("WARNING cannot read the net namespace of the ovs process, so "
           "guest eths fall back to the fixed %d ms delay before they can "
           "be cabled (first seen on %s %d, tap %s)",
           KVM_ETH_READY_FALLBACK_MS, ald->name, ald->num, cur->vhost);
      }
    }
  else
    {
    int ms = ald->count * KVM_ETH_READY_POLL * 5;
    g_tap_nb += 1;
    g_tap_total_ms += ms;
    if ((g_tap_min_ms == 0) || (ms < g_tap_min_ms))
      g_tap_min_ms = ms;
    if (ms > g_tap_max_ms)
      g_tap_max_ms = ms;
    if ((g_tap_nb % 50) == 1)
      KTRC("guest eth taps: %lu seen, %d ms mean, %d ms min, %d ms max "
           "(the fixed delay this replaced was %d ms)",
           g_tap_nb, (int)(g_tap_total_ms / g_tap_nb), g_tap_min_ms,
           g_tap_max_ms, KVM_ETH_READY_FALLBACK_MS);
    }
  cur->ready = 1;
  clownix_free(ald, __FUNCTION__);
}

static int kvm_add_vm(char *name, int num, int endp_type, char *vhost)
{
  int result;
  t_alloc_delay *ald;
  if ((endp_type == endp_type_eths) ||
      (endp_type == endp_type_ethv))
    {
    ald = (t_alloc_delay *) clownix_malloc(sizeof(t_alloc_delay), 6);
    memset(ald, 0, sizeof(t_alloc_delay));
    strncpy(ald->name, name, MAX_NAME_LEN-1);
    ald->num = num;
    ethv_alloc(name, num, endp_type, vhost);
    clownix_timeout_add(KVM_ETH_READY_POLL, timer_alloc_delay, ald,
                        NULL, NULL);
    result = 0;
    }
  else
    KOUT("%d", endp_type);
  return result;
}

/* The eth record exists, ready or not. */
int kvm_eth_present(char *name, int num)
{
  return (ethv_find(name, num) != NULL);
}

/* the vhost_up ack came: a snf may start on the eth */
int kvm_vhost_up(char *name, int num, int set)
{
  t_ethv_cnx *cur = ethv_find(name, num);
  if ((cur) && (set))
    cur->vhost_up = 1;
  return ((cur) && (cur->vhost_up));
}

int kvm_exists(char *name, int num)
{
  int result = 0;
  t_ethv_cnx *cur = ethv_find(name, num);
  if ((cur) && (cur->ready == 1))
    result = cur->endp_type;
  return result;
}

void kvm_add_whole_vm(char *name, int nb_tot_eth, t_eth_table *eth_tab)
{
  int i;
  char *eth_name;
  for (i = 0; i < nb_tot_eth; i++)
    {
    eth_name = eth_tab[i].vhost_ifname;
    if ((eth_tab[i].endp_type == endp_type_eths) ||
        (eth_tab[i].endp_type == endp_type_ethv))
      {
      kvm_add_vm(name, i, eth_tab[i].endp_type, eth_name);
      }
    else
      KERR("ERROR %s %s", name, eth_name);
    }
}

int kvm_lan_added_exists(char *name, int num)
{
  int result = 0;
  t_ethv_cnx *cur  = ethv_find(name, num);
    if (cur != NULL)
      {
      if (strlen(cur->lan_added))
        result = 1;
      if (cur->attached_lan_ok)
        result = 1;
      if (cur->waiting_ack_del_lan)
        result = 1;
      if (strlen(cur->lan))
        result = 1;
      }
  return result;
}

void kvm_del(int llid, int tid, char *name, int num,
             int endp_type, int *can_delete)
{
  int val;
  t_ethv_cnx *cur;
  char tap[MAX_NAME_LEN];
  char *vh;

  *can_delete = 1;
  if ((endp_type == endp_type_eths) ||
      (endp_type == endp_type_ethv))
    {
    cur  = ethv_find(name, num);
    if (cur != NULL)
      {
      if ((endp_type == endp_type_eths) &&
          (cur->del_snf_ethv_sent == 0))
        {
        cur->del_snf_ethv_sent = 1;
        *can_delete = 0;
        if (strlen(cur->lan))
          {
          if (ovs_snf_send_del_snf_lan(name, num, cur->vhost, cur->lan))
            KERR("ERROR SNF %s %d %s %s", name, num, cur->vhost, cur->lan);
          }
        memset(tap, 0, MAX_NAME_LEN);
        vh = cur->vhost;
        utils_get_snf_tap_name(tap, vh);
        ovs_dyn_snf_stop_process(tap);
        }
      else if ((ovs_snf_is_running(name, num)) && (cur->del_snf_ethv_sent == 0))
        {
        KERR("WARNING %s %d: being deleted with a capture running and no "
             "stop ever sent for it (endp_type=%d, eths is %d) -- "
             "stopping it now",
             name, num, endp_type, endp_type_eths);
        memset(tap, 0, MAX_NAME_LEN);
        utils_get_snf_tap_name(tap, cur->vhost);
        ovs_dyn_snf_stop_process(tap);
        }
      else if (strlen(cur->lan_added))
        {
        mactopo_del_req(item_kvm, name, num, cur->lan_added);
        *can_delete = 0;
        val = lan_del_name(cur->lan_added, item_kvm, name, num);
        if (val != 2*MAX_LAN)
          {
          if (msg_send_del_lan_endp(ovsreq_del_kvm_lan, 
                            name, num, cur->vhost, cur->lan_added))
            {
            KERR("ERROR %s %d", name, num);
            *can_delete = 1;
            ethv_free(name, num);
            }
          }
        memset(cur->lan_added, 0, MAX_NAME_LEN);
        }
      else
        {
        ethv_free(name, num);
        }
      }
    }
  else
    KERR("ERROR DELETH %s %d %d", name, num, endp_type);
}

void kvm_resp_add_lan(int is_ko, char *name, int num, char *vhost, char *lan)
{
  t_ethv_cnx *cur = ethv_find(name, num);
  if (!cur)
    {
    mactopo_add_resp(0, item_kvm, name, num, lan);
    KERR("ERROR ovs answered the add of %s %d to lan %s (ko=%d) after the "
         "eth was released: the kvm went while the add was in flight",
         name, num, lan, is_ko);
    }
  else
    {
    if (is_ko)
      {
      mactopo_add_resp(0, item_kvm, name, num, lan);
      KTRC("%s %d: the add of lan %s was refused", name, num, lan);
      utils_send_status_ko(&(cur->llid), &(cur->tid), "KO");
      }
    else
      {
      mactopo_add_resp(1, item_kvm, name, num, lan);
      cur->attached_lan_ok = 1;
      utils_send_status_ok(&(cur->llid), &(cur->tid));
      if ((cur->endp_type == endp_type_eths) &&
          (strlen(cur->lan)))
        {
        ovs_snf_send_add_snf_lan(cur->name, cur->num, cur->vhost, cur->lan);
        }
      }
    cur->waiting_ack_add_lan = 0;
    cfg_hysteresis_send_topo_info();
    }
}

void kvm_resp_del_lan(int is_ko, char *name, int num, char *vhost, char *lan)
{
  t_ethv_cnx *cur = ethv_find(name, num);
  if (!cur)
    {
    mactopo_del_resp(0, name, num, lan);
    {
    static unsigned long nb_late_del;
    nb_late_del += 1;
    KTRC("%s eth %d: the del of lan %s arrived after the guest was gone "
         "(%lu so far)", name, num, lan, nb_late_del);
    }
    }
  else
    {
    mactopo_del_resp(item_kvm, name, num, lan);
    if ((cur->endp_type == endp_type_eths) &&
        (strlen(cur->lan)) &&
        (cur->del_snf_ethv_sent == 0))
      {
      if (ovs_snf_send_del_snf_lan(name, num, vhost, lan))
        KERR("ERROR %s %d %s %s", name, num, vhost, lan);
      }
    if (cur->attached_lan_ok == 1)
      {
      if (!strlen(cur->lan))
        KERR("ERROR: %s %d", name, num);
      }
    cur->attached_lan_ok = 0;
    cur->waiting_ack_del_lan = 0;
    memset(cur->lan, 0, MAX_NAME_LEN);
    utils_send_status_ok(&(cur->llid),&(cur->tid));
    cfg_hysteresis_send_topo_info();
    }
}

/* The reason of a refusal goes to info, the caller sends it. */
int kvm_add_lan(int llid, int tid, char *name, int num,
                     char *lan, int endp_type, t_eth_table *eth_tab,
                     char *info)
{
  int result = -1;
  t_ethv_cnx *cur;
  char err[MAX_PATH_LEN];
  memset(info, 0, MAX_PATH_LEN);
  if ((endp_type == endp_type_eths) ||
      (endp_type == endp_type_ethv))
    {
    cur  = ethv_find(name, num);
    if (cur == NULL)
      snprintf(info, MAX_PATH_LEN-1, "%s eth%d has no interface record",
               name, num);
    else if (cur->waiting_ack_del_lan)
      snprintf(info, MAX_PATH_LEN-1, "%s eth%d is being uncabled from lan "
               "%s, ask again when that is done", name, num, cur->lan);
    else if (cur->waiting_ack_add_lan)
      snprintf(info, MAX_PATH_LEN-1, "%s eth%d is being cabled to lan %s",
               name, num, cur->lan);
    else if (cur->attached_lan_ok)
      snprintf(info, MAX_PATH_LEN-1, "%s eth%d is already cabled to lan %s",
               name, num, cur->lan);
    else
      {
      lan_add_name(lan, item_kvm, name, num);
      if (mactopo_add_req(item_kvm, name, num, lan,
                          cur->vhost, eth_tab[num].mac_addr, err))
        {
        snprintf(info, MAX_PATH_LEN-1, "%s eth%d lan %s: %s",
                 name, num, lan, err);
        lan_del_name(lan, item_kvm, name, num);
        }
      else if (msg_send_add_lan_endp(ovsreq_add_kvm_lan, name, num,
                                     cur->vhost,lan))
        {
        snprintf(info, MAX_PATH_LEN-1, "%s eth%d lan %s: the ovs request "
                 "could not be sent", name, num, lan);
        lan_del_name(lan, item_kvm, name, num);
        }
      else
        {
        strncpy(cur->lan_added, lan, MAX_NAME_LEN-1);
        if (strlen(cur->lan))
          {
          KERR("ERROR %s eth%d still named lan %s, replaced by %s",
               name, num, cur->lan, lan);
          memset(cur->lan, 0, MAX_NAME_LEN);
          }
        cur->waiting_ack_add_lan = 1;
        strncpy(cur->lan, lan, MAX_NAME_LEN-1);
        cur->llid = llid;
        cur->tid = tid;
        result = 0;
        }
      }
    }
  else
    snprintf(info, MAX_PATH_LEN-1, "%s eth%d is of a kind that takes no "
             "lan", name, num);
  if (result)
    KERR("ERROR %s", info);
  return result;
}

int kvm_del_lan(int llid, int tid, char *name, int num, char *lan,
                     int endp_type)
{
  int val=0,  result = -1;
  t_ethv_cnx *cur;
  if ((endp_type == endp_type_eths) ||
      (endp_type == endp_type_ethv))
    {
    cur  = ethv_find(name, num);
    if (cur == NULL)
      KERR("ERROR DEL LAN ETHV %s %d %s", name, num, lan);
    else if (cur->waiting_ack_del_lan)
      KERR("ERROR DEL LAN %s %d %s", name, num, lan);
    else if (cur->waiting_ack_add_lan)
      KERR("ERROR DEL LAN %s %d %s", name, num, lan);
    else if (cur->attached_lan_ok == 0)
      KERR("ERROR DEL LAN %s %d %s", name, num, lan);
    else if (strcmp(lan, cur->lan))
      KERR("ERROR DEL LAN %s %d %s %s", name, num, lan, cur->lan);
    else
      {
      if (!strlen(cur->lan_added))
        KERR("ERROR: %s %d %s", name, num, lan);
      else
        {
        mactopo_del_req(item_kvm, name, num, cur->lan_added);
        val = lan_del_name(cur->lan_added, item_kvm, name, num);
        memset(cur->lan_added, 0, MAX_NAME_LEN);
        }
      cur->llid = llid;
      cur->tid = tid;
      if (val != 2*MAX_LAN)
        {
        if (msg_send_del_lan_endp(ovsreq_del_kvm_lan,name,num,cur->vhost,lan))
          KERR("ERROR: %s %d %s", name, num, lan);
        cur->waiting_ack_del_lan = 1;
        }
      else
        {
        cur->attached_lan_ok = 0;
        cur->waiting_ack_del_lan = 0;
        memset(cur->lan, 0, MAX_NAME_LEN);
        utils_send_status_ok(&(cur->llid),&(cur->tid));
        }
      result = 0;
      }
    }
  else
    KERR("ERROR KVMETH DELLAN %s %d %s %d", name, num, lan, endp_type);
  return result;
}

void kvm_topo_endp_push(void)
{
  t_ethv_cnx *cur = g_head_ethv;
  while(cur)
    {
    cfg_topo_endp_push(cur->name, cur->num, cur->endp_type, cur->lan);
    cur = cur->next;
    }
}

static void snf_process_started(char *name, int num, char *vhost)
{
  t_ethv_cnx *cur  = ethv_find(name, num);
  char *eth_name;
  t_vm *vm = cfg_get_vm(name);
  t_eth_table *eth_tab;

  if (vm == NULL)
    KERR("ERROR snf asked for %s %d: there is no such machine (it was "
         "deleted while the request was in flight)", name, num);
  else if (cur == NULL)
    KERR("ERROR snf asked for %s %d: the machine is there but eth%d has "
         "no vhost connection, so nothing can be captured on it",
         name, num, num);
  else
    {
    eth_tab = vm->kvm.eth_table;
    if (cur->attached_lan_ok == 1)
      {
      eth_name = eth_tab[num].vhost_ifname;
      ovs_snf_send_add_snf_lan(name, num, eth_name, cur->lan);
      }
    }
}

int kvm_dyn_snf(char *name, int num, int val)
{
  char *eth_name;
  int result = -1;
  t_ethv_cnx *cur  = ethv_find(name, num);
  char tap[MAX_NAME_LEN];
  t_vm *vm = cfg_get_vm(name);
  t_eth_table *eth_tab;

  if (vm == NULL)
    KERR("ERROR snf %d asked on %s %d: no kvm of that name", val, name, num);
  else if (cur == NULL)
    KERR("ERROR snf %d asked on %s %d: the kvm exists but eth%d has no vhost "
         "record yet (still starting) or no more (going)", val, name, num, num);
  else
    {
    eth_tab = vm->kvm.eth_table;
    if (val)
      {
      if (eth_tab[num].endp_type == endp_type_eths)
        {
        if (ovs_snf_is_running(name, num))
          result = 0;
        else
          {
          eth_name = eth_tab[num].vhost_ifname;
          cur->del_snf_ethv_sent = 0;
          KERR("WARNING %s %d is marked sniffable and has no capture "
               "running, starting one", name, num);
          ovs_dyn_snf_start_process(name, num, item_type_keth,
                                    eth_name, snf_process_started);
          result = 0;
          }
        }
      else
        {
        eth_name = eth_tab[num].vhost_ifname;
        eth_tab[num].endp_type = endp_type_eths;
        cur->endp_type = endp_type_eths;
        cur->del_snf_ethv_sent = 0;
        ovs_dyn_snf_start_process(name, num, item_type_keth,
                                  eth_name, snf_process_started);
        result = 0;
        }
      }
    else
      {
      if (eth_tab[num].endp_type == endp_type_ethv)
        {
        result = 0;
        }
      else
        {
        eth_name = eth_tab[num].vhost_ifname;
        eth_tab[num].endp_type = endp_type_ethv;
        cur->endp_type = endp_type_ethv;
        memset(tap, 0, MAX_NAME_LEN);
        utils_get_snf_tap_name(tap, eth_name);
        if (cur->del_snf_ethv_sent == 0)
          {
          cur->del_snf_ethv_sent = 1;
          if (strlen(cur->lan))
            {
            if (ovs_snf_send_del_snf_lan(name, num, eth_name, cur->lan))
              KERR("ERROR DEL KVMETH %s %d %s %s", name,num,eth_name,cur->lan);
            }
          ovs_dyn_snf_stop_process(tap);
          }
        result = 0;
        }
      }
    }
  return result;
} 

t_ethv_cnx *get_first_head_ethv(void)
{
  return g_head_ethv;
}

void kvm_init(void)
{
  g_head_ethv = NULL;
  g_nb_ethv = 0;
}

