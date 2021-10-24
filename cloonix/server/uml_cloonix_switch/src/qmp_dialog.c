/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "util_sock.h"
#include "cfg_store.h"
#include "utils_cmd_line_maker.h"
#include "llid_trace.h"
#include "qmp_dialog.h"
#include "qmp.h"
#include "event_subscriber.h"
#include "machine_create.h"
#include "suid_power.h"


typedef struct t_timeout_resp
{
  char name[MAX_NAME_LEN];
  int ref_id;
} t_timeout_resp;

typedef struct t_qrec
{
  char name[MAX_NAME_LEN];
  int  state;
  int  llid;
  t_end_conn end_cb;
  char req[MAX_RPC_MSG_LEN];
  int resp_llid;
  int resp_tid;
  t_dialog_resp resp_cb;
  char resp[MAX_RPC_MSG_LEN];
  int resp_offset;
  int ref_id;
  int count_conn_timeout;
  struct t_qrec *prev;
  struct t_qrec *next;
} t_qrec;
/*--------------------------------------------------------------------------*/

static t_qrec *g_head_qrec;
static t_qrec *g_llid_qrec[CLOWNIX_MAX_CHANNELS];

/****************************************************************************/
static t_qrec *get_qrec_with_name(char *name)
{
  t_qrec *cur = g_head_qrec;
  while(cur)
    {
    if (!strcmp(name, cur->name))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_qrec *get_qrec_with_llid(int llid)
{
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  return (g_llid_qrec[llid]);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void set_qrec_with_llid(int llid, t_qrec *q)
{
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  g_llid_qrec[llid] = q;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qrec_alloc(char *name, t_end_conn end_cb)
{
  t_qrec *q = (t_qrec *) clownix_malloc(sizeof(t_qrec), 6);
  memset(q, 0, sizeof(t_qrec));
  strncpy(q->name, name, MAX_NAME_LEN-1);
  q->end_cb = end_cb;
  q->prev = NULL;
  if (g_head_qrec)
    g_head_qrec->prev = q;
  q->next = g_head_qrec; 
  g_head_qrec = q;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qrec_free(t_qrec *qrec, int transmit)
{
  t_qrec *q;
  char name[MAX_NAME_LEN];
  t_end_conn end_cb = qrec->end_cb;
  memset(name, 0, MAX_NAME_LEN);
  strncpy(name, qrec->name, MAX_NAME_LEN-1);
  if (qrec->llid)
    {
    q = get_qrec_with_llid(qrec->llid);
    if (qrec != q)
      KERR("%s", name); 
    set_qrec_with_llid(qrec->llid, NULL);
    llid_trace_free(qrec->llid, 0, __FUNCTION__);
    }
  if (qrec->prev)
    qrec->prev->next = qrec->next;
  if (qrec->next)
    qrec->next->prev = qrec->prev;
  if (qrec == g_head_qrec) 
    g_head_qrec = qrec->next;
  if (transmit)
    end_cb(name);
  clownix_free(qrec, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int req_message_braces_complete(char *whole_rx)
{
  int j, i=0, count=0, result = 0;
  if (strchr(whole_rx, '{'))
    {
    do
      {
      j = whole_rx[i];
      if (j == '{')
        count += 1;
      if (j == '}')
        count -= 1;
      i += 1;
      } while (j);
    if (count == 0)
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int resp_message_braces_complete(char *whole_rx, char **next_rx)
{
  int i, count, result = 0;
  char c;
  char *ptr, *ptr_first = strchr(whole_rx, '{');
  *next_rx = NULL;
  if (whole_rx[0] != '{')
    KERR("ERROR: %s", whole_rx);
  if (ptr_first)
    {
    i = 0;
    count = 1;
    ptr = ptr_first+1;
    do
      {
      c = ptr[i];
      if (c == '{')
        count += 1;
      if (c == '}')
        count -= 1;
      if (count == 0)
        {
        result = 1;
        ptr[i] = 0;
        *next_rx = strchr(&(ptr[i+1]), '{');
        break;
        }
      i += 1;
      } while (c);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void reset_diag(t_qrec *q)
{
  q->ref_id += 1;
  memset(q->req, 0, MAX_RPC_MSG_LEN);
  memset(q->resp, 0, MAX_RPC_MSG_LEN);
  q->resp_offset = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void call_cb(t_qrec *q, char *resp, int is_timeout)
{
  t_dialog_resp cb;
  if ((strlen(q->req)) && (q->resp_cb))
    {
    if (is_timeout)
      {
      strcpy(q->resp, "timeout_qmp_resp");
      cb = q->resp_cb;
      q->resp_cb = NULL;
      cb(q->name, q->resp_llid, q->resp_tid, q->req, q->resp);
      }
    else
      {
      if ((!strncmp(resp, "{\"return\":", strlen("{\"return\":"))) ||
          (!strncmp(resp, "{\"error\":", strlen("{\"error\":"))) ||
          (!strncmp(resp, "{\"timestamp\":", strlen("{\"timestamp\":"))))
        {
        cb = q->resp_cb;
        q->resp_cb = NULL;
        cb(q->name, q->resp_llid, q->resp_tid, q->req, resp);
        }
      else
        KERR("TOLOOKINTO %s %d %s %s", q->name, q->resp_llid, q->req, resp);
      }
    } 
  else if ((strlen(q->req)==0) && (q->resp_cb))
    {
    cb = q->resp_cb;
    q->resp_cb = NULL;
    cb(q->name, q->resp_llid, q->resp_tid, q->req, resp);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int qmp_rx_cb(int llid, int fd)
{
  int len, max;
  char *buf, *ptr, *next_ptr;
  t_qrec *qrec = get_qrec_with_llid(llid);
  if (!qrec)
    KERR(" ");
  else
    {
    max = MAX_RPC_MSG_LEN - qrec->resp_offset;
    buf = qrec->resp + qrec->resp_offset;
    len = util_read(buf, max, fd);
    if (len < 0)
      {
      KERR("%s", qrec->name);
      qrec_free(qrec, 1);
      }
    else
      {
      if (len == max)
        KERR("%s %s %d", qrec->name, qrec->resp, len);
      else
        {
        ptr = qrec->resp;
        while (ptr)
          {
          if (resp_message_braces_complete(ptr, &next_ptr))
            {
            if ((!strncmp(ptr, "{\"timestamp\":", 
                          strlen("{\"timestamp\":"))) &&
                (strstr(ptr, "JOB_STATUS_CHANGE")))
              {
              //KERR("DROP %s", ptr);
              ptr = next_ptr;
              }
            else
              {
              qmp_msg_recv(qrec->name, qrec->resp);
              call_cb(qrec, ptr, 0);
              ptr = NULL;
              reset_diag(qrec);
              }
            }
          else
            {
            ptr = NULL;
            qrec->resp_offset += len;
            }
          }
        }
      }
    }
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qmp_err_cb (int llid, int err, int from)
{
  t_qrec *qrec = get_qrec_with_llid(llid);
  if (!qrec)
    KERR(" ");
  else
    qrec_free(qrec, 1);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void timer_connect_qmp(void *data)
{
  char *pname = (char *) data;
  char *qmp_path;
  int fd, timer_restarted = 0;;
  t_vm *vm;
  t_small_evt vm_evt;
  t_qrec *qrec = get_qrec_with_name(pname);
  if (!qrec)
    return;
  if (qrec->llid)
    KERR("%s %d", pname, qrec->llid);
  else
    {
    vm = cfg_get_vm(pname);
    if (!vm)
      {
      KERR("%s", pname);
      qrec_free(qrec, 1);
      }
    else
      {
      if (!suid_power_get_pid(vm->kvm.vm_id))
        {
        qrec->count_conn_timeout += 1;
        if (qrec->count_conn_timeout > 200)
          {
          KERR("ERROR FAILED START KVM TIMEOUT %s", pname);
          }
        else
          {
          if (qrec->count_conn_timeout == 80)
            KERR("WARNING 1 FAILED START KVM TIMEOUT %s", pname);
          if (qrec->count_conn_timeout == 130)
            KERR("WARNING 2 FAILED START KVM TIMEOUT %s", pname);
          clownix_timeout_add(20,timer_connect_qmp,(void *) pname,NULL,NULL);
          timer_restarted = 1;
          }
        }
      else
        {
        qmp_path = utils_get_qmp_path(vm->kvm.vm_id);
        if (util_nonblock_client_socket_unix(qmp_path, &fd))
          {
          qrec->count_conn_timeout += 1;
          if (qrec->count_conn_timeout > 150)
            {
            KERR("%s", pname);
            machine_death(pname, error_death_noovstime);
            qrec_free(qrec, 1);
            }
          else
            {
            clownix_timeout_add(20,timer_connect_qmp,(void *) pname,NULL,NULL);
            timer_restarted = 1;
            }
          }
        else
          {
          if (fd < 0)
            KOUT(" ");
          qrec->llid = msg_watch_fd(fd, qmp_rx_cb, qmp_err_cb, "qmp");
          if (qrec->llid == 0)
            KOUT(" ");
          llid_trace_alloc(qrec->llid,"QMP",0,0,type_llid_trace_unix_qmonitor);
          set_qrec_with_llid(qrec->llid, qrec);
          vm->qmp_conn = 1;
          memset(&vm_evt, 0, sizeof(t_small_evt));
          strncpy(vm_evt.name, vm->kvm.name, MAX_NAME_LEN-1);
          vm_evt.evt = vm_evt_qmp_conn_ok;
          event_subscriber_send(topo_small_event, (void *) &vm_evt);
          }
        }
      }
    }
  if (!timer_restarted)
    clownix_free(pname, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_resp_qmp(void *data)
{
  t_timeout_resp *timeout = (t_timeout_resp *) data;
  t_qrec *qrec = get_qrec_with_name(timeout->name);
  if ((qrec) && (qrec->ref_id == timeout->ref_id))
    {
    call_cb(qrec, qrec->resp, 1);
    reset_diag(qrec);
    }
  clownix_free(timeout, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int qmp_dialog_req(char *name, int llid, int tid, char *req, t_dialog_resp cb)
{
  t_vm *vm;
  int result = -1;
  t_qrec *qrec = get_qrec_with_name(name);
  t_timeout_resp *timeout;
  vm = cfg_get_vm(name);
  if (!vm)
    {
    KERR("%s", name);
    qrec_free(qrec, 1);
    return result;
    }
  else
    {
    if (vm->vm_to_be_killed)
      {
      qrec_free(qrec, 1);
      KERR("%s", name);
      return result;
      }
    }
  if (!qrec)
    KERR("%s", name);
  else if (qrec->resp_cb)
    KERR("%s", name);
  else if (strlen(req) >= MAX_RPC_MSG_LEN)
    KERR("%s %d %d", name, (int)strlen(req), MAX_RPC_MSG_LEN);
  else if ((!qrec->llid) || (!msg_exist_channel(qrec->llid)))  
    {
    KERR("%s", name);
    qmp_dialog_free(name);
    }
  else if ((strlen(req)) && (!req_message_braces_complete(req)))
    {
    KERR("%s", name);
    cb(name, llid, tid, req, "invalid braces syntax"); 
    }
  else
    {
    if (strlen(req))
      {
      timeout = (t_timeout_resp *) clownix_malloc(sizeof(t_timeout_resp), 6);
      memset(timeout, 0, sizeof(t_timeout_resp));
      strncpy(timeout->name, name, MAX_NAME_LEN-1);
      timeout->ref_id = qrec->ref_id;
      clownix_timeout_add(1500, timeout_resp_qmp, (void *) timeout, NULL, NULL);
      strncpy(qrec->req, req, MAX_RPC_MSG_LEN-1);
      qmp_msg_send(qrec->name, req);
      watch_tx(qrec->llid, strlen(req), req);
      }
    qrec->resp_llid = llid;
    qrec->resp_tid = tid;
    qrec->resp_cb = cb;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_dialog_free(char *name)
{
  t_qrec *qrec = get_qrec_with_name(name);
  if (!qrec)
    KERR("%s", name);
  else
    qrec_free(qrec, 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_dialog_alloc(char *name, t_end_conn cb)
{
  char *pname;
  t_qrec *qrec;
  if ((!name) || (!cb))
    KOUT("%p %p", name, cb);
  if (strlen(name) < 1)
    KERR(" ");
  else 
    {
    qrec = get_qrec_with_name(name);
    if (qrec)
      KERR("%s exists", name);
    else
      {
      qrec_alloc(name, cb);
      pname = (char *) clownix_malloc(MAX_NAME_LEN, 6);
      memset(pname, 0, MAX_NAME_LEN);
      strncpy(pname, name, MAX_NAME_LEN-1);
      clownix_timeout_add(10, timer_connect_qmp, (void *) pname, NULL, NULL);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qmp_dialog_init(void)
{
  g_head_qrec = NULL;
  memset(g_llid_qrec, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_qrec *));
}
/*--------------------------------------------------------------------------*/
