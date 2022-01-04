/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "hop_event.h"
#include "cfg_store.h"
#include "utils_cmd_line_maker.h"
#include "container.h"
#include "event_subscriber.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "system_callers.h"
#include "suid_power.h"
#include "dpdk_msg.h"

static t_cnt *g_head_container;

/****************************************************************************/
static int get_nb_container(void)
{
  int result = 0;
  t_cnt *cur = g_head_container;
  while (cur)
    {
    result += 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_cnt *find_container(char *name)
{
  t_cnt *cur = g_head_container;
  while (cur)
    {
    if (!strcmp(cur->cnt.name, name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char *get_eth_name(int rank, int vm_id, int num)
{
  static char eth_name[MAX_NAME_LEN];
  memset(eth_name, 0, MAX_NAME_LEN);
  snprintf(eth_name, MAX_PATH_LEN-1, "vgt_%d_%d_%d", rank, vm_id, num);
  return eth_name;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int alloc_container(int cli_llid, int cli_tid, t_topo_cnt *cnt,
                           int cloonix_rank, int vm_id,
                           char *cnt_dir, char *err)
{
  int i, result = -1; 
  char *eth_name;
  t_cnt *cur = find_container(cnt->name);
  if (cur != NULL)
    {
    snprintf(err, MAX_PATH_LEN-1, 
    "ERROR %s already exists", cnt->name);
    KERR("%s", err);
    }
  else if (mk_cnt_dirs(cnt_dir, cnt->name))
    {
    snprintf(err, MAX_PATH_LEN-1, 
    "ERROR creating %s for %s", cnt_dir, cnt->name);
    KERR("%s", err);
    }
  else
    {
    cur = (t_cnt *) malloc(sizeof(t_cnt));
    memset(cur, 0, sizeof(t_cnt));
    memcpy(&(cur->cnt), cnt, sizeof(t_topo_cnt));
    cur->cloonix_rank = cloonix_rank;
    cur->vm_id = vm_id;
    cur->cli_llid = cli_llid;
    cur->cli_tid = cli_tid;
    if (g_head_container)
      g_head_container->prev = cur;
    cur->next = g_head_container;
    g_head_container = cur;
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    result = 0;
    layout_add_vm(cnt->name, cli_llid);
    for (i=0; i<cnt->nb_tot_eth; i++)
      {
      eth_name = get_eth_name(cloonix_rank, vm_id, i);
      suid_power_rec_name(eth_name, 1);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int free_container(char *name)
{
  int i, result = -1;
  t_cnt *cur = find_container(name);
  char *eth_name;
  if (cur != NULL)
    {
    layout_del_vm(name);
    cfg_free_vm_id(cur->vm_id);
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == g_head_container)
      g_head_container = cur->next;
    for (i=0; i<cur->cnt.nb_tot_eth; i++)
      {
      eth_name = get_eth_name(cur->cloonix_rank, cur->vm_id, i);
      suid_power_rec_name(eth_name, 0);
      }
    free(cur);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int send_sig_suid_power(int llid, char *msg)
{
  int result = -1;
  if ((llid) && (msg_exist_channel(llid)))
    {
    hop_event_hook(llid, FLAG_HOP_SIGDIAG, msg);
    rpct_send_sigdiag_msg(llid, type_hop_suid_power, msg);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void container_resp_add_lan(int is_ko, char *lan, char *name, int num)
{
  t_cnt *cur = find_container(name);
  if (cur == NULL)
    KERR("ERROR RESP ADD LAN %d %s %s %d", is_ko, lan, name, num);
  else
    {
    if (is_ko)
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), "KO");
    else
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
    cur->lan_add_waiting_ack = 0;
    cur->att_lan[num].lan_attached_ok = 1; 
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void container_resp_del_lan(int is_ko, char *lan, char *name, int num)
{
  t_cnt *cur = find_container(name);
  if (cur == NULL)
    KERR("ERROR RESP ADD LAN %d %s %s %d", is_ko, lan, name, num);
  else
    {
    if (is_ko)
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), "KO");
    else
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
    cur->lan_del_waiting_ack = 0;
    cur->att_lan[num].lan_attached_ok = 0; 
    memset(cur->att_lan[num].lan, 0, MAX_NAME_LEN);
    event_subscriber_send(sub_evt_topo, cfg_produce_topo_info());
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_cnt *container_get_first_cnt(int *nb)
{
  *nb = get_nb_container();
  return g_head_container;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *translate_topo_endp_cnt(int *nb)
{
  t_cnt *cur;
  int i, len, count = 0, nb_endp = 0;
  t_topo_endp *endp;
  cur = g_head_container;
  while(cur)
    {
    for (i=0; i<MAX_DPDK_VM; i++)
      {
      if (cur->att_lan[i].lan_attached_ok) 
        count += 1;
      }
    cur = cur->next;
    }
  endp = (t_topo_endp *) clownix_malloc(count * sizeof(t_topo_endp), 13);
  memset(endp, 0, count * sizeof(t_topo_endp));
  cur = g_head_container;
  while(cur)
    {
    for (i=0; i<MAX_DPDK_VM; i++)
      {
      if (cur->att_lan[i].lan_attached_ok) 
        {
        len = sizeof(t_lan_group_item);
        endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
        memset(endp[nb_endp].lan.lan, 0, len);
        strncpy(endp[nb_endp].name, cur->cnt.name, MAX_NAME_LEN-1);
        endp[nb_endp].num = i;
        endp[nb_endp].type = endp_type_ethv;
        if (strlen(cur->att_lan[i].lan))
          {
          strncpy(endp[nb_endp].lan.lan[0].lan, cur->att_lan[i].lan,
                  MAX_NAME_LEN-1);
          endp[nb_endp].lan.nb_lan = 1;
          }
        nb_endp += 1;
        }
      }
    cur = cur->next;
    }
  *nb = nb_endp;
  return endp;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void container_poldiag_resp(int llid, char *line)
{
  t_small_evt vm_evt;
  t_cnt *cur;
  char name[MAX_NAME_LEN];
  if (!strcmp(line, "cloonixsuid_container_infos"))
    {
    }
  else if (sscanf(line,
      "cloonixsuid_container_get_crun_run_check_resp_ok %s", name) == 1) 
    {
    cur = find_container(name);
    if (cur == NULL)
      KERR("ERROR %s", line);
    else
      {
      if (cur->cnt.ping_ok == 0)
        {
        cur->cnt.ping_ok = 1;
        memset(&vm_evt, 0, sizeof(vm_evt));
        strncpy(vm_evt.name, name, MAX_NAME_LEN-1);
        vm_evt.evt = vm_evt_cloonix_ga_ping_ok;
        event_subscriber_send(topo_small_event, (void *) &vm_evt);
        }
      }
    }
  else if (sscanf(line,
      "cloonixsuid_container_get_crun_run_check_resp_ko %s", name) == 1) 
    {
    cur = find_container(name);
    if (cur == NULL)
      KERR("ERROR %s", line);
    else
      {
      if (cur->cnt.ping_ok == 1)
        {
        cur->cnt.ping_ok = 0;
        memset(&vm_evt, 0, sizeof(vm_evt));
        strncpy(vm_evt.name, name, MAX_NAME_LEN-1);
        vm_evt.evt = vm_evt_cloonix_ga_ping_ko; 
        event_subscriber_send(topo_small_event, (void *) &vm_evt);
        }
      }
    }
  else
    KERR("ERROR %s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void container_poldiag_req(int llid, int tid)
{
  t_cnt *cur = g_head_container;
  char req[MAX_PATH_LEN];
  memset(req, 0, MAX_PATH_LEN);
  snprintf(req, MAX_PATH_LEN-1, "cloonixsuid_container_get_infos");
  hop_event_hook(llid, tid, req);
  rpct_send_poldiag_msg(llid, type_hop_suid_power, req);
  while (cur)
    {
    if (cur->crun_pid > 0)
      {
      memset(req, 0, MAX_PATH_LEN);
      snprintf(req, MAX_PATH_LEN-1,
      "cloonixsuid_container_get_crun_run_check %s", cur->cnt.name);
      hop_event_hook(llid, tid, req);
      rpct_send_poldiag_msg(llid, type_hop_suid_power, req);
      }
    cur = cur->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void container_sigdiag_resp(int llid, char *line)
{
  int crun_pid;
  t_cnt *cur;
  char name[MAX_NAME_LEN];
  char req[MAX_PATH_LEN];
  memset(req, 0, MAX_PATH_LEN);

  if (sscanf(line, 
  "cloonixsuid_container_create_net_resp_ok name=%s", name) == 1)
    {
    cur = find_container(name);
    if (cur == NULL) 
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, MAX_PATH_LEN-1, 
               "cloonixsuid_container_create_config_json name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }

  else if (sscanf(line,
  "cloonixsuid_container_create_config_json_resp_ok name=%s", name) == 1)
    {
    cur = find_container(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, MAX_PATH_LEN-1,
               "cloonixsuid_container_create_loop_img name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }

  else if (sscanf(line,
  "cloonixsuid_container_create_loop_img_resp_ok name=%s", name) == 1)
    {
    cur = find_container(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, MAX_PATH_LEN-1,
               "cloonixsuid_container_create_overlay name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }

  else if (sscanf(line,
  "cloonixsuid_container_create_overlay_resp_ok name=%s", name) == 1)
    {
    cur = find_container(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, MAX_PATH_LEN-1,
               "cloonixsuid_container_create_crun_start name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }

  else if (sscanf(line,
  "cloonixsuid_container_create_crun_start_resp_ok name=%s crun_pid=%d",
  name, &crun_pid) == 2)
    {
    cur = find_container(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      cur->crun_pid = crun_pid;
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      }
    }

  else if (sscanf(line, 
  "cloonixsuid_container_delete_resp_ok name=%s", name) == 1)
    {
    cur = find_container(name);
    if (cur == NULL) 
      KERR("ERROR %s", name);
    else
      {
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      free_container(name);
      }
    }

  else if (sscanf(line,
  "cloonixsuid_container_killed name=%s crun_pid=%d", name, &crun_pid) == 2)
    {
    KERR("ERROR %s", line);
    cur = find_container(name);
    if (cur == NULL) 
      KERR("ERROR %s", name);
    else
      {
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"CNT KILLED");
      free_container(name);
      }
    }

  else if ((sscanf(line,
  "cloonixsuid_container_create_net_resp_ko name=%s", name) == 1) ||
           (sscanf(line,
  "cloonixsuid_container_create_overlay_resp_ko name=%s", name) == 1) ||
           (sscanf(line,
  "cloonixsuid_container_create_crun_start_resp_ko name=%s", name) == 1) ||
           (sscanf(line,
  "cloonixsuid_container_delete_resp_ko name=%s", name) == 1))
    {
    KERR("ERROR %s", line);
    cur = find_container(name);
    if (cur != NULL)
      {
      memset(req, 0, MAX_PATH_LEN);
      snprintf(req, MAX_PATH_LEN-1,
      "cloonixsuid_container_ERROR name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"CNT ERROR");
      free_container(name);
      }
    }
  else if (sscanf(line, 
  "cloonixsuid_container_ERROR name=%s", name) == 1)
    {
    KERR("ERROR %s", name);
    cur = find_container(name);
    if (cur != NULL)
      {
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"ERROR CNT");
      free_container(name);
      }
    }

  else
    KERR("ERROR %s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int container_name_exists(char *name, int *nb_eth)
{
  int result = 0;
  t_cnt *cur = find_container(name);
  if (cur != NULL)
    {
    *nb_eth = cur->cnt.nb_tot_eth;
    result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int container_info(char *name, int *nb_eth, t_eth_table **eth_table)
{
  int result = 0;
  t_cnt *cur = find_container(name);
  if (cur != NULL)
    {
    if (cur->cli_llid)
      KERR("ERROR CNT %s NOT READY", name);
    else if (cur->lan_add_waiting_ack)
      KERR("ERROR CNT %s NOT READY", name);
    else if (cur->lan_del_waiting_ack)
      KERR("ERROR CNT %s NOT READY", name);
    else
      {
      *nb_eth = cur->cnt.nb_tot_eth;
      *eth_table = cur->cnt.eth_table;
      result = 1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int container_create(int llid, int cli_llid, int cli_tid,
                     int cloonix_rank, int vm_id,
                     t_topo_cnt *cnt, char *err)
{
  int i, abort = 0, result = -1;
  char req[2*MAX_PATH_LEN];
  char agent_dir[MAX_PATH_LEN];
  char *image, *mac;
  char *cnt_dir = utils_get_cnt_dir();
  char *agd = agent_dir; 

  for (i=0; i<cnt->nb_tot_eth; i++)
    {
    if (cnt->eth_table[i].endp_type != endp_type_ethv)
      {
      snprintf(err, MAX_PATH_LEN-1,
      "ERROR %s eth%d must be vhost", cnt->name, i);
      KERR("%s", err);
      abort = 1;
      }
    }
  if (abort == 0)
    {
    if (alloc_container(cli_llid, cli_tid, cnt, cloonix_rank,
                        vm_id, cnt_dir, err) == 0)
      {
      memset(agent_dir, 0, MAX_PATH_LEN);
      snprintf(agent_dir, MAX_PATH_LEN-1, 
      "%s/common/agent_dropbear/agent_bin_alien", cfg_get_bin_dir());
      image = cnt->image;
      memset(req, 0, 2*MAX_PATH_LEN);
      snprintf(req, 2*MAX_PATH_LEN-1, 
      "cloonixsuid_container_create_net name=%s "
      "bulk=%s image=%s nb=%d rank=%d vm_id=%d cnt_dir=%s "
      "agent_dir=%s",
      cnt->name, cfg_get_bulk(), image, cnt->nb_tot_eth,
      cloonix_rank, vm_id, cnt_dir, agd);
      if (send_sig_suid_power(llid, req))
        {
        snprintf(err, MAX_PATH_LEN-1,
        "ERROR %s Bad command create_net to suid_power", cnt->name);
        KERR("%s", err);
        free_container(cnt->name);
        }
      else
        {
        for (i=0; i<cnt->nb_tot_eth; i++)
          {
          memset(req, 0, 2*MAX_PATH_LEN);
          mac = cnt->eth_table[i].mac_addr;
          snprintf(req, 2*MAX_PATH_LEN-1,
          "cloonixsuid_container_create_eth name=%s num=%d "
          "mac=0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx:0x%02hhx",
          cnt->name, i, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
          if (send_sig_suid_power(llid, req))
            KERR("ERROR %s", cnt->name);
          }
        result = 0;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int container_delete(int llid, int cli_llid, int cli_tid,
                     char *name, char *err)
{
  int result = -1;
  char req[MAX_PATH_LEN];
  t_cnt *cur = find_container(name);
  if (cur == NULL)
    {
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s NOT FOUND", name);
    KERR("%s", err);
    }
  else
    {
    if ((cur->cli_llid != 0) || (cur->cli_tid != 0))
      {
      KERR("WARNING %s sending late ko", name);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), "STOPPED CNT");
      }
    memset(req, 0, MAX_PATH_LEN);
    snprintf(req, MAX_PATH_LEN-1,
    "cloonixsuid_container_delete name=%s", name);
    if (send_sig_suid_power(llid, req))
      {
      snprintf(err, MAX_PATH_LEN-1,
      "ERROR %s Bad command delete to suid_power", name);
      KERR("%s", err);
      }
    else
      {
      cur->cli_llid = cli_llid;
      cur->cli_tid = cli_tid;
      result = 0;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int container_delete_all(int llid)
{
  t_cnt *next, *cur = g_head_container;
  char req[MAX_PATH_LEN];
  int result = 0;
  while (cur)
    {
    next = cur->next;
    memset(req, 0, MAX_PATH_LEN);
    snprintf(req, MAX_PATH_LEN-1,
    "cloonixsuid_container_delete name=%s", cur->cnt.name);
    if (send_sig_suid_power(llid, req))
      KERR("ERROR %d %s", llid, cur->cnt.name);
    result += 1;
    cur = next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void error_timer_beat_action(int llid, t_cnt *cur)
{
  char req[MAX_PATH_LEN];
  memset(req, 0, MAX_PATH_LEN);
  snprintf(req, MAX_PATH_LEN-1,
  "cloonixsuid_container_ERROR name=%s", cur->cnt.name);
  if (send_sig_suid_power(llid, req))
    KERR("ERROR %d %s", llid, cur->cnt.name);
  utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), "TIMOUT");
  free_container(cur->cnt.name);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void container_timer_beat(int llid)
{
  t_cnt *next, *cur = g_head_container;
  while (cur)
    {
    next = cur->next;
    if (cur->lan_add_waiting_ack == 0)
      cur->count_add = 0;
    else
      {
      cur->count_add += 1;
      if (cur->count_add >= 10)
        { 
        KERR("ERROR TIMOUT %s", cur->cnt.name);
        error_timer_beat_action(llid, cur);
        }
      }
   if (cur->lan_del_waiting_ack == 0)
      cur->count_del = 0;
    else
      {
      cur->count_del += 1;
      if (cur->count_del >= 10)
        { 
        KERR("ERROR TIMOUT %s", cur->cnt.name);
        error_timer_beat_action(llid, cur);
        }
      }
   if (cur->cli_llid == 0)
      cur->count_llid = 0;
    else
      {
      cur->count_llid += 1;
      if (cur->count_llid >= 10)
        { 
        KERR("ERROR TIMOUT %s", cur->cnt.name);
        error_timer_beat_action(llid, cur);
        }
      }
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int  container_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_cnt *cur = g_head_container;
  int i, result = 0;
  event_print("%s", __FUNCTION__);
  while(cur)
    {
    if (cur->crun_pid)
      result++;
    cur = cur->next;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid),5);
    memset(glob_lst, 0, result*sizeof(t_lst_pid));
    cur = g_head_container;
    i = 0;
    while(cur)
      {
      if (cur->crun_pid)
        {
        strncpy(glob_lst[i].name, cur->cnt.name, MAX_NAME_LEN-1);
        glob_lst[i].pid = cur->crun_pid;
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
int container_add_lan(int llid, int tid, char *name, int num,
                      char *lan, char *err)
{
  int result = -1;
  t_cnt *cur = find_container(name);
  char *eth_name;
  memset(err, 0, MAX_PATH_LEN);
  if (cur == NULL)
    {
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s NOT FOUND", name); 
    KERR("%s", err);
    }
  else if ((cur->cnt.nb_tot_eth <= num) ||
           (cur->cnt.eth_table[num].endp_type != endp_type_ethv))
    {
    KERR("ERROR %s %d %d", name, num, cur->cnt.nb_tot_eth);
    snprintf(err, MAX_PATH_LEN-1,
    "ERROR %s %d Bad endp_type or num max=%d",
    name, num, cur->cnt.nb_tot_eth); 
    KERR("%s %d %d", err, cur->cnt.nb_tot_eth, num);
    }
  else if ((cur->cli_llid) ||
           (cur->lan_add_waiting_ack) ||
           (cur->lan_del_waiting_ack) ||
           (cur->att_lan[num].lan_attached_ok)) 
    {
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s BAD STATE FOR PROCESSING", name); 
    KERR("%s llid:%d add:%d del:%d att:%d", err, cur->cli_llid,
    cur->lan_add_waiting_ack, cur->lan_del_waiting_ack,
    cur->att_lan[num].lan_attached_ok);
    }
  else
    {
    eth_name = get_eth_name(cur->cloonix_rank, cur->vm_id, num);
    if (dpdk_msg_send_add_lan_ethv_cnt(lan, name, num, eth_name))
      {
      snprintf(err, MAX_PATH_LEN-1, "ERROR ADD LAN %s %d %s", name, num, lan); 
      KERR("%s", err);
      }
    else
      {
      result = 0;
      cur->cli_llid = llid;
      cur->cli_tid = tid;
      cur->lan_add_waiting_ack = 1;
      memset(cur->att_lan[num].lan, 0, MAX_NAME_LEN);
      strncpy(cur->att_lan[num].lan, lan, MAX_NAME_LEN-1);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int container_del_lan(int llid, int tid, char *name, int num,
                      char *lan, char *err)
{
  int result = -1;
  t_cnt *cur = find_container(name);
  char *eth_name;
  memset(err, 0, MAX_PATH_LEN);
  if (cur == NULL)
    {
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s NOT FOUND", name);
    KERR("%s", err);
    }
  else if ((cur->cnt.nb_tot_eth <= num) ||
           (cur->cnt.eth_table[num].endp_type != endp_type_ethv))
    {
    KERR("ERROR %s %d %d", name, num, cur->cnt.nb_tot_eth);
    snprintf(err, MAX_PATH_LEN-1,
    "ERROR %s %d Bad endp_type or num max=%d",
    name, num, cur->cnt.nb_tot_eth); 
    KERR("%s %d %d", err, cur->cnt.nb_tot_eth, num);
    }
  else if ((cur->cli_llid) ||
           (cur->lan_add_waiting_ack) ||
           (cur->lan_del_waiting_ack) ||
           (cur->att_lan[num].lan_attached_ok == 0))
    {
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s BAD STATE FOR PROCESSING", name);
    KERR("%s llid:%d add:%d del:%d att:%d", err, cur->cli_llid,
    cur->lan_add_waiting_ack, cur->lan_del_waiting_ack,
    cur->att_lan[num].lan_attached_ok);
    }
  else if (strcmp(cur->att_lan[num].lan, lan))
    {
    snprintf(err, MAX_PATH_LEN-1,
    "ERROR CNT cannot attach %s, %s %d attached to %s",
    lan, name, num, cur->att_lan[num].lan);
    KERR("%s", err);
    }
  else
    {
    eth_name = get_eth_name(cur->cloonix_rank, cur->vm_id, num);
    if (dpdk_msg_send_del_lan_ethv_cnt(lan, name, num, eth_name))
      {
      snprintf(err, MAX_PATH_LEN-1, "ERROR DEL LAN %s %d %s", name, num, lan); 
      KERR("%s", err);
      }
    else
      { 
      result = 0;
      cur->cli_llid = llid;
      cur->cli_tid = tid;
      cur->lan_del_waiting_ack = 1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void container_init(void)
{
  g_head_container = NULL;
}
/*---------------------------------------------------------------------------*/

