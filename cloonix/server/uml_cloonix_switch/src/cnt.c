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
#include "cnt.h"
#include "event_subscriber.h"
#include "layout_rpc.h"
#include "layout_topo.h"
#include "system_callers.h"
#include "suid_power.h"
#include "ovs_snf.h"
#include "msg.h"
#include "lan_to_name.h"
#include "doors_rpc.h"
#include "doorways_mngt.h"

typedef struct t_cnt_delete
{
  int llid;
  int cli_llid;
  int cli_tid;
  char name[MAX_NAME_LEN];
  int count;
} t_cnt_delete;

static t_cnt *g_head_cnt;

/****************************************************************************/
static int get_nb_cnt(void)
{
  int result = 0;
  t_cnt *cur = g_head_cnt;
  while (cur)
    {
    result += 1;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_cnt *find_cnt(char *name)
{
  t_cnt *cur = g_head_cnt;
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
static char *get_eth_name(int vm_id, int num)
{
  static char eth_name[MAX_NAME_LEN];
  memset(eth_name, 0, MAX_NAME_LEN);
  snprintf(eth_name, MAX_PATH_LEN-1, "%s%d_%d",
           OVS_BRIDGE_PORT, vm_id, num);
  return eth_name;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int alloc_cnt(int cli_llid, int cli_tid, t_topo_cnt *cnt,
                           int vm_id, char *cnt_dir, char *err)
{
  int result = -1; 
  t_cnt *cur = find_cnt(cnt->name);
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
    cur->cnt.vm_id = vm_id;
    cur->cli_llid = cli_llid;
    cur->cli_tid = cli_tid;
    if (g_head_cnt)
      g_head_cnt->prev = cur;
    cur->next = g_head_cnt;
    g_head_cnt = cur;
    cfg_hysteresis_send_topo_info();
    result = 0;
    layout_add_vm(cnt->name, cli_llid);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int free_cnt(char *name)
{
  int i, result = -1;
  t_cnt *cur = find_cnt(name);
  if (cur != NULL)
    {
    for (i=0; i<MAX_ETH_VM; i++)
      {
      if (strlen(cur->att_lan[i].lan_added))
        lan_del_name(cur->att_lan[i].lan_added, item_cnt, name, i);
      }
    layout_del_vm(name);
    cfg_free_obj_id(cur->cnt.vm_id);
    if (cur->prev)
      cur->prev->next = cur->next;
    if (cur->next)
      cur->next->prev = cur->prev;
    if (cur == g_head_cnt)
      g_head_cnt = cur->next;
    free(cur);
    cfg_hysteresis_send_topo_info();
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
static void cnt_vhost_begin(t_cnt *cnt)
{
  char *eth_name, *name;
  int i;
  for (i=0; i<cnt->cnt.nb_tot_eth; i++)
    {
    eth_name = cnt->cnt.eth_table[i].vhost_ifname;
    name = cnt->cnt.name;
    if (msg_send_vhost_up(name, i, eth_name))
      KERR("ERROR KVMETH %s %d %s", name, i, eth_name);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cnt_resp_add_lan(int is_ko, char *name, int num, char *vhost, char *lan)
{
  t_cnt *cur = find_cnt(name);
  char *eth_name;
  if (cur == NULL)
    KERR("ERROR RESP ADD LAN %d %s %s %d", is_ko, lan, name, num);
  else
    {
    if (is_ko)
      {
      KERR("ERROR RESP ADD LAN %s %s %d", lan, name, num);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), "KO");
      }
    else
      {
      cur->att_lan[num].lan_attached_ok = 1; 
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      if ((num < 0) || (num >= cur->cnt.nb_tot_eth))
        KOUT("ERROR %d", num);
      eth_name = cur->cnt.eth_table[num].vhost_ifname;
      if (cur->cnt.eth_table[num].endp_type == endp_type_eths)
        {
        ovs_snf_send_add_snf_lan(name, num, eth_name, lan);
        }
      }
    cur->lan_add_waiting_ack = 0;
    cfg_hysteresis_send_topo_info();
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void cnt_resp_del_lan(int is_ko, char *name, int num, char *vhost, char *lan)
{
  t_cnt *cur = find_cnt(name);
  if (cur == NULL)
    KERR("ERROR RESP ADD LAN %d %s %s %d", is_ko, lan, name, num);
  else
    {
    if (is_ko)
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), "KO");
    else
      {
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      if ((num < 0) || (num >= cur->cnt.nb_tot_eth))
        KOUT("ERROR %d", num);
      if ((cur->cnt.eth_table[num].endp_type == endp_type_eths) &&
          (strlen(lan)) &&
          (cur->del_snf_ethv_sent[num] == 0))
        KERR("ERROR: %s %d", name, num);
      }
    cur->lan_del_waiting_ack = 0;
    if (cur->att_lan[num].lan_attached_ok)
      {
      lan = cur->att_lan[num].lan;
      if (!strlen(lan))
        KERR("ERROR: %s", name);
      }
    cur->att_lan[num].lan_attached_ok = 0; 
    memset(cur->att_lan[num].lan, 0, MAX_NAME_LEN);
    cfg_hysteresis_send_topo_info();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_cnt *cnt_get_first_cnt(int *nb)
{
  *nb = get_nb_cnt();
  return g_head_cnt;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
t_topo_endp *cnt_translate_topo_endp(int *nb)
{
  t_cnt *cur;
  int i, len, count = 0, nb_endp = 0;
  t_topo_endp *endp;
  cur = g_head_cnt;
  while(cur)
    {
    for (i=0; i<MAX_ETH_VM; i++)
      {
      if (cur->att_lan[i].lan_attached_ok) 
        count += 1;
      }
    cur = cur->next;
    }
  endp = (t_topo_endp *) clownix_malloc(count * sizeof(t_topo_endp), 13);
  memset(endp, 0, count * sizeof(t_topo_endp));
  cur = g_head_cnt;
  while(cur)
    {
    for (i=0; i<MAX_ETH_VM; i++)
      {
      if (cur->att_lan[i].lan_attached_ok) 
        {
        len = sizeof(t_lan_group_item);
        endp[nb_endp].lan.lan = (t_lan_group_item *)clownix_malloc(len, 2);
        memset(endp[nb_endp].lan.lan, 0, len);
        strncpy(endp[nb_endp].name, cur->cnt.name, MAX_NAME_LEN-1);
        endp[nb_endp].num = i;
        endp[nb_endp].type = cur->cnt.eth_table[i].endp_type;
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
void cnt_sigdiag_resp(int llid, char *line)
{
  int crun_pid;
  t_cnt *cur;
  char name[MAX_NAME_LEN];
  char req[MAX_PATH_LEN];
  memset(req, 0, MAX_PATH_LEN);

  if (sscanf(line, 
  "cloonsuid_cnt_create_net_resp_ok name=%s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL) 
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, MAX_PATH_LEN-1, 
               "cloonsuid_cnt_create_config_json name=%s is_persistent=%d",
               name, cur->cnt.is_persistent);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }

  else if (sscanf(line,
  "cloonsuid_cnt_create_config_json_resp_ok name=%s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, MAX_PATH_LEN-1,
               "cloonsuid_cnt_create_loop_img name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }

  else if (sscanf(line,
  "cloonsuid_cnt_create_loop_img_resp_ok name=%s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, MAX_PATH_LEN-1,
               "cloonsuid_cnt_create_overlay name=%s is_persistent=%d",
               name, cur->cnt.is_persistent);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }

  else if (sscanf(line,
  "cloonsuid_cnt_create_overlay_resp_ok name=%s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      snprintf(req, MAX_PATH_LEN-1,
               "cloonsuid_cnt_create_crun_start name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      }
    }

  else if (sscanf(line,
  "cloonsuid_cnt_create_crun_start_resp_ok name=%s crun_pid=%d",
  name, &crun_pid) == 2)
    {
    cur = find_cnt(name);
    if (cur == NULL)
      KERR("ERROR %s", name);
    else
      {
      cur->crun_pid = crun_pid;
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cnt_vhost_begin(cur);
      doors_send_del_vm(get_doorways_llid(), 0, name);
      doors_send_add_vm(get_doorways_llid(), 0, name, 
                        utils_get_cnt_dropbear(name));
      }
    }

  else if (sscanf(line, 
  "cloonsuid_cnt_delete_resp_ok name=%s", name) == 1)
    {
    cur = find_cnt(name);
    if (cur == NULL) 
      KERR("ERROR %s", name);
    else
      {
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      free_cnt(name);
      }
    }

  else if (sscanf(line,
  "cloonsuid_cnt_killed name=%s crun_pid=%d", name, &crun_pid) == 2)
    {
    KERR("ERROR %s", line);
    cur = find_cnt(name);
    if (cur == NULL) 
      KERR("ERROR %s", name);
    else
      {
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"CNT KILLED");
      free_cnt(name);
      }
    }

  else if ((sscanf(line,
  "cloonsuid_cnt_create_net_resp_ko name=%s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_cnt_create_overlay_resp_ko name=%s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_cnt_create_crun_start_resp_ko name=%s", name) == 1) ||
           (sscanf(line,
  "cloonsuid_cnt_delete_resp_ko name=%s", name) == 1))
    {
    KERR("ERROR %s", line);
    cur = find_cnt(name);
    if (cur != NULL)
      {
      memset(req, 0, MAX_PATH_LEN);
      snprintf(req, MAX_PATH_LEN-1,
      "cloonsuid_cnt_ERROR name=%s", name);
      if (send_sig_suid_power(llid, req))
        KERR("ERROR %d %s", llid, name);
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"CNT ERROR");
      free_cnt(name);
      }
    }
  else if (sscanf(line, 
  "cloonsuid_cnt_ERROR name=%s", name) == 1)
    {
    KERR("ERROR %s", name);
    cur = find_cnt(name);
    if (cur != NULL)
      {
      utils_send_status_ko(&(cur->cli_llid),&(cur->cli_tid),"ERROR CNT");
      free_cnt(name);
      }
    }

  else
    KERR("ERROR %s", line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cnt_name_exists(char *name, int *nb_eth)
{
  int result = 0;
  t_cnt *cur = find_cnt(name);
  if (cur != NULL)
    {
    *nb_eth = cur->cnt.nb_tot_eth;
    result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cnt_info(char *name, int *nb_eth, t_eth_table **eth_table)
{
  int result = 0;
  t_cnt *cur = find_cnt(name);
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
int cnt_create(int llid, int cli_llid, int cli_tid, int vm_id,
                     t_topo_cnt *cnt, char *err)
{
  int i, abort = 0, result = -1;
  char req[2*MAX_PATH_LEN];
  char agent_dir[MAX_PATH_LEN];
  char *image, *mac;
  char *cnt_dir = utils_get_cnt_dir();
  char *agd = agent_dir; 
  char *eth_name;

  for (i=0; i<cnt->nb_tot_eth; i++)
    {
    if ((cnt->eth_table[i].endp_type != endp_type_ethv) &&
        (cnt->eth_table[i].endp_type != endp_type_eths))
      {
      snprintf(err, MAX_PATH_LEN-1,
      "ERROR %s eth%d must be vhost", cnt->name, i);
      KERR("%s", err);
      abort = 1;
      }
    eth_name = get_eth_name(vm_id, i);
    memset(cnt->eth_table[i].vhost_ifname, 0, MAX_NAME_LEN);
    strncpy(cnt->eth_table[i].vhost_ifname, eth_name, MAX_NAME_LEN-1); 
    }
  if (abort == 0)
    {

    if (alloc_cnt(cli_llid, cli_tid, cnt,
                        vm_id, cnt_dir, err) == 0)
      {
      memset(agent_dir, 0, MAX_PATH_LEN);
      snprintf(agent_dir, MAX_PATH_LEN-1, 
      "%s/common/agent_dropbear/agent_bin_alien", cfg_get_bin_dir());
      image = cnt->image;
      memset(req, 0, 2*MAX_PATH_LEN);
      snprintf(req, 2*MAX_PATH_LEN-1, 
      "cloonsuid_cnt_create_net name=%s "
      "bulk=%s image=%s nb=%d vm_id=%d cnt_dir=%s "
      "agent_dir=%s customer_launch=%s",
      cnt->name, cfg_get_bulk(), image, cnt->nb_tot_eth,
      vm_id, cnt_dir, agd, cnt->customer_launch);
      if (send_sig_suid_power(llid, req))
        {
        snprintf(err, MAX_PATH_LEN-1,
        "ERROR %s Bad command create_net to suid_power", cnt->name);
        KERR("%s", err);
        free_cnt(cnt->name);
        }
      else
        {
        for (i=0; i<cnt->nb_tot_eth; i++)
          {
          memset(req, 0, 2*MAX_PATH_LEN);
          mac = cnt->eth_table[i].mac_addr;
          snprintf(req, 2*MAX_PATH_LEN-1,
          "cloonsuid_cnt_create_eth name=%s num=%d "
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
static void timer_cnt_delete(void *data)
{
  t_cnt_delete *cd = (t_cnt_delete *) data;
  char err[MAX_PATH_LEN];
  t_cnt *cur = find_cnt(cd->name);
  char req[MAX_PATH_LEN];
  char tap[MAX_NAME_LEN];
  int val, i, cannot_be_deleted = 0;
  char *eth_name, *lan;

  if (cur == NULL)
    {
    memset(err, 0, MAX_PATH_LEN);
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s NOT FOUND", cd->name);
    KERR("%s", err);
    if (cd->cli_llid)
      send_status_ko(cd->cli_llid, cd->cli_tid, err);
    }
  else if (msg_ovsreq_get_qty() > 50)
    clownix_timeout_add(10, timer_cnt_delete, (void *) cd, NULL, NULL);
  else
    {
    for (i=0; i<cur->cnt.nb_tot_eth; i++)
      {
      eth_name = cur->cnt.eth_table[i].vhost_ifname;
      lan = cur->att_lan[i].lan;
      memset(tap, 0, MAX_NAME_LEN); 
      snprintf(tap, MAX_NAME_LEN-1, "s%s", eth_name);
      if ((cur->cnt.eth_table[i].endp_type == endp_type_eths) &&
          (cur->del_snf_ethv_sent[i] == 0))
        {
        cur->del_snf_ethv_sent[i] = 1;
        cannot_be_deleted += 1;
        if (strlen(lan))
          {
          if (ovs_snf_send_del_snf_lan(cd->name, i, eth_name, lan))
            KERR("ERROR DEL KVMETH %s %d %s %s", cd->name, i, eth_name, lan);
          }
        ovs_dyn_snf_stop_process(tap);
        }
      }
    if ((cannot_be_deleted) && (cd->count < 100))
      {
      cd->count += 1;
      clownix_timeout_add(1, timer_cnt_delete, (void *) cd, NULL, NULL);
      }
    else
      {
      if (cannot_be_deleted)
        KERR("ERROR DEL SNF ETH %s", cd->name);
      cannot_be_deleted = 0;
      cd->count = 0;
      for (i=0; i<cur->cnt.nb_tot_eth; i++)
        {
        if (strlen(cur->att_lan[i].lan_added))
          {
          eth_name = cur->cnt.eth_table[i].vhost_ifname;
          cannot_be_deleted += 1;
          val = lan_del_name(cur->att_lan[i].lan_added, item_cnt, cd->name, i);
          if (val != 2*MAX_LAN)
            {
            eth_name = get_eth_name(cur->cnt.vm_id, i);
            if (msg_send_del_lan_endp(ovsreq_del_cnt_lan, cd->name, i,
                                      eth_name, cur->att_lan[i].lan_added))
              KERR("ERROR %s %d %s", cd->name, i, cur->att_lan[i].lan_added);
            }
          else
            {
            cur->lan_del_waiting_ack = 0;
            cur->att_lan[i].lan_attached_ok = 0;
            memset(cur->att_lan[i].lan, 0, MAX_NAME_LEN);
            }
          memset(cur->att_lan[i].lan_added, 0, MAX_NAME_LEN);
          }
        }
      if (msg_ovsreq_exists_name(cd->name))
        {
        cannot_be_deleted += 1;
        }
      if ((cannot_be_deleted) && (cd->count < 400))
        {
        cd->count += 1;
        clownix_timeout_add(10, timer_cnt_delete, (void *) cd, NULL, NULL);
        }
      else
        {
        if (cannot_be_deleted)
          KERR("ERROR DEL KVM ETH %s", cd->name);
        memset(req, 0, MAX_PATH_LEN);
        snprintf(req,MAX_PATH_LEN-1,"cloonsuid_cnt_delete name=%s",cd->name);
        doors_send_del_vm(get_doorways_llid(), 0, cd->name);
        if (send_sig_suid_power(cd->llid, req))
          {
          snprintf(err, MAX_PATH_LEN-1, "ERROR %s delete", cd->name);
          KERR("%s", err);
          if (cd->cli_llid)
            send_status_ko(cd->cli_llid, cd->cli_tid, err);
          }
        else
          {
          cur->cli_llid = cd->cli_llid;
          cur->cli_tid = cd->cli_tid;
          }
        clownix_free(cd, __FUNCTION__);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cnt_delete(int llid, int cli_llid, int cli_tid, char *name)
{
  char err[MAX_PATH_LEN];
  t_cnt *cur = find_cnt(name);
  t_cnt_delete *cd;
  int result = -1;
  if (cur == NULL)
    {
    memset(err, 0, MAX_PATH_LEN);
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s NOT FOUND", name);
    KERR("%s", err);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, err);
    }
  else if (cur->poweroff_done == 0)
    {
    cur->poweroff_done = 1;
    if ((cur->cli_llid != 0) || (cur->cli_tid != 0))
      {
      KERR("WARNING %s sending late ko", name);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), "STOPPED CNT");
      }
    cd = (t_cnt_delete *) clownix_malloc(sizeof(t_cnt_delete), 3);
    memset(cd, 0, sizeof(t_cnt_delete));
    cd->llid = llid;
    cd->cli_llid = cli_llid;
    cd->cli_tid = cli_tid;
    strncpy(cd->name, name, MAX_NAME_LEN-1);
    timer_cnt_delete((void *) cd);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int cnt_delete_all(int llid)
{
  t_cnt *next, *cur = g_head_cnt;
  int result = 0;
  while (cur)
    {
    next = cur->next;
    cnt_delete(llid, 0, 0, cur->cnt.name);
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
  "cloonsuid_cnt_ERROR name=%s", cur->cnt.name);
  if (send_sig_suid_power(llid, req))
    KERR("ERROR %d %s", llid, cur->cnt.name);
  utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), "TIMOUT");
  free_cnt(cur->cnt.name);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cnt_timer_beat(int llid)
{
  t_cnt *next, *cur = g_head_cnt;
  while (cur)
    {
    next = cur->next;
    if (cur->lan_add_waiting_ack == 0)
      cur->count_add = 0;
    else
      {
      cur->count_add += 1;
      if (cur->count_add >= 30)
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
      if (cur->count_del >= 30)
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
      if (cur->count_llid >= 100)
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
int  cnt_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_cnt *cur = g_head_cnt;
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
    cur = g_head_cnt;
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
int cnt_add_lan(int llid, int tid, char *name, int num,
                      char *lan, char *err)
{
  int result = -1;
  t_cnt *cur = find_cnt(name);
  char *eth_name;
  char *mac;
  char str_mac[MAX_NAME_LEN];
  memset(err, 0, MAX_PATH_LEN);
  if (cur == NULL)
    {
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s NOT FOUND", name); 
    KERR("%s", err);
    }
  else if ((cur->cnt.nb_tot_eth <= num) ||
           ((cur->cnt.eth_table[num].endp_type != endp_type_ethv) &&
            (cur->cnt.eth_table[num].endp_type != endp_type_eths)))
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
    memset(cur->att_lan[num].lan_added, 0, MAX_NAME_LEN);
    strncpy(cur->att_lan[num].lan_added, lan, MAX_NAME_LEN-1);
    lan_add_name(cur->att_lan[num].lan_added, item_cnt, name, num);
    mac = cur->cnt.eth_table[num].mac_addr;
    memset(str_mac, 0, MAX_NAME_LEN);
    snprintf(str_mac, MAX_NAME_LEN-1,
             "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    lan_add_mac(cur->att_lan[num].lan_added, item_cnt, name, num, str_mac);
    eth_name = get_eth_name(cur->cnt.vm_id, num);
    if (msg_send_add_lan_endp(ovsreq_add_cnt_lan, name, num, eth_name, lan))
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
int cnt_del_lan(int llid, int tid, char *name, int num,
                      char *lan, char *err)
{
  int val, result = -1;
  t_cnt *cur = find_cnt(name);
  char *eth_name;
  memset(err, 0, MAX_PATH_LEN);
  if (cur == NULL)
    {
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s NOT FOUND", name);
    KERR("%s", err);
    }
  else if ((cur->cnt.nb_tot_eth <= num) ||
           ((cur->cnt.eth_table[num].endp_type != endp_type_ethv) &&
            (cur->cnt.eth_table[num].endp_type != endp_type_eths)))
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
    if (!strlen(cur->att_lan[num].lan_added))
      KERR("ERROR: %s %d %s", name, num, lan);
    else
      {
      val = lan_del_name(cur->att_lan[num].lan_added, item_cnt, name, num);
      memset(cur->att_lan[num].lan_added, 0, MAX_NAME_LEN);
      }
    if (val != 2*MAX_LAN)
      {
      eth_name = get_eth_name(cur->cnt.vm_id, num);
      if (msg_send_del_lan_endp(ovsreq_del_cnt_lan, name, num, eth_name, lan))
        {
        KERR("ERROR %s %d %s", name, num, lan);
        snprintf(err, MAX_PATH_LEN-1, "ERROR DEL LAN %s %d %s", name, num, lan); 
        }
      else
        { 
        cur->cli_llid = llid;
        cur->cli_tid = tid;
        cur->lan_del_waiting_ack = 1;
        result = 0;
        }
      }
    else
      {
      cur->lan_del_waiting_ack = 0;
      cur->att_lan[num].lan_attached_ok = 0;
      memset(cur->att_lan[num].lan, 0, MAX_NAME_LEN);
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void snf_process_started(char *name, int num, char *vhost)
{
  t_cnt *cur = find_cnt(name);
  char *lan, *eth_name;
  if (cur == NULL)
    KERR("ERROR %s %d", name, num);
  else
    {
    if ((num < 0) || (num >= cur->cnt.nb_tot_eth))
      KOUT("ERROR %s %d", name, num);
    if (cur->att_lan[num].lan_attached_ok == 1)
      {
      lan = cur->att_lan[num].lan;
      eth_name = cur->cnt.eth_table[num].vhost_ifname;
      ovs_snf_send_add_snf_lan(name, num, eth_name, lan);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int cnt_dyn_snf(char *name, int num, int val)
{
  char *eth_name, *lan;
  int result = -1;
  t_cnt *cur = find_cnt(name);
  char tap[MAX_NAME_LEN];
  if (cur == NULL)
    KERR("ERROR %s %d", name, num);
  else
    {
    if ((num < 0) || (num >= cur->cnt.nb_tot_eth))
      KOUT("ERROR %s %d", name, num);
    if (val)
      {
      if (cur->cnt.eth_table[num].endp_type == endp_type_eths)
        KERR("ERROR %s %d", name, num);
      else
        {
        eth_name = cur->cnt.eth_table[num].vhost_ifname;
        cur->cnt.eth_table[num].endp_type = endp_type_eths;
        ovs_dyn_snf_start_process(name, num, item_type_ceth,
                                  eth_name, snf_process_started);
        result = 0;
        }
      }
    else
      {
      if (cur->cnt.eth_table[num].endp_type == endp_type_ethv)
        KERR("ERROR %s %d", name, num);
      else
        {
        cur->cnt.eth_table[num].endp_type = endp_type_ethv;
        eth_name = cur->cnt.eth_table[num].vhost_ifname;
        lan = cur->att_lan[num].lan;
        memset(tap, 0, MAX_NAME_LEN);
        snprintf(tap, MAX_NAME_LEN-1, "s%s", eth_name);
        if (cur->del_snf_ethv_sent[num] == 0)
          {
          cur->del_snf_ethv_sent[num] = 1;
          if (strlen(lan))
            {
            if (ovs_snf_send_del_snf_lan(name, num, eth_name, lan))
              KERR("ERROR DEL KVMETH %s %d %s %s", name, num, eth_name, lan);
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
void cnt_set_color(char *name, int color)
{
  t_cnt *cur = find_cnt(name);
  if (!cur)
    KERR("ERROR %s", name);
  else
    {
    cur->cnt.color = color;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void cnt_doorways_ping_ok(char *name)
{
  t_cnt *cur = find_cnt(name);
  if (cur == NULL)
    KERR("ERROR %s", name);
  else
    {
    cur->cnt.ping_ok = 1;
    cfg_send_vm_evt_cloonix_ga_ping_ok(name);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void cnt_doorways_ping_ko(char *name)
{
  t_cnt *cur = find_cnt(name);
  if (cur == NULL)
    KERR("ERROR %s", name);
  else
    {
    cur->cnt.ping_ok = 0;
    cfg_send_vm_evt_cloonix_ga_ping_ko(name);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void cnt_doorways_up_and_running(char *name)
{
  int door_llid = get_doorways_llid(); 
  t_cnt *cur = find_cnt(name);
  if (cur == NULL)
    KERR("ERROR %s", name);
  else
    doors_send_command(door_llid, 0, name, CLOONIX_UP_VPORT_AND_RUNNING);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void cnt_init(void)
{
  g_head_cnt = NULL;
}
/*---------------------------------------------------------------------------*/

