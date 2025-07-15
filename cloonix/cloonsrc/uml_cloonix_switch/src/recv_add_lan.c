/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "heartbeat.h"
#include "machine_create.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "system_callers.h"
#include "automates.h"
#include "lan_to_name.h"
#include "utils_cmd_line_maker.h"
#include "file_read_write.h"
#include "qmp.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "xwy.h"
#include "hop_event.h"
#include "ovs.h"
#include "kvm.h"
#include "suid_power.h"
#include "list_commands.h"
#include "cnt.h"
#include "ovs_snf.h"
#include "ovs_nat_main.h"
#include "ovs_a2b.h"
#include "ovs_tap.h"
#include "ovs_phy.h"
#include "ovs_c2c.h"
#include "msg.h"
#include "crun.h"
#include "novnc.h"
#include "proxymous.h"
#include "cloonix_conf_info.h"


static void local_add_lan(int llid, int tid, char *name, int num, char *lan);
int get_inhib_new_clients(void);
int get_uml_cloonix_started(void);

/*---------------------------------------------------------------------------*/
typedef struct t_coherency_delay
{   
  int llid;
  int tid;
  char name[MAX_NAME_LEN];
  int  num;
  char lan[MAX_NAME_LEN];
  struct t_coherency_delay *prev;
  struct t_coherency_delay *next;
} t_coherency_delay;
/*---------------------------------------------------------------------------*/
typedef struct t_timer_endp
{
  int  timer_lan_ko_resp;
  int  llid;
  int  tid;
  char name[MAX_NAME_LEN];
  int  num;
  char lan[MAX_NAME_LEN];
  int  count;
  struct t_timer_endp *prev;
  struct t_timer_endp *next;
} t_timer_endp;
/*--------------------------------------------------------------------------*/
typedef struct t_shadown_lan
{
  char name[MAX_NAME_LEN];
  int num;
  char lan[MAX_NAME_LEN];
  struct t_shadown_lan *prev;
  struct t_shadown_lan *next;
} t_shadown_lan;
/*--------------------------------------------------------------------------*/
static t_coherency_delay *g_head_coherency;
static t_timer_endp *g_head_timer;
static int glob_coherency;
static int glob_coherency_fail_count;
static long long g_coherency_abs_beat_timer;
static int g_coherency_ref_timer;
static t_shadown_lan *g_head_shadown_lan;
static int g_nb_shadown_lan;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void shadown_lan_alloc(char *name, int num, char *lan)
{
  t_shadown_lan *cur = (t_shadown_lan *) malloc(sizeof(t_shadown_lan));
  memset(cur, 0, sizeof(t_shadown_lan));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  strncpy(cur->lan, lan, MAX_NAME_LEN-1);
  if (g_head_shadown_lan)
    g_head_shadown_lan->prev = cur;
  cur->next = g_head_shadown_lan;
  g_head_shadown_lan = cur;
  g_nb_shadown_lan += 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void shadown_lan_free(t_shadown_lan *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_head_shadown_lan == cur)
    g_head_shadown_lan = g_head_shadown_lan->next;
  g_nb_shadown_lan -= 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void shadown_lan_del_all(void)
{
  t_shadown_lan *next, *cur = g_head_shadown_lan;
  while (cur)
    {
    next = cur->next;
    shadown_lan_free(cur);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_shadown_lan *shadown_lan_find(char *name, int num, char *lan)
{
  t_shadown_lan *cur = g_head_shadown_lan;
  while (cur)
    {
    if ((strlen(name)) && (strlen(lan)) &&
        (!strcmp(cur->name, name)) &&
        (cur->num == num) &&
        (!strcmp(cur->lan, lan)))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int shadown_lan_exists(char *name, int num, char *lan)
{
  int result = 0;
  t_shadown_lan *cur = g_head_shadown_lan;
  memset(lan, 0, MAX_NAME_LEN);
  while (cur)
    {
    if ((!strcmp(cur->name, name)) &&
        (cur->num == num))
      break;
    cur = cur->next;
    }
  if (cur)
    {
    strncpy(lan, cur->lan, MAX_NAME_LEN-1); 
    result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/
 
/*****************************************************************************/
void recv_coherency_unlock(void)
{
  glob_coherency -= 1;
  if (glob_coherency < 0)
    KOUT(" ");
} 
/*---------------------------------------------------------------------------*/
  
/*****************************************************************************/
void recv_coherency_lock(void)
{
  glob_coherency += 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_timer_endp *timer_find(char *name, int num)
{
  t_timer_endp *cur = NULL;
  if (name[0])
    {
    cur = g_head_timer;
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
static void timer_free(t_timer_endp *timer)
{
  if (timer->prev)
    timer->prev->next = timer->next;
  if (timer->next)
    timer->next->prev = timer->prev;
  if (timer == g_head_timer)
    g_head_timer = timer->next;
  clownix_free(timer, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int recv_coherency_locked(void)
{
  if (glob_coherency > 0)
    glob_coherency_fail_count += 1;
  else 
    glob_coherency_fail_count = 0;
  if  ((glob_coherency_fail_count > 0) &&
       (glob_coherency_fail_count % 300 == 0))
    KERR("ERROR LOCK TOO LONG %d", glob_coherency_fail_count);
  return glob_coherency;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static t_coherency_delay *coherency_add_chain(int llid, int tid,
                                              char *name, int num,
                                              char *lan)
{
  t_coherency_delay *cur, *elem;
  elem = (t_coherency_delay *) clownix_malloc(sizeof(t_coherency_delay), 16);
  memset(elem, 0, sizeof(t_coherency_delay));
  elem->llid = llid;
  elem->tid = tid;
  elem->num = num;
  strcpy(elem->name, name);
  strcpy(elem->lan, lan);
  if (g_head_coherency)
    {
    cur = g_head_coherency;
    while (cur && cur->next)
      cur = cur->next;
    cur->next = elem;
    elem->prev = cur;
    }
  else
    g_head_coherency = elem;
  return g_head_coherency;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void coherency_del_chain(t_coherency_delay *cd)
{
  if (cd->prev)
    cd->prev->next = cd->next;
  if (cd->next)
    cd->next->prev = cd->prev;
  if (cd == g_head_coherency)
    g_head_coherency = cd->next;
  clownix_free(cd, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_endp(void *data)
{
  t_timer_endp *te = (t_timer_endp *) data;
  int endp_kvm = kvm_exists(te->name, te->num);
  int nb_eth;
  t_eth_table *eth_tab;
  int cnt_exists = cnt_info(te->name, &nb_eth, &eth_tab);
  t_ovs_tap *tap_exists = ovs_tap_exists(te->name);
  t_ovs_phy *phy_exists = ovs_phy_exists_vhost(te->name);
  t_ovs_nat_main *nat_exists = ovs_nat_main_exists(te->name);
  t_ovs_a2b *a2b_exists = ovs_a2b_exists(te->name);

  if (endp_kvm || cnt_exists || tap_exists ||
      phy_exists || nat_exists || a2b_exists)
    {
    local_add_lan(te->llid, te->tid, te->name, te->num, te->lan);
    timer_free(te);
    }
  else
    {
    te->count++;
    if (te->count >= 50)
      {
      send_status_ok(te->llid, te->tid, "ok");
      shadown_lan_alloc(te->name, te->num, te->lan);
      timer_free(te);
      }
    else
      {
      clownix_timeout_add(20, timer_endp, (void *) te, NULL, NULL);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_endp_init(int llid, int tid, char *name, int num, char *lan)
{       
  t_timer_endp *te = timer_find(name, num);
  if (te) 
    { 
    send_status_ko(llid, tid, "endpoint already waiting");
    KERR("ERROR %s %d %s", name, num, lan);
    }
  else
    {
    te = (t_timer_endp *)clownix_malloc(sizeof(t_timer_endp), 4); 
    memset(te, 0, sizeof(t_timer_endp));
    strncpy(te->name, name, MAX_NAME_LEN-1);
    strncpy(te->lan, lan, MAX_NAME_LEN-1);
    te->num = num;
    te->llid = llid;
    te->tid = tid;
    if (g_head_timer)
      g_head_timer->prev = te;
    te->next = g_head_timer;
    g_head_timer = te;
    clownix_timeout_add(20, timer_endp, (void *) te, NULL, NULL);
    }
} 
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void delayed_coherency_cmd_timeout(void *data)
{
  t_coherency_delay *next, *cur = (t_coherency_delay *) data;
  g_coherency_abs_beat_timer = 0;
  g_coherency_ref_timer = 0;
  if (recv_coherency_locked())
    clownix_timeout_add(20, delayed_coherency_cmd_timeout, data,
                        &g_coherency_abs_beat_timer, &g_coherency_ref_timer);
  else
    {
    while (cur)
      {
      next = cur->next;
      if (cfg_is_a_zombie(cur->name))
        {
        clownix_timeout_add(20, delayed_coherency_cmd_timeout, data,
                            &g_coherency_abs_beat_timer,
                            &g_coherency_ref_timer);
        KERR("WAIT %s", cur->name);
        }
      else
        {
        timer_endp_init(cur->llid, cur->tid, cur->name, cur->num, cur->lan);
        coherency_del_chain(cur);
        }
      cur = next;
      }
    }
}   
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_add_lan(int llid, int tid, char *name, int num, char *lan)
{
  t_vm *vm = cfg_get_vm(name);
  t_eth_table *eth_tab = NULL;
  int nb_eth;
  int endp_kvm = kvm_exists(name, num);
  int cnt_exists = cnt_info(name, &nb_eth, &eth_tab);
  t_ovs_tap *tap_exists = ovs_tap_exists(name);
  t_ovs_phy *phy_exists = ovs_phy_exists_vhost(name);
  t_ovs_nat_main *nat_exists = ovs_nat_main_exists(name);
  t_ovs_a2b *a2b_exists = ovs_a2b_exists(name);
  char info[MAX_PATH_LEN];
  if (vm != NULL)
    eth_tab = vm->kvm.eth_table;

  if (get_inhib_new_clients())
    {
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    KERR("ERROR %s %d %s", name, num, lan);
    }
  else if (cfg_name_is_in_use(1, lan, info))
    {
    send_status_ko(llid, tid, info);
    KERR("ERROR %s %d %s", name, num, lan);
    }
  else if (cnt_exists)
    {
    if (nb_eth <= num)
      {
      send_status_ko(llid, tid, "eth does not exist");
      KERR("ERROR %s %d %s", name, num, lan);
      }
    else
      {
      if (cnt_add_lan(llid, tid, name, num, lan, info))
        {
        send_status_ko(llid, tid, info);
        KERR("ERROR %s", info);
        }
      }
    }
  else if ((endp_kvm == endp_type_eths) ||
           (endp_kvm == endp_type_ethv))
    {
    if ((vm == NULL) ||
        ((eth_tab[num].endp_type != endp_type_eths) && 
         (eth_tab[num].endp_type != endp_type_ethv)) ||
        (num >= vm->kvm.nb_tot_eth))
      {
      send_status_ko(llid, tid, "failure");
      KERR("ERROR %s %d %s", name, num, lan);
      }
    else
      {
      if (kvm_add_lan(llid, tid, name, num, lan, endp_kvm, eth_tab))
        {
        send_status_ko(llid, tid, "failure");
        KERR("ERROR %s %d %s", name, num, lan);
        }
      }
    }
  else if ((tap_exists) && (num == 0))
    ovs_tap_add_lan(llid, tid, name, lan);
  else if ((phy_exists) && (num == 0))
    ovs_phy_add_lan(llid, tid, name, lan);
  else if ((nat_exists) && (num == 0))
    ovs_nat_main_add_lan(llid, tid, name, lan);
  else if ((a2b_exists) && ((num == 0) || (num == 1)))
    ovs_a2b_add_lan(llid, tid, name, num, lan);
  else
    {
    KERR("ERROR %s %d %s", name, num, lan);
    send_status_ko(llid, tid, "ko");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_add_lan_endp(int llid, int tid, char *name, int num, char *lan)
{
  char info[MAX_PATH_LEN];
  t_shadown_lan *cur = shadown_lan_find(name, num, lan);
  t_ovs_c2c *c2c_exists = ovs_c2c_exists(name);

  event_print("Rx Req add lan %s in %s %d", lan, name, num);

  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (get_inhib_new_clients())
    {
    send_status_ko(llid, tid, "AUTODESTRUCT_ON");
    KERR("ERROR AUTODESTRUCT_ON");
    }
  else if (cfg_name_is_in_use(1, lan, info))
    {
    send_status_ko(llid, tid, info);
    KERR("ERROR %s", info);
    }
  else if ((!strlen(name)) || (!strlen(lan)))
    {
    send_status_ko(llid, tid, "bad input");
    KERR("ERROR %s", info);
    }
  else if (cur)
    {
    strcpy(info, "Already exists in shadow_lan");
    send_status_ko(llid, tid, info);
    KERR("ERROR %s", info);
    }
  else 
    {
    if (c2c_exists)
      {
      send_status_ok(llid, tid, "ok");
      shadown_lan_alloc(name, num, lan);
      }
    else
      {
      if ((recv_coherency_locked()) || cfg_is_a_zombie(name))
        {
        g_head_coherency = coherency_add_chain(llid, tid, name, num, lan);
        if (!g_coherency_abs_beat_timer)
          {
          clownix_timeout_add(20, delayed_coherency_cmd_timeout,
                              (void *) g_head_coherency,
                              &g_coherency_abs_beat_timer,
                              &g_coherency_ref_timer);
          }
        }  
      else
        {
        timer_endp_init(llid, tid, name, num, lan);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_del_lan_endp(int llid, int tid, char *name, int num, char *lan)
{
  char info[MAX_PATH_LEN];
  int endp_kvm = kvm_exists(name, num);
  t_eth_table *eth_tab = NULL;
  int nb_eth;
  int cnt_exists = cnt_info(name, &nb_eth, &eth_tab);
  t_ovs_tap *tap_exists = ovs_tap_exists(name);
  t_ovs_phy *phy_exists = ovs_phy_exists_vhost(name);
  t_ovs_nat_main *nat_exists = ovs_nat_main_exists(name);
  t_ovs_a2b *a2b_exists = ovs_a2b_exists(name);
  t_shadown_lan *cur = shadown_lan_find(name, num, lan);

  event_print("Rx Req del lan %s of %s %d", lan, name, num);
  if (!get_uml_cloonix_started())
    {
    send_status_ko(llid, tid, "SERVER NOT READY");
    KERR("ERROR SERVER NOT READY");
    return;
    }

  if (cur)
    shadown_lan_free(cur);
  if (cnt_name_exists(name, &nb_eth))
    {
    if (!cnt_exists)
      {
      send_status_ko(llid, tid, "eth operation ongoing");
      KERR("ERROR %s %d %s", name, num, lan);
      }
    else if (nb_eth <= num)
      {
      send_status_ko(llid, tid, "eth does not exist");
      KERR("ERROR %s %d %s", name, num, lan);
      }
    else
      {
      if (cnt_del_lan(llid, tid, name, num, lan, info))
        {
        send_status_ko(llid, tid, info);
        KERR("ERROR %s", info);
        }
      }
    }
  else if ((endp_kvm == endp_type_eths) ||
           (endp_kvm == endp_type_ethv))
    {
    if (kvm_del_lan(llid, tid, name, num, lan, endp_kvm))
      send_status_ko(llid, tid, "failure");
    }
  else if ((tap_exists) && (num == 0))
    ovs_tap_del_lan(llid, tid, name, lan);
  else if ((phy_exists) && (num == 0))
    ovs_phy_del_lan(llid, tid, name, lan);
  else if ((nat_exists) && (num == 0))
    ovs_nat_main_del_lan(llid, tid, name, lan);
  else if ((a2b_exists) && ((num == 0) || (num == 1)))
    ovs_a2b_del_lan(llid, tid, name, num, lan);
  else
    {
    if (cur)
      send_status_ok(llid, tid, "ok");
    else
      {
      sprintf(info, "Del lan %s %d %s fail", name, num, lan);
      send_status_ko(llid, tid, info);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_add_lan_init(void)
{
  g_head_coherency = NULL;
  glob_coherency = 0;
  glob_coherency_fail_count = 0;
  g_coherency_abs_beat_timer = 0;
  g_coherency_ref_timer = 0;
  g_head_shadown_lan = NULL;
  g_nb_shadown_lan = 0;
}
/*---------------------------------------------------------------------------*/


