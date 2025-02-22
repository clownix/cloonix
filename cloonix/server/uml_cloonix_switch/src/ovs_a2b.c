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
#include "ovs_a2b.h"
#include "llid_trace.h"
#include "msg.h"
#include "cnt.h"
#include "lan_to_name.h"
#include "ovs_snf.h"
#include "kvm.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "mactopo.h"

static char g_cloonix_net[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_bin_a2b[MAX_PATH_LEN];

static t_ovs_a2b *g_head_a2b;
static int g_nb_a2b;

int get_glob_req_self_destruction(void);

/****************************************************************************/
static t_ovs_a2b *find_a2b(char *name)
{
  t_ovs_a2b *cur = g_head_a2b;
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
static void free_a2b(t_ovs_a2b *cur)
{
  cfg_free_obj_id(cur->a2b_id);
  layout_del_sat(cur->name);
  if (strlen(cur->side[0].lan_added))
    {
    KERR("ERROR %s %s", cur->name, cur->side[0].lan_added);
    mactopo_del_req(item_a2b, cur->name, 0, cur->side[0].lan_added);
    lan_del_name(cur->side[0].lan_added, item_a2b, cur->name, 0);
    }
  if (strlen(cur->side[1].lan_added))
    {
    KERR("ERROR %s %s", cur->name, cur->side[1].lan_added);
    mactopo_del_req(item_a2b, cur->name, 1, cur->side[1].lan_added);
    lan_del_name(cur->side[1].lan_added, item_a2b, cur->name, 1);
    }
  if (cur->llid)
    {
    llid_trace_free(cur->llid, 0, __FUNCTION__);
    }
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_a2b)
    g_head_a2b = cur->next;
  g_nb_a2b -= 1;
  free(cur);
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_free_a2b(void *data)
{
  char *name = (char *) data;
  t_ovs_a2b *cur = find_a2b(name);
  if (cur)
    free_a2b(cur);
  clownix_free(data, __FUNCTION__);
} 
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void launch_timer_free_a2b(char *name)
{
  char *dname;
  dname = (char *) clownix_malloc(MAX_NAME_LEN, 5);
  memset(dname, 0, MAX_NAME_LEN);
  strncpy(dname, name, MAX_NAME_LEN-1);
  clownix_timeout_add(2, timer_free_a2b, (void *) dname, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void alloc_a2b(int cli_llid, int cli_tid, char *name)
{
  char *root = g_root_path;
  int id = cfg_alloc_obj_id();
  t_ovs_a2b *cur = (t_ovs_a2b *) malloc(sizeof(t_ovs_a2b));
  memset(cur, 0, sizeof(t_ovs_a2b));
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->a2b_id = id;
  cur->cli_llid = cli_llid;
  cur->cli_tid = cli_tid;
  cur->side[0].endp_type = endp_type_ethv;
  cur->side[1].endp_type = endp_type_ethv;
  snprintf(cur->socket, MAX_NAME_LEN-1, "%s/%s/c%s", root, A2B_DIR, name);
  snprintf(cur->side[0].vhost, (MAX_NAME_LEN-1), "%s%d_0", OVS_BRIDGE_PORT, id);
  snprintf(cur->side[1].vhost, (MAX_NAME_LEN-1), "%s%d_1", OVS_BRIDGE_PORT, id);
  if (g_head_a2b)
    g_head_a2b->prev = cur;
  cur->next = g_head_a2b;
  g_head_a2b = cur;
  g_nb_a2b += 1;
  cfg_hysteresis_send_topo_info();
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void process_demonized(void *unused_data, int status, char *name)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void a2b_start(char *name, char *vhost0, char *vhost1)
{
  static char *argv[7];
  argv[0] = g_bin_a2b;
  argv[1] = g_cloonix_net;
  argv[2] = g_root_path;
  argv[3] = name;
  argv[4] = vhost0;
  argv[5] = vhost1;
  argv[6] = NULL;
  pid_clone_launch(utils_execv, process_demonized, NULL,
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
    if (hop_event_alloc(llid, type_hop_a2b, name, 0))
      KERR(" ");
    llid_trace_alloc(llid, name, 0, 0, type_llid_trace_endp_ovsdb);
    rpct_send_pid_req(llid, type_hop_a2b, name, 0);
    }
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat(void *data)
{
  t_ovs_a2b *next, *cur = g_head_a2b;
  int llid;
  char *msg = "a2b_suidroot";
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
        cur->count = 0;
        }
      else
        {
        cur->count += 1;
        if (cur->count == 50)
          {
          KERR("%s %s", cur->socket, cur->name);
          utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
          free_a2b(cur);
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
        free_a2b(cur);
        }
      else
        {
        rpct_send_sigdiag_msg(cur->llid, type_hop_a2b, msg);
        hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
        }
      }
    else if (cur->watchdog_count >= 150)
      {
      KERR("%s %s", cur->socket, cur->name);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
      free_a2b(cur);
      }
    else
      {
      cur->count += 1;
      if (cur->count == 5)
        {
        rpct_send_pid_req(cur->llid, type_hop_a2b, cur->name, 0);
        cur->count = 0;
        }
      if (cur->closed_count > 0)
        { 
        cur->closed_count -= 1;
        if (cur->closed_count == 0)
          {
          utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
          free_a2b(cur);
          }
        }
      }
    cur = next;
    }
  clownix_timeout_add(20, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int param_config_check(char *name, char *input, int *dir,
                              unsigned char targ[4], unsigned char src[4])
{
  int val, result = -1;
  char cmd[MAX_PATH_LEN];
  if (sscanf(input, "dir=%d cmd=%s val=%d", dir, cmd, &val) == 3) 
    {
    result = 1;
    }
  else if (strstr(input, "unreplace_ipdst"))
    {
    if (sscanf(input, "dir=%d", dir) == 1)
      result = 2;
    else
      KERR("ERROR %s %s", name, input); 
    }
  else if (strstr(input, "unreplace_ipsrc"))
    {
    if (sscanf(input, "dir=%d", dir) == 1)
      result = 3;
    else
      KERR("ERROR %s %s", name, input);
    }
  else if (strstr(input, "replace_ipdst"))
    {
    if (sscanf(input,
        "dir=%d replace_ipdst %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
         dir, &(targ[0]), &(targ[1]), &(targ[2]), &(targ[3]),
         &(src[0]), &(src[1]), &(src[2]), &(src[3])) == 9)
      result = 4;
    else
      KERR("ERROR %s %s", name, input); 
    }
  else if (strstr(input, "replace_ipsrc"))
    {
    if (sscanf(input,
        "dir=%d replace_ipsrc %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
         dir, &(targ[0]), &(targ[1]), &(targ[2]), &(targ[3]),
         &(src[0]), &(src[1]), &(src[2]), &(src[3])) == 9)
      result = 5;
    else
      KERR("ERROR %s %s", name, input); 
    }
  else
    KERR("ERROR %s %s", name, input); 
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_a2b_pid_resp(int llid, char *name, int pid)
{
  t_ovs_a2b *cur = find_a2b(name);
  if (!cur)
    KERR("%s %d", name, pid);
  else
    {
    cur->watchdog_count = 0;
    if ((cur->pid == 0) && (cur->suid_root_done == 1))
      {
      layout_add_sat(name, cur->cli_llid, 1);
      cur->pid = pid;
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cfg_hysteresis_send_topo_info();
      }
    else if (cur->pid && (cur->pid != pid))
      {
      KERR("ERROR %s %d", name, pid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_a2b_diag_llid(int llid)
{
  t_ovs_a2b *cur = g_head_a2b;
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
void ovs_a2b_poldiag_resp(int llid, int tid, char *line)
{
    KERR("ERROR %s", line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_a2b_sigdiag_resp(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  char msg[MAX_PATH_LEN];
  t_ovs_a2b *cur;
  char ip[MAX_NAME_LEN];
  int cli_llid, cli_tid;

  if (sscanf(line,
  "a2b_suidroot_ko %s", name) == 1)
    {
    cur = find_a2b(name);
    if (!cur)
      KERR("ERROR a2b: %s %s", g_cloonix_net, line);
    else
      KERR("ERROR Started a2b: %s %s", g_cloonix_net, name);
    }

  else if (sscanf(line,
  "a2b_suidroot_ok %s", name) == 1)
    {
    cur = find_a2b(name);
    if (!cur)
      KERR("ERROR a2b: %s %s", g_cloonix_net, line);
    else
      {
      cur->suid_root_done = 1;
      cur->count = 0;
      }
    }

  else if (sscanf(line,
  "a2b_whatip_ok cli_llid=%d cli_tid=%d name=%s ip=%s",
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
  "a2b_whatip_ko cli_llid=%d cli_tid=%d name=%s",
           &cli_llid, &cli_tid, name) == 3)
    {
    if (msg_exist_channel(cli_llid))
      send_status_ko(cli_llid, cli_tid, "KO");
    else
      KERR("ERROR %s", line);
    }

  else if (sscanf(line,
  "a2b_param_config_ok %d %d %s", &cli_llid, &cli_tid, name) == 3)
    {
    if (msg_exist_channel(cli_llid))
      send_status_ok(cli_llid, cli_tid, name);
    else
      KERR("ERROR %s", line);
    }

  else if (sscanf(line,
  "a2b_param_config_ko %d %d %s", &cli_llid, &cli_tid, name) == 3)
    {
    if (msg_exist_channel(cli_llid))
      send_status_ko(cli_llid, cli_tid, name);
    else
      KERR("ERROR %s", line);
    }

  else
    KERR("ERROR a2b: %s %s", g_cloonix_net, line);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int ovs_a2b_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_ovs_a2b *cur = g_head_a2b;
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
    cur = g_head_a2b;
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
void ovs_a2b_llid_closed(int llid, int from_clone)
{
  t_ovs_a2b *cur = g_head_a2b;
  if (!from_clone)
    {
    while(cur)
      {
      if (cur->llid == llid)
        {
        cur->closed_count = 2;
        }
      cur = cur->next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void snf_started(char *name, int num, char *vhost)
{
  t_ovs_a2b *cur = find_a2b(name);
  char *lan;

  if (cur == NULL)
    KERR("ERROR %s %d", name, num);
  else
    {
    lan = cur->side[num].lan;
    if (strlen(lan))
      {
      ovs_snf_send_add_snf_lan(name, num, cur->side[num].vhost, lan);
      }
    else
      {
      cur->side[num].must_call_snf_started = 1;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_a2b_resp_add_lan(int is_ko, char *name, int num,
                         char *vhost, char *lan)
{
  t_ovs_a2b *cur = find_a2b(name);
  if (!cur)
    {
    mactopo_add_resp(0, name, num, lan);
    KERR("ERROR %d %s", is_ko, name); 
    }
  else
    {
    if (strlen(cur->side[num].lan))
      KERR("ERROR: %s %d %s", cur->name, num, cur->side[num].lan);
    memset(cur->side[num].lan, 0, MAX_NAME_LEN);
    if (is_ko)
      {
      mactopo_add_resp(0, name, num, lan);
      KERR("ERROR %d %s", is_ko, name);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), name);
      }
    else
      {
      mactopo_add_resp(item_a2b, name, num, lan);
      strncpy(cur->side[num].lan, lan, MAX_NAME_LEN);
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cfg_hysteresis_send_topo_info();
      if (cur->side[num].must_call_snf_started)
        {
        snf_started(name, num, cur->side[num].vhost);
        cur->side[num].must_call_snf_started = 0;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_a2b_resp_del_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan)
{
  t_ovs_a2b *cur = find_a2b(name);
  int other;
  if (num == 0)
    other = 1;
  else if (num == 1)
    other = 0;
  else
    KERR("ERROR %d %s", is_ko, name);
  if (!cur)
    {
    mactopo_del_resp(0, name, num, lan);
    KERR("ERROR %d %s", is_ko, name);
    }
  else if (cur->side[num].del_a2b_req == 1)
    {
    mactopo_del_resp(item_a2b, name, num, lan);
    memset(cur->side[num].lan, 0, MAX_NAME_LEN);
    cur->side[num].del_a2b_req = 0;
    if (cur->side[other].del_a2b_req == 0)
      {
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      rpct_send_kil_req(cur->llid, type_hop_a2b);
      launch_timer_free_a2b(cur->name);
      }
    }
  else
    {
    mactopo_del_resp(item_a2b, name, num, lan);
    memset(cur->side[num].lan, 0, MAX_NAME_LEN);
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
t_ovs_a2b *ovs_a2b_exists(char *name)
{
  return (find_a2b(name));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_ovs_a2b *ovs_a2b_get_first(int *nb_a2b)
{
  t_ovs_a2b *result = g_head_a2b;
  *nb_a2b = g_nb_a2b;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *ovs_a2b_translate_topo_endp(int *nb)
{
  t_ovs_a2b *cur;
  int len, count = 0, nb_endp = 0;
  t_topo_endp *endp;
  cur = g_head_a2b;
  while(cur)
    {
    if (strlen(cur->side[0].lan))
      count += 1;
    if (strlen(cur->side[1].lan))
      count += 1;
    cur = cur->next;
    }
  endp = (t_topo_endp *) clownix_malloc(count * sizeof(t_topo_endp), 13);
  memset(endp, 0, count * sizeof(t_topo_endp));
  cur = g_head_a2b;
  while(cur)
    {
    if (strlen(cur->side[0].lan))
      {
      len = sizeof(t_lan_group_item);
      endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
      memset(endp[nb_endp].lan.lan, 0, len);
      strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
      endp[nb_endp].num = 0;
      endp[nb_endp].type = cur->side[0].endp_type;
      strncpy(endp[nb_endp].lan.lan[0].lan, cur->side[0].lan, MAX_NAME_LEN-1);
      endp[nb_endp].lan.nb_lan = 1;
      nb_endp += 1;
      }
    if (strlen(cur->side[1].lan))
      {
      len = sizeof(t_lan_group_item);
      endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
      memset(endp[nb_endp].lan.lan, 0, len); 
      strncpy(endp[nb_endp].name, cur->name, MAX_NAME_LEN-1);
      endp[nb_endp].num = 1;
      endp[nb_endp].type = cur->side[1].endp_type;
      strncpy(endp[nb_endp].lan.lan[0].lan, cur->side[1].lan, MAX_NAME_LEN-1);
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
void ovs_a2b_add(int cli_llid, int cli_tid, char *name)
{
  t_ovs_a2b *cur = find_a2b(name);
  if (cur)
    {
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Exists already");
    }
  else
    {
    alloc_a2b(cli_llid, cli_tid, name);
    cur = find_a2b(name);
    if (!cur)
      KOUT(" ");
    a2b_start(name, cur->side[0].vhost, cur->side[1].vhost);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_a2b_del(int cli_llid, int cli_tid, char *name)
{
  t_ovs_a2b *cur = find_a2b(name);
  int val, val1, val2;
  if (!cur)
    {
    KERR("ERROR %s", name);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Does not exist");
    }
  else if ((cur->cli_llid) ||
           (cur->side[0].del_a2b_req) ||
           (cur->side[1].del_a2b_req))
    {
    KERR("ERROR %s %d %d", name, cur->cli_llid, cur->side[0].del_a2b_req);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Not ready");
    }
  else
    {
    cur->cli_llid = cli_llid;
    cur->cli_tid = cli_tid;
    if ((strlen(cur->side[0].lan_added)) && (strlen(cur->side[1].lan_added))) 
      {
      if (!strlen(cur->side[0].lan))
        KERR("ERROR %s %s", name, cur->side[0].lan_added);
      if (!strlen(cur->side[1].lan))
        KERR("ERROR %s %s", name, cur->side[1].lan_added);
      mactopo_del_req(item_a2b, cur->name, 0, cur->side[0].lan_added);
      mactopo_del_req(item_a2b, cur->name, 1, cur->side[1].lan_added);
      val1 = lan_del_name(cur->side[0].lan_added, item_a2b, cur->name, 0);
      val2 = lan_del_name(cur->side[1].lan_added, item_a2b, cur->name, 1);
      memset(cur->side[0].lan_added, 0, MAX_NAME_LEN);
      memset(cur->side[1].lan_added, 0, MAX_NAME_LEN);
      if (val1 != 2*MAX_LAN)
        {
        msg_send_del_lan_endp(ovsreq_del_a2b_lan, name, 0,
                              cur->side[0].vhost, cur->side[0].lan);
        cur->side[0].del_a2b_req = 1;
        }
      if (val2 != 2*MAX_LAN)
        {
        msg_send_del_lan_endp(ovsreq_del_a2b_lan, name, 1,
                              cur->side[1].vhost, cur->side[1].lan);
        cur->side[1].del_a2b_req = 1;
        }
      if ((val1 == 2*MAX_LAN) && (val2 == 2*MAX_LAN))
        {
        utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
        rpct_send_kil_req(cur->llid, type_hop_a2b);
        launch_timer_free_a2b(cur->name);
        }
      }
    else if (strlen(cur->side[0].lan_added))
      {
      if (!strlen(cur->side[0].lan))
        KERR("ERROR %s %s", name, cur->side[0].lan_added);
      mactopo_del_req(item_a2b, cur->name, 0, cur->side[0].lan_added);
      val = lan_del_name(cur->side[0].lan_added, item_a2b, cur->name, 0);
      memset(cur->side[0].lan_added, 0, MAX_NAME_LEN);
      if (val == 2*MAX_LAN)
        {
        utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
        rpct_send_kil_req(cur->llid, type_hop_a2b);
        launch_timer_free_a2b(cur->name);
        }
      else
        {
        msg_send_del_lan_endp(ovsreq_del_a2b_lan, name, 0,
                              cur->side[0].vhost, cur->side[0].lan);
        cur->side[0].del_a2b_req = 1;
        }
      }
    else if (strlen(cur->side[1].lan_added))
      {
      if (!strlen(cur->side[1].lan))
        KERR("ERROR %s %s", name, cur->side[1].lan_added);
      mactopo_del_req(item_a2b, cur->name, 1, cur->side[1].lan_added);
      val = lan_del_name(cur->side[1].lan_added, item_a2b, cur->name, 1);
      memset(cur->side[1].lan_added, 0, MAX_NAME_LEN);
      if (val == 2*MAX_LAN)
        {
        utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
        rpct_send_kil_req(cur->llid, type_hop_a2b);
        launch_timer_free_a2b(cur->name);
        }
      else
        {
        msg_send_del_lan_endp(ovsreq_del_a2b_lan, name, 0,
                              cur->side[1].vhost, cur->side[1].lan);
        cur->side[1].del_a2b_req = 1;
        }
      }
    else
      {
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      rpct_send_kil_req(cur->llid, type_hop_a2b);
      launch_timer_free_a2b(cur->name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_a2b_add_lan(int cli_llid, int cli_tid, char *name, int num, char *lan)
{
  t_ovs_a2b *cur = find_a2b(name);
  char err[MAX_PATH_LEN];
  if ((num != 0) && (num != 1))
    {
    KERR("ERROR %s %d", name, num);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Bad num");
    }
  else if (!cur)
    {
    KERR("ERROR %s", name);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Does not exist");
    }
  else if ((cur->cli_llid) ||
           (cur->side[0].del_a2b_req) ||
           (cur->side[1].del_a2b_req))
    {
    KERR("ERROR %s %d %d %d", name, cur->cli_llid,
                              cur->side[0].del_a2b_req,
                              cur->side[1].del_a2b_req);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Not ready");
    }
  else if (strlen(cur->side[num].lan))
    {
    KERR("ERROR %s %s", name, cur->side[num].lan);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Lan exists");
    }
  else
    {
    lan_add_name(lan, item_a2b, name, num);
    if (mactopo_add_req(item_a2b, name, num, lan, NULL, NULL, err))
      {
      KERR("ERROR %s %d %s %s", name, num, lan, err);
      send_status_ko(cli_llid, cli_tid, err);
      lan_del_name(lan, item_a2b, name, num);
      }
    else if (msg_send_add_lan_endp(ovsreq_add_a2b_lan, name, num,
                                   cur->side[num].vhost, lan))
      {
      KERR("ERROR %s", name);
      send_status_ko(cli_llid, cli_tid, "msg_send_add_lan_endp error");
      lan_del_name(lan, item_a2b, name, num);
      }
    else
      {
      strncpy(cur->side[num].lan_added, lan, MAX_NAME_LEN);
      cur->cli_llid = cli_llid;
      cur->cli_tid = cli_tid;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_a2b_del_lan(int cli_llid, int cli_tid, char *name, int num, char *lan)
{
  t_ovs_a2b *cur = find_a2b(name);
  int val = 0;
  if (!cur)
    {
    KERR("ERROR %s", name);
    send_status_ko(cli_llid, cli_tid, "Does not exist");
    }
  else if ((cur->cli_llid) ||
           (cur->side[0].del_a2b_req) ||
           (cur->side[1].del_a2b_req))
    {
    KERR("ERROR %s %d %d %d", name, cur->cli_llid,
                              cur->side[0].del_a2b_req,
                              cur->side[1].del_a2b_req);
    send_status_ko(cli_llid, cli_tid, "Not ready");
    }
  else if ((num != 0) && (num != 1))
    {
    KERR("ERROR %s %d", name, num);
    send_status_ko(cli_llid, cli_tid, "Bad param");
    }
  else
    {
    if (!strlen(cur->side[num].lan_added))
      KERR("ERROR: %s %d %s", name, num, lan);
    else
      {
      mactopo_del_req(item_a2b, cur->name, num, cur->side[num].lan_added);
      val = lan_del_name(cur->side[num].lan_added, item_a2b, cur->name, num);
      memset(cur->side[num].lan_added, 0, MAX_NAME_LEN);
      }
    if (val == 2*MAX_LAN)
      {
      memset(cur->side[num].lan, 0, MAX_NAME_LEN);
      cfg_hysteresis_send_topo_info();
      send_status_ok(cli_llid, cli_tid, "OK");
      }
    else
      {
      cur->cli_llid = cli_llid;
      cur->cli_tid = cli_tid;
      if (msg_send_del_lan_endp(ovsreq_del_a2b_lan, name, num,
                                cur->side[num].vhost, lan))
        utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int  ovs_a2b_dyn_snf(char *name, int num, int val)
{
  int result = -1;
  t_ovs_a2b *cur = find_a2b(name);
  int endp_type = 0;
  char *vh, *lan;
  char tap[MAX_NAME_LEN];

  if (cur == NULL)
    KERR("ERROR %s %d", name, val);
  else
    {
    if ((num == 0) || (num == 1))
      endp_type = cur->side[num].endp_type;
    else
      KERR("ERROR %s %d %d", name, num, val);
    if (val)
      {
      if (endp_type == endp_type_eths)
        KERR("ERROR %s %d %d", name, num, val);
      else
        {
        cur->side[num].endp_type = endp_type_eths;
        vh = cur->side[num].vhost;
        cur->side[num].del_snf_ethv_sent = 0;
        ovs_dyn_snf_start_process(name, num, item_type_a2b, vh, snf_started);
        result = 0;
        }
      }
    else
      {
      if (endp_type == endp_type_ethv)
        KERR("ERROR %s %d %d", name, num, val);
      else
        {
        cur->side[num].endp_type = endp_type_ethv;
        memset(tap, 0, MAX_NAME_LEN);
        vh = cur->side[num].vhost;
        lan = cur->side[num].lan;
        snprintf(tap, MAX_NAME_LEN-1, "s%s", vh);
        if (cur->side[num].del_snf_ethv_sent == 0)
          {
          cur->side[num].del_snf_ethv_sent = 1;
          if (strlen(lan))
            {
            if (ovs_snf_send_del_snf_lan(name, num, vh, lan))
              KERR("ERROR %s %d %d %s", name, num, val, lan);
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

/****************************************************************************/
int ovs_a2b_param_config(int llid, int tid, char *name, char *cmd)
{
  int ret, dir, result = -1;
  char msg[MAX_PATH_LEN];
  unsigned char targ[4];
  unsigned char src[4];
  t_ovs_a2b *cur = find_a2b(name);
  
  memset(msg, 0, MAX_PATH_LEN);
  if (cur)
    {
    ret = param_config_check(name, cmd, &dir, targ, src);
    if (ret == 1)
      {
      result = 0; 
      snprintf(msg, MAX_PATH_LEN-1,
               "a2b_param_config %d %d %s dir_cmd_val=%s", llid, tid, name, cmd);
      }
    else if (ret == 2)
      {
      result = 0;
      snprintf(msg, MAX_PATH_LEN-1,
               "a2b_param_config %d %d %s unreplace_ipdst dir=%d",
               llid, tid, name, dir);
      }
    else if (ret == 3)
      {
      result = 0;
      snprintf(msg, MAX_PATH_LEN-1,
               "a2b_param_config %d %d %s unreplace_ipsrc dir=%d",
               llid, tid, name, dir);
      }
    else if (ret == 4)
      {
      result = 0; 
      snprintf(msg, MAX_PATH_LEN-1,
               "a2b_param_config %d %d %s replace_ipdst "
               "dir=%d %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
               llid, tid, name, dir,
               targ[0], targ[1], targ[2], targ[3],
               src[0], src[1], src[2], src[3]);
      }
    else if (ret == 5)
      {
      result = 0; 
      snprintf(msg, MAX_PATH_LEN-1,
               "a2b_param_config %d %d %s replace_ipsrc "
               "dir=%d %hhu.%hhu.%hhu.%hhu with %hhu.%hhu.%hhu.%hhu",
               llid, tid, name, dir,
               targ[0], targ[1], targ[2], targ[3],
               src[0], src[1], src[2], src[3]);
      }
    else
      KERR("ERROR %s %s", name, cmd);
    }
  else
    KERR("ERROR %s %s", name, cmd);
  if (result == 0)
    {
    rpct_send_sigdiag_msg(cur->llid, type_hop_a2b, msg);
    hop_event_hook(cur->llid, FLAG_HOP_SIGDIAG, msg);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void ovs_a2b_init(void)
{
  char *root = cfg_get_root_work();
  char *net = cfg_get_cloonix_name();
  char *bin_a2b = utils_get_ovs_a2b_bin_dir();

  g_nb_a2b = 0;
  g_head_a2b = NULL;
  memset(g_cloonix_net, 0, MAX_NAME_LEN);
  memset(g_root_path, 0, MAX_PATH_LEN);
  memset(g_bin_a2b, 0, MAX_PATH_LEN);
  strncpy(g_cloonix_net, net, MAX_NAME_LEN-1);
  strncpy(g_root_path, root, MAX_PATH_LEN-1);
  strncpy(g_bin_a2b, bin_a2b, MAX_PATH_LEN-1);
  clownix_timeout_add(100, timer_heartbeat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

