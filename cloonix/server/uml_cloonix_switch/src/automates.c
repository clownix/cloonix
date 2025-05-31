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
#include "xwy.h"
#include "novnc.h"
#include "ovs.h"
#include "cnt.h"
#include "llid_trace.h"
#include "doorways_mngt.h"
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


t_pid_lst *create_list_pid(int *nb);

static t_callback_end callback_end_tab[MAX_CALLBACK_END];

static int lock_self_destruction_dir;
static int glob_req_self_destruction;

/*****************************************************************************/
static int self_destruction_ok(void)
{
  if ((ovs_still_present() == 0) &&
      (lock_self_destruction_dir == 0)) 
    return 1;
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void inc_lock_self_destruction_dir(void)
{ 
  lock_self_destruction_dir++; 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void dec_lock_self_destruction_dir(void)
{ 
  lock_self_destruction_dir--; 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_lock_self_destruction_dir(void)
{ 
  return lock_self_destruction_dir; 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int get_glob_req_self_destruction(void)
{
  return glob_req_self_destruction;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void extremely_last_action(void *data)
{
  static int count_time=0;
  int i, nb, tot;
  t_pid_lst *lst = create_list_pid(&nb);
  (void) lst;
  if (nb)
    {
    count_time += 1;
    if (count_time > 3)
      {
      for (i=0; i<nb; i++)
        if (lst[i].pid != 0)
          KERR("process %s %d", lst[i].name, lst[i].pid);
      KERR("clone number %d", get_nb_running_pids());
      kerr_running_pids();
      pid_clone_kill_all();
      } 
    if (count_time > 5)
      exit(0);
    tot = nb;
    for (i=0; i<nb; i++)
      {
      if (!strcmp(lst[i].name, "doors"))
        tot -= 1;
      if (!strcmp(lst[i].name, "xwy"))
        tot -= 1;
      if (!strcmp(lst[i].name, "cloonix_server"))
        tot -= 1;
      if (!strcmp(lst[i].name, "suid_power"))
        tot -= 1;
      }
    if (tot == 0)
      exit(0);
    }
  else
    KOUT(" ");
  llid_free_all_llid();
  clownix_timeout_add(30, extremely_last_action, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void last_action_self_destruction(void *data)
{
  t_llid_tid *llid_tid = (t_llid_tid *) data;
  char path[MAX_PATH_LEN];
  char err[MAX_PRINT_LEN];
  if (doorways_get_distant_pid())
    {
    if (pid_clone_kill_single(doorways_get_distant_pid()))
      KERR("ERROR DOOR CLONE KILL %d", doorways_get_distant_pid());
    }
  sprintf(path, "%s/cloonix_lock", cfg_get_root_work());
  unlink(path);
  sprintf(path, "%s", pid_get_clone_internal_com());
  unlink(path);
  if (unlink_sub_dir_files_except_dir(utils_get_ovs_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_dtach_sock_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_snf_pcap_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_log_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_crun_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_nginx_conf_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_nginx_logs_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_nginx_client_body_temp_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files(utils_get_nginx_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_c2c_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_nat_main_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_nat_proxy_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_a2b_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_run_config_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_run_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(cfg_get_work(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(cfg_get_root_work(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  clownix_timeout_add(20, extremely_last_action, NULL, NULL, NULL);
  if (llid_tid)
    send_status_ok(llid_tid->llid, llid_tid->tid, "selfdestruct");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void after_action_self_destruction(void *data)
{
  t_llid_tid *llid_tid = (t_llid_tid *) data;
  kill_doors();
  ovs_nat_main_del_all();
  kill_xwy();
  event_print("Self-destruction triggered");
  clownix_timeout_add(20, last_action_self_destruction, (void *)llid_tid,
                      NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void action_self_destruction(void *data)
{
  static int count = 0;
  int nb_kvm, nb_cnt;
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
      if ((count < 300) && (nb_kvm+nb_cnt)) 
        {
        count++;
        clownix_timeout_add(50, action_self_destruction, (void *)llid_tid, 
                            NULL, NULL);
        }
      else
        {
        clownix_timeout_add(200, after_action_self_destruction, (void *)llid_tid, 
                            NULL, NULL);
        }
      }
    }
  else
    {
    event_print("%s %d", __FUNCTION__, get_lock_self_destruction_dir());
    count++;
    clownix_timeout_add(50, action_self_destruction, (void *)llid_tid, 
                        NULL, NULL);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void auto_self_destruction(int llid, int tid)
{
  t_llid_tid *llid_tid = (t_llid_tid *) clownix_malloc(sizeof(t_llid_tid), 11);
  glob_req_self_destruction = 1;
  end_novnc(1);
  proxycrun_kill();
  proxymous_kill();
  llid_tid->llid = llid;
  llid_tid->tid = tid;
  clownix_timeout_add(10, action_self_destruction, (void *)llid_tid, 
                      NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void automates_init(void)
{
  lock_self_destruction_dir = 0;
  glob_req_self_destruction = 0;
  memset(callback_end_tab, 0, MAX_CALLBACK_END * sizeof(t_callback_end));
}
/*---------------------------------------------------------------------------*/

