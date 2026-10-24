/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include "io_clownix.h"
#include "commun_daemon.h"
#include "rpc_clownix.h"
#include "pid_clone.h"
#include "cfg_store.h"
#include "automates.h"
#include "event_subscriber.h"
#include "machine_create.h"
#include "system_callers.h"
#include "utils_cmd_line_maker.h"
#include "qmp.h"
#include "ovs.h"
#include "ovs_snf.h"
#include "cnt.h"
#include "llid_trace.h"
#include "doorways_mngt.h"
#include "gui_server_mngt.h"
#include "doors_rpc.h"
#include "suid_power.h"
#include "proxymous.h"
#include "proxycrun.h"
#include "ovs_nat_main.h"

#define MAX_CALLBACK_END 50

typedef struct t_callback_end
{
  t_cb_end_automate cb; 
  char name[MAX_NAME_LEN]; 
  int safety_counter;
} t_callback_end;

char *get_proxymous_dir(void);
t_pid_lst *create_list_pid(int *nb);
int pid_blocks_shutdown(char *name, int pid);

static t_callback_end callback_end_tab[MAX_CALLBACK_END];

static int lock_self_destruction_dir;
static int glob_req_self_destruction;

static int self_destruction_ok(void)
{
  if ((ovs_still_present() == 0) &&
      (lock_self_destruction_dir == 0)) 
    return 1;
  return 0;
}

void inc_lock_self_destruction_dir(void)
{ 
  lock_self_destruction_dir++; 
}

void dec_lock_self_destruction_dir(void)
{ 
  lock_self_destruction_dir--; 
}

int get_glob_req_self_destruction(void)
{
  return glob_req_self_destruction;
}

static void extremely_last_action(void *data)
{
  static int count_time=0;
  int i, nb, tot = 0;
  t_pid_lst *lst = create_list_pid(&nb);
  if (nb)
    {
    count_time += 1;
    for (i=0; i<nb; i++)
      {
      if (pid_blocks_shutdown(lst[i].name, lst[i].pid))
        tot += 1;
      }
    if (count_time > 3)
      {
      for (i=0; i<nb; i++)
        if ((lst[i].pid != 0) && pid_blocks_shutdown(lst[i].name, lst[i].pid))
          KERR("WARNING still here at the last step: process %s %d",
               lst[i].name, lst[i].pid);
      kerr_running_pids();
      pid_clone_kill_all();
      }
    if (count_time > 5)
      {
      clownix_free(lst, __FUNCTION__);
      exit(0);
      }
    if (tot == 0)
      {
      clownix_free(lst, __FUNCTION__);
      KERR("INFO stopped");
      exit(0);
      }
    }
  else
    KOUT(" ");
  clownix_free(lst, __FUNCTION__);
  llid_free_all_llid();
  clownix_timeout_add(30, extremely_last_action, NULL, NULL, NULL);
}

void last_action_self_destruction(void *data)
{
  t_llid_tid *llid_tid = (t_llid_tid *) data;
  char path[MAX_PATH_LEN];
  char err[MAX_PRINT_LEN];
  KTRC("shutdown stage 3: stopping proxymous last, everything that "
       "needed it is done");
  proxycrun_kill();
  proxymous_kill();
  gui_server_stop();
  if (doorways_get_distant_pid())
    {
    if (pid_clone_kill_single(doorways_get_distant_pid()))
      KERR("ERROR DOOR CLONE KILL %d", doorways_get_distant_pid());
    }
  snprintf(path, sizeof(path), "%s/cloonix_lock", cfg_get_root_work());
  unlink(path);
  snprintf(path, sizeof(path), "%s", pid_get_clone_internal_com());
  unlink(path);
  if (rm_work_dir(utils_get_ovs_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(utils_get_dtach_sock_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(utils_get_snf_pcap_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(utils_get_log_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(utils_get_crun_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(utils_get_c2c_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(utils_get_nat_main_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(utils_get_nat_proxy_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(utils_get_a2b_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(utils_get_run_config_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(utils_get_run_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(cfg_get_work(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(cfg_get_root_work(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  if (rm_work_dir(get_proxymous_dir(), err))
    {
    KERR("ERROR DELETE PROBLEM: %s", err);
    }
  KTRC("shutdown stage 4: directories cleared, the switch exits next");
  clownix_timeout_add(20, extremely_last_action, NULL, NULL, NULL);
  if (llid_tid)
    send_status_ok(llid_tid->llid, llid_tid->tid, "selfdestruct");
}

static void after_action_self_destruction(void *data)
{
  t_llid_tid *llid_tid = (t_llid_tid *) data;
  KTRC("shutdown stage 2: stopping doors and nats");
  kill_doors();
  ovs_nat_main_del_all();
  clownix_timeout_add(20, last_action_self_destruction, (void *)llid_tid,
                      NULL, NULL);
}

static void action_self_destruction(void *data)
{
  static int count = 0;
  int nb_kvm, nb_cnt;
  /* One line every five seconds with the three counts. */
  if ((count > 0) && ((count % 10) == 0))
    {
    int nk, nc;
    t_lst_pid *lst = NULL;
    cfg_get_first_vm(&nk);
    nc = cnt_get_all_pid(&lst);
    if (lst)
      clownix_free(lst, __FUNCTION__);
    /* Read by the trace below only, which compiles to nothing when traces are
       off. */
    (void) nc;
    KTRC("shutdown waiting %ds so far: ovs_present=%d dir_locks=%d "
         "kvm=%d cnt=%d", count / 2, ovs_still_present(),
         get_lock_self_destruction_dir(), nk, nc);
    }
  t_llid_tid *llid_tid = (t_llid_tid *) data;
  t_lst_pid *lst_pid;

  if ((self_destruction_ok()) || (count > 500))
    {
    if ((count < 300) && (ovs_still_present()))
      {
      count++;
      clownix_timeout_add(50, action_self_destruction, (void *)llid_tid, 
                          NULL, NULL);
      }
    else
      {
      cfg_get_first_vm(&nb_kvm);
      nb_cnt = cnt_get_all_pid(&lst_pid);
      if (lst_pid)
        clownix_free(lst_pid, __FUNCTION__);
      if ((count < 300) && (nb_kvm+nb_cnt)) 
        {
        count++;
        clownix_timeout_add(50, action_self_destruction, (void *)llid_tid, 
                            NULL, NULL);
        }
      else
        {
        /* Reached only when ovs has stopped and no guest is left. */
        KTRC("shutdown stage 1 done after %d poll(s), about %d ms of "
             "waiting for ovs and the guests to go", count, count * 250);
        clownix_timeout_add(20, after_action_self_destruction,
                            (void *)llid_tid, NULL, NULL);
        }
      }
    }
  else
    {
    count++;
    clownix_timeout_add(50, action_self_destruction, (void *)llid_tid, 
                        NULL, NULL);
    }
}

void auto_self_destruction(int llid, int tid, char *reason)
{
  t_llid_tid *llid_tid = (t_llid_tid *) clownix_malloc(sizeof(t_llid_tid), 11);
  /* Name the caller: this stops the entire net and there are two ways in. */
  if (llid)
    KTRC("net stopping, asked by: %s (llid=%d tid=%d)",
         reason ? reason : "a client", llid, tid);
  else
    KERR("WARNING net stopping, asked by: %s", reason ? reason : "nobody said");
  glob_req_self_destruction = 1;
  llid_tid->llid = llid;
  llid_tid->tid = tid;
  /* Told to stop BEFORE the wait begins, not merely waited for. */
  ovs_snf_stop_all();
  KTRC("shutdown stage 1: waiting for ovs, guests and directories");
  clownix_timeout_add(10, action_self_destruction, (void *)llid_tid, 
                      NULL, NULL);
}

void automates_safety_heartbeat(void)
{
  int i;
  t_callback_end *cb_end;
  for (i=0; i<MAX_CALLBACK_END; i++)
    {
    if (callback_end_tab[i].cb)
      {
      cb_end = &(callback_end_tab[i]);
      cb_end->safety_counter++;
      if (cb_end->safety_counter > 30)
        KOUT(" ");
      }
    }
}

void automates_init(void)
{
  lock_self_destruction_dir = 0;
  glob_req_self_destruction = 0;
  memset(callback_end_tab, 0, MAX_CALLBACK_END * sizeof(t_callback_end));
}

