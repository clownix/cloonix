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
#include "nat_dpdk_process.h"
#include "uml_clownix_switch.h"
#include "hop_event.h"
#include "dpdk_tap.h"
#include "dpdk_dyn.h"
#include "dpdk_nat.h"
#include "dpdk_ovs.h"

typedef struct t_nat_dpdk
{
  char name[MAX_NAME_LEN];
  char lan[MAX_NAME_LEN];
  char socket[MAX_PATH_LEN];
  int count;
  int llid;
  int pid;
  int suid_root_done;
  struct t_nat_dpdk *prev;
  struct t_nat_dpdk *next;
} t_nat_dpdk;

static char g_cloonix_net[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_bin_nat[MAX_PATH_LEN];
static t_nat_dpdk *g_head_nat_dpdk;

int get_glob_req_self_destruction(void);

/****************************************************************************/
static t_nat_dpdk *find_nat_dpdk(char *name)
{
  t_nat_dpdk *cur = g_head_nat_dpdk;
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
static t_nat_dpdk *find_nat_dpdk_with_lan(char *lan)
{
  t_nat_dpdk *cur = g_head_nat_dpdk;
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
static t_nat_dpdk *find_nat_dpdk_with_llid(int llid)
{
  t_nat_dpdk *cur = g_head_nat_dpdk;
  while(cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_nat_dpdk(t_nat_dpdk *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_nat_dpdk)
    g_head_nat_dpdk = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void nat_dpdk_start(char *name)
{
  static char *argv[5];
  argv[0] = g_bin_nat;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = name;
  argv[4] = NULL;
  pid_clone_launch(utils_execve, process_demonized, NULL,
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
    if (hop_event_alloc(llid, type_hop_nat_dpdk, name, 0))
      KERR(" ");
    rpct_send_pid_req(NULL, llid, type_hop_nat_dpdk, name, 0);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(void *data)
{
  t_nat_dpdk *next, *cur = g_head_nat_dpdk;
  int llid;
  if (get_glob_req_self_destruction())
    return;
  while(cur)
    {
    next = cur->next;
    if (cur->llid == 0)
      {
      llid = try_connect(cur->socket, cur->name);
      if (llid)
        cur->llid = llid;
      else
        {
        cur->count += 1;
        if (cur->count == 20)
          {
          KERR("%s", cur->socket);
          dpdk_nat_event_from_nat_dpdk_process(cur->name, cur->lan, -1);
          free_nat_dpdk(cur);
          }
        }
      }
    else if (cur->suid_root_done == 0)
      {
      rpct_send_diag_msg(NULL, cur->llid,
                         type_hop_nat_dpdk, "cloonixnat_suidroot");
      cur->suid_root_done = 1;
      }
    else
      {
      cur->count += 1;
      if (cur->count == 5)
        {
        rpct_send_pid_req(NULL, cur->llid, type_hop_nat_dpdk, cur->name, 0);
        cur->count = 0;
        }
      }
    cur = next;
    }
  clownix_timeout_add(20, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_dpdk_pid_resp(int llid, int tid, char *name, int pid)
{
  t_nat_dpdk *cur = find_nat_dpdk(name);
  if (!cur)
    KERR("%s %d", name, pid);
  else
    {
    if (cur->pid == 0)
      {
      cur->pid = pid;
      dpdk_nat_event_from_nat_dpdk_process(name, cur->lan, 1);
      }
    else if (cur->pid != pid)
      KERR("%s %d", name, pid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int nat_dpdk_diag_llid(int llid)
{
  t_nat_dpdk *cur = g_head_nat_dpdk;
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
void nat_dpdk_diag_resp(int llid, int tid, char *line)
{
  if (!strcmp(line,
  "cloonixnat_suidroot_ko"))
    {
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    KERR("Started nat_dpdk: %s %s", g_cloonix_net, line);
    }
  else if (!strcmp(line,
  "cloonixnat_suidroot_ok"))
    {
    nat_dpdk_vm_event();
    hop_event_hook(llid, FLAG_HOP_DIAG, line);
    }
  else
    KERR("ERROR nat_dpdk: %s %s", g_cloonix_net, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_dpdk_start_process(char *name, char *lan, int on)
{
  t_nat_dpdk *cur = find_nat_dpdk(name);
  char *nat = utils_get_dpdk_nat_dir();
  if (on)
    {
    if (cur)
      KERR("%s %s", name, lan);
    else
      {
      if (find_nat_dpdk_with_lan(lan))
        KERR("%s %s", name, lan);
      else
        {
        cur = (t_nat_dpdk *) malloc(sizeof(t_nat_dpdk));
        memset(cur, 0, sizeof(t_nat_dpdk));
        strncpy(cur->name, name, MAX_NAME_LEN-1);
        strncpy(cur->lan, lan, MAX_NAME_LEN-1);
        snprintf(cur->socket, MAX_NAME_LEN-1, "%s/%s", nat, name);
        if (g_head_nat_dpdk)
          g_head_nat_dpdk->prev = cur;
        cur->next = g_head_nat_dpdk;
        g_head_nat_dpdk = cur; 
        nat_dpdk_start(name);
        }
      }
    }
  else
    {
    if (!cur)
      KERR("%s %s", name, lan);
    else
      rpct_send_kil_req(NULL, cur->llid, type_hop_nat_dpdk);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  nat_dpdk_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_nat_dpdk *cur = g_head_nat_dpdk;
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
    cur = g_head_nat_dpdk;
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
void nat_dpdk_llid_closed(int llid)
{
  t_nat_dpdk *cur = g_head_nat_dpdk;
  while(cur)
    {
    if (cur->llid == llid)
      {
      dpdk_nat_event_from_nat_dpdk_process(cur->name, cur->lan, 0);
      free_nat_dpdk(cur);
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_dpdk_vm_event(void)
{
  int i, j, nb;
  t_vm *vm;
  char mac[MAX_NAME_LEN];
  char msg[MAX_PATH_LEN];
  char *m, *name;

  t_nat_dpdk *cur = g_head_nat_dpdk;
  while(cur)
    {
    if (cur->llid)
      {
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "cloonixnat_machine_begin");
      rpct_send_diag_msg(NULL, cur->llid, type_hop_nat_dpdk, msg);
      vm = cfg_get_first_vm(&nb);
      for (i=0; i<nb; i++)
        {
        for (j=0; j<vm->kvm.nb_tot_eth; j++)
          {
          if (vm->kvm.eth_table[j].eth_type == eth_type_dpdk)
            {
            m = vm->kvm.eth_table[j].mac_addr;
            name = vm->kvm.name;
            memset(mac, 0, MAX_NAME_LEN);
            snprintf(mac, MAX_NAME_LEN-1, 
                     "%02x:%02x:%02x:%02x:%02x:%02x",
                     m[0]&0xff,m[1]&0xff,m[2]&0xff,
                     m[3]&0xff,m[4]&0xff,m[5]&0xff);
            memset(msg, 0, MAX_PATH_LEN);
            snprintf(msg, MAX_PATH_LEN-1,
            "cloonixnat_machine_add %s eth:%d mac:%s", name, j, mac);
            rpct_send_diag_msg(NULL,cur->llid,type_hop_nat_dpdk,msg);
            }
          }
        vm = vm->next;
        }
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "cloonixnat_machine_end");
      rpct_send_diag_msg(NULL, cur->llid, type_hop_nat_dpdk, msg);
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int nat_dpdk_name_exists_with_llid(int llid, char *name)
{
  int result = 0;
  t_nat_dpdk *cur = find_nat_dpdk_with_llid(llid);
  memset(name, 0, MAX_NAME_LEN);
  if (cur)
    {
    strncpy(name, cur->name, MAX_NAME_LEN-1);
    result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int nat_dpdk_llid_exists_with_name(char *name)
{
  int result = 0;
  t_nat_dpdk *cur = find_nat_dpdk(name);
  if (cur)
    result = cur->llid;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_dpdk_init(void)
{
  char *root = cfg_get_root_work();
  char *net = cfg_get_cloonix_name();
  char *bin_nat = utils_get_nat_dpdk_bin_path();

  g_head_nat_dpdk = NULL;
  memset(g_cloonix_net, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_bin_nat, 0, MAX_PATH_LEN);
  strncpy(g_cloonix_net, net, MAX_NAME_LEN-1);
  strncpy(g_root_path, root, MAX_PATH_LEN-1);
  strncpy(g_bin_nat, bin_nat, MAX_PATH_LEN-1);
  clownix_timeout_add(100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

