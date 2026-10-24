/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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
#include "suid_power.h"
#include "llid_trace.h"
#include "msg.h"
#include "cnt.h"
#include "lan_to_name.h"
#include "ovs_snf.h"
#include "kvm.h"
#include "mactopo.h"
#include "pthexec.h"
#include "ack_pending.h"

static char g_cloonix_net[MAX_NAME_LEN];
static char g_root_path[MAX_PATH_LEN];
static char g_bin_a2b[MAX_PATH_LEN];

static t_ovs_a2b *g_head_a2b;

#define A2B_TRACE 0

#if A2B_TRACE
#define ALOG(fmt, ...) KERR("A2BTRACE " fmt, ##__VA_ARGS__)
#else
#define ALOG(fmt, ...) do {} while(0)
#endif

static int g_nb_a2b;

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

static void free_a2b(t_ovs_a2b *cur)
{
  cfg_free_obj_id(cur->a2b_id);
  int num, val;
  for (num=0; num<2; num++)
    {
    if (!strlen(cur->side[num].lan_added))
      continue;
    KERR("WARNING a2b %s is freed while side %d is still cabled to lan %s, "
         "telling the mac topology and ovs", cur->name, num,
         cur->side[num].lan_added);
    mactopo_del_req(item_a2b, cur->name, num, cur->side[num].lan_added);
    val = lan_del_name(cur->side[num].lan_added, item_a2b, cur->name, num);
    if (val != 2*MAX_LAN)
      msg_send_del_lan_endp(ovsreq_del_a2b_lan, cur->name, num,
                            cur->side[num].vhost, cur->side[num].lan_added);
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

static void timer_free_a2b(void *data)
{
  ALOG("free timer fired for %s", (char *) data);
  char *name = (char *) data;
  t_ovs_a2b *cur = find_a2b(name);
  if (cur)
    free_a2b(cur);
  clownix_free(data, __FUNCTION__);
} 

static void launch_timer_free_a2b(char *name)
{
  char *dname;
  dname = (char *) clownix_malloc(MAX_NAME_LEN, 37);
  memset(dname, 0, MAX_NAME_LEN);
  strncpy(dname, name, MAX_NAME_LEN-1);
  clownix_timeout_add(2, timer_free_a2b, (void *) dname, NULL, NULL);
}

static void stop_a2b(t_ovs_a2b *cur)
{
  utils_send_status_ok(&(cur->cli_llid), &(cur->cli_tid));
  cur->kil_sent = 1;
  kil_req_and_check(cur->llid, type_hop_a2b, cur->name);
  launch_timer_free_a2b(cur->name);
}

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
  if (snprintf(cur->socket, MAX_PATH_LEN, "%s/%s/c%s", root, A2B_DIR, name)
      >= MAX_PATH_LEN)
    KOUT("ERROR the a2b socket path does not fit: %s/%s/c%s",
         root, A2B_DIR, name);
  snprintf(cur->side[0].vhost, (MAX_NAME_LEN-1), "%s", pthexec_ovs_port(id, 0));
  snprintf(cur->side[1].vhost, (MAX_NAME_LEN-1), "%s", pthexec_ovs_port(id, 1));
  if (g_head_a2b)
    g_head_a2b->prev = cur;
  cur->next = g_head_a2b;
  g_head_a2b = cur;
  g_nb_a2b += 1;
  cfg_hysteresis_send_topo_info();
}

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
  suid_power_spawn(argv);
}

static int try_connect(char *socket, char *name)
{
  int llid = string_client_unix(socket, uml_clownix_switch_error_cb,
                                uml_clownix_switch_rx_cb, name);
  if (llid)
    {
    llid_trace_alloc(llid, name, 0, 0, type_llid_trace_endp_ovsdb);
    rpct_send_pid_req(llid, type_hop_a2b, name, 0);
    }
  return llid;
}

static void timer_heartbeat(void *data)
{
  t_ovs_a2b *next, *cur = g_head_a2b;
  int llid;
  char *msg = "a2b_suidroot";
  while(cur)
    {
    next = cur->next;
    cur->watchdog_count += 1;
    if (cur->closed_count)
      {
      /* The helper never restarts: a closed channel is a dead helper. */
      KERR("WARNING a2b %s: its channel closed, the a2b is freed%s",
           cur->name, (cur->cli_llid) ?
           ", the client request it held is refused" : "");
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
      free_a2b(cur);
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
          KERR("ERROR a2b %s never answered on %s after 50 tries, its record is freed", cur->name, cur->socket);
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
        KERR("ERROR a2b %s never became root after 50 tries, its record is freed", cur->name);
        utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), cur->name);
        free_a2b(cur);
        }
      else
        {
        rpct_send_sigdiag_msg(cur->llid, type_hop_a2b, msg);
        }
      }
    else if (cur->watchdog_count >= 150)
      {
        KERR("ERROR a2b %s stopped answering on %s, its record is freed", cur->name, cur->socket);
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
      }
    cur = next;
    }
  clownix_timeout_add(20, timer_heartbeat, NULL, NULL, NULL);
}

static char *g_cnf_word[A2B_NB_CNF] =
  { "delay", "loss", "rate", "silentms", "qsize", "bsize" };

char *ovs_a2b_cnf_word(int i)
{
  return g_cnf_word[i];
}

/* a setting the a2b took, kept for dump commands */
static void cnf_apply(t_ovs_a2b *cur, char *cmd)
{
  char word[MAX_NAME_LEN], *p;
  int i, dir, val;
  memset(word, 0, MAX_NAME_LEN);
  if (sscanf(cmd, "dir=%d", &dir) != 1)
    KERR("ERROR a2b %s: %s kept for nothing, no dir", cur->name, cmd);
  else if ((dir != 0) && (dir != 1))
    KERR("ERROR a2b %s: %s kept for nothing, dir %d", cur->name, cmd, dir);
  else if (sscanf(cmd, "dir=%d cmd=%63s val=%d", &dir, word, &val) == 3)
    {
    for (i = 0; (i < A2B_NB_CNF) && strcmp(word, g_cnf_word[i]); i++);
    if (i == A2B_NB_CNF)
      KERR("WARNING a2b %s: %s is no setting dump commands knows",
           cur->name, word);
    else
      {
      cur->cnf_set[dir][i] = 1;
      cur->cnf_val[dir][i] = val;
      /* rate sets qsize and bsize by itself */
      if (!strcmp(word, "rate"))
        cur->cnf_set[dir][4] = cur->cnf_set[dir][5] = 0;
      }
    }
  else if (strstr(cmd, "unreplace_ipsrc"))
    memset(cur->cnf_repsrc[dir], 0, MAX_NAME_LEN);
  else if (strstr(cmd, "unreplace_ipdst"))
    memset(cur->cnf_repdst[dir], 0, MAX_NAME_LEN);
  else if ((p = strstr(cmd, "replace_ipsrc ")) != NULL)
    {
    memset(cur->cnf_repsrc[dir], 0, MAX_NAME_LEN);
    strncpy(cur->cnf_repsrc[dir], p + strlen("replace_ipsrc "),
            MAX_NAME_LEN-1);
    }
  else if ((p = strstr(cmd, "replace_ipdst ")) != NULL)
    {
    memset(cur->cnf_repdst[dir], 0, MAX_NAME_LEN);
    strncpy(cur->cnf_repdst[dir], p + strlen("replace_ipdst "),
            MAX_NAME_LEN-1);
    }
}

/* the answer of the a2b to a setting: kept once it took it */
static void cnf_answer(t_ovs_a2b *cur, int llid, int tid, int ok)
{
  int i;
  for (i = 0; i < A2B_NB_ASKED; i++)
    if ((cur->asked[i].cmd[0]) && (cur->asked[i].llid == llid) &&
        (cur->asked[i].tid == tid))
      {
      if (ok)
        cnf_apply(cur, cur->asked[i].cmd);
      memset(&(cur->asked[i]), 0, sizeof(t_a2b_asked));
      return;
      }
  KERR("WARNING a2b %s: an answer %d %d to a setting not recorded, dump "
       "commands does not know it", cur->name, llid, tid);
}

static void cnf_asked(t_ovs_a2b *cur, int llid, int tid, char *cmd)
{
  int i;
  for (i = 0; (i < A2B_NB_ASKED) && (cur->asked[i].cmd[0]); i++);
  if (i == A2B_NB_ASKED)
    KERR("WARNING a2b %s: %d settings waiting already, %s not recorded for "
         "dump commands", cur->name, A2B_NB_ASKED, cmd);
  else
    {
    cur->asked[i].llid = llid;
    cur->asked[i].tid = tid;
    strncpy(cur->asked[i].cmd, cmd, MAX_PATH_LEN-1);
    }
}

static int param_config_check(char *name, char *input, int *dir,
                              unsigned char targ[4], unsigned char src[4])
{
  int val, result = -1;
  char cmd[MAX_PATH_LEN];
  if (sscanf(input, "dir=%d cmd=%299s val=%d", dir, cmd, &val) == 3) 
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

void ovs_a2b_pid_resp(int llid, char *name, int pid)
{
  t_ovs_a2b *cur = find_a2b(name);
  if (!cur)
    KERR("WARNING a pid %d arrived for a2b %s, which has no record any more", pid, name);
  else
    {
    cur->watchdog_count = 0;
    if ((cur->pid == 0) && (cur->suid_root_done == 1))
      {
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

void ovs_a2b_sigdiag_resp(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN];
  t_ovs_a2b *cur;
  int cli_llid, cli_tid;

  if (sscanf(line,
  "a2b_suidroot_ko %63s", name) == 1)
    {
    cur = find_a2b(name);
    if (!cur)
      KERR("ERROR a2b: %s %s", g_cloonix_net, line);
    else
      KERR("ERROR Started a2b: %s %s", g_cloonix_net, name);
    }

  else if (sscanf(line,
  "a2b_suidroot_ok %63s", name) == 1)
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
  "a2b_param_config_ok %d %d %63s", &cli_llid, &cli_tid, name) == 3)
    {
    cur = find_a2b(name);
    if (cur)
      cnf_answer(cur, cli_llid, cli_tid, 1);
    if (msg_exist_channel(cli_llid))
      send_status_ok(cli_llid, cli_tid, name);
    else
      KERR("ERROR %s", line);
    }

  else if (sscanf(line,
  "a2b_param_config_ko %d %d %63s", &cli_llid, &cli_tid, name) == 3)
    {
    cur = find_a2b(name);
    if (cur)
      cnf_answer(cur, cli_llid, cli_tid, 0);
    if (msg_exist_channel(cli_llid))
      send_status_ko(cli_llid, cli_tid, name);
    else
      KERR("ERROR %s", line);
    }

  else
    KERR("ERROR a2b: %s %s", g_cloonix_net, line);
}

int ovs_a2b_get_all_pid(t_lst_pid **lst_pid)
{
  t_lst_pid *glob_lst = NULL;
  t_ovs_a2b *cur = g_head_a2b;
  int i, result = 0;
  while(cur)
    {
    if (cur->pid)
      result++;
    cur = cur->next;
    }
  if (result)
    {
    glob_lst = (t_lst_pid *)clownix_malloc(result*sizeof(t_lst_pid), 37);
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

/* The channel is gone by the time this is called, so the record must not keep
   its number */
void ovs_a2b_llid_closed(int llid, int from_clone)
{
  t_ovs_a2b *cur = g_head_a2b;
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

void ovs_a2b_resp_add_lan(int is_ko, char *name, int num,
                         char *vhost, char *lan)
{
  t_ovs_a2b *cur = find_a2b(name);
  if ((num != 0) && (num != 1))
    {
    KERR("ERROR a2b %s: a plug response for side %d, which is not a "
         "side of an a2b (is_ko=%d)", name, num, is_ko);
    return;
    }
  if (!cur)
    {
    mactopo_add_resp(0, item_a2b, name, num, lan);
    KTRC("plug response for a2b %s side %d (is_ko=%d): no a2b of that "
         "name any more", name, num, is_ko);
    }
  else
    {
    if (strlen(cur->side[num].lan))
      KERR("ERROR: %s %d %s", cur->name, num, cur->side[num].lan);
    memset(cur->side[num].lan, 0, MAX_NAME_LEN);
    if (is_ko)
      {
      mactopo_add_resp(0, item_a2b, name, num, lan);
      KERR("ERROR a2b %s side %d: the add of lan %s was refused",
           name, num, lan);
      utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), name);
      if (strlen(cur->side[num].lan_added))
        {
        mactopo_del_req(item_a2b, name, num, cur->side[num].lan_added);
        lan_del_name(cur->side[num].lan_added, item_a2b, name, num);
        memset(cur->side[num].lan_added, 0, MAX_NAME_LEN);
        }
      }
    else
      {
      mactopo_add_resp(1, item_a2b, name, num, lan);
      strncpy(cur->side[num].lan, lan, MAX_NAME_LEN-1);
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

void ovs_a2b_resp_del_lan(int is_ko, char *name, int num,
                          char *vhost, char *lan)
{
  t_ovs_a2b *cur = find_a2b(name);
  int other;
  /* An a2b has exactly two sides. */
  if (num == 0)
    other = 1;
  else if (num == 1)
    other = 0;
  else
    {
    KERR("ERROR a2b %s: an unplug response for side %d, which is not a "
         "side of an a2b (is_ko=%d)", name, num, is_ko);
    return;
    }
  if (!cur)
    {
    mactopo_del_resp(0, name, num, lan);
    KTRC("unplug response for a2b %s side %d (is_ko=%d): no a2b of that "
         "name any more", name, num, is_ko);
    }
  else if (cur->side[num].del_a2b_req == 1)
    {
    mactopo_del_resp(item_a2b, name, num, lan);
    memset(cur->side[num].lan, 0, MAX_NAME_LEN);
    cur->side[num].del_a2b_req = 0;
    if (cur->side[other].del_a2b_req == 0)
      {
      stop_a2b(cur);
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

t_ovs_a2b *ovs_a2b_exists(char *name)
{
  return (find_a2b(name));
}

t_ovs_a2b *ovs_a2b_get_first(int *nb_a2b)
{
  t_ovs_a2b *result = g_head_a2b;
  *nb_a2b = g_nb_a2b;
  return result;
}

void ovs_a2b_topo_endp_push(void)
{
  t_ovs_a2b *cur = g_head_a2b;
  int i;
  while(cur)
    {
    for (i=0; i<2; i++)
      {
      if (strlen(cur->side[i].lan))
        cfg_topo_endp_push(cur->name, i, cur->side[i].endp_type,
                           cur->side[i].lan);
      }
    cur = cur->next;
    }
}

void ovs_a2b_add(int cli_llid, int cli_tid, char *name)
{
  t_ovs_a2b *cur = find_a2b(name);
  if (cur)
    {
    if (cli_llid)
      {
      KERR("WARNING add a2b %s refused: an a2b of that name already exists",
           name);
      send_status_ko(cli_llid, cli_tid, "Exists already");
      }
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

static int a2b_del_busy(char *name)
{
  t_ovs_a2b *cur = find_a2b(name);
  if (!cur)
    return -1;
  return ((cur->cli_llid) || (cur->side[0].del_a2b_req) ||
          (cur->side[1].del_a2b_req));
}

void ovs_a2b_del(int cli_llid, int cli_tid, char *name)
{
  t_ovs_a2b *cur = find_a2b(name);
  int val;
  if (cur)
    ALOG("del %s: cli_llid=%d del_req=%d/%d lan_added=%s/%s",
         name, cur->cli_llid,
         cur->side[0].del_a2b_req, cur->side[1].del_a2b_req,
         cur->side[0].lan_added[0] ? cur->side[0].lan_added : "-",
         cur->side[1].lan_added[0] ? cur->side[1].lan_added : "-");
  else
    ALOG("del %s: there is no such a2b", name);
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
    /* No client: the del_all pass asking again while ovs answers. */
    if (!cli_llid)
      KTRC("%s: delete already in flight, the del_all pass waits", name);
    else
      del_when_free(cli_llid, cli_tid, name, "a2b", a2b_del_busy, ovs_a2b_del);
    }
  else
    {
    int num, in_flight = 0;
    cur->cli_llid = cli_llid;
    cur->cli_tid = cli_tid;
    for (num=0; num<2; num++)
      {
      if (!strlen(cur->side[num].lan_added))
        continue;
      if (!strlen(cur->side[num].lan))
        KERR("ERROR a2b %s side %d: lan_added %s with no lan attached", name,
             num, cur->side[num].lan_added);
      mactopo_del_req(item_a2b, cur->name, num, cur->side[num].lan_added);
      val = lan_del_name(cur->side[num].lan_added, item_a2b, cur->name, num);
      memset(cur->side[num].lan_added, 0, MAX_NAME_LEN);
      if (val == 2*MAX_LAN)
        continue;
      if (msg_send_del_lan_endp(ovsreq_del_a2b_lan, name, num,
                                cur->side[num].vhost, cur->side[num].lan))
        KERR("ERROR a2b %s side %d: the del of lan %s could not be sent, "
             "deleting the a2b anyway", name, num, cur->side[num].lan);
      else
        {
        cur->side[num].del_a2b_req = 1;
        in_flight += 1;
        }
      }
    if (in_flight == 0)
      {
      stop_a2b(cur);
      }
    }
}

void ovs_a2b_add_lan(int cli_llid, int cli_tid, char *name, int num, char *lan)
{
  t_ovs_a2b *cur = find_a2b(name);
  char err[MAX_PATH_LEN];
  if ((num != 0) && (num != 1))
    {
    KERR("ERROR a2b %s: %d is not a side, an a2b has sides 0 and 1",
         name, num);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Bad num");
    }
  else if (!cur)
    {
    KERR("ERROR plug %s to a2b %s side %d: no a2b of that name", lan, name,
         num);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Does not exist");
    }
  else if ((cur->cli_llid) ||
           (cur->side[0].del_a2b_req) ||
           (cur->side[1].del_a2b_req))
    {
    KERR("ERROR a2b %s cannot take lan %s on side %d now: a client is "
         "still waiting (llid %d), side 0 is being deleted (%d), side 1 is "
         "being deleted (%d)", name, lan, num, cur->cli_llid,
         cur->side[0].del_a2b_req, cur->side[1].del_a2b_req);
    if (cli_llid)
      send_status_ko(cli_llid, cli_tid, "Not ready");
    }
  else if (strlen(cur->side[num].lan))
    {
    KERR("ERROR a2b %s side %d already has lan %s, refusing %s",
         name, num, cur->side[num].lan, lan);
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
      strncpy(cur->side[num].lan_added, lan, MAX_NAME_LEN-1);
      cur->cli_llid = cli_llid;
      cur->cli_tid = cli_tid;
      }
    }
}

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
        {
        KERR("ERROR a2b %s side %d: the del of lan %s could not be sent to "
             "ovs", name, num, lan);
        utils_send_status_ko(&(cur->cli_llid), &(cur->cli_tid), name);
        }
      }
    }
}

int  ovs_a2b_dyn_snf(char *name, int num, int val)
{
  int result = -1;
  t_ovs_a2b *cur = find_a2b(name);
  int endp_type = 0;
  char *vh, *lan;
  char tap[MAX_NAME_LEN];

  if (cur == NULL)
    KERR("ERROR %s %d", name, val);
  else if ((num != 0) && (num != 1))
    KERR("ERROR %s %d %d", name, num, val);
  else
    {
    endp_type = cur->side[num].endp_type;
    if (val)
      {
      if (endp_type == endp_type_eths)
        result = 0;
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
        result = 0;
      else
        {
        cur->side[num].endp_type = endp_type_ethv;
        memset(tap, 0, MAX_NAME_LEN);
        vh = cur->side[num].vhost;
        lan = cur->side[num].lan;
        utils_get_snf_tap_name(tap, vh);
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
    cnf_asked(cur, llid, tid, cmd);
    rpct_send_sigdiag_msg(cur->llid, type_hop_a2b, msg);
    }
  return result;
}

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

