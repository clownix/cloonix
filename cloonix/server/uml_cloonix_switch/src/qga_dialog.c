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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "util_sock.h"
#include "cfg_store.h"
#include "utils_cmd_line_maker.h"
#include "llid_trace.h"
#include "qga_dialog.h"
#include "event_subscriber.h"
#include "machine_create.h"
#include "suid_power.h"
#include "base64.h"
#include "doors_rpc.h"
#include "doorways_mngt.h"
#include "qmp.h"
#include "file_read_write.h"

#define MAX_QGA_MSG_LEN 3000

#define QGA_PING "{\"execute\":\"guest-ping\"}"
#define QGA_SYNC "{\"execute\":\"guest-sync\", \"arguments\":{\"id\":%d}}"

#define QGA_OPEN_FILE "{\"execute\":\"guest-file-open\", \"arguments\":{\"path\":\"/cloonix_launcher\",\"mode\":\"w+\"}}"

#define QGA_WRITE_FILE "{\"execute\":\"guest-file-write\", \"arguments\":{\"handle\":%d,\"buf-b64\":\"%s\"}}"

#define QGA_CLOSE_FILE "{\"execute\":\"guest-file-close\", \"arguments\":{\"handle\":%d}}"

#define QGA_EXEC_CHMOD "{\"execute\": \"guest-exec\", \"arguments\": { \"path\": \"chmod\", \"arg\": [\"+x\", \"/cloonix_launcher\"],  \"capture-output\": true }}"

#define QGA_EXEC_FILE "{\"execute\": \"guest-exec\", \"arguments\": { \"path\": \"/cloonix_launcher\", \"capture-output\": true }}"

#define QGA_EXEC_STATUS_FILE "{\"execute\": \"guest-exec-status\", \"arguments\": {\"pid\": %d}}"


#define QGA_SCRIPT_LAUNCH \
                "#!/bin/sh\n"\
                "mkdir -p /mnt/cloonix_config_fs\n"\
                "mount /dev/sr0 /mnt/cloonix_config_fs\n"\
                "mount -o remount,exec /dev/sr0\n"\
                "/mnt/cloonix_config_fs/cloonix-agent\n"\
                "/mnt/cloonix_config_fs/cloonix-dropbear-sshd\n"


typedef struct t_timeout_resp
{
  char name[MAX_NAME_LEN];
  int ref_id;
} t_timeout_resp;

typedef struct t_qrec
{
  char name[MAX_NAME_LEN];
  int  llid;
  int wait_before_first_call;
  int count_no_response;
  int request_reboot_done;
  int ping_launch;
  int sync_launch;
  int sync_rand;
  int file_open;
  int file_ref;
  int file_write;
  int file_close;
  int chmod_exec;
  int chmod_pid;
  int chmod_status;
  int file_exec;
  int sync_launch_count;
  int file_open_count;
  int file_exec_count;
  int chmod_exec_count;
  int file_pid;
  int file_status;
  int file_qga_end;
  char req[MAX_QGA_MSG_LEN];
  char resp[MAX_QGA_MSG_LEN];
  int resp_offset;
  int ref_id;
  int count_conn_timeout;
  int time_end;
  struct t_qrec *prev;
  struct t_qrec *next;
} t_qrec;
/*--------------------------------------------------------------------------*/

static int resp_message_braces_complete(char *whole_rx, char **next_rx);
static int automate_rx_qga_msg(t_qrec *cur, char *msg);
static t_qrec *g_head_qrec;
static t_qrec *g_llid_qrec[CLOWNIX_MAX_CHANNELS];

/****************************************************************************/
static void reset_diag(t_qrec *q)
{
  q->ref_id += 1;
  memset(q->req, 0, MAX_QGA_MSG_LEN);
  memset(q->resp, 0, MAX_QGA_MSG_LEN);
  q->resp_offset = 0;
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
static void qrec_alloc(char *name)
{
  t_qrec *q;
  if (strlen(name) <= 1)
    KERR("ERROR ");
  else
    {
    q = (t_qrec *) clownix_malloc(sizeof(t_qrec), 6);
    memset(q, 0, sizeof(t_qrec));
    strncpy(q->name, name, MAX_NAME_LEN-1);
    q->time_end = util_get_max_tempo_fail();
    q->prev = NULL;
    if (g_head_qrec)
      g_head_qrec->prev = q;
    q->next = g_head_qrec;
    g_head_qrec = q;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qrec_free(t_qrec *qrec)
{
  t_qrec *q;
  char name[MAX_NAME_LEN];
  memset(name, 0, MAX_NAME_LEN);
  strncpy(name, qrec->name, MAX_NAME_LEN-1);
  if (qrec->llid)
    {
    q = get_qrec_with_llid(qrec->llid);
    if (qrec != q)
      KERR("ERROR %s", name);
    set_qrec_with_llid(qrec->llid, NULL);
    llid_trace_free(qrec->llid, 0, __FUNCTION__);
    }
  if (qrec->prev)
    qrec->prev->next = qrec->next;
  if (qrec->next)
    qrec->next->prev = qrec->prev;
  if (qrec == g_head_qrec)
    g_head_qrec = qrec->next;
  clownix_free(qrec, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void flag_ping_to_cloonix_agent_ko(char *name)
{
  t_vm *vm;
  vm = cfg_get_vm(name);
  if ((vm) && (vm->kvm.vm_config_flags & VM_FLAG_CLOONIX_AGENT_PING_OK))
    {
    vm->kvm.vm_config_flags &= ~VM_FLAG_CLOONIX_AGENT_PING_OK;
    cfg_send_vm_evt_cloonix_ga_ping_ko(name);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void flag_ping_to_cloonix_agent_ok(char *name)
{
  t_vm *vm;
  vm = cfg_get_vm(name);
  if ((vm) &&
      (!(vm->kvm.vm_config_flags & VM_FLAG_CLOONIX_AGENT_PING_OK)))
    {
    vm->kvm.vm_config_flags |= VM_FLAG_CLOONIX_AGENT_PING_OK;
    cfg_send_vm_evt_cloonix_ga_ping_ok(name);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static t_qrec *get_qrec_with_name(char *name)
{
  t_qrec *cur = g_head_qrec;
  if (strlen(name) <= 1)
    {
    cur = NULL;
    KERR("ERROR  ");
    }
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
static void timer_reload_qmp(void *data)
{
  char *name = (char *) data;
  KERR("ERROR timer_reload_qmp, %s", name);
  qmp_begin_qemu_unix(name, 0);
  clownix_free(name, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void reload_upon_problem(char *name)
{
  char *pname;
  t_qrec *cur;
  pname = (char *) clownix_malloc(MAX_NAME_LEN, 6);
  memset(pname, 0, MAX_NAME_LEN);
  strncpy(pname, name, MAX_NAME_LEN-1);
  clownix_timeout_add(400, timer_reload_qmp, (void *) pname, NULL, NULL);
  KERR("ERROR LAUNCH timer_reload_qmp, %s", name);
  cur = get_qrec_with_name(pname);
  if (cur)
    qrec_free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qemu_ga_fail(char *name)
{
  t_qrec *cur = get_qrec_with_name(name);
  t_qrec *prev, *next;
  char tmp_name[MAX_NAME_LEN];
  int time_end, llid; 
  if (!cur)
    KOUT("ERROR %s", name);
  KERR("FAILED qemu_ga_fail %s", name);
  memset(tmp_name, 0, MAX_NAME_LEN);
  strncpy(tmp_name, name, MAX_NAME_LEN);
  prev = cur->prev;
  next = cur->next;
  time_end = cur->time_end;
  llid = cur->llid;
  memset(cur, 0, sizeof(t_qrec));
  strncpy(cur->name, tmp_name, MAX_NAME_LEN-1);
  cur->time_end = time_end;
  cur->prev = prev;
  cur->next = next;
  cur->llid = llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int qga_rx_cb(int llid, int fd)
{
  int len, max;
  char *buf, *ptr, *next_ptr;
  t_qrec *cur = get_qrec_with_llid(llid);
  if (!cur)
    KERR("ERROR  ");
  else
    {
    max = MAX_QGA_MSG_LEN - cur->resp_offset;
    buf = cur->resp + cur->resp_offset;
    len = util_read(buf, max, fd);
    if (len < 0)
      {
      KERR("ERROR %s", cur->name);
      qrec_free(cur);
      }
    else
      {
      if (len == max)
        KERR("ERROR %s %s %d", cur->name, cur->resp, len);
      else
        {
        ptr = cur->resp;
        if (ptr)
          {
          if (resp_message_braces_complete(ptr, &next_ptr))
            {
            if (automate_rx_qga_msg(cur, ptr) == 0)
              reset_diag(cur);
            }
          else
            cur->resp_offset += len;
          }
        }
      }
    }
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qga_err_cb (int llid, int err, int from)
{
  t_qrec *cur = get_qrec_with_llid(llid);
  if (!cur)
    KERR("ERROR ");
  else
    {
    qrec_free(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_connect_qga(void *data)
{
  char *pname = (char *) data;
  char *qga_path;
  t_vm *vm = cfg_get_vm(pname);
  t_qrec *cur = get_qrec_with_name(pname);
  int fd, timer_restarted = 0;
  if (!cur)
    KERR("ERROR %s", pname);
  else if (cur->llid)
    KERR("ERROR %s %d", pname, cur->llid);
  else if (!vm)
    {
    KERR("ERROR %s", pname);
    qrec_free(cur);
    }
  else
    {
    qga_path = utils_get_qga_path(vm->kvm.vm_id);
    if (util_nonblock_client_socket_unix(qga_path, &fd))
      {
      KERR("WARNING util_nonblock_client_socket_unix fail %s", pname);
      cur->count_conn_timeout += 1;
      if (cur->count_conn_timeout > 10)
        {
        KERR("ERROR %s", pname);
        qrec_free(cur);
        }
      else
        {
        clownix_timeout_add(1, timer_connect_qga,(void *) pname, NULL, NULL);
        timer_restarted = 1;
        }
      }
    else
      {
      if (fd < 0)
        KOUT(" ");
      cur->llid = msg_watch_fd(fd, qga_rx_cb, qga_err_cb, "qga");
      if (cur->llid == 0)
        KOUT(" ");
      llid_trace_alloc(cur->llid,"QMP", 0, 0, type_llid_trace_unix_qmp);
      set_qrec_with_llid(cur->llid, cur);
      }
    }
  if (!timer_restarted)
    clownix_free(pname, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void qga_dialog_begin(char *name)
{
  t_vm *vm;
  char *pname;
  t_qrec *qrec;

  if (!name)
    KOUT("ERROR");
  if (strlen(name) < 1)
    KERR("ERROR ");
  else
    {
    vm = cfg_get_vm(name);
    if (vm == NULL)
      KERR("ERROR %s", name);
    else
      {
      qrec = get_qrec_with_name(name);
      if (!qrec)
        {
        qrec_alloc(name);
        qrec = get_qrec_with_name(name);
        if (!qrec)
          KOUT("ERROR");
        pname = (char *) clownix_malloc(MAX_NAME_LEN, 6);
        memset(pname, 0, MAX_NAME_LEN);
        strncpy(pname, name, MAX_NAME_LEN-1);
        clownix_timeout_add(1, timer_connect_qga, (void *) pname, NULL, NULL);
        }
      else
        {
        KERR("ERROR  %s exists", name);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void qga_event_backdoor(char *name, int backdoor_evt)
{
  int llid;
    if (backdoor_evt == backdoor_evt_connected)
      {
      qga_dialog_begin(name);
      }
    else if (backdoor_evt == backdoor_evt_disconnected)
      {
      KERR("ERROR backdoor_disconnected %s", name);
      }
    else if (backdoor_evt == backdoor_evt_ping_ok)
      {
      flag_ping_to_cloonix_agent_ok(name);
      }
    else if (backdoor_evt == backdoor_evt_ping_ko)
      {
      flag_ping_to_cloonix_agent_ko(name);
      llid = get_doorways_llid();
      doors_send_command(llid, 0, name, CLOONIX_DOWN_AND_NOT_RUNNING);
      KERR("ERROR backdoor agent disconnected %s", name);
      reload_upon_problem(name);
      }
    else
      KOUT("%d", backdoor_evt);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void automate_tx_qga_msg(t_qrec *cur)
{
  char req[MAX_QGA_MSG_LEN];
  char out[MAX_QGA_MSG_LEN];
  char *content = out;
  int llid;

  if ((cur->ping_launch == 0) || (cur->ping_launch == 2))
    {
    cur->count_no_response = 0;
    strncpy(req, QGA_PING, MAX_QGA_MSG_LEN-1);
    watch_tx(cur->llid, strlen(req), req);
    cur->ping_launch += 1;
    }
  else if ((cur->ping_launch == 1) || (cur->ping_launch == 3))
    {
    cur->count_no_response += 1;
    if ((cur->count_no_response%20) == 0 )
      {
      strncpy(req, QGA_PING, MAX_QGA_MSG_LEN-1);
      watch_tx(cur->llid, strlen(req), req);
      }
    if (cur->count_no_response > 800)
      {
      if (cur->request_reboot_done == 0)
        { 
        cur->count_no_response = 0;
        cur->request_reboot_done = 1;
        KERR("WARNING!!! BAD QEMU GUEST AGENT START %s %d REBOOT", cur->name, cur->ping_launch);
        qmp_request_qemu_reboot(cur->name);
        qemu_ga_fail(cur->name);
        }
      else
        KERR("ERROR BAD QEMU GUEST AGENT START %s", cur->name);
      }
    }
  else if (cur->sync_launch == 0)
    {
    cur->sync_rand = rand();
    snprintf(req, MAX_QGA_MSG_LEN-1, QGA_SYNC, cur->sync_rand);
    watch_tx(cur->llid, strlen(req), req);
    cur->sync_launch += 1;
    }
  else if (cur->sync_launch == 1)
    {
    cur->sync_launch_count += 1;
    if (cur->sync_launch_count > 400)
      {
      if (cur->request_reboot_done == 0)
        { 
        cur->sync_launch_count = 0;
        cur->request_reboot_done = 1;
        KERR("WARNING REBOOT %s", cur->name);
        qmp_request_qemu_reboot(cur->name);
        qemu_ga_fail(cur->name);
        }
      else
        KERR("ERROR REBOOT %s", cur->name);
      }
    KERR("WARNING SYNC WAIT %s", cur->name);
    }
  else if ((cur->file_open == 0) && (cur->ping_launch == 4))
    {
    strncpy(req, QGA_OPEN_FILE, MAX_QGA_MSG_LEN-1);
    watch_tx(cur->llid, strlen(req), req);
    cur->file_open = 1;
    }
  else if (cur->file_open == 1)
    {
    cur->file_open_count += 1;
    if(cur->file_open_count > 400)
      {
      if (cur->request_reboot_done == 0)
        {
        cur->file_open_count = 0;
        cur->request_reboot_done = 1;
        KERR("WARNING REBOOT %s", cur->name);
        qmp_request_qemu_reboot(cur->name);
        qemu_ga_fail(cur->name);
        }
      else
        KERR("ERROR REBOOT %s", cur->name);
      }
    KERR("WARNING OPEN WAIT %s", cur->name);
    }
  else if ((cur->file_open == 2) && (cur->file_write == 0))
    {
    memset(out, 0, MAX_QGA_MSG_LEN);
    base64_encode(QGA_SCRIPT_LAUNCH, strlen(QGA_SCRIPT_LAUNCH), out);
    snprintf(req, MAX_QGA_MSG_LEN-1, QGA_WRITE_FILE, cur->file_ref, content);
    watch_tx(cur->llid, strlen(req), req);
    cur->file_write = 1;
    }
  else if (cur->file_close == 0)
    {
    snprintf(req, MAX_QGA_MSG_LEN-1, QGA_CLOSE_FILE, cur->file_ref);
    watch_tx(cur->llid, strlen(req), req);
    cur->file_close = 1;
    }
  else if (cur->chmod_exec == 0)
    {
    snprintf(req, MAX_QGA_MSG_LEN-1, QGA_EXEC_CHMOD);
    watch_tx(cur->llid, strlen(req), req);
    cur->chmod_exec = 1;
    }
  else if (cur->chmod_exec == 1)
    {
    cur->chmod_exec_count += 1;
    if (cur->chmod_exec_count > 400)
      {
      if (cur->request_reboot_done == 0)
        {
        cur->chmod_exec_count = 0;
        cur->request_reboot_done = 1;
        KERR("WARNING REBOOT %s", cur->name);
        qmp_request_qemu_reboot(cur->name);
        qemu_ga_fail(cur->name);
        }
      else
        KERR("ERROR REBOOT %s", cur->name);
      }
    KERR("WARNING CHMOD WAIT %s", cur->name);
    cur->file_status = 2;
    }
  else if ((cur->chmod_exec == 2) && (cur->chmod_status == 0))
    {
    snprintf(req, MAX_QGA_MSG_LEN-1, QGA_EXEC_STATUS_FILE, cur->chmod_pid);
    watch_tx(cur->llid, strlen(req), req);
    cur->chmod_status = 1;
    }
  else if ((cur->chmod_exec = 2) && (cur->file_exec == 0))
    {
    snprintf(req, MAX_QGA_MSG_LEN-1, QGA_EXEC_FILE);
    watch_tx(cur->llid, strlen(req), req);
    cur->file_exec = 1;
    }
  else if (cur->file_exec == 1)
    {
    cur->file_exec_count += 1;
    if (cur->file_exec_count > 400) 
      {
      if (cur->request_reboot_done == 0)
        {
        cur->file_exec_count = 0;
        cur->request_reboot_done = 1;
        KERR("WARNING REBOOT %s", cur->name);
        qmp_request_qemu_reboot(cur->name);
        qemu_ga_fail(cur->name);
        }
      else
        KERR("ERROR REBOOT %s", cur->name);
      }
    KERR("WARNING EXEC WAIT %s", cur->name);
    }
  else if ((cur->file_exec == 2) && (cur->file_status == 0))
    {
    snprintf(req, MAX_QGA_MSG_LEN-1, QGA_EXEC_STATUS_FILE, cur->file_pid);
    watch_tx(cur->llid, strlen(req), req);
    cur->file_status = 1;
    }
  else if (cur->file_status == 2)
    {
    llid = get_doorways_llid();
    doors_send_command(llid, 0, cur->name, CLOONIX_UP_VPORT_AND_RUNNING);
    qrec_free(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_qga_status(t_qrec *cur, char *msg)
{
  char *ptr;
  char txt[MAX_QGA_MSG_LEN];
  ptr = strstr(msg, "out-data\":");
  if (!ptr)
    {
    ptr = strstr(msg, "err-data\":");
    if (!ptr)
      {
      ptr = strstr(msg, "exited\":");
      if (!ptr)
        KERR("ERROR %s %s", cur->name, msg);
      cur->file_status = 2;
      }
    else if (sscanf(ptr, "err-data\": \"%s\"", txt) == 1)
      {
      ptr = strchr(txt, '"');
      if (!ptr)
        KERR("ERROR %s %s", cur->name, msg);
      cur->file_status = 2;
      }
    else
      KERR("ERROR %s %s", cur->name, msg);
    }
  else if (sscanf(ptr, "out-data\": \"%s\"", txt) == 1)
    {
    ptr = strchr(txt, '"');
    if (!ptr)
      KERR("ERROR %s %s", cur->name, msg);
    cur->file_status = 2;
    }
  else
    KERR("ERROR %s %s", cur->name, msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int automate_rx_qga_msg(t_qrec *cur, char *msg)
{
  char *ptr;
  int val, result = 0;
  
  if ((cur->ping_launch == 1) || (cur->ping_launch == 3))
    {
    ptr = strstr(msg, "return\":");
    if (!ptr)
      KERR("ERROR %s %s", cur->name, msg);
    else
      cur->ping_launch += 1;
    }

  else if ((cur->ping_launch == 4) && (cur->sync_launch == 1))
    {
    ptr = strstr(msg, "return\":");
    if (!ptr)
      KERR("ERROR %s %s", cur->name, msg);
    else if (sscanf(ptr, "return\": %d", &val) == 1)
      {
      if (val != cur->sync_rand)
        KERR("ERROR NOT SYNC %s %s %d", cur->name, msg, cur->sync_rand);
      else
        cur->sync_launch += 1;
      }
    else
      KERR("ERROR %s %s", cur->name, msg);
    }

  else if (cur->file_open == 1)
    {
    ptr = strstr(msg, "return\":");
    if (!ptr)
      KERR("ERROR %s %s", cur->name, msg); 
    else if (sscanf(ptr, "return\": %d", &val) == 1)
      {
      cur->file_open = 2;
      cur->file_ref = val;
      }
    else
      KERR("ERROR %s %s", cur->name, msg);
    }

  else if (cur->chmod_exec == 1)
    {
    ptr = strstr(msg, "pid\":");
    if (!ptr)
      KERR("ERROR %s %s", cur->name, msg);
    else if (sscanf(ptr, "pid\": %d", &val) == 1)
      {
      cur->chmod_exec = 2;
      cur->chmod_pid = val;
      }
    else
      KERR("ERROR %s %s", cur->name, msg);
    }

  else if (cur->chmod_status == 1)
    {
    ptr = strstr(msg, "exitcode\":");
    if (!ptr)
      KERR("ERROR %s %s", cur->name, msg);
    else if (sscanf(ptr, "exitcode\": %d", &val) == 1)
      {
      if (val != 0)
        KERR("ERROR %s %s", cur->name, msg);
      else
        cur->chmod_status = 2;
      }
    else
      KERR("ERROR %s %s", cur->name, msg);
    }

  else if (cur->file_exec == 1)
    {
    ptr = strstr(msg, "pid\":");
    if (!ptr)
      KERR("ERROR %s %s", cur->name, msg);
    else if (sscanf(ptr, "pid\": %d", &val) == 1)
      {
      cur->file_exec = 2;
      cur->file_pid = val;
      }
    else
      KERR("ERROR %s %s", cur->name, msg);
    }

  else if (cur->file_status == 1)
    {
    ptr = strstr(msg, "exitcode\":");
    if (!ptr)
      {
      if (cur->request_reboot_done == 0)
        {
        cur->request_reboot_done = 1;
        cur->file_status = 2;
        if (!strstr(msg, "exited\":"))
          KERR("ERROR UNRECOV NOT REBOOTING %s %s", cur->name, msg);
        }
      else
        KERR("ERROR TWO RELOADING %s %s", cur->name, msg);
      }
    else if (sscanf(ptr, "exitcode\": %d", &val) == 1)
      {
      if (val != 0)
        KERR("ERROR %s %s", cur->name, msg);
      else
        rx_qga_status(cur, msg);
      }
    else
      KERR("ERROR %s %s", cur->name, msg);
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
static int qga_dialog_is_ok(char *name)
{
  t_vm *vm = cfg_get_vm(name);
  int result = 0;
  t_qrec *cur = get_qrec_with_name(name);
  if (cur && vm && (vm->vm_to_be_killed==0))
    {
    if ((cur->llid) && (msg_exist_channel(cur->llid)))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_heartbeat_qga(void *data)
{
 t_qrec *next, *cur = g_head_qrec;
  while(cur)
    {
    next = cur->next;
    if (qga_dialog_is_ok(cur->name))
      {
      if (cur->wait_before_first_call > 10)
        automate_tx_qga_msg(cur);
      else
        cur->wait_before_first_call += 1;
      }
    cur = next;
    }
  clownix_timeout_add(10, timer_heartbeat_qga, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void qga_dialog_init(void)
{
  g_head_qrec = NULL;
  memset(g_llid_qrec, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_qrec *));
  clownix_timeout_add(200, timer_heartbeat_qga, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
