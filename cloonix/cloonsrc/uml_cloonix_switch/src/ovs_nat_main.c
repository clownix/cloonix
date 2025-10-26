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
#include <errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "ovs_nat_main.h"
#include "llid_trace.h"
#include "msg.h"
#include "cnt.h"
#include "lan_to_name.h"
#include "ovs_snf.h"
#include "kvm.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "mactopo.h"
#include "proxycrun.h"

static char g_cloonix_net[MAX_NAME_LEN];
static char g_bin_nat_main[MAX_PATH_LEN];

static t_ovs_nat_main *g_head_nat;
static int g_nb_nat;

/****************************************************************************/
static t_ovs_nat_main *find_nat(char *name)
{
  t_ovs_nat_main *cur = g_head_nat;
  while(cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_nat(t_ovs_nat_main *cur)
{
  cfg_free_obj_id(cur->nat_id);
  layout_del_sat(cur->name);
  if (strlen(cur->lan_added))
    {
    KERR("%s %s", cur->name, cur->lan_added);
    mactopo_del_req(item_nat, cur->name, 0, cur->lan_added);
    lan_del_name(cur->lan_added, item_nat, cur->name, 0);
    }
  if (cur->llid)
    {
    llid_trace_free(cur->llid, 0, __FUNCTION__);
    cur->llid = 0;
    }
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_nat)
    g_head_nat = cur->next;
  g_nb_nat -= 1;
  free(cur);
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_free_nat(void *data)
{
  char *name = (char *) data;
  t_ovs_nat_main *cur = find_nat(name);
  proxycrun_transmit_req_nat(name, 0);
  if (cur)
    free_nat(cur);
  clownix_free(data, __FUNCTION__);
} 
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void launch_timer_free_nat(char *name)
{
  char *dname;
  dname = (char *) clownix_malloc(MAX_NAME_LEN, 5);
  memset(dname, 0, MAX_NAME_LEN);
  strncpy(dname, name, MAX_NAME_LEN-1);
  clownix_timeout_add(2, timer_free_nat, (void *) dname, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_nat(int cli_llid, int cli_tid, char *name)
{
  uint8_t mc[6];
  int id = cfg_alloc_obj_id();
  t_ovs_nat_main *cur = (t_ovs_nat_main *) malloc(sizeof(t_ovs_nat_main));
  memset(cur, 0, sizeof(t_ovs_nat_main));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->nat_id = id;
  cur->cli_llid = cli_llid;
  cur->cli_tid = cli_tid;
  cur->endp_type = endp_type_natv;
  mc[0] = 0x2;
  mc[1] = 0xFF & rand();
  mc[2] = 0xFF & rand();
  mc[3] = 0xFF & rand();
  mc[4] = cur->nat_id % 100;
  mc[5] = 0xFF & rand();
  snprintf(cur->socket, MAX_PATH_LEN-1, "%s/c%s",
           utils_get_nat_main_dir(), name);
  snprintf(cur->vhost, (MAX_NAME_LEN-1), "%s%d_0", OVS_BRIDGE_PORT, id);
  snprintf(cur->mac, (MAX_NAME_LEN-1),
           "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
           mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
  if (g_head_nat)
    g_head_nat->prev = cur;
  cur->next = g_head_nat;
  g_head_nat = cur;
  proxycrun_transmit_req_nat(name, 1);
  g_nb_nat += 1;
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized_main(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void nat_start(char *name, char *vhost)
{
  char *argv[7];
  argv[0] = g_bin_nat_main;
  argv[1] = g_cloonix_net;
  argv[2] = cfg_get_root_work();
  argv[3] = name;
  argv[4] = vhost;
  argv[5] = NULL;
  pid_clone_launch(utils_execv, process_demonized_main, NULL,
                   (void *) argv, NULL, NULL, name, -1, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int try_connect(char *socket, char *name)
{
  int llid = string_client_unix(socket, uml_clownix_switch_error_cb,
                                uml_clownix_switch_rx_cb, name);
  if (llid)
    {
    if (hop_event_alloc(llid, type_hop_nat_main, name, 0))
      KERR(" ");
    llid_trace_alloc(llid, name, 0, 0, type_llid_trace_endp_ovsdb);
    rpct_send_pid_req(llid, type_hop_nat_main, name, 0);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(void *data)
{
  t_ovs_nat_main *next, *cur = g_head_nat;
  int llid;
  char *msg = "nat_suidroot";
  while(cur)
    {
    next = cur->next;
    cur->watchdog_count += 1;
    if (cur->llid == 0)
      {
      llid = try_connect(cur->socket, cur->name);
      if (llid)
        {
        cur->llid = llid;
        cur->count = 0;
        }
      else
        {
        cur->count += 1;
        if (cur->count == 50)
          {
          KERR("%s %s", cur->socket, cur->name);
          utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
          free_nat(cur);
          }
        }
      }
    else if (cur->suid_root_done == 0)
      {
      cur->count += 1;
      if (cur->count == 50)
        {
        KERR("%s %s", cur->socket, cur->name);
        utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
        free_nat(cur);
        }
      else
        {
        rpct_send_sigdiag_msg(cur->llid, type_hop_nat_main, msg);
        hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
        }
      }
    else if (cur->watchdog_count >= 150)
      {
      KERR("%s %s", cur->socket, cur->name);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
      free_nat(cur);
      }
    else
      {
      cur->count += 1;
      if (cur->count == 5)
        {
        rpct_send_pid_req(cur->llid, type_hop_nat_main, cur->name, 0);
        cur->count = 0;
        }
      if (cur->closed_count > 0)
        { 
        cur->closed_count -= 1;
        if (cur->closed_count == 0)
          {
          utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
          free_nat(cur);
          }
        }
      }
    cur = next;
    }
  clownix_timeout_add(20, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_pid_resp(int llid, char *name, int pid)
{
  t_ovs_nat_main *cur = find_nat(name);
  if (!cur)
    KERR("%s %d", name, pid);
  else
    {
    cur->watchdog_count = 0;
    if ((cur->pid == 0) && (cur->suid_root_done == 1))
      {
      layout_add_sat(name, cur->cli_llid, 0);
      cur->pid = pid;
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cfg_hysteresis_send_topo_info();
      ovs_nat_main_vm_event();
      }
    else if (cur->pid && (cur->pid != pid))
      {
      KERR("ERROR %s %d", name, pid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_nat_main_diag_llid(int llid)
{
  t_ovs_nat_main *cur = g_head_nat;
  int result = 0;
  while(cur)
    {
    if (cur->llid == llid)
      {
      result = 1;
      break; 
      } 
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_poldiag_resp(int llid, int tid, char *line)
{
    KERR("ERROR %s", line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_sigdiag_resp(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  char msg[MAX_PATH_LEN];
  t_ovs_nat_main *cur;
  char ip[MAX_NAME_LEN];
  int cli_llid, cli_tid;

  if (sscanf(line,
  "nat_suidroot_ko %s", name) == 1)
    {
    cur = find_nat(name);
    if (!cur)
      KERR("ERROR nat: %s %s", g_cloonix_net, line);
    else
      KERR("ERROR Started nat: %s %s", g_cloonix_net, name);
    }
  else if (sscanf(line,
  "nat_suidroot_ok %s", name) == 1)
    {
    cur = find_nat(name);
    if (!cur)
      KERR("ERROR nat: %s %s", g_cloonix_net, line);
    else
      {
      cur->suid_root_done = 1;
      cur->count = 0;
      }
    }

 else if (sscanf(line,
  "nat_whatip_ok cli_llid=%d cli_tid=%d name=%s ip=%s",
           &cli_llid, &cli_tid, name, ip) == 4)
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1, "OK=%s", ip);
    if (msg_exist_channel(cli_llid))
      send_status_ok(cli_llid, cli_tid, msg);
    else
      KERR("ERROR %s", line);
    }
  else if (sscanf(line,
  "nat_whatip_ko cli_llid=%d cli_tid=%d name=%s",
           &cli_llid, &cli_tid, name) == 3)
    {
    if (msg_exist_channel(cli_llid))
      send_status_ko(cli_llid, cli_tid, "KO");
    else
      KERR("ERROR %s", line);
    }


  else
    KERR("ERROR nat: %s %s", g_cloonix_net, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_nat_main_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_ovs_nat_main *cur = g_head_nat;
  int i, result = 0;
  event_print("%s", __FUNCTION__);
  while(cur)
    {
    if (cur->pid)
      result++;
    cur = cur->next;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid),5);
    memset(glob_lst, 0, result*sizeof(t_lst_pid));
    cur = g_head_nat;
    i = 0;
    while(cur)
      {
      if (cur->pid)
        {
        strncpy(glob_lst[i].name, cur->name, MAX_NAME_LEN-1);
        glob_lst[i].pid = cur->pid;
        i++;
        }
      cur = cur->next;
      }
    if (i != result)
      KOUT("%d %d", i, result);
    }
  *lst_pid = glob_lst;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_del_all(void)
{   
  t_ovs_nat_main *next, *cur = g_head_nat;
  while(cur)
    {
    next = cur->next;   
    ovs_nat_main_del(0, 0, cur->name);
    cur = next;
    }
}     
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_llid_closed(int llid, int from_clone)
{
  t_ovs_nat_main *cur = g_head_nat;
  if (!from_clone)
    {
    while(cur)
      {
      if (cur->llid == llid)
        {
        cur->llid = 0;
        cur->closed_count = 2;
        }
      cur = cur->next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_resp_add_lan(int is_ko, char *name, int num,
                         char *vhost, char *lan)
{
  t_ovs_nat_main *cur = find_nat(name);
  if (!cur)
    {
    mactopo_add_resp(0, item_nat, name, num, lan);
    KERR("ERROR %d %s", is_ko, name); 
    }
  else
    {
    if (strlen(cur->lan))
      KERR("ERROR: %s %s", cur->name, cur->lan);
    memset(cur->lan, 0, MAX_NAME_LEN);
    if (is_ko)
      {
      mactopo_add_resp(0, item_nat, name, num, lan);
      KERR("ERROR %d %s", is_ko, name);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), name);
      }
    else
      {
      mactopo_add_resp(1, item_nat, name, num, lan);
      strncpy(cur->lan, lan, MAX_NAME_LEN);
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cfg_hysteresis_send_topo_info();
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_resp_del_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan)
{
  t_ovs_nat_main *cur = find_nat(name);
  if (!cur)
    {
    mactopo_del_resp(0, name, num, lan);
    KERR("ERROR %d %s", is_ko, name);
    }
  else if (cur->del_nat_req == 1)
    {
    mactopo_del_resp(item_nat, name, num, lan);
    memset(cur->lan, 0, MAX_NAME_LEN);
    utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
    rpct_send_kil_req(cur->llid, type_hop_nat_main);
    launch_timer_free_nat(cur->name);
    }
  else
    {
    mactopo_del_resp(item_nat, name, num, lan);
    memset(cur->lan, 0, MAX_NAME_LEN);
    if (is_ko)
      {
      KERR("ERROR %d %s", is_ko, name);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), name);
      }
    else
      {
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cfg_hysteresis_send_topo_info();
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_ovs_nat_main *ovs_nat_main_exists(char *name)
{
  return (find_nat(name));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_ovs_nat_main *ovs_nat_main_get_first(int *nb_nat)
{
  t_ovs_nat_main *result = g_head_nat;
  *nb_nat = g_nb_nat;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *ovs_nat_main_translate_topo_endp(int *nb)
{
  t_ovs_nat_main *cur;
  int len, count = 0, nb_endp = 0;
  t_topo_endp *endp;
  cur = g_head_nat;
  while(cur)
    {
    if (strlen(cur->lan))
      count += 1;
    cur = cur->next;
    }
  endp = (t_topo_endp *) clownix_malloc(count * sizeof(t_topo_endp), 13);
  memset(endp, 0, count * sizeof(t_topo_endp));
  cur = g_head_nat;
  while(cur)
    {
    if (strlen(cur->lan))
      {
      len = sizeof(t_lan_group_item);
      endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
      memset(endp[nb_endp].lan.lan, 0, len);
      strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
      endp[nb_endp].num = 0;
      endp[nb_endp].type = cur->endp_type;
      strncpy(endp[nb_endp].lan.lan[0].lan, cur->lan, MAX_NAME_LEN-1);
      endp[nb_endp].lan.nb_lan = 1;
      nb_endp += 1;
      }
    cur = cur->next;
    }
  *nb = nb_endp;
  return endp;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_add(int cli_llid, int cli_tid, char *name)
{
  t_ovs_nat_main *cur = find_nat(name);
  if (cur)
    {
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Exists already");
    }
  else
    {
    alloc_nat(cli_llid, cli_tid, name);
    cur = find_nat(name);
    if (!cur)
      KOUT(" ");
    nat_start(name, cur->vhost);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_del(int cli_llid, int cli_tid, char *name)
{
  t_ovs_nat_main *cur = find_nat(name);
  int val;
  if (!cur)
    {
    KERR("ERROR %s", name);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Does not exist");
    }
  else if ((cur->cli_llid) || (cur->del_nat_req))
    {
    KERR("ERROR %s %d %d", name, cur->cli_llid, cur->del_nat_req);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Not ready");
    }
  else
    {
    cur->cli_llid = cli_llid;
    cur->cli_tid = cli_tid;
    if (strlen(cur->lan_added))
      {
      if (!strlen(cur->lan))
        KERR("ERROR %s %s", name, cur->lan_added);
      mactopo_del_req(item_nat, cur->name, 0, cur->lan_added);
      val = lan_del_name(cur->lan_added, item_nat, cur->name, 0);
      memset(cur->lan_added, 0, MAX_NAME_LEN);
      if (val == 2*MAX_LAN)
        {
        utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
        rpct_send_kil_req(cur->llid, type_hop_nat_main);
        launch_timer_free_nat(cur->name);
        }
      else
        {
        msg_send_del_lan_endp(ovsreq_del_nat_lan, name, 0,
                              cur->vhost, cur->lan);
        cur->del_nat_req = 1;
        }
      }
    else
      {
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      rpct_send_kil_req(cur->llid, type_hop_nat_main);
      launch_timer_free_nat(cur->name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_add_lan(int cli_llid, int cli_tid, char *name, char *lan)
{
  t_ovs_nat_main *cur = find_nat(name);
  char err[MAX_PATH_LEN];
  if (!cur)
    {
    KERR("ERROR %s", name);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Does not exist");
    }
  else if ((cur->cli_llid) || (cur->del_nat_req))
    {
    KERR("ERROR %s %d %d", name, cur->cli_llid, cur->del_nat_req);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Not ready");
    }
  else if (strlen(cur->lan))
    {
    KERR("ERROR %s %s", name, cur->lan);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Lan exists");
    }
  else
    {
    lan_add_name(lan, item_nat, name, 0);
    if (mactopo_add_req(item_nat, name, 0, lan, NULL, NULL, err))
      {
      KERR("ERROR %s %d %s %s", name, 0, lan, err);
      send_status_ko(cli_llid, cli_tid, err);
      lan_del_name(lan, item_nat, name, 0);
      }
    else if (msg_send_add_lan_endp(ovsreq_add_nat_lan, name, 0, cur->vhost, lan))
      {
      KERR("ERROR %s %s", name, lan);
      send_status_ko(cli_llid, cli_tid, "msg_send_add_lan_endp error");
      lan_del_name(lan, item_nat, name, 0);
      }
    else
      {
      strncpy(cur->lan_added, lan, MAX_NAME_LEN);
      cur->cli_llid = cli_llid;
      cur->cli_tid = cli_tid;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_del_lan(int cli_llid, int cli_tid, char *name, char *lan)
{
  t_ovs_nat_main *cur = find_nat(name);
  int val = 0;
  if (!cur)
    {
    KERR("ERROR %s", name);
    send_status_ko(cli_llid, cli_tid, "Does not exist");
    }
  else if ((cur->cli_llid) || (cur->del_nat_req))
    {
    KERR("ERROR %s %d %d", name, cur->cli_llid, cur->del_nat_req);
    send_status_ko(cli_llid, cli_tid, "Not ready");
    }
  else
    {
    if (!strlen(cur->lan_added))
      KERR("ERROR: %s %s", name, lan);
    else
      {
      mactopo_del_req(item_nat, cur->name, 0, cur->lan_added);
      val = lan_del_name(cur->lan_added, item_nat, cur->name, 0);
      memset(cur->lan_added, 0, MAX_NAME_LEN);
      }
    if (val == 2*MAX_LAN)
      {
      memset(cur->lan, 0, MAX_NAME_LEN);
      cfg_hysteresis_send_topo_info();
      send_status_ok(cli_llid, cli_tid, "OK");
      }
    else
      {
      cur->cli_llid = cli_llid;
      cur->cli_tid = cli_tid;
      if (msg_send_del_lan_endp(ovsreq_del_nat_lan, name, 0, cur->vhost, lan))
        utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_vm_event(void)
{
  int i, j, nb;
  t_vm *vm;
  t_cnt *cnt;
  char mac[MAX_NAME_LEN];
  char msg[MAX_PATH_LEN];
  char *name;
  unsigned char *m;
  t_ovs_nat_main *cur = g_head_nat;
  while(cur)
    {
    if (cur->llid)
      {
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "nat_machine_begin");
      rpct_send_sigdiag_msg(cur->llid, type_hop_nat_main, msg);
      hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
      vm = cfg_get_first_vm(&nb);
      for (i=0; i<nb; i++)
        {
        for (j=0; j<vm->kvm.nb_tot_eth; j++)
          {
          if ((vm->kvm.eth_table[j].endp_type == endp_type_eths) ||
              (vm->kvm.eth_table[j].endp_type == endp_type_ethv))
            {
            m = vm->kvm.eth_table[j].mac_addr;
            name = vm->kvm.name;
            snprintf(mac, MAX_NAME_LEN-1,
                     "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                     m[0], m[1], m[2], m[3], m[4], m[5]);
            memset(msg, 0, MAX_PATH_LEN);
            snprintf(msg, MAX_PATH_LEN-1,
            "nat_machine_add %s eth:%d mac:%s", name, j, mac);
            rpct_send_sigdiag_msg(cur->llid, type_hop_nat_main, msg);
            hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
            }
          }
        vm = vm->next;
        }
      cnt = cnt_get_first_cnt(&nb);
      for (i=0; i<nb; i++)
        {
        for (j=0; j<cnt->cnt.nb_tot_eth; j++)
          {
          if ((cnt->cnt.eth_table[j].endp_type == endp_type_eths) ||
              (cnt->cnt.eth_table[j].endp_type == endp_type_ethv))
            {
            m = cnt->cnt.eth_table[j].mac_addr;
            name = cnt->cnt.name;
            snprintf(mac, MAX_NAME_LEN-1,
                     "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                     m[0], m[1], m[2], m[3], m[4], m[5]);
            memset(msg, 0, MAX_PATH_LEN);
            snprintf(msg, MAX_PATH_LEN-1,
            "nat_machine_add %s eth:%d mac:%s", name, j, mac);
            rpct_send_sigdiag_msg(cur->llid, type_hop_nat_main, msg);
            hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
            }
          }
        cnt = cnt->next;
        }

      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "nat_machine_end");
      rpct_send_sigdiag_msg(cur->llid, type_hop_nat_main, msg);
      hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
      }
    cur = cur->next;
    }

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_cisco_add(char *name)
{
  t_vm *vm = cfg_get_vm(name);
  char cisconat[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  t_eth_table *eth_tab;
  int num, endp_type;
  char *nm;
  if (!vm)
    KERR("ERROR CREATING CISCO NAT %s", name);
  else
    {
    memset(cisconat, 0, MAX_NAME_LEN);
    snprintf(cisconat, MAX_NAME_LEN-1, "nat_%s", name);
    ovs_nat_main_add(0, 0, cisconat);
    nm = (char *) malloc(MAX_NAME_LEN);
    memset(nm, 0, MAX_NAME_LEN);
    strncpy(nm, name, MAX_NAME_LEN-1);
    memset(cisconat, 0, MAX_NAME_LEN);
    memset(lan, 0, MAX_NAME_LEN);
    snprintf(cisconat, MAX_NAME_LEN-1, "nat_%s", name);
    snprintf(lan, MAX_NAME_LEN-1, "lan_nat_%s", name);
    num = vm->kvm.vm_config_param;
    ovs_nat_main_add_lan(0, 0, cisconat, lan);
    eth_tab = vm->kvm.eth_table;
    endp_type = kvm_exists(name, num);
    if (kvm_add_lan(0, 0, name, num, lan, endp_type, eth_tab))
      KERR("ERROR %s %d %s", name, num, lan);

    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void snf_process_started(char *name, int num, char *vhost)
{
  t_ovs_nat_main *cur = find_nat(name);

  if (cur == NULL)
    KERR("ERROR %s %d", name, num);
  else
    {
    if (strlen(cur->lan))
      ovs_snf_send_add_snf_lan(name, num, cur->vhost, cur->lan);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  ovs_nat_main_dyn_snf(char *name, int val)
{
  int result = -1;
  t_ovs_nat_main *cur = find_nat(name);
  char tap[MAX_NAME_LEN];
  char *vh;

  if (cur == NULL)
    KERR("ERROR %s %d", name, val);
  else
    {
    if (val)
      {
      if (cur->endp_type == endp_type_nats)
        KERR("ERROR %s", name);
      else
        {
        cur->del_snf_ethv_sent = 0;
        cur->endp_type = endp_type_nats;
        ovs_dyn_snf_start_process(name, 0, item_type_nat,
                                  cur->vhost, snf_process_started);
        result = 0;
        }
      }
    else
      {
      if (cur->endp_type == endp_type_natv)
        KERR("ERROR %s", name);
      else
        {
        cur->endp_type = endp_type_natv;
        memset(tap, 0, MAX_NAME_LEN);
        vh = cur->vhost;
        snprintf(tap, MAX_NAME_LEN-1, "s%s", vh);
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
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void ovs_nat_main_cisco_nat_destroy(char *name)
{
  char cisconat[MAX_NAME_LEN];
  memset(cisconat, 0, MAX_NAME_LEN);
  snprintf(cisconat, MAX_NAME_LEN-1, "nat_%s", name);
  ovs_nat_main_del(0, 0, cisconat);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_nat_main_whatip(int llid, int tid, char *nat_name, char *name)
{
  char msg[MAX_PATH_LEN];
  t_ovs_nat_main *cur = find_nat(nat_name);
  int result = -1;
  if (cur == NULL)
    KERR("ERROR %s not found", nat_name);
  else if (!msg_exist_channel(cur->llid))
    KERR("ERROR %s not connected", nat_name);
  else
    {
    result = 0;
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1,
    "nat_whatip cli_llid=%d cli_tid=%d name=%s", llid, tid, name);
    rpct_send_sigdiag_msg(cur->llid, type_hop_nat_main, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_nat_main_init(void)
{
  char *net = cfg_get_cloonix_name();
  char *bin_nat_main = utils_get_ovs_nat_main_bin_dir();

  g_nb_nat = 0;
  g_head_nat = NULL;
  memset(g_cloonix_net, 0, MAX_NAME_LEN);
  memset(g_bin_nat_main, 0, MAX_PATH_LEN);
  strncpy(g_cloonix_net, net, MAX_NAME_LEN-1);
  strncpy(g_bin_nat_main, bin_nat_main, MAX_PATH_LEN-1);
  clownix_timeout_add(100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

