/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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

/*****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_alloc_delay(void *data)
{
  t_alloc_delay *ald = (t_alloc_delay *) data;
  t_ethv_cnx *cur  = ethv_find(ald->name, ald->num);
  if (!cur)
    {
    ald->count += 1;
    if (ald->count < 100)
      {
      clownix_timeout_add(50, timer_alloc_delay, ald, NULL, NULL);
      }
    else
      {
      KERR("ERROR KVMETH ALLOC DELAY %s %d", ald->name, ald->num);
      clownix_free(ald, __FUNCTION__);
      }
    }
  else
    {
    cur->ready = 1;
    clownix_free(ald, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
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
    clownix_timeout_add(50, timer_alloc_delay, ald, NULL, NULL);
    result = 0;
    }
  else
    KOUT("%d", endp_type);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int kvm_exists(char *name, int num)
{
  int result = 0;
  t_ethv_cnx *cur = ethv_find(name, num);
  if ((cur) && (cur->ready == 1))
    result = cur->endp_type;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
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
        snprintf(tap, MAX_NAME_LEN-1, "s%s", vh);
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void kvm_resp_add_lan(int is_ko, char *name, int num, char *vhost, char *lan)
{
  t_ethv_cnx *cur = ethv_find(name, num);
  if (!cur)
    {
    mactopo_add_resp(0, name, num, lan);
    KERR("ERROR ADD LAN %s %d %s %d", name, num, lan, is_ko);
    }
  else
    {
    if (is_ko)
      {
      mactopo_add_resp(0, name, num, lan);
      KERR("ERROR RESP ADD LAN %s %s %d", lan, name, num);
      utils_send_status_ko(&(cur->llid), &(cur->tid), "KO");
      }
    else
      {
      mactopo_add_resp(item_kvm, name, num, lan);
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void kvm_resp_del_lan(int is_ko, char *name, int num, char *vhost, char *lan)
{
  t_ethv_cnx *cur = ethv_find(name, num);
  if (!cur)
    {
    mactopo_del_resp(0, name, num, lan);
    KERR("ERROR: %s %d %s", name, num, lan);
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int kvm_add_lan(int llid, int tid, char *name, int num,
                     char *lan, int endp_type, t_eth_table *eth_tab)
{
  int result = -1;
  t_ethv_cnx *cur;
  char err[MAX_PATH_LEN];
  if ((endp_type == endp_type_eths) ||
      (endp_type == endp_type_ethv))
    {
    cur  = ethv_find(name, num);
    if (cur == NULL)
      KERR("ERROR ADD LAN %s %d %s", name, num, lan);
    else if (cur->waiting_ack_del_lan)
      KERR("ERROR ADD LAN %s %d %s", name, num, lan);
    else if (cur->waiting_ack_add_lan)
      KERR("ERROR ADD LAN %s %d %s", name, num, lan);
    else if (cur->attached_lan_ok)
      KERR("ERROR ADD LAN %s %d %s", name, num, lan);
    else
      {
      lan_add_name(lan, item_kvm, name, num);
      if (mactopo_add_req(item_kvm, name, num, lan,
                          cur->vhost, eth_tab[num].mac_addr, err))
        {
        KERR("ERROR %s %d %s %s", name, num, lan, err);
        lan_del_name(lan, item_kvm, name, num);
        }
      else if (msg_send_add_lan_endp(ovsreq_add_kvm_lan, name, num,
                                     cur->vhost,lan))
        {
        KERR("ERROR ADD LAN %s %d %s", name, num, lan);
        lan_del_name(lan, item_kvm, name, num);
        }
      else
        {
        strncpy(cur->lan_added, lan, MAX_NAME_LEN);
        if (strlen(cur->lan))
          {
          KERR("ERROR ADD LAN %s %d %s", name, num, lan);
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
    {
    KERR("ERROR KVMETH ADDLAN %s %d %s", name, num, lan);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *kvm_translate_topo_endp(int *nb)
{
  t_ethv_cnx *cur = g_head_ethv;
  int len, nb_endp = 0;
  t_topo_endp *endp;
  len = g_nb_ethv * sizeof(t_topo_endp);
  endp = (t_topo_endp *) clownix_malloc( len, 13);
  memset(endp, 0, len);
  while(cur)
    {
    len = sizeof(t_lan_group_item);
    endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
    memset(endp[nb_endp].lan.lan, 0, len);
    strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
    endp[nb_endp].num = cur->num;
    endp[nb_endp].type = cur->endp_type;
    if (strlen(cur->lan))
      {
      strncpy(endp[nb_endp].lan.lan[0].lan, cur->lan, MAX_NAME_LEN-1);
      endp[nb_endp].lan.nb_lan = 1;
      }
    nb_endp += 1;
    cur = cur->next;
    }
  *nb = nb_endp;
  return endp;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void snf_process_started(char *name, int num, char *vhost)
{
  t_ethv_cnx *cur  = ethv_find(name, num);
  char *eth_name;
  t_vm *vm = cfg_get_vm(name);
  t_eth_table *eth_tab;

  if (vm == NULL)
    KERR("ERROR %s %d", name, num);
  else if (cur == NULL)
    KERR("ERROR %s %d", name, num);
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int kvm_dyn_snf(char *name, int num, int val)
{
  char *eth_name;
  int result = -1;
  t_ethv_cnx *cur  = ethv_find(name, num);
  char tap[MAX_NAME_LEN];
  t_vm *vm = cfg_get_vm(name);
  t_eth_table *eth_tab;

  if (vm == NULL)
    KERR("ERROR %s %d", name, num);
  else if (cur == NULL)
    KERR("ERROR %s %d", name, num);
  else
    {
    eth_tab = vm->kvm.eth_table;
    if (val)
      {
      if (eth_tab[num].endp_type == endp_type_eths)
        KERR("ERROR %s %d", name, num);
      else
        {
        eth_name = eth_tab[num].vhost_ifname;
        eth_tab[num].endp_type = endp_type_eths;
        cur->endp_type = endp_type_eths;
        ovs_dyn_snf_start_process(name, num, item_type_keth,
                                  eth_name, snf_process_started);
        result = 0;
        }
      }
    else
      {
      if (eth_tab[num].endp_type == endp_type_ethv)
        KERR("ERROR %s %d", name, num);
      else
        {
        eth_name = eth_tab[num].vhost_ifname;
        eth_tab[num].endp_type = endp_type_ethv;
        cur->endp_type = endp_type_ethv;
        memset(tap, 0, MAX_NAME_LEN);
        snprintf(tap, MAX_NAME_LEN-1, "s%s", eth_name);
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_ethv_cnx *get_first_head_ethv(void)
{
  return g_head_ethv;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void kvm_init(void)
{
  g_head_ethv = NULL;
  g_nb_ethv = 0;
}
/*--------------------------------------------------------------------------*/

