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
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "utils_cmd_line_maker.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "qmp.h"
#include "qmp_dialog.h"
#include "qga_dialog.h"
#include "llid_trace.h"
#include "doors_rpc.h"
#include "doorways_mngt.h"
#include "ovs.h"
#include "kvm.h"
#include "msg.h"


#define QMP_RESET     "{ \"execute\": \"system_reset\" }\n"
#define QMP_STOP      "{ \"execute\": \"stop\" }\n"
#define QMP_CONT      "{ \"execute\": \"cont\" }\n"
#define QMP_CAPA      "{ \"execute\": \"qmp_capabilities\" }\n"
#define QMP_QUERY     "{\"execute\":\"query-status\"}\n"
#define QMP_BALLOON   "{\"execute\":\"balloon\",\"arguments\":{\"value\":%llu}}\n"
#define QMP_QUERY_CMD "{ \"execute\": \"query-commands\" }\n"
#define QMP_SHUTDOWN  "{ \"execute\": \"system_powerdown\" }\n"
#define QMP_QUIT      "{ \"execute\": \"quit\" }\n"
#define QMP_QUERY_BLOCK "{ \"execute\": \"query-block-jobs\",\"arguments\":{}}\n"


#define QMP_START_SAVE "{ \"execute\": \"drive-mirror\", "\
                       "    \"arguments\": {             "\
                       "        \"device\": \"%s\",      "\
                       "        \"job-id\": \"job_%s\",  "\
                       "        \"target\": \"%s\",      "\
                       "        \"sync\": \"%s\"         "\
                       "    }                            "\
                       "}                                "

#define QMP_END_SAVE "{\"execute\": \"block-job-cancel\", "\
                     "    \"arguments\": {                "\
                     "        \"device\": \"job_%s\"      "\
                     "          }                         "\
                     "}                                   "

enum {
  waiting_for_nothing = 0,
  waiting_for_capa_return,
  waiting_for_reboot_return,
  waiting_for_shutdown_return,
  waiting_for_save_return,
  waiting_for_save_block_job_ready,
  waiting_for_save_end_return,
  waiting_for_quit_return,
  waiting_for_max,
};

/*--------------------------------------------------------------------------*/
typedef struct t_timer_save_rootfs
{
char name[MAX_NAME_LEN];
char path[MAX_PATH_LEN];
int llid;
int tid;
} t_timer_save_rootfs;
/*--------------------------------------------------------------------------*/
typedef struct t_qmp_req
{
  int llid;
  int tid;
  int count;
  char req[MAX_RPC_MSG_LEN];
  struct t_qmp_req *next;
} t_qmp_req;
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
typedef struct t_qmp
{
  char name[MAX_NAME_LEN];
  int capa_sent;
  int capa_acked;
  int waiting_for;
  int first_connect;
  int renew_request_qemu_halt;
  t_qmp_req *head_qmp_req;
  struct t_qmp *prev;
  struct t_qmp *next;
} t_qmp;
/*--------------------------------------------------------------------------*/

static t_qmp *g_head_qmp;

/****************************************************************************/
static void alloc_tail_qmp_req(t_qmp *qmp, int llid, int tid, char *msg)
{
  t_qmp_req *req = (t_qmp_req *) clownix_malloc(sizeof(t_qmp_req), 3);
  t_qmp_req *next, *cur = qmp->head_qmp_req;
  memset(req, 0, sizeof(t_qmp_req));
  if (!cur)
    qmp->head_qmp_req = req;
  else
    {
    next = cur->next;
    while(next)
      {
      cur = next;
      next = cur->next;
      }
    cur->next = req;
    }
  strncpy(req->req, msg, MAX_RPC_MSG_LEN-1);
  req->llid = llid;
  req->tid = tid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_head_qmp_req(t_qmp *qmp)
{
  t_qmp_req *cur = qmp->head_qmp_req;
  if (cur)
    {
    qmp->head_qmp_req = cur->next;
    clownix_free(cur, __FUNCTION__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_qmp *find_qmp(char *name)
{
  t_qmp *cur = g_head_qmp;
  while (cur)
    {
    if (!strcmp(name, cur->name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_qmp(char *name, int first)
{
  t_qmp *cur = (t_qmp *) clownix_malloc(sizeof(t_qmp), 3);
  memset(cur, 0, sizeof(t_qmp));
  strncpy(cur->name, name, MAX_NAME_LEN);
  if (g_head_qmp)
    g_head_qmp->prev = cur;
  cur->next = g_head_qmp;
  cur->first_connect = first;
  g_head_qmp = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_qmp(t_qmp *cur)
{
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur == g_head_qmp)
    g_head_qmp = cur->next;
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_conn_end(char *name)
{
  t_qmp *qmp = find_qmp(name);
  qmp_dialog_free(name);
  if (qmp)
    free_qmp(qmp);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_begin_qemu_unix(char *name, int first)
{
  if (find_qmp(name))
    {
    if (first == 0)
      KERR("ERROR LAUNCH %s", name);
    qmp_conn_end(name);
    alloc_qmp(name, first);
    qmp_dialog_alloc(name, qmp_conn_end);
    }
  else
    {
    if (first == 0)
      KERR("ERROR LAUNCH %s", name);
    alloc_qmp(name, first);
    qmp_dialog_alloc(name, qmp_conn_end);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_clean_all_data(void)
{
  t_qmp *next, *cur = g_head_qmp;
  while(cur)
    {
    next = cur->next; 
    while(cur->head_qmp_req)
      free_head_qmp_req(cur);
    free_qmp(cur);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_waiting_for_shutdown(void *data)
{
  char *pname = (char *) data;
  t_qmp *qmp = find_qmp(pname);
  if (qmp)
    {
    if (qmp->waiting_for == waiting_for_shutdown_return)
      {
      alloc_tail_qmp_req(qmp, 0, 0, QMP_QUIT);
      qmp->waiting_for = waiting_for_quit_return;
      }
    }
  clownix_free(pname, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dialog_resp_save(char *name, int llid, int tid, 
                             char *unused_req, char *resp)
{
  char req[MAX_RPC_MSG_LEN];
  char job[MAX_NAME_LEN+10];
  t_qmp *qmp = find_qmp(name);
  if (!qmp)
    KERR("%d %s", llid, name);
  else
    {
    if (qmp->waiting_for != waiting_for_save_block_job_ready)
      KERR("%s %d", name, qmp->waiting_for);
    else
      {
      if (strstr(resp, "BLOCK_JOB_READY"))
        {
        memset(job, 0, MAX_NAME_LEN+10);
        snprintf(job, MAX_NAME_LEN+9, "job_%s", name);
        if (strstr(resp, job))
          {
          memset(req, 0, MAX_RPC_MSG_LEN);
          snprintf(req, MAX_RPC_MSG_LEN-1, QMP_END_SAVE, qmp->name);
          alloc_tail_qmp_req(qmp, llid, tid, req);
          qmp->waiting_for = waiting_for_save_end_return;
          }
        else
          KERR("%s", resp);
        }
      else
        KERR("%s", resp);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dialog_resp_return(t_qmp *qmp, int llid, int tid, int status)
{
  char *pname;
  t_vm *vm = cfg_get_vm(qmp->name);
  char *eth_name;
  t_eth_table *eth_tab;
  int i;
  if (qmp->waiting_for == waiting_for_capa_return)
    {
    if (!vm)
      KERR("%s", qmp->name);
    else
      {
      if (status)
        KERR("ERROR %s", qmp->name);
      else
        {
        if (!(vm->kvm.vm_config_flags & VM_CONFIG_FLAG_NO_QEMU_GA))
          {
          doors_send_del_vm(get_doorways_llid(), 0, vm->kvm.name);
          doors_send_add_vm(get_doorways_llid(), 0, vm->kvm.name,
                            utils_get_qbackdoor_path(vm->kvm.vm_id));
          }
        if (qmp->first_connect)
          {
          eth_tab = vm->kvm.eth_table;
          for (i = 0; i < vm->kvm.nb_tot_eth; i++)
            {
            eth_name = eth_tab[i].vhost_ifname;
            if (msg_send_vhost_up(qmp->name, i, eth_name))
              KERR("ERROR KVMETH %s %d %s", qmp->name, i, eth_name);
            }
          }
        }
      }
    qmp->capa_acked = 1;
    qmp->waiting_for = waiting_for_nothing;
    }
  else if (qmp->waiting_for == waiting_for_reboot_return)
    {
    qmp->waiting_for = waiting_for_nothing;
    if (status)
      KERR("ERROR %s", qmp->name);
    else
      {
      if (qmp->first_connect == 0)
        {
        qmp_conn_end(qmp->name);
        }
      }
    }
  else if (qmp->waiting_for == waiting_for_shutdown_return)
    {
    if (status)
      KERR("ERROR %s", qmp->name);
    pname = (char *) clownix_malloc(MAX_NAME_LEN, 3);
    memset(pname, 0, MAX_NAME_LEN);
    strncpy(pname, qmp->name, MAX_NAME_LEN-1);
    clownix_timeout_add(5, timer_waiting_for_shutdown, pname, NULL, NULL);
    }
  else if (qmp->waiting_for == waiting_for_quit_return)
    {
    qmp->waiting_for = waiting_for_nothing;
    if (qmp->first_connect == 0)
      {
      qmp_conn_end(qmp->name);
      }
    }
  else if (qmp->waiting_for == waiting_for_save_return)
    {
    if (status)
      {
      KERR("ERROR %s", qmp->name);
      if ((llid) && (llid_trace_exists(llid)))
        send_status_ko(llid, tid, "qmp save ko");
      qmp->waiting_for = waiting_for_nothing;
      if (qmp->first_connect == 0)
        {
        qmp_conn_end(qmp->name);
        }
      }
    else
      {
      if (qmp_dialog_req(qmp->name, llid, tid, "", dialog_resp_save))
        {
        KERR("ERROR %s", qmp->name);
        if ((llid) && (llid_trace_exists(llid)))
          send_status_ko(llid, tid, "qmp save ko");
        }
      else
        {
        qmp->waiting_for = waiting_for_save_block_job_ready;
        }
      }
    }
  else if (qmp->waiting_for == waiting_for_save_end_return)
    {
    qmp->waiting_for = waiting_for_nothing;
    if (status)
      {
      KERR("ERROR %s", qmp->name);
      if ((llid) && (llid_trace_exists(llid)))
        send_status_ko(llid, tid, "qmp save ko");
      }
    else
      {
      if ((llid) && (llid_trace_exists(llid)))
        send_status_ok(llid, tid, "qmp save ok");
      if (qmp->first_connect == 0)
        {
        qmp_conn_end(qmp->name);
        }
      }
    }
  else if (qmp->waiting_for == waiting_for_nothing)
    {
    KERR("ERROR %s", qmp->name);
    if (qmp->first_connect == 0)
      {
      qmp_conn_end(qmp->name);
      }
    }
  else
    {
    KERR("ERROR %s %d", qmp->name, qmp->waiting_for);
    if (qmp->first_connect == 0)
      {
      qmp_conn_end(qmp->name);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void dialog_resp(char *name, int llid, int tid, char *req, char *resp)
{
  t_qmp *qmp = find_qmp(name);
  if (!qmp)
    KERR("%d %s", llid, name);
  else
    {
    if (!qmp->waiting_for)
      KERR("%s %s %s", name, req, resp);
    else
      {
      if ((!strncmp(resp, "{\"return\":", strlen("{\"return\":"))) ||
          (!strncmp(resp, "{\"timestamp\":", strlen("{\"timestamp\":"))))
        {
        dialog_resp_return(qmp, llid, tid, 0);
        }
      else if (!strncmp(resp, "{\"error\":", strlen("{\"error\":")))
        {
        KERR("ERROR %s %s %s", name, resp, req);
        dialog_resp_return(qmp, llid, tid, -1);
        }
      else if (!strncmp(resp, "timeout_qmp_resp", strlen("timeout_qmp_resp")))
        {
        KERR("WARNING TIMEOUT %s %s %s", name, resp, req);
        dialog_resp_return(qmp, llid, tid, -1);
        }
      else
        KERR("ERROR %d %s %s %s", qmp->waiting_for, name, req, resp);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_msg_recv(char *name, char *msg)
{
  t_qmp *qmp = find_qmp(name);
  if (!qmp)
    KERR("%s", name);
  else
    {
    if (qmp->capa_sent == 0)
      {
      if (!strncmp(msg, "{\"QMP\":", strlen("{\"QMP\":")))
        {
        alloc_tail_qmp_req(qmp, 0, 0, QMP_CAPA);
        qmp->capa_sent = 1;
        qmp->waiting_for = waiting_for_capa_return;
        }
      }
    else if (qmp->waiting_for == waiting_for_shutdown_return)
      {
      if (strstr(msg, "{\"SHUTDOWN\":"))
        {
        qmp->waiting_for = waiting_for_nothing;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_fifo_visit(void *data)
{
  t_qmp *next, *cur = g_head_qmp;
  t_qmp_req *req;
  t_vm *vm;
  while(cur)
    {
    next = cur->next;
    req = cur->head_qmp_req;
    if (req)
      {
      if (cur->waiting_for == waiting_for_save_block_job_ready)
        {
        if ((req->llid) && llid_trace_exists(req->llid))
          send_status_ko(req->llid, req->tid, "qmp ko");
        free_head_qmp_req(cur);
        }
      else if (((cur->capa_acked && (req->llid)) || (req->llid == 0))  && 
         (!qmp_dialog_req(cur->name,req->llid,req->tid,req->req,dialog_resp)))
        {
        free_head_qmp_req(cur);
        }
      else
        {
        vm = cfg_get_vm(cur->name);
        if (!vm)
          {
          free_head_qmp_req(cur);
          }
        else
          {
          if (vm->vm_to_be_killed)
            {
            free_head_qmp_req(cur);
            }
          else
            {
            req->count += 1;
            if (req->count > 20)
              {
              KERR("ERROR TIMEOUT %s", req->req);
              if ((req->llid) && llid_trace_exists(req->llid))
                send_status_ko(req->llid, req->tid, "qmp ko");
              free_head_qmp_req(cur);
              }
            }
          }
        }
      }
    cur = next;
    }
  clownix_timeout_add(10, timer_fifo_visit, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_init(void)
{
  g_head_qmp = NULL;
  clownix_timeout_add(10, timer_fifo_visit, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void format_qmp_start_save(char *name, char *path, char *req)
{
  char type[MAX_NAME_LEN];
  memset(req, 0, MAX_RPC_MSG_LEN);
  memset(type, 0, MAX_NAME_LEN);
  snprintf(type, MAX_NAME_LEN-1, "full");
  snprintf(req, MAX_RPC_MSG_LEN-1, QMP_START_SAVE, name, name, path, type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_request_save_rootfs(void *data)
{
  t_timer_save_rootfs *tm = (t_timer_save_rootfs *) data;
  t_qmp *qmp = find_qmp(tm->name);
  if (qmp)
    {
    qmp_request_save_rootfs(tm->name, tm->path, tm->llid, tm->tid);
    }
  clownix_free(data, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_request_save_rootfs(char *name, char *path, int llid, int tid)
{
  char req[MAX_RPC_MSG_LEN];
  t_qmp *qmp = find_qmp(name);
  t_timer_save_rootfs *tm;
  if (!qmp)
    {
    qmp_begin_qemu_unix(name, 0);
    tm = (t_timer_save_rootfs *) clownix_malloc(sizeof(t_timer_save_rootfs), 3);
    memset(tm, 0, sizeof(t_timer_save_rootfs));
    strncpy(tm->name, name, MAX_NAME_LEN-1);
    strncpy(tm->path, path, MAX_PATH_LEN-1);
    tm->llid = llid;
    tm->tid = tid;
    clownix_timeout_add(20, timer_request_save_rootfs, tm, NULL, NULL);
    }
  else if (qmp->waiting_for != waiting_for_nothing)
    {
    memset(req, 0, MAX_RPC_MSG_LEN);
    snprintf(req, MAX_RPC_MSG_LEN-1, "error qmp doing something %d",
             qmp->waiting_for);
    send_status_ko(llid, tid, req);
    }
  else
    {
    format_qmp_start_save(name, path, req);
    alloc_tail_qmp_req(qmp, llid, tid, req);
    qmp->waiting_for = waiting_for_save_return;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_request_qemu_reboot(void *data)
{
  char *pname = (char *) data;
  t_qmp *qmp = find_qmp(pname);
  if (qmp)
    {
    qmp_request_qemu_reboot(pname);
    }
  clownix_free(pname, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void qmp_request_qemu_reboot(char *name)
{
  t_qmp *qmp = find_qmp(name);
  char *pname;
  if (!qmp)
    {
    KERR("WARNING RECONNECT FOR REQUEST REBOOT ONE %s", name);
    qmp_begin_qemu_unix(name, 0);
    pname = (char *) clownix_malloc(MAX_NAME_LEN, 3);
    memset(pname, 0, MAX_NAME_LEN);
    strncpy(pname, name, MAX_NAME_LEN-1);
    clownix_timeout_add(20, timer_request_qemu_reboot, pname, NULL, NULL);
    }
  else if (qmp->waiting_for != waiting_for_nothing)
    {
    KERR("WARNING ERROR? DIFFERED REQUEST REBOOT %s", name);
    KERR("WARNING QMP DOING SOMETHING %s %d", name, qmp->waiting_for);
    alloc_tail_qmp_req(qmp, 0, 0, QMP_RESET);
    qmp->waiting_for = waiting_for_reboot_return;
    }
  else
    {
    alloc_tail_qmp_req(qmp, 0, 0, QMP_RESET);
    qmp->waiting_for = waiting_for_reboot_return;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_qmp_request_qemu_halt(void *data)
{
  char *name = (char *) data;
  t_qmp *qmp = find_qmp(name);
  if (!qmp)
    {
    KERR("ERROR %s", name);
    clownix_free(data, __FUNCTION__);
    }
  else
    {
    if (qmp->waiting_for != waiting_for_nothing)
      {
      qmp->renew_request_qemu_halt += 1;
      if (qmp->renew_request_qemu_halt > 50)
        {
        KERR("ERROR %s", name);
        clownix_free(data, __FUNCTION__);
        }
      else 
        clownix_timeout_add(5, timer_qmp_request_qemu_halt, name, NULL, NULL);
      }
    else
      {
      alloc_tail_qmp_req(qmp, 0, 0, QMP_SHUTDOWN);
      qmp->waiting_for = waiting_for_shutdown_return;
      clownix_free(data, __FUNCTION__);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_request_qemu_halt(char *name)
{
  t_qmp *qmp = find_qmp(name);
  char *pname;
  if (!qmp)
    {
    qmp_begin_qemu_unix(name, 0);
    pname = (char *) clownix_malloc(MAX_NAME_LEN, 3);
    memset(pname, 0, MAX_NAME_LEN);
    strncpy(pname, name, MAX_NAME_LEN-1);
    clownix_timeout_add(10, timer_qmp_request_qemu_halt, pname, NULL, NULL);
    }
  else if (qmp->waiting_for != waiting_for_nothing)
    {
    pname = (char *) clownix_malloc(MAX_NAME_LEN, 3);
    memset(pname, 0, MAX_NAME_LEN);
    strncpy(pname, name, MAX_NAME_LEN-1);
    clownix_timeout_add(10, timer_qmp_request_qemu_halt, pname, NULL, NULL);
    }
  else
    {
    alloc_tail_qmp_req(qmp, 0, 0, QMP_SHUTDOWN);
    qmp->waiting_for = waiting_for_shutdown_return;
    }
}
/*--------------------------------------------------------------------------*/


