/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include "llid_trace.h"
#include "doors_rpc.h"
#include "doorways_mngt.h"
#include "dpdk_ovs.h"


#define QMP_RESET     "{ \"execute\": \"system_reset\" }\n"
#define QMP_STOP      "{ \"execute\": \"stop\" }\n"
#define QMP_CONT      "{ \"execute\": \"cont\" }\n"
#define QMP_CAPA      "{ \"execute\": \"qmp_capabilities\" }\n"
#define QMP_QUERY     "{\"execute\":\"query-status\"}\n"
#define QMP_BALLOON   "{\"execute\":\"balloon\",\"arguments\":{\"value\":%llu}}\n"
#define QMP_QUERY_CMD "{ \"execute\": \"query-commands\" }\n"
#define QMP_SHUTDOWN  "{ \"execute\": \"system_powerdown\" }\n"
#define QMP_QUIT      "{ \"execute\": \"quit\" }\n"


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
typedef struct t_qmp_sub
{
  int llid;
  int tid;
  struct t_qmp_sub *prev;
  struct t_qmp_sub *next;
} t_qmp_sub;
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
typedef struct t_qmp
{
  char name[MAX_NAME_LEN];
  int capa_sent;
  int capa_acked;
  int waiting_for;
  t_qmp_req *head_qmp_req;
  t_qmp_sub *head_qmp_sub;
  struct t_qmp *prev;
  struct t_qmp *next;
} t_qmp;
/*--------------------------------------------------------------------------*/

static t_qmp *g_head_qmp;
static t_qmp_sub *g_head_all_qmp_sub;

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
static void alloc_qmp(char *name)
{
  t_qmp *cur = (t_qmp *) clownix_malloc(sizeof(t_qmp), 3);
  memset(cur, 0, sizeof(t_qmp));
  strncpy(cur->name, name, MAX_NAME_LEN);
  if (g_head_qmp)
    g_head_qmp->prev = cur;
  cur->next = g_head_qmp;
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
static t_qmp_sub *find_qmp_sub(t_qmp *qmp, int llid)
{
  t_qmp_sub *cur;
  if (qmp)
    cur = qmp->head_qmp_sub;
  else
    cur = g_head_all_qmp_sub;
  while (cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_qmp_sub(t_qmp *qmp, int llid, int tid)
{
  t_qmp_sub *cur = find_qmp_sub(qmp, llid);
  if (cur)
    KERR(" ");
  else
    {
    cur = (t_qmp_sub *) clownix_malloc(sizeof(t_qmp_sub), 3);
    memset(cur, 0, sizeof(t_qmp_sub));
    cur->llid = llid;
    cur->tid = tid;
    if (qmp)
      {
      if (qmp->head_qmp_sub)
        qmp->head_qmp_sub->prev = cur;
      cur->next = qmp->head_qmp_sub;
      qmp->head_qmp_sub = cur;
      }
    else
      {
      if (g_head_all_qmp_sub)
        g_head_all_qmp_sub->prev = cur;
      cur->next = g_head_all_qmp_sub;
      g_head_all_qmp_sub = cur;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_qmp_sub(t_qmp *qmp, t_qmp_sub *cur)
{
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (qmp)
    {
    if (cur == qmp->head_qmp_sub)
      qmp->head_qmp_sub = cur->next;
    }
  else
    {
    if (cur == g_head_all_qmp_sub)
      g_head_all_qmp_sub = cur->next;
    }
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_agent_sysinfo(char *name, int used_mem_agent)
{
  t_vm   *vm;
  vm = cfg_get_vm(name);
  if ((vm) && 
      (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_BALLOONING))
    {
//TODO
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void report_msg(int llid, int tid, char *name, char *msg, int to_vm)
{
  char fmsg[MAX_RPC_MSG_LEN];
  if (strlen(msg) > (MAX_RPC_MSG_LEN-100))
    KERR("%s %d", msg, (int) strlen(msg));
  memset(fmsg, 0, MAX_RPC_MSG_LEN);
  if (to_vm)
    snprintf(fmsg, MAX_RPC_MSG_LEN-1, "cloonix-->%s:%s", name, msg);
  else
    snprintf(fmsg, MAX_RPC_MSG_LEN-1, "%s-->cloonix:%s", name, msg);
  send_qmp_resp(llid, tid, name, fmsg, 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_msg_send(char *name, char *msg)
{
  t_qmp *qmp = find_qmp(name);
  t_qmp_sub *cur;
  if (!qmp)
    KERR("%s", name);
  else
    {
    cur = qmp->head_qmp_sub;
    while(cur)
      {
      report_msg(cur->llid, cur->tid, name, msg, 1);
      cur = cur->next;
      }
    cur = g_head_all_qmp_sub;
    while(cur)
      {
      report_msg(cur->llid, cur->tid, name, msg, 1);
      cur = cur->next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_conn_end(char *name)
{
  t_qmp *qmp = find_qmp(name);
  t_qmp_sub *cur, *next;
  t_qmp_req *req;
  t_vm   *vm = cfg_get_vm(name);
  t_eth_table *eth_tab;
  int i;
  if (!qmp)
    KERR("%s", name);
  else
    {
    if (vm)
      {
      eth_tab = vm->kvm.eth_table;
      for (i=0; i<vm->kvm.nb_tot_eth; i++)
        {
        if (eth_tab[i].eth_type == eth_type_dpdk)
          {
          dpdk_ovs_del_vm(name);
          break;
          }
        }
      }
    cur = qmp->head_qmp_sub;
    while(cur)
      {
      next = cur->next;
      if (llid_trace_exists(cur->llid))
        llid_trace_free(cur->llid, 0, __FUNCTION__);
      else
        {
        KERR("%s", name);
        free_qmp_sub(qmp, cur);
        }
      cur = next;
      }
    while(qmp->head_qmp_req)
      {
      req = qmp->head_qmp_req;
      if (llid_trace_exists(req->llid))
        send_qmp_resp(req->llid, req->tid, name, "error machine deleted", -1);
      free_head_qmp_req(qmp);
      }
    free_qmp(qmp);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_event_free(int llid)
{
  t_qmp *cur = g_head_qmp;
  t_qmp_sub *sub;
  while(cur)
    {
    sub = find_qmp_sub(cur, llid);
    if (sub)
      free_qmp_sub(cur, sub);
    cur = cur->next;
    }
  sub = find_qmp_sub(NULL, llid);
  if (sub)
    free_qmp_sub(NULL, sub);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_begin_qemu_unix(char *name)
{
  alloc_qmp(name);
  qmp_dialog_alloc(name, qmp_conn_end);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_clean_all_data(void)
{
  t_qmp *next, *cur = g_head_qmp;
  t_qmp_sub *cursub, *nextsub;
  while(cur)
    {
    next = cur->next; 
    cursub = cur->head_qmp_sub;
    while (cursub)
      {
      nextsub = cursub->next;
      free_qmp_sub(cur, cursub);
      cursub = nextsub;
      }
    while(cur->head_qmp_req)
      free_head_qmp_req(cur);
    free_qmp(cur);
    cur = next;
    }
  cursub = g_head_all_qmp_sub;
  while (cursub)
    {
    nextsub = cursub->next;
    free_qmp_sub(cur, cursub);
    cursub = nextsub;
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
  if (qmp->waiting_for == waiting_for_capa_return)
    {
    if (!vm)
      KERR("%s", qmp->name);
    else
      doors_send_add_vm(get_doorways_llid(), 0, vm->kvm.name,
                        utils_get_qbackdoor_path(vm->kvm.vm_id));
    qmp->capa_acked = 1;
    qmp->waiting_for = waiting_for_nothing;
    }
  else if (qmp->waiting_for == waiting_for_reboot_return)
    {
    if ((llid) && (llid_trace_exists(llid)))
      {
      if (status)
        send_status_ko(llid, tid, "qmp reboot ko");
      else
        send_status_ok(llid, tid, "qmp reboot ok");
      }
    else
      {
      if (status)
        KERR("%s", qmp->name);
      }
    qmp->waiting_for = waiting_for_nothing;
    }
  else if (qmp->waiting_for == waiting_for_shutdown_return)
    {
    if ((llid) && (llid_trace_exists(llid)))
      {
      if (status)
        send_status_ko(llid, tid, "qmp shutdown ko");
      else
        send_status_ok(llid, tid, "qmp shutdown ok");
      }
    else
      {
      if (status)
        KERR("%s", qmp->name);
      }
    pname = (char *) clownix_malloc(MAX_NAME_LEN, 3);
    memset(pname, 0, MAX_NAME_LEN);
    strncpy(pname, qmp->name, MAX_NAME_LEN-1);
    clownix_timeout_add(250, timer_waiting_for_shutdown, pname, NULL, NULL);
    }
  else if (qmp->waiting_for == waiting_for_quit_return)
    {
    qmp->waiting_for = waiting_for_nothing;
    }
  else if (qmp->waiting_for == waiting_for_save_return)
    {
    if (status)
      {
      KERR("%s", qmp->name);
      if ((llid) && (llid_trace_exists(llid)))
        send_status_ko(llid, tid, "qmp save ko");
      }
    else
      {
      if (qmp_dialog_req(qmp->name, llid, tid, "", dialog_resp_save))
        {
        KERR("%s", qmp->name);
        if ((llid) && (llid_trace_exists(llid)))
          send_status_ko(llid, tid, "qmp save ko");
        }
      else
        qmp->waiting_for = waiting_for_save_block_job_ready;
      }
    }
  else if (qmp->waiting_for == waiting_for_save_end_return)
    {
    if (status)
      {
      KERR("%s", qmp->name);
      if ((llid) && (llid_trace_exists(llid)))
        send_status_ko(llid, tid, "qmp save ko");
      }
    else
      {
      if ((llid) && (llid_trace_exists(llid)))
        send_status_ok(llid, tid, "qmp save ok");
      }
    }
  else if (qmp->waiting_for == waiting_for_nothing)
    {
    }
  else
    KERR("%s %d", qmp->name, qmp->waiting_for);
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
      {
      if ((llid) && (llid_trace_exists(llid)))
        report_msg(llid, tid, name, resp, 0);
      }
    else
      {
      if ((!strncmp(resp, "{\"return\":", strlen("{\"return\":"))) ||
          (!strncmp(resp, "{\"timestamp\":", strlen("{\"timestamp\":"))))
        dialog_resp_return(qmp, llid, tid, 0);
      else if (!strncmp(resp, "{\"error\":", strlen("{\"error\":")))
        dialog_resp_return(qmp, llid, tid, -1);
      else if (!strncmp(resp, "timeout_qmp_resp", strlen("timeout_qmp_resp")))
        dialog_resp_return(qmp, llid, tid, -1);
      else
        KERR("%d %s %s %s", qmp->waiting_for, name, req, resp);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_msg_recv(char *name, char *msg)
{
  t_qmp *qmp = find_qmp(name);
  t_qmp_sub *cur;
  if (!qmp)
    KERR("%s", name);
  else
    {
    cur = qmp->head_qmp_sub;
    while(cur)
      {
      report_msg(cur->llid, cur->tid, name, msg, 0);
      cur = cur->next;
      }
    cur = g_head_all_qmp_sub;
    while(cur)
      {
      report_msg(cur->llid, cur->tid, name, msg, 0);
      cur = cur->next;
      }
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
  t_qmp *cur = g_head_qmp;
  t_qmp_req *req;
  t_vm *vm;
  while(cur)
    {
    req = cur->head_qmp_req;
    if (req)
      {
      if (cur->waiting_for == waiting_for_save_block_job_ready)
        {
        if ((req->llid) && llid_trace_exists(req->llid))
          send_qmp_resp(req->llid,req->tid,cur->name,"error saving block",-1);
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
            if (req->count > 10)
              {
              if ((req->llid) && llid_trace_exists(req->llid))
                send_qmp_resp(req->llid,req->tid,cur->name,"error timeout",-1);
              free_head_qmp_req(cur);
              }
            }
          }
        }
      }
    cur = cur->next;
    }
  clownix_timeout_add(10, timer_fifo_visit, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_init(void)
{
  g_head_qmp = NULL;
  g_head_all_qmp_sub = NULL;
  clownix_timeout_add(10, timer_fifo_visit, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void format_qmp_start_save(char *name, char *path, int stype, char *req)
{
  char type[MAX_NAME_LEN];
  memset(req, 0, MAX_RPC_MSG_LEN);
  memset(type, 0, MAX_NAME_LEN);
  if (stype)
    snprintf(type, MAX_NAME_LEN-1, "full");
  else
    snprintf(type, MAX_NAME_LEN-1, "top");
  snprintf(req, MAX_RPC_MSG_LEN-1, QMP_START_SAVE, name, name, path, type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_request_save_rootfs(char *name, char *path, int llid,
                             int tid, int stype)
{
  char req[MAX_RPC_MSG_LEN];
  t_qmp *qmp = find_qmp(name);
  if (!qmp)
    send_status_ko(llid, tid, "error qmp not found");
  else if (qmp->waiting_for != waiting_for_nothing)
    send_status_ko(llid, tid, "error qmp doing something");
  else
    {
    format_qmp_start_save(name, path, stype, req);
    alloc_tail_qmp_req(qmp, llid, tid, req);
    qmp->waiting_for = waiting_for_save_return;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_request_qemu_reboot(char *name, int llid, int tid)
{
  t_qmp *qmp = find_qmp(name);
  if (!qmp)
    send_status_ko(llid, tid, "error qmp not found");
  else if (qmp->waiting_for != waiting_for_nothing)
    send_status_ko(llid, tid, "error qmp doing something");
  else
    {
    alloc_tail_qmp_req(qmp, llid, tid, QMP_RESET);
    qmp->waiting_for = waiting_for_reboot_return;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_request_qemu_halt(char *name, int llid, int tid)
{
  t_qmp *qmp = find_qmp(name);
  if (!qmp)
    {
    if (llid)
      send_status_ko(llid, tid, "error qmp not found");
    }
  else if (qmp->waiting_for != waiting_for_nothing)
    {
    if (qmp->waiting_for != waiting_for_shutdown_return)
      KERR("%s  %d", name, qmp->waiting_for);
    if (llid)
      send_status_ko(llid, tid, "error qmp doing something");
    }
  else
    {
    alloc_tail_qmp_req(qmp, llid, tid, QMP_SHUTDOWN);
    qmp->waiting_for = waiting_for_shutdown_return;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_request_sub(char *name, int llid, int tid)
{
  t_qmp *qmp = NULL;
  if (name)
    qmp = find_qmp(name);
  if (!llid_trace_exists(llid))
    KERR("%s %d", name, llid);
  else if ((name) && (!qmp))
    send_qmp_resp(llid, tid, name, "error qmp not found", -1);
  else
    alloc_qmp_sub(qmp, llid, tid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_request_snd(char *name, int llid, int tid, char *msg)
{ 
  t_qmp *qmp = find_qmp(name);
  if (!qmp)
    send_qmp_resp(llid, tid, name, "error qmp not found", -1);
  else
    {
    if (qmp->waiting_for != waiting_for_nothing)
      send_qmp_resp(llid, tid, name, "error qmp doing something", -1);
    else
      alloc_tail_qmp_req(qmp, llid, tid, msg);
    }
}
/*--------------------------------------------------------------------------*/

