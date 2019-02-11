/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include "qmonitor.h"
#include "qmp.h"
#include "qhvc0.h"
#include "xwy.h"
#include "llid_trace.h"
#include "doorways_mngt.h"

#define MAX_CALLBACK_END 50

typedef struct t_callback_end
{
  t_cb_end_automate cb; 
  char name[MAX_NAME_LEN]; 
  int safety_counter;
} t_callback_end;


static t_callback_end callback_end_tab[MAX_CALLBACK_END];
static t_cb_end_automate callback_rm_cloonix_ok = NULL;

static int lock_self_destruction_dir;
static int glob_req_self_destruction;

/*****************************************************************************/
static int self_destruction_ok(void)
{
  if (lock_self_destruction_dir == 0 ) 
    return 1;
  else
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
static void end_clownix_rm(void *ptr, int status, char *nm)
{
  char *path = (char *) ptr;
  if (status)
    {
    KERR("rm %s ko", path);
    exit(-1);
    }
  else
    {
    if (callback_rm_cloonix_ok)
      callback_rm_cloonix_ok(NULL, 0, NULL);
    else
      KOUT(" ");
    } 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int start_clownix_rm(void *ptr)
{
  char *path = (char *) ptr;
  char *argv[] =  {"/bin/rm", "-rf", path, NULL};
  execv("/bin/rm", argv);
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void auto_rm_cloonix(t_cb_end_automate cb)
{
  static char path[MAX_PATH_LEN];
  strcpy(path, cfg_get_work());
  callback_rm_cloonix_ok = cb;
  pid_clone_launch(start_clownix_rm, end_clownix_rm, NULL,
                   (void *)path, (void *)path,  NULL, 
                   (char *)__FUNCTION__, -1, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void check_for_work_dir_inexistance(t_cb_end_automate cb)
{
  struct stat stat_path;
  if (stat(cfg_get_work(), &stat_path) == 0)
    {
    printf( "Path: \"%s\" already exists, destroing it\n\n",cfg_get_work());
    auto_rm_cloonix(cb);
    }
  else
    {
    cb(NULL, 0, NULL);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void extremely_last_action(void *data)
{
  static int count_time=0;
  if (get_nb_running_pids())
    {
    count_time += 1;
    KERR(" %d", get_nb_running_pids());
    if (count_time > 3)
      {
      KERR("clone wont die %d", get_nb_running_pids());
      kerr_running_pids();
      pid_clone_kill_all();
      } 
    if (count_time > 5)
      exit(0);
    }
  else
    exit(0);
  clownix_timeout_add(20, extremely_last_action, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void last_action_self_destruction(void *data)
{
  t_llid_tid *llid_tid = (t_llid_tid *) data;
  char path[MAX_PATH_LEN];
  char err[MAX_PRINT_LEN];
  pid_clone_kill_single(doorways_get_distant_pid());
  llid_free_all_llid();
  sprintf(path, "%s/cloonix_lock", cfg_get_root_work());
  unlink(path);
  sprintf(path, "%s", pid_get_clone_internal_com());
  unlink(path);
  if (unlink_sub_dir_files_except_dir(utils_get_endp_sock_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_dtach_sock_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_cli_sock_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_snf_pcap_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_muswitch_sock_dir(), err))
    event_print("DELETE PROBLEM: %s\n", err);
  if (unlink_sub_dir_files_except_dir(utils_get_muswitch_traf_dir(), err))
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
static void action_self_destruction(void *data)
{
  static int count = 0;
  int nb;
  t_llid_tid *llid_tid = (t_llid_tid *) data;

  if (count == 0)
    kill_xwy();
  if ((self_destruction_ok()) || (count > 500))
    {
    if ((count < 300) && 
        ((qmonitor_still_present()) || 
          qhvc0_still_present())) 
      {
      count++;
      clownix_timeout_add(20, action_self_destruction, (void *)llid_tid, 
                          NULL, NULL);
      }
    else
      {
      cfg_get_first_vm(&nb);
      if ((count < 300) && nb) 
        {
        count++;
        clownix_timeout_add(20, action_self_destruction, (void *)llid_tid, 
                            NULL, NULL);
        }
      else
        {
        event_print("Self-destruction triggered");
        clownix_timeout_add(20, last_action_self_destruction, (void *)llid_tid, 
                            NULL, NULL);
        }
      }
    }
  else
    {
    event_print("%s %d", __FUNCTION__, get_lock_self_destruction_dir());
    count++;
    clownix_timeout_add(10, action_self_destruction, (void *)llid_tid, 
                        NULL, NULL);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void auto_self_destruction(int llid, int tid)
{
  t_llid_tid *llid_tid = (t_llid_tid *) clownix_malloc(sizeof(t_llid_tid), 11);
  glob_req_self_destruction = 1;
  llid_tid->llid = llid;
  llid_tid->tid = tid;
  clownix_timeout_add(10, action_self_destruction, (void *)llid_tid, 
                      NULL, NULL);
  kill_doors();
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

