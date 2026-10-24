/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "hop_event.h"
#include "cfg_store.h"
#include "utils_cmd_line_maker.h"
#include "cnt.h"
#include "event_subscriber.h"
#include "system_callers.h"
#include "suid_power.h"
#include "ovs_snf.h"
#include "msg.h"
#include "lan_to_name.h"
#include "doors_rpc.h"
#include "doorways_mngt.h"
#include "crun.h"
#include "ovs_phy.h"
#include "mactopo.h"
#include "pthexec.h"
#include "uml_clownix_switch.h"

typedef struct t_cnt_delete
{
  int llid;
  int cli_llid;
  int cli_tid;
  char brandtype[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int count;
  int busy_count;
} t_cnt_delete;

static t_cnt *g_head_cnt;

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

t_cnt *find_cnt(char *name)
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

static char *get_eth_name(int vm_id, int num)
{
  static char eth_name[MAX_NAME_LEN];
  memset(eth_name, 0, MAX_NAME_LEN);
  snprintf(eth_name, MAX_NAME_LEN-1, "%s", pthexec_ovs_port(vm_id, num));
  return eth_name;
}

static int alloc_cnt(int cli_llid, int cli_tid, t_topo_cnt *cnt,
                           int vm_id, char *cnt_dir, char *err)
{
  int i, result = -1; 
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
    memset(cur->cnt.vwif_air, 0, sizeof(cur->cnt.vwif_air));
    for (i = 0; i < cnt->nb_tot_nb_vwif; i++)
      strncpy(cur->cnt.vwif_air[i], CNTWIFI_LAN, MAX_NAME_LEN-1);
    cur->cnt.vm_id = vm_id;
    cur->cli_llid = cli_llid;
    cur->cli_tid = cli_tid;
    if (g_head_cnt)
      g_head_cnt->prev = cur;
    cur->next = g_head_cnt;
    g_head_cnt = cur;
    cfg_hysteresis_send_topo_info();
    result = 0;
    }
  return result;
}

static int free_cnt(char *name)
{
  int i, result = -1;
  t_cnt *cur = find_cnt(name);
  if (cur != NULL)
    {
    for (i=0; i<MAX_ETH_VM; i++)
      {
      if (strlen(cur->att_lan[i].lan_added))
        {
        KERR("ERROR %s %d %s", name, i, cur->att_lan[i].lan_added);
        mactopo_del_req(item_cnt, name, i, cur->att_lan[i].lan_added);
        lan_del_name(cur->att_lan[i].lan_added, item_cnt, name, i);
        }
      }
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

int cnt_free_cnt(char *name)
{
  t_cnt *cur = find_cnt(name);
  int result;
  if (cur != NULL)
    cfg_free_obj_id(cur->cnt.vm_id);
  result = free_cnt(name);
  return result;
}

int send_sig_suid_power(int llid, char *msg)
{
  int result = -1;
  if ((llid) && (msg_exist_channel(llid)))
    {
    rpct_send_sigdiag_msg(llid, type_hop_suid_power, msg);
    result = 0;
    }
  return result;
}

void cnt_vhost_and_doors_begin(t_cnt *cnt)
{
  char *eth_name, *name;
  int i;
  char sock[MAX_PATH_LEN];
  memset(sock, 0, MAX_PATH_LEN);
  name = cnt->cnt.name;
  for (i=0; i<cnt->cnt.nb_tot_eth; i++)
    {
    eth_name = cnt->cnt.eth_table[i].vhost_ifname;
    if (msg_send_vhost_up(name, i, eth_name))
      KERR("ERROR KVMETH %s %d %s", name, i, eth_name);
    }
  doors_send_del_vm(get_doorways_llid(), 0, name);
  snprintf(sock, MAX_PATH_LEN-1, "%s/mountbear/sock", cnt->mountbear);
  doors_send_add_vm(get_doorways_llid(), 0, name, sock);
}

void cnt_resp_add_lan(int is_ko, char *name, int num, char *vhost, char *lan)
{
  t_cnt *cur = find_cnt(name);
  char *eth_name;
  if (cur == NULL)
    {
    mactopo_add_resp(0, item_cnt, name, num, lan);
    KTRC("add of lan %s for %s %d (ko=%d): the container is gone",
         lan, name, num, is_ko);
    }
  else
    {
    if (is_ko)
      {
      mactopo_add_resp(0, item_cnt, name, num, lan);
      if (cur->poweroff_done)
        KTRC("%s %d: the add of lan %s ends with its delete", name, num, lan);
      else
        KERR("ERROR %s %d: the add of lan %s was refused", name, num, lan);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), "KO");
      if (strlen(cur->att_lan[num].lan_added))
        {
        mactopo_del_req(item_cnt, name, num, cur->att_lan[num].lan_added);
        lan_del_name(cur->att_lan[num].lan_added, item_cnt, name, num);
        memset(cur->att_lan[num].lan_added, 0, MAX_NAME_LEN);
        }
      memset(cur->att_lan[num].lan, 0, MAX_NAME_LEN);
      }
    else
      {
      mactopo_add_resp(1, item_cnt, name, num, lan);
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

void cnt_resp_del_lan(int is_ko, char *name, int num, char *vhost, char *lan)
{
  t_cnt *cur = find_cnt(name);
  if (!cur)
    {
    mactopo_del_resp(0, name, num, lan);
    KERR("ERROR RESP DEL LAN %d %s %s %d", is_ko, lan, name, num);
    }
  else
    {
    mactopo_del_resp(item_cnt, name, num, lan);
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
        {
        if (ovs_snf_send_del_snf_lan(name, num, vhost, lan))
          KERR("ERROR %s %d %s %s", name, num, vhost, lan);
        }
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

t_cnt *cnt_get_first_cnt(int *nb)
{
  *nb = get_nb_cnt();
  return g_head_cnt;
}

void cnt_topo_endp_push(void)
{
  t_cnt *cur = g_head_cnt;
  int i;
  while(cur)
    {
    for (i=0; i<MAX_ETH_VM; i++)
      {
      if (cur->att_lan[i].lan_attached_ok)
        cfg_topo_endp_push(cur->cnt.name, i, cur->cnt.eth_table[i].endp_type,
                           cur->att_lan[i].lan);
      }
    cur = cur->next;
    }
}

/* no cnt of that name, or one being deleted */
int cnt_is_going(char *name)
{
  t_cnt *cur = find_cnt(name);
  return ((!cur) || (cur->poweroff_done));
}

static char *cnt_pending_what(t_cnt *cur)
{
  if (cur->lan_add_waiting_ack)
    return "its plug";
  if (cur->lan_del_waiting_ack)
    return "its unplug";
  if (!cur->cnt_pid)
    return "its create";
  return "a request on it";
}

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

int cnt_info(char *name, int *nb_eth, t_eth_table **eth_table)
{
  int result = 0;
  t_cnt *cur = find_cnt(name);
  if (cur != NULL)
    {
    if ((cur->cli_llid == 0) &&
        (cur->lan_add_waiting_ack == 0) &&
        (cur->lan_del_waiting_ack == 0))
      {
      *nb_eth = cur->cnt.nb_tot_eth;
      *eth_table = cur->cnt.eth_table;
      result = 1;
      }
    }
  return result;
}

/* the vhost_up ack came: a snf may start on the eth */
int cnt_vhost_up(char *name, int num, int set)
{
  t_cnt *cur = find_cnt(name);
  if ((!cur) || (num < 0) || (num >= cur->cnt.nb_tot_eth))
    return 0;
  if (set)
    cur->vhost_up[num] = 1;
  return cur->vhost_up[num];
}

/* Same answer whatever the state: the vhost_up ack must find its snf. */
int cnt_eth_table(char *name, int *nb_eth, t_eth_table **eth_table)
{
  int result = 0;
  t_cnt *cur = find_cnt(name);
  if (cur != NULL)
    {
    *nb_eth = cur->cnt.nb_tot_eth;
    *eth_table = cur->cnt.eth_table;
    result = 1;
    }
  return result;
}

static int cnt_root_net_refused(char *err)
{
  if (get_running_in_crun())
    return 0;
  if (getuid() != 0)
    return 0;
  snprintf(err, MAX_PATH_LEN-1,
           "this net runs as root and a container needs a user: start it "
           "as a user, cloonix start <net> without sudo");
  return -1;
}

int cnt_create(int llid, int cli_llid, int cli_tid, int vm_id,
               t_topo_cnt *cnt, char *err)
{
  int i, abort = 0, result = -1;
  char *cnt_dir = utils_get_cnt_dir();
  char *eth_name;

  for (i=0; i<cnt->nb_tot_eth; i++)
    {
    if ((cnt->eth_table[i].endp_type != endp_type_ethv) &&
        (cnt->eth_table[i].endp_type != endp_type_eths))
      {
      snprintf(err, MAX_PATH_LEN-1,
               "%s eth%d must be vhost or spy", cnt->name, i);
      KERR("ERROR %s", err);
      abort = 1;
      }
    eth_name = get_eth_name(vm_id, i);
    memset(cnt->eth_table[i].vhost_ifname, 0, MAX_NAME_LEN);
    strncpy(cnt->eth_table[i].vhost_ifname, eth_name, MAX_NAME_LEN-1); 
    }
  if ((abort == 0) && cnt_root_net_refused(err))
    {
    KERR("ERROR %s", err);
    abort = 1;
    }
  /* On failure everything taken here is released; vm_id is the caller's. */
  if ((abort == 0) &&
      (alloc_cnt(cli_llid, cli_tid, cnt, vm_id, cnt_dir, err) == 0))
    {
    if (strcmp(cnt->brandtype, "brandcnt"))
      KOUT("ERROR TYPE %s", cnt->brandtype);
    if (crun_create(llid, vm_id, cnt))
      {
      snprintf(err, MAX_PATH_LEN-1, "%s crun create", cnt->name);
      KERR("ERROR %s: the create could not be sent to suid_power",
           cnt->name);
      free_cnt(cnt->name);
      }
    else
      result = 0;
    }
  return result;
}

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
    KTRC("%s: gone before its delete ran", cd->name);
    if (cd->cli_llid)
      send_status_ok(cd->cli_llid, cd->cli_tid, "");
    clownix_free(cd, __FUNCTION__);
    }
  else if (msg_ovsreq_get_qty() > 50)
    {
    /* Unbounded on purpose, a delete through a congested ovs waits. A mass
       delete keeps the queue full for seconds: said after 30 s only. */
    cd->busy_count += 1;
    if ((cd->busy_count % 300) == 0)
      KERR("WARNING %s: still waiting to be deleted, the ovs queue has %d "
           "requests in it and has been busy for %d tries",
           cd->name, msg_ovsreq_get_qty(), cd->busy_count);
    clownix_timeout_add(10, timer_cnt_delete, (void *) cd, NULL, NULL);
    }
  else
    {
    for (i=0; i<cur->cnt.nb_tot_eth; i++)
      {
      eth_name = cur->cnt.eth_table[i].vhost_ifname;
      lan = cur->att_lan[i].lan;
      memset(tap, 0, MAX_NAME_LEN); 
      utils_get_snf_tap_name(tap, eth_name);
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
      else if ((ovs_snf_is_running(cd->name, i)) && (cur->del_snf_ethv_sent[i] == 0))
        {
        KERR("WARNING %s %d: being deleted with a capture running and no "
             "stop ever sent for it (endp_type=%d, eths is %d) -- "
             "stopping it now",
             cd->name, i, cur->cnt.eth_table[i].endp_type, endp_type_eths);
        memset(tap, 0, MAX_NAME_LEN);
        utils_get_snf_tap_name(tap, cur->cnt.eth_table[i].vhost_ifname);
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
          mactopo_del_req(item_cnt, cd->name, i, cur->att_lan[i].lan_added);
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
        if (!strcmp(cur->cnt.brandtype, "brandcnt"))
          snprintf(req,MAX_PATH_LEN-1,"cloonsuid_crun_delete name=%s", cd->name);
        else
          KOUT("ERROR TYPE %s (only brandcnt)", cur->cnt.brandtype);
        doors_send_del_vm(get_doorways_llid(), 0, cd->name);
        if (cur->process_gone)
          {
          /* suid_power tore it down on its own, no delete to send. */
          if (cd->cli_llid)
            send_status_ok(cd->cli_llid, cd->cli_tid, "");
          cnt_free_cnt(cur->cnt.name);
          }
        else if (send_sig_suid_power(cd->llid, req))
          {
          snprintf(err, MAX_PATH_LEN-1, "ERROR %s delete", cd->name);
          KERR("ERROR %s FREE ", cd->name);
          if (cd->cli_llid)
            send_status_ko(cd->cli_llid, cd->cli_tid, err);
          cnt_free_cnt(cur->cnt.name);
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
      KERR("WARNING %s: deleted while %s was waiting for its answer, that "
           "request is refused", name, cnt_pending_what(cur));
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), "STOPPED CNT");
      }
    cd = (t_cnt_delete *) clownix_malloc(sizeof(t_cnt_delete), 3);
    memset(cd, 0, sizeof(t_cnt_delete));
    cd->llid = llid;
    cd->cli_llid = cli_llid;
    cd->cli_tid = cli_tid;
    strncpy(cd->brandtype, cur->cnt.brandtype, MAX_NAME_LEN-1);
    strncpy(cd->name, name, MAX_NAME_LEN-1);
    timer_cnt_delete((void *) cd);
    result = 0;
    }
  else
    {
    KERR("WARNING %s: delete asked twice, the first is still running", name);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "DELETE ALREADY IN PROGRESS");
    }
  return result;
}

/* cmd: "cpu=<percent of one core>" and/or "ram=<mega bytes>", 0 for no
   cap; a cap not named keeps its value. */
static int limit_parse(t_cnt *cur, char *cmd, int *cpu, int *ram)
{
  char buf[MAX_PATH_LEN], *tok, *save = NULL;
  int nb = 0;
  *cpu = cur->lim_cpu;
  *ram = cur->lim_ram;
  memset(buf, 0, MAX_PATH_LEN);
  strncpy(buf, cmd, MAX_PATH_LEN-1);
  for (tok = strtok_r(buf, " ", &save); tok; tok = strtok_r(NULL, " ", &save))
    {
    if ((sscanf(tok, "cpu=%d", cpu) != 1) && (sscanf(tok, "ram=%d", ram) != 1))
      return -1;
    nb += 1;
    }
  return ((nb == 0) || (*cpu < 0) || (*cpu > 100000) || (*ram < 0));
}

int cnt_limit(int llid, int cli_llid, int cli_tid, char *name, char *cmd,
              char *err)
{
  t_cnt *cur = find_cnt(name);
  char req[MAX_PATH_LEN];
  int cpu, ram, result = -1;
  memset(err, 0, MAX_PATH_LEN);
  if (cur == NULL)
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s NOT FOUND", name);
  else if (cur->cnt.is_privileged)
    snprintf(err, MAX_PATH_LEN-1,
             "ERROR %s is privileged, it has no cgroup and cannot be capped",
             name);
  else if (limit_parse(cur, cmd, &cpu, &ram))
    snprintf(err, MAX_PATH_LEN-1,
             "ERROR %s: want cpu=<percent> and/or ram=<megabytes>, got: %s",
             name, cmd);
  else if ((cur->cli_llid != 0) || (cur->cli_tid != 0))
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s: another request is running",
             name);
  else if (cur->poweroff_done)
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s is being deleted", name);
  else
    {
    memset(req, 0, MAX_PATH_LEN);
    snprintf(req, MAX_PATH_LEN-1,
             "cloonsuid_crun_update name=%s cpu=%d ram=%d", name, cpu, ram);
    if (send_sig_suid_power(llid, req))
      snprintf(err, MAX_PATH_LEN-1, "ERROR %s: suid_power not reachable",
               name);
    else
      {
      cur->cli_llid = cli_llid;
      cur->cli_tid = cli_tid;
      cur->lim_cpu_asked = cpu;
      cur->lim_ram_asked = ram;
      result = 0;
      }
    }
  return result;
}

int cnt_air(int llid, int cli_llid, int cli_tid, char *name, int idx,
            char *air, char *err)
{
  t_cnt *cur = find_cnt(name);
  char req[MAX_PATH_LEN];
  int result = -1;
  memset(err, 0, MAX_PATH_LEN);
  if (cur == NULL)
    snprintf(err, MAX_PATH_LEN-1, "%s NOT FOUND", name);
  else if ((idx < 0) || (idx >= cur->cnt.nb_tot_nb_vwif))
    snprintf(err, MAX_PATH_LEN-1, "%s has no wlan%d", name, idx);
  else if ((cur->cli_llid != 0) || (cur->cli_tid != 0))
    snprintf(err, MAX_PATH_LEN-1, "%s: another request is running", name);
  else if ((cur->poweroff_done) || (!cur->cnt_pid))
    snprintf(err, MAX_PATH_LEN-1, "%s is not running", name);
  else
    {
    memset(req, 0, MAX_PATH_LEN);
    snprintf(req, MAX_PATH_LEN-1, "cloonsuid_air_cnt name=%s idx=%d air=%s",
             name, idx, air[0] ? air : "-");
    if (send_sig_suid_power(llid, req))
      snprintf(err, MAX_PATH_LEN-1, "%s: suid_power not reachable", name);
    else
      {
      cur->cli_llid = cli_llid;
      cur->cli_tid = cli_tid;
      result = 0;
      }
    }
  return result;
}

int cnt_delete_all(int llid)
{
  t_cnt *next, *cur = g_head_cnt;
  int result = 0;
  while (cur)
    {
    next = cur->next;
    if (cur->count_delete > 50)
      {
      KERR("ERROR %s", cur->cnt.name);
      cnt_free_cnt(cur->cnt.name);
      }
    else if (cur->poweroff_done == 0)
      cnt_delete(llid, 0, 0, cur->cnt.name);
    cur->count_delete += 1;
    result += 1;
    cur = next;
    }
  return result;
}

static void error_timer_beat_action(int llid, t_cnt *cur)
{
  char req[MAX_PATH_LEN];
  memset(req, 0, MAX_PATH_LEN);
  if (!strcmp(cur->cnt.brandtype, "brandcnt"))
    snprintf(req, MAX_PATH_LEN-1, "cloonsuid_crun_ERROR name=%s", cur->cnt.name);
  else
    KOUT("ERROR TYPE %s (only brandcnt)", cur->cnt.brandtype);
  if (send_sig_suid_power(llid, req))
    KERR("ERROR %s: the error could not be sent to suid_power, llid %d",
         cur->cnt.name, llid);
  utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid),
                       "no answer of suid_power in time");
  cnt_free_cnt(cur->cnt.name);
}

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
        KERR("ERROR %s: a lan add without answer of suid_power in 30 s",
             cur->cnt.name);
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
        KERR("ERROR %s: a lan del without answer of suid_power in 30 s",
             cur->cnt.name);
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
        KERR("ERROR %s: a request without answer of suid_power in 100 s",
             cur->cnt.name);
        error_timer_beat_action(llid, cur);
        }
      }
    cur = next;
    }
}

int  cnt_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_cnt *cur = g_head_cnt;
  int i, result = 0;
  while(cur)
    {
    if (cur->cnt_pid)
      result++;
    cur = cur->next;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid), 33);
    memset(glob_lst, 0, result*sizeof(t_lst_pid));
    cur = g_head_cnt;
    i = 0;
    while(cur)
      {
      if (cur->cnt_pid)
        {
        strncpy(glob_lst[i].name, cur->cnt.name, MAX_NAME_LEN-1);
        glob_lst[i].pid = cur->cnt_pid;
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

/* The cnt owning eth num, fit for a lan; NULL with err and one KERR if not. */
static t_cnt *cnt_lan_eth(char *name, int num, char *err)
{
  t_cnt *cur = find_cnt(name);
  memset(err, 0, MAX_PATH_LEN);
  if (cur == NULL)
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s NOT FOUND", name);
  else if ((cur->cnt.nb_tot_eth <= num) ||
           ((cur->cnt.eth_table[num].endp_type != endp_type_ethv) &&
            (cur->cnt.eth_table[num].endp_type != endp_type_eths)))
    {
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s has no eth%d (it has %d)",
             name, num, cur->cnt.nb_tot_eth);
    cur = NULL;
    }
  if (cur == NULL)
    KERR("%s", err);
  return cur;
}

int cnt_add_lan(int llid, int tid, char *name, int num, char *lan, char *err)
{
  int result = -1;
  t_cnt *cur = cnt_lan_eth(name, num, err);
  char *eth_name;
  if (cur == NULL)
    result = -1;
  else if ((cur->cli_llid) ||
           (cur->lan_add_waiting_ack) ||
           (cur->lan_del_waiting_ack) ||
           (cur->att_lan[num].lan_attached_ok)) 
    {
    if (cur->att_lan[num].lan_attached_ok)
      snprintf(err, MAX_PATH_LEN-1, "ERROR %s eth%d is already cabled to "
               "lan %s", name, num, cur->att_lan[num].lan);
    else if (cur->lan_del_waiting_ack)
      snprintf(err, MAX_PATH_LEN-1, "WARNING %s is being uncabled from a "
               "lan, ask again when that is done", name);
    else
      snprintf(err, MAX_PATH_LEN-1, "WARNING %s is being cabled to a lan, "
               "ask again when that is done", name);
    KERR("%s", err);
    }
  else
    {
    memset(cur->att_lan[num].lan, 0, MAX_NAME_LEN);
    memset(cur->att_lan[num].lan_added, 0, MAX_NAME_LEN);
    eth_name = get_eth_name(cur->cnt.vm_id, num);
    lan_add_name(lan, item_cnt, name, num);
    if (mactopo_add_req(item_cnt, name, num, lan,
                        cur->cnt.eth_table[num].vhost_ifname,
                        cur->cnt.eth_table[num].mac_addr, err))
      {     
      KERR("ERROR %s %d %s %s", name, num, lan, err);
      lan_del_name(lan, item_cnt, name, num);
      }       

    else if (msg_send_add_lan_endp(ovsreq_add_cnt_lan, name, num,
                                   eth_name, lan))
      {
      snprintf(err, MAX_PATH_LEN-1, "ERROR ADD LAN %s %d %s", name, num, lan); 
      KERR("%s", err);
      lan_del_name(lan, item_cnt, name, num);
      }
    else
      {
      strncpy(cur->att_lan[num].lan, lan, MAX_NAME_LEN-1);
      strncpy(cur->att_lan[num].lan_added, lan, MAX_NAME_LEN-1);
      result = 0;
      cur->cli_llid = llid;
      cur->cli_tid = tid;
      cur->lan_add_waiting_ack = 1;
      }
    }
  return result;
}

int cnt_del_lan(int llid, int tid, char *name, int num, char *lan, char *err)
{
  int val = 0, result = -1;
  t_cnt *cur = cnt_lan_eth(name, num, err);
  char *eth_name;
  if (cur == NULL)
    result = -1;
  else if ((cur->cli_llid) ||
           (cur->lan_add_waiting_ack) ||
           (cur->lan_del_waiting_ack) ||
           (cur->att_lan[num].lan_attached_ok == 0))
    {
    snprintf(err, MAX_PATH_LEN-1, "ERROR %s BAD STATE FOR PROCESSING", name);
    if ((!cur->cli_llid) && (!cur->lan_add_waiting_ack) &&
        (!cur->lan_del_waiting_ack))
      KTRC("%s eth %d has no lan attached, nothing to detach", name, num);
    else
      KERR("ERROR %s eth %d cannot be processed now: a client is waiting "
           "(llid %d), an add is in flight (%d), a del is in flight (%d)",
           name, num, cur->cli_llid, cur->lan_add_waiting_ack,
           cur->lan_del_waiting_ack);
    }
  else if (strcmp(cur->att_lan[num].lan, lan))
    {
    snprintf(err, MAX_PATH_LEN-1, "unplug %s eth%d %s refused: eth%d is on %s",
             name, num, lan, num, cur->att_lan[num].lan);
    KERR("ERROR %s", err);
    }
  else
    {
    if (!strlen(cur->att_lan[num].lan_added))
      KERR("ERROR: %s %d %s", name, num, lan);
    else
      {
      mactopo_del_req(item_cnt, name, num, cur->att_lan[num].lan_added);
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
      cur->cli_llid = llid;
      cur->cli_tid = tid;
      cur->lan_del_waiting_ack = 0;
      cur->att_lan[num].lan_attached_ok = 0;
      memset(cur->att_lan[num].lan, 0, MAX_NAME_LEN);
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      result = 0;
      }
    }
  return result;
}

static void snf_process_started(char *name, int num, char *vhost)
{
  t_cnt *cur = find_cnt(name);
  char *lan, *eth_name;
  if (cur == NULL)
    KTRC("the snf of %s eth %d started after the container was deleted",
         name, num);
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
        {
        if (ovs_snf_is_running(name, num))
          result = 0;
        else
          {
          eth_name = cur->cnt.eth_table[num].vhost_ifname;
          cur->del_snf_ethv_sent[num] = 0;
          KERR("WARNING %s %d is marked sniffable and has no capture "
               "running, starting one", name, num);
          ovs_dyn_snf_start_process(name, num, item_type_ceth,
                                    eth_name, snf_process_started);
          result = 0;
          }
        }
      else
        {
        eth_name = cur->cnt.eth_table[num].vhost_ifname;
        cur->cnt.eth_table[num].endp_type = endp_type_eths;
        cur->del_snf_ethv_sent[num] = 0;
        ovs_dyn_snf_start_process(name, num, item_type_ceth,
                                  eth_name, snf_process_started);
        result = 0;
        }
      }
    else
      {
      if (cur->cnt.eth_table[num].endp_type == endp_type_ethv)
        {
        /* Already not sniffed: same reasoning the other way round. */
        result = 0;
        }
      else
        {
        cur->cnt.eth_table[num].endp_type = endp_type_ethv;
        eth_name = cur->cnt.eth_table[num].vhost_ifname;
        lan = cur->att_lan[num].lan;
        memset(tap, 0, MAX_NAME_LEN);
        utils_get_snf_tap_name(tap, eth_name);
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

void cnt_doorways_up_and_running(char *name)
{
  int door_llid = get_doorways_llid(); 
  t_cnt *cur = find_cnt(name);
  if (cur == NULL)
    KERR("ERROR %s", name);
  else
    doors_send_command(door_llid, 0, name, CLOONIX_UP_VPORT_AND_RUNNING);
}

void cnt_init(void)
{
  g_head_cnt = NULL;
}

