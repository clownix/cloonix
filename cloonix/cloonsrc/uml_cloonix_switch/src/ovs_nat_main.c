/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

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
#include "suid_power.h"
#include "llid_trace.h"
#include "msg.h"
#include "cnt.h"
#include "stall_detect.h"
#include "lan_to_name.h"
#include "ovs_snf.h"
#include "kvm.h"
#include "mactopo.h"
#include "proxycrun.h"
#include "pthexec.h"

static char g_cloonix_net[MAX_NAME_LEN];
static char g_bin_nat_main[MAX_PATH_LEN];

static t_ovs_nat_main *g_head_nat;
static int g_nb_nat;

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

static void free_nat(t_ovs_nat_main *cur)
{
  proxycrun_transmit_req_nat(cur->name, 0, "none");
  cfg_free_obj_id(cur->nat_id);
  if (strlen(cur->lan_added))
    {
    int val;
    KERR("WARNING nat %s is freed while still cabled to lan %s, telling the "
         "mac topology and ovs", cur->name, cur->lan_added);
    mactopo_del_req(item_nat, cur->name, 0, cur->lan_added);
    val = lan_del_name(cur->lan_added, item_nat, cur->name, 0);
    if (val != 2*MAX_LAN)
      msg_send_del_lan_endp(ovsreq_del_nat_lan, cur->name, 0, cur->vhost,
                            cur->lan_added);
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

static void timer_free_nat(void *data)
{
  char *name = (char *) data;
  t_ovs_nat_main *cur = find_nat(name);
  if (cur)
    free_nat(cur);  /* free_nat() sends nat_off itself */
  else
    proxycrun_transmit_req_nat(name, 0, "none");
  clownix_free(data, __FUNCTION__);
} 

static void launch_timer_free_nat(char *name)
{
  char *dname;
  dname = (char *) clownix_malloc(MAX_NAME_LEN, 38);
  memset(dname, 0, MAX_NAME_LEN);
  strncpy(dname, name, MAX_NAME_LEN-1);
  clownix_timeout_add(2, timer_free_nat, (void *) dname, NULL, NULL);
}

static void stop_nat(t_ovs_nat_main *cur)
{
  utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
  cur->kil_sent = 1;
  kil_req_and_check(cur->llid, type_hop_nat_main, cur->name);
  launch_timer_free_nat(cur->name);
}

static void alloc_nat(int cli_llid, int cli_tid, char *name, char *ports)
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
  if (ports)
    snprintf(cur->ports, MAX_PATH_LEN-1, "%s", ports);
  snprintf(cur->vhost, (MAX_NAME_LEN-1), "%s", pthexec_ovs_port(id, 0));
  snprintf(cur->mac, (MAX_NAME_LEN-1),
           "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
           mc[0], mc[1], mc[2], mc[3], mc[4], mc[5]);
  if (g_head_nat)
    g_head_nat->prev = cur;
  cur->next = g_head_nat;
  g_head_nat = cur;
  proxycrun_transmit_req_nat(name, 1, cur->ports);
  g_nb_nat += 1;
  cfg_hysteresis_send_topo_info();
}

static void nat_start(char *name, char *vhost)
{
  char *argv[7];
  argv[0] = g_bin_nat_main;
  argv[1] = g_cloonix_net;
  argv[2] = cfg_get_root_work();
  argv[3] = name;
  argv[4] = vhost;
  argv[5] = NULL;
  suid_power_spawn(argv);
}

static int try_connect(char *socket, char *name)
{
  int llid = string_client_unix(socket, uml_clownix_switch_error_cb,
                                uml_clownix_switch_rx_cb, name);
  if (llid)
    {
    llid_trace_alloc(llid, name, 0, 0, type_llid_trace_endp_ovsdb);
    rpct_send_pid_req(llid, type_hop_nat_main, name, 0);
    }
  return llid;
}

static int passt_pid_of(char *name)
{
  char path[MAX_PATH_LEN];
  int pid = 0;
  FILE *f;
  snprintf(path, MAX_PATH_LEN-1, "%s/%s_passt.pid",
           utils_get_nat_proxy_dir(), name);
  f = fopen(path, "r");
  if (f)
    {
    if (fscanf(f, "%d", &pid) != 1)
      pid = 0;
    fclose(f);
    }
  return pid;
}

/* Wall clock: a loaded loop beats late. */
#define PUB_MAX_MS 15000

static void timer_heartbeat(void *data)
{
  t_ovs_nat_main *next, *cur = g_head_nat;
  int llid;
  char msg2[MAX_PATH_LEN];
  char *msg = "nat_suidroot";
  while(cur)
    {
    next = cur->next;
    /* An answer that never comes must not block the nat for ever. The
       lan goes with its last cable before the answer of the del clears
       it here: only a name still held after 5 s is stale. */
    if ((strlen(cur->lan)) && (!lan_get_with_name(cur->lan)))
      {
      cur->lan_gone_count += 1;
      if (cur->lan_gone_count >= 25)
        {
        KERR("WARNING nat %s still names lan %s, gone for 5 s: "
             "forgetting it so the nat can be cabled again",
             cur->name, cur->lan);
        memset(cur->lan, 0, MAX_NAME_LEN);
        cur->lan_gone_count = 0;
        }
      }
    else
      cur->lan_gone_count = 0;
    if ((cur->cli_llid) || (cur->del_nat_req))
      {
      cur->stuck_count += 1;
      if (cur->stuck_count > 300)
        {
        KERR("ERROR nat %s: no answer for about a minute (cli_llid=%d "
             "del_nat_req=%d), forgetting it so the nat can be deleted",
             cur->name, cur->cli_llid, cur->del_nat_req);
        cur->cli_llid = 0;
        cur->cli_tid = 0;
        cur->del_nat_req = 0;
        cur->stuck_count = 0;
        }
      }
    else
      cur->stuck_count = 0;
    cur->watchdog_count += 1;
    if ((cur->watchdog_count % 50) == 0)
      KTRC("nat %s watchdog_count=%d (reap at 500)",
           cur->name, cur->watchdog_count);
    if (cur->closed_count)
      {
      /* The helper never restarts: a closed channel is a dead helper. */
      KERR("WARNING nat %s: its channel closed, the client request is "
           "refused and the nat is freed", cur->name);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
      utils_send_status_ko(&(cur->pub_llid), &(cur->pub_tid), cur->name);
      free_nat(cur);
      }
    else if (cur->llid == 0)
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
          KERR("ERROR nat %s never answered on %s after 50 tries, its record is freed and nothing of that name will appear", cur->name, cur->socket);
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
        KERR("ERROR nat %s never became root after 50 tries, its record is freed and nothing of that name will appear", cur->name);
        utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
        free_nat(cur);
        }
      else
        {
        rpct_send_sigdiag_msg(cur->llid, type_hop_nat_main, msg);
        }
      }
    else if (cur->watchdog_count >= 500)
      {
      KERR("KILL nat %s REAPED by watchdog: watchdog_count reached %d "
           "(~10s no heartbeat answer) socket=%s",
           cur->name, cur->watchdog_count, cur->socket);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
      free_nat(cur);
      }
    else
      {
      /* A passt gone is the helper's call: it retries 15 s then dies, and
         the watchdog above reaps it. The state is asked with the pid, to
         keep last_gen current for a publish. */
      cur->count += 1;
      if (cur->count == 5)
        {
        char m3[MAX_PATH_LEN];
        rpct_send_pid_req(cur->llid, type_hop_nat_main, cur->name, 0);
        snprintf(m3, MAX_PATH_LEN-1, "nat_passt_state_req %s", cur->name);
        rpct_send_sigdiag_msg(cur->llid, type_hop_nat_main, m3);
        cur->count = 0;
        }
      if (cur->pub_llid)
        {
        if (stall_now_ms() - cur->pub_since_ms > PUB_MAX_MS)
          {
          KERR("ERROR nat %s: passt not back with the new ports after %d "
               "ms", cur->name, PUB_MAX_MS);
          if (msg_exist_channel(cur->pub_llid))
            send_status_ko(cur->pub_llid, cur->pub_tid,
                           "passt did not come back");
          cur->pub_llid = 0;
          }
        else
          {
          snprintf(msg2, MAX_PATH_LEN-1, "nat_passt_state_req %s", cur->name);
          rpct_send_sigdiag_msg(cur->llid, type_hop_nat_main, msg2);
          }
        }
      }
    cur = next;
    }
  clownix_timeout_add(20, timer_heartbeat, NULL, NULL, NULL);
}

void ovs_nat_main_pid_resp(int llid, char *name, int pid)
{
  t_ovs_nat_main *cur = find_nat(name);
  if (!cur)
    KERR("WARNING a pid %d arrived for nat %s, which has no record any more", pid, name);
  else
    {
    if (cur->watchdog_count > 100)
      KTRC("nat %s watchdog RESET from count=%d (heartbeat answered)",
           name, cur->watchdog_count);
    cur->watchdog_count = 0;
    if ((cur->pid == 0) && (cur->suid_root_done == 1))
      {
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

void ovs_nat_main_poldiag_resp(int llid, int tid, char *line)
{
    KERR("ERROR %s", line);
}

void ovs_nat_main_sigdiag_resp(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  char msg[MAX_PATH_LEN];
  t_ovs_nat_main *cur;
  char ip[MAX_NAME_LEN];
  int cli_llid, cli_tid, connected, gen, alive, pid;

  if (sscanf(line,
  "nat_suidroot_ko %63s", name) == 1)
    {
    cur = find_nat(name);
    if (!cur)
      KERR("ERROR nat: %s %s", g_cloonix_net, line);
    else
      KERR("ERROR Started nat: %s %s", g_cloonix_net, name);
    }
  else if (sscanf(line,
  "nat_passt_state_resp %63s %d %d %d", name, &connected, &gen, &alive) == 4)
    {
    cur = find_nat(name);
    if (cur)
      {
      if (cur->pub_llid == 0)
        cur->last_gen = gen;
      /* Up only on a connection newer than the request (generation grew), a
         pid written by the new passt (the old connection is the passt being
         replaced) and a frame back from it: it knows the guest. */
      pid = passt_pid_of(name);
      if ((cur->pub_llid) && (connected) && (alive) &&
          (gen != cur->pub_old_gen) && (pid != 0) && (pid != cur->pub_old_pid))
        {
        if (msg_exist_channel(cur->pub_llid))
          send_status_ok(cur->pub_llid, cur->pub_tid, "OK");
        cur->pub_llid = 0;
        cur->last_gen = gen;
        }
      }
    }
  else if (sscanf(line,
  "nat_suidroot_ok %63s", name) == 1)
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
  "nat_whatip_ok cli_llid=%d cli_tid=%d name=%63s ip=%63s",
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
  "nat_whatip_ko cli_llid=%d cli_tid=%d name=%63s",
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

int ovs_nat_main_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_ovs_nat_main *cur = g_head_nat;
  int i, result = 0;
  while(cur)
    {
    if (cur->pid)
      result++;
    cur = cur->next;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid), 38);
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
        if (!cur->kil_sent)
          cur->closed_count = 1;
        }
      cur = cur->next;
      }
    }
}

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
      KTRC("nat %s: the add of lan %s was refused", name, lan);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), name);
      if (strlen(cur->lan_added))
        {
        mactopo_del_req(item_nat, name, 0, cur->lan_added);
        lan_del_name(cur->lan_added, item_nat, name, 0);
        memset(cur->lan_added, 0, MAX_NAME_LEN);
        }
      }
    else
      {
      mactopo_add_resp(1, item_nat, name, num, lan);
      strncpy(cur->lan, lan, MAX_NAME_LEN-1);
      utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
      cfg_hysteresis_send_topo_info();
      }
    }
}

void ovs_nat_main_resp_del_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan)
{
  t_ovs_nat_main *cur = find_nat(name);
  if (!cur)
    {
    mactopo_del_resp(0, name, num, lan);
    if (is_ko)
      KERR("ERROR nat %s: ovs refused the del of lan %s after the nat was "
           "freed, port %s may be left on the bridge", name, lan, vhost);
    else
      KTRC("nat %s: the del of lan %s answered after the nat was freed",
           name, lan);
    }
  else if (cur->del_nat_req == 1)
    {
    mactopo_del_resp(item_nat, name, num, lan);
    memset(cur->lan, 0, MAX_NAME_LEN);
    stop_nat(cur);
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

t_ovs_nat_main *ovs_nat_main_exists(char *name)
{
  return (find_nat(name));
}

t_ovs_nat_main *ovs_nat_main_get_first(int *nb_nat)
{
  t_ovs_nat_main *result = g_head_nat;
  *nb_nat = g_nb_nat;
  return result;
}

void ovs_nat_main_topo_endp_push(void)
{
  t_ovs_nat_main *cur = g_head_nat;
  while(cur)
    {
    if (strlen(cur->lan))
      cfg_topo_endp_push(cur->name, 0, cur->endp_type, cur->lan);
    cur = cur->next;
    }
}

void ovs_nat_main_add(int cli_llid, int cli_tid, char *name, char *ports)
{
  t_ovs_nat_main *cur = find_nat(name);
  if (cur)
    {
    KERR("WARNING add nat %s refused: a nat of that name already exists",
         name);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Exists already");
    }
  else
    {
    alloc_nat(cli_llid, cli_tid, name, ports);
    cur = find_nat(name);
    if (!cur)
      KOUT(" ");
    nat_start(name, cur->vhost);
    }
}

/* Beats a request may be outstanding before it counts as slow. */
#define NAT_SLOW_ANSWER_BEATS 50

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
    if (cur->stuck_count < NAT_SLOW_ANSWER_BEATS)
      KTRC("nat %s is busy with a request sent %d beat(s) ago, this delete "
           "is refused", name, cur->stuck_count);
    else
      KERR("ERROR nat %s not ready for a delete: cli_llid=%d "
           "del_nat_req=%d, the previous request has been outstanding for "
           "%d beats (freed at 300)", name, cur->cli_llid,
           cur->del_nat_req, cur->stuck_count);
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
        stop_nat(cur);
        }
      else
        {
        cur->del_nat_req = 1;
        if (msg_send_del_lan_endp(ovsreq_del_nat_lan, name, 0,
                                  cur->vhost, cur->lan))
          {
          KERR("ERROR nat %s: the del of lan %s could not be sent, deleting "
               "the nat anyway", name, cur->lan);
          memset(cur->lan, 0, MAX_NAME_LEN);
          stop_nat(cur);
          }
        }
      }
    else
      {
      stop_nat(cur);
      }
    }
}

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
      strncpy(cur->lan_added, lan, MAX_NAME_LEN-1);
      cur->cli_llid = cli_llid;
      cur->cli_tid = cli_tid;
      }
    }
}

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
        {
        KERR("ERROR nat %s: the del of lan %s could not be sent to ovs",
             name, lan);
        utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), name);
        }
      }
    }
}

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
            }
          }
        cnt = cnt->next;
        }

      memset(msg, 0, MAX_PATH_LEN);
      snprintf(msg, MAX_PATH_LEN-1, "nat_machine_end");
      rpct_send_sigdiag_msg(cur->llid, type_hop_nat_main, msg);
      }
    cur = cur->next;
    }

}

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
        result = 0;
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
        result = 0;
      else
        {
        cur->endp_type = endp_type_natv;
        memset(tap, 0, MAX_NAME_LEN);
        vh = cur->vhost;
        utils_get_snf_tap_name(tap, vh);
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
    }
  return result;
}

void ovs_nat_main_publish_ko(char *name)
{
  char err[MAX_PATH_LEN];
  t_ovs_nat_main *cur = find_nat(name);
  if (cur == NULL)
    KERR("ERROR %s not found", name);
  else
    {
    memset(err, 0, MAX_PATH_LEN);
    snprintf(err, MAX_PATH_LEN-1, "nat %s: passt could not hold the ports "
             "%s (a host port in use?), the previous publish %s is kept",
             name, cur->ports, cur->prev_ports);
    KERR("ERROR %s", err);
    memset(cur->ports, 0, MAX_PATH_LEN);
    snprintf(cur->ports, MAX_PATH_LEN-1, "%s", cur->prev_ports);
    utils_send_status_ko(&(cur->pub_llid), &(cur->pub_tid), err);
    }
}

int ovs_nat_main_publish(int llid, int tid, char *name, char *ports)
{
  t_ovs_nat_main *cur = find_nat(name);
  int result = -1;
  if (cur == NULL)
    KERR("ERROR %s not found", name);
  else if (cur->pub_llid)
    KERR("ERROR %s: a publish is already in progress", name);
  else
    {
    memset(cur->prev_ports, 0, MAX_PATH_LEN);
    snprintf(cur->prev_ports, MAX_PATH_LEN-1, "%s", cur->ports);
    memset(cur->ports, 0, MAX_PATH_LEN);
    snprintf(cur->ports, MAX_PATH_LEN-1, "%s", ports);
    cur->pub_llid = llid;
    cur->pub_tid = tid;
    cur->pub_old_pid = passt_pid_of(name);
    cur->pub_old_gen = cur->last_gen;
    cur->pub_since_ms = stall_now_ms();
    proxycrun_transmit_req_nat_publish(name, cur->ports);
    result = 0;
    }
  return result;
}

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

