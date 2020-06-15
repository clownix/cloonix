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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "pid_clone.h"
#include "utils_cmd_line_maker.h"
#include "snf_dpdk_process.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "dpdk_tap.h"
#include "dpdk_nat.h"
#include "dpdk_dyn.h"
#include "dpdk_snf.h"
#include "dpdk_ovs.h"

typedef struct t_snf_dpdk
{
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  char socket[MAX_PATH_LEN];
  int count;
  int llid;
  int pid;
  int closed_count;
  int suid_root_done;
  struct t_snf_dpdk *prev;
  struct t_snf_dpdk *next;
} t_snf_dpdk;

static char g_cloonix_net[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_bin_snf[MAX_PATH_LEN];
static char g_pcap_dir[MAX_PATH_LEN];
static t_snf_dpdk *g_head_snf_dpdk;

void snf_globtopo_small_event(char *name, int num_evt, char *path);
int get_glob_req_self_destruction(void);

/****************************************************************************/
static t_snf_dpdk *find_snf_dpdk(char *name)
{
  t_snf_dpdk *cur = g_head_snf_dpdk;
  while(cur)
    {
    if (!strcmp(cur->name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_snf_dpdk *find_snf_dpdk_with_lan(char *lan)
{
  t_snf_dpdk *cur = g_head_snf_dpdk;
  while(cur)
    {
    if (!strcmp(cur->lan, lan))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_snf_dpdk(t_snf_dpdk *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_snf_dpdk)
    g_head_snf_dpdk = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void snf_dpdk_start(char *name)
{
  static char *argv[5];
  argv[0] = g_bin_snf;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = name;
  argv[4] = g_pcap_dir;
  argv[5] = NULL;
  pid_clone_launch(utils_execve, process_demonized, NULL,
                   (void *) argv, NULL, NULL, name, -1, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_all_nat_mac_to_snf(int llid, char *lan)
{
  char msg[MAX_PATH_LEN];
  char *name, *strmac;
  int i;
  name = dpdk_nat_get_next_matching_lan(lan, NULL);
  while(name)
    {
    for (i=0; i<3; i++)
      {
      strmac = dpdk_nat_get_mac(name, i);
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1,
      "cloonixsnf_mac name=%s num=%d mac=%s", name, i, strmac);
      rpct_send_diag_msg(NULL, llid, type_hop_snf_dpdk, msg);
      hop_event_alloc(llid, type_hop_endp, name, i);
      hop_event_hook(llid, FLAG_HOP_DIAG, msg);
      }
    name = dpdk_nat_get_next_matching_lan(lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_all_tap_mac_to_snf(int llid, char *lan)
{
  char msg[MAX_PATH_LEN];
  char *name, *strmac;
  name = dpdk_tap_get_next_matching_lan(lan, NULL);
  while(name)
    {
    strmac = dpdk_tap_get_mac(name);
    if (strmac)
      {
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1,
      "cloonixsnf_mac name=%s num=%d mac=%s", name, 0, strmac);
      rpct_send_diag_msg(NULL, llid, type_hop_snf_dpdk, msg);
      hop_event_alloc(llid, type_hop_endp, name, 0);
      hop_event_hook(llid, FLAG_HOP_DIAG, msg);
      }
    name = dpdk_tap_get_next_matching_lan(lan, name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_all_vm_mac_to_snf(int llid, char *lan)
{
 t_vm *vm;
  char msg[MAX_PATH_LEN];
  char *name, *mc;
  char mac[MAX_NAME_LEN];
  int i, j, nb_vm;

  vm = cfg_get_first_vm(&nb_vm);
  for (i=0; i < nb_vm; i++)
    {
    for (j=0; j < vm->kvm.nb_tot_eth; j++)
      {
      if (vm->kvm.eth_table[j].eth_type == eth_type_dpdk)
        {
        if (dpdk_dyn_lan_exists_in_vm(vm->kvm.name, j, lan))
          {
          name = vm->kvm.name;
          mc = vm->kvm.eth_table[j].mac_addr;
          memset(mac, 0, MAX_NAME_LEN);
          snprintf(mac, MAX_NAME_LEN-1, "%02x:%02x:%02x:%02x:%02x:%02x",
                                      mc[0]&0xff, mc[1]&0xff, mc[2]&0xff,
                                      mc[3]&0xff, mc[4]&0xff, mc[5]&0xff);
          memset(msg, 0, MAX_PATH_LEN);
          snprintf(msg, MAX_PATH_LEN-1,
          "cloonixsnf_mac name=%s num=%d mac=%s", name, j, mac);
          rpct_send_diag_msg(NULL, llid, type_hop_snf_dpdk, msg);
          hop_event_alloc(llid, type_hop_endp, name, j);
          hop_event_hook(llid, FLAG_HOP_DIAG, msg);
          }
        }
      }
    vm = vm->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int try_connect(char *socket, char *name)
{
  int llid = string_client_unix(socket, uml_clownix_switch_error_cb,
                                uml_clownix_switch_rx_cb, name);
  if (llid)
    {
    if (hop_event_alloc(llid, type_hop_snf_dpdk, "snf_dpdk", 0))
      KERR(" ");
    rpct_send_pid_req(NULL, llid, type_hop_snf_dpdk, name, 0);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(void *data)
{
  t_snf_dpdk *next, *cur = g_head_snf_dpdk;
  int llid;
  char *msg = "cloonixsnf_suidroot";
  if (get_glob_req_self_destruction())
    return;
  while(cur)
    {
    next = cur->next;
    if (cur->llid == 0)
      {
      llid = try_connect(cur->socket, cur->name);
      if (llid)
        {
        cur->llid = llid;
        }
      else
        {
        cur->count += 1;
        if (cur->count == 20)
          {
          KERR("%s", cur->socket);
          dpdk_snf_event_from_snf_dpdk_process(cur->name, cur->lan, -1);
          free_snf_dpdk(cur);
          }
        }
      }
    else if (cur->suid_root_done == 0)
      {
      rpct_send_diag_msg(NULL, cur->llid, type_hop_snf_dpdk, msg);
      hop_event_hook(cur->llid, FLAG_HOP_DIAG, msg);
      cur->suid_root_done = 1;
      }
    else
      {
      cur->count += 1;
      if (cur->count == 5)
        {
        rpct_send_pid_req(NULL, cur->llid, type_hop_snf_dpdk, cur->name, 0);
        cur->count = 0;
        }
      if (cur->closed_count > 0)
        {
        cur->closed_count -= 1;
        if (cur->closed_count == 0)
          {
          dpdk_snf_event_from_snf_dpdk_process(cur->name, cur->lan, 0);
          free_snf_dpdk(cur);
          }
        }
      }
    cur = next;
    }
  clownix_timeout_add(20, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void snf_dpdk_pid_resp(int llid, int tid, char *name, int pid)
{
  t_snf_dpdk *cur = find_snf_dpdk(name);
  if (!cur)
    KERR("%s %d", name, pid);
  else
    {
    if (cur->pid == 0)
      {
      dpdk_snf_event_from_snf_dpdk_process(name, cur->lan, 1);
      cur->pid = pid;
      }
    else if (cur->pid != pid)
      {
      KERR("%s %d", name, pid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int snf_dpdk_diag_llid(int llid)
{
  t_snf_dpdk *cur = g_head_snf_dpdk;
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
void snf_dpdk_diag_resp(int llid, int tid, char *line)
{
  int on, ms, ptx, btx, prx, brx, num;
  char name[MAX_NAME_LEN];
  if (sscanf(line,
  "endp_eventfull_tx_rx %s %d %d %d %d %d %d",
                        name, &num, &ms, &ptx, &btx, &prx, &brx) == 7)
    {
    hop_event_hook(llid, FLAG_HOP_EVT, line);
    if (dpdk_tap_eventfull(name, ms, ptx, btx, prx, brx))
      {
      if (dpdk_ovs_fill_eventfull(name, num, ms, ptx, prx, btx, brx))
        dpdk_nat_eventfull(name, ms, ptx, prx, btx, brx);
      }
    } 
  else if (!strcmp(line,
  "cloonixsnf_suidroot_ko"))
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    KERR("Started snf_dpdk: %s %s", g_cloonix_net, line);
    }
  else if (!strcmp(line,
  "cloonixsnf_suidroot_ok"))
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    }
  else if (sscanf(line,
  "cloonixsnf_GET_CONF_RESP %s %d", name, &on))
    {
    if (on)
      snf_globtopo_small_event(name, snf_evt_capture_on, NULL);
    else
      snf_globtopo_small_event(name, snf_evt_capture_off, NULL);
    }
  else
    KERR("ERROR snf_dpdk: %s %s", g_cloonix_net, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void snf_dpdk_start_stop_process(char *name, char *lan, int on)
{
  t_snf_dpdk *cur = find_snf_dpdk(name);
  char *snf = utils_get_dpdk_snf_dir();
  char msg[MAX_PATH_LEN];
  if (on)
    {
    if (cur)
      KERR("%s %s", name, lan);
    else
      {
      cur = find_snf_dpdk_with_lan(lan);
      if (cur)
        KERR("%s %s %s", name, cur->name, lan);
      else
        {
        cur = (t_snf_dpdk *) malloc(sizeof(t_snf_dpdk));
        memset(cur, 0, sizeof(t_snf_dpdk));
        strncpy(cur->name, name, MAX_NAME_LEN-1);
        strncpy(cur->lan, lan, MAX_NAME_LEN-1);
        snprintf(cur->socket, MAX_NAME_LEN-1, "%s/%s", snf, name);
        if (g_head_snf_dpdk)
          g_head_snf_dpdk->prev = cur;
        cur->next = g_head_snf_dpdk;
        g_head_snf_dpdk = cur; 
        snf_dpdk_start(name);
        }
      }
    }
  else
    {
    if (!cur)
      KERR("%s %s", name, lan);
    else
      {
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "rpct_send_kil_req to %s", name);
      rpct_send_kil_req(NULL, cur->llid, type_hop_snf_dpdk);
      hop_event_hook(cur->llid, FLAG_HOP_DIAG, msg);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  snf_dpdk_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_snf_dpdk *cur = g_head_snf_dpdk;
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
    cur = g_head_snf_dpdk;
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
void snf_dpdk_process_possible_change(char *lan)
{
  t_snf_dpdk *cur = find_snf_dpdk_with_lan(lan);
  char msg[MAX_PATH_LEN];
  if ((cur) && (cur->llid))
    {
    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1, "%s", "cloonixsnf_macliststart");
    rpct_send_diag_msg(NULL, cur->llid, type_hop_snf_dpdk, msg);
    hop_event_hook(cur->llid, FLAG_HOP_DIAG, msg);
  
    send_all_nat_mac_to_snf(cur->llid, lan);
    send_all_tap_mac_to_snf(cur->llid, lan);
    send_all_vm_mac_to_snf(cur->llid, lan);

    memset(msg, 0, MAX_PATH_LEN);
    snprintf(msg, MAX_PATH_LEN-1, "%s", "cloonixsnf_maclistend");
    rpct_send_diag_msg(NULL, cur->llid, type_hop_snf_dpdk, msg);
    hop_event_hook(cur->llid, FLAG_HOP_DIAG, msg);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void snf_dpdk_llid_closed(int llid)
{
  t_snf_dpdk *cur = g_head_snf_dpdk;
  while(cur)
    {
    if (cur->llid == llid)
      {
      cur->closed_count = 2;
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void snf_dpdk_init(void)
{
  char *root = cfg_get_root_work();
  char *net = cfg_get_cloonix_name();
  char *bin_snf = utils_get_snf_dpdk_bin_path();
  char *pcap_dir = utils_get_snf_pcap_dir();

  g_head_snf_dpdk = NULL;
  memset(g_cloonix_net, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_bin_snf, 0, MAX_PATH_LEN);
  memset(g_pcap_dir, 0, MAX_PATH_LEN);
  strncpy(g_cloonix_net, net, MAX_NAME_LEN-1);
  strncpy(g_root_path, root, MAX_PATH_LEN-1);
  strncpy(g_bin_snf, bin_snf, MAX_PATH_LEN-1);
  strncpy(g_pcap_dir, pcap_dir, MAX_PATH_LEN-1);
  clownix_timeout_add(100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

