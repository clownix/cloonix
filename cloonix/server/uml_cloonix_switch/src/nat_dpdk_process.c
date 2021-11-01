/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include "dpdk_nat.h"
#include "llid_trace.h"

typedef struct t_nat_dpdk
{
  char name[MAX_NAME_LEN];
  char socket[MAX_PATH_LEN];
  int count;
  int llid;
  int pid;
  int closed_count;
  int suid_root_done;
  int watchdog_count;
  struct t_nat_dpdk *prev;
  struct t_nat_dpdk *next;
} t_nat_dpdk;

static char g_ascii_cpu_mask[MAX_NAME_LEN];
static char g_cloonix_net[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_bin_nat[MAX_PATH_LEN];
static t_nat_dpdk *g_head_nat_dpdk;

char *get_memid(void);
int get_glob_req_self_destruction(void);
uint32_t get_cpu_mask(void);

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
  if (cur->llid)
    {
    llid_trace_free(cur->llid, 0, __FUNCTION__);
    hop_event_free(cur->llid);
    }
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
  static char *argv[6];
  argv[0] = g_bin_nat;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = name;
  argv[4] = g_ascii_cpu_mask;
  argv[5] = get_memid();
  argv[6] = NULL;
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
      KERR("ERROR %s", name);
    llid_trace_alloc(llid, name, 0, 0, type_llid_trace_endp_ovsdb);
    rpct_send_pid_req(llid, type_hop_nat_dpdk, name, 0);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(void *data)
{
  t_nat_dpdk *next, *cur = g_head_nat_dpdk;
  int llid;
  char *msg = "cloonixnat_suidroot";
  if (get_glob_req_self_destruction())
    return;
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
        }
      else
        {
        cur->count += 1;
        if (cur->count == 50)
          {
          KERR("ERROR %s", cur->socket);
          dpdk_nat_event_from_nat_dpdk_process(cur->name, -1);
          free_nat_dpdk(cur);
          }
        }
      }
    else if (cur->suid_root_done == 0)
      {
      rpct_send_sigdiag_msg(cur->llid, type_hop_nat_dpdk, msg);
      hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
      cur->suid_root_done = 1;
      }
    else if (cur->watchdog_count >= 150)
      {
      dpdk_nat_event_from_nat_dpdk_process(cur->name, -1);
      free_nat_dpdk(cur);
      }
    else
      {
      cur->count += 1;
      if (cur->count == 5)
        {
        rpct_send_pid_req(cur->llid, type_hop_nat_dpdk, cur->name, 0);
        cur->count = 0;
        }
      if (cur->closed_count > 0)
        { 
        cur->closed_count -= 1;
        if (cur->closed_count == 0)
          {
          dpdk_nat_event_from_nat_dpdk_process(cur->name, 0);
          free_nat_dpdk(cur);
          }
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
    KERR("ERROR %s %d", name, pid);
  else
    {
    cur->watchdog_count = 0;
    if (cur->pid == 0)
      {
      cur->pid = pid;
      }
    else if (cur->pid != pid)
      {
      KERR("ERROR %s %d", name, pid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int nat_dpdk_diag_llid(int llid)
{
  t_nat_dpdk *cur = find_nat_dpdk_with_llid(llid);
  int result = 0;
  if (cur)
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_dpdk_sigdiag_resp(int llid, int tid, char *line)
{
  t_nat_dpdk *cur = find_nat_dpdk_with_llid(llid);
  char name[MAX_NAME_LEN];
  char ip[MAX_NAME_LEN];
  char msg[MAX_PATH_LEN];
  int cli_llid, cli_tid;
  if (cur == NULL)
    KERR("ERROR %s", line);
  else if (!strcmp(line,
  "cloonixnat_suidroot_ko"))
    {
    KERR("ERROR Started nat_dpdk: %s %s", g_cloonix_net, line);
    }
  else if (!strcmp(line,
  "cloonixnat_suidroot_ok"))
    {
    nat_dpdk_vm_event();
    dpdk_nat_event_from_nat_dpdk_process(cur->name, 1);
    }
  else if (sscanf(line,
  "cloonixnat_whatip_ok cli_llid=%d cli_tid=%d name=%s ip=%s",
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
  "cloonixnat_whatip_ko cli_llid=%d cli_tid=%d name=%s",
           &cli_llid, &cli_tid, name) == 3)
    {
    if (msg_exist_channel(cli_llid))
      send_status_ko(cli_llid, cli_tid, "KO");
    else
      KERR("ERROR %s", line);
    }
  else
    KERR("ERROR nat_dpdk: %s %s", g_cloonix_net, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int nat_dpdk_whatip(int llid, int tid, char *nat_name, char *name)
{
  char msg[MAX_PATH_LEN];
  t_nat_dpdk *cur = find_nat_dpdk(nat_name);
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
    "cloonixnat_whatip cli_llid=%d cli_tid=%d name=%s", llid, tid, name);
    rpct_send_sigdiag_msg(cur->llid, type_hop_nat_dpdk, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_dpdk_start_stop_process(char *name, int on)
{
  t_nat_dpdk *cur = find_nat_dpdk(name);
  char *nat = utils_get_dpdk_nat_dir();
  char msg[MAX_PATH_LEN];
  if (on)
    {
    if (cur)
      KERR("ERROR %s", name);
    else
      {
      cur = (t_nat_dpdk *) malloc(sizeof(t_nat_dpdk));
      memset(cur, 0, sizeof(t_nat_dpdk));
      strncpy(cur->name, name, MAX_NAME_LEN-1);
      snprintf(cur->socket, MAX_NAME_LEN-1, "%s/%s", nat, name);
      if (g_head_nat_dpdk)
        g_head_nat_dpdk->prev = cur;
      cur->next = g_head_nat_dpdk;
      g_head_nat_dpdk = cur; 
      nat_dpdk_start(name);
      }
    }
  else
    {
    if (!cur)
      KERR("ERROR %s", name);
    else
      {
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "rpct_send_kil_req to %s", name);
      rpct_send_kil_req(cur->llid, type_hop_nat_dpdk);
      hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
      }
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
      cur->closed_count = 2;
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
      rpct_send_sigdiag_msg(cur->llid, type_hop_nat_dpdk, msg);
      hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
      vm = cfg_get_first_vm(&nb);
      for (i=0; i<nb; i++)
        {
        for (j=0; j<vm->kvm.nb_tot_eth; j++)
          {
          if ((vm->kvm.eth_table[j].endp_type == endp_type_ethd) ||
              (vm->kvm.eth_table[j].endp_type == endp_type_eths) ||
              (vm->kvm.eth_table[j].endp_type == endp_type_ethv))
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
            rpct_send_sigdiag_msg(cur->llid,type_hop_nat_dpdk,msg);
            hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
            }
          }
        vm = vm->next;
        }
      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "cloonixnat_machine_end");
      rpct_send_sigdiag_msg(cur->llid, type_hop_nat_dpdk, msg);
      hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
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
  memset(g_ascii_cpu_mask, 0, MAX_NAME_LEN);
  snprintf(g_ascii_cpu_mask, MAX_NAME_LEN-1, "%X",  get_cpu_mask());
  strncpy(g_cloonix_net, net, MAX_NAME_LEN-1);
  strncpy(g_root_path, root, MAX_PATH_LEN-1);
  strncpy(g_bin_nat, bin_nat, MAX_PATH_LEN-1);
  clownix_timeout_add(100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

