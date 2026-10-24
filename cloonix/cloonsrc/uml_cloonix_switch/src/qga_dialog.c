/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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

int get_running_in_crun(void);

#define MAX_QGA_MSG_LEN 3000
#define MAX_QGA_B64_LEN BASE64_ENCODE_OUT_SIZE(MAX_QGA_MSG_LEN)

/* EVERY REQUEST BELOW ENDS IN \n, AND THAT IS NOT COSMETIC. */
#define QGA_PING_BEATS   2400
/* Ping prodded every 20 beats while the agent is silent; unanswered prods
   bounded. */
#define QGA_PING_MAX_OUT 10
#define QGA_MAX_REBOOTS  2

#define QGA_PING "{\"execute\":\"guest-ping\"}\n"
#define QGA_SYNC "{\"execute\":\"guest-sync\", \"arguments\":{\"id\":%d}}\n"

#define QGA_OPEN_FILE "{\"execute\":\"guest-file-open\", \"arguments\":{\"path\":\"/agent_cloonix_launcher\",\"mode\":\"w+\"}}\n"

#define QGA_WRITE_FILE "{\"execute\":\"guest-file-write\", \"arguments\":{\"handle\":%d,\"buf-b64\":\"%s\"}}\n"

#define QGA_CLOSE_FILE "{\"execute\":\"guest-file-close\", \"arguments\":{\"handle\":%d}}\n"

#define QGA_EXEC_CHMOD "{\"execute\": \"guest-exec\", \"arguments\": { \"path\": \"chmod\", \"arg\": [\"+x\", \"/agent_cloonix_launcher\"],  \"capture-output\": true }}\n"

#define QGA_EXEC_FILE "{\"execute\": \"guest-exec\", \"arguments\": { \"path\": \"/agent_cloonix_launcher\", \"capture-output\": true }}\n"

#define QGA_EXEC_STATUS_FILE "{\"execute\": \"guest-exec-status\", \"arguments\": {\"pid\": %d}}\n"

#define QGA_SCRIPT_LAUNCH \
                "#!/bin/sh\n"\
                "mkdir -p /cloonixmnt/cnf_fs\n"\
                "mkdir -p /cloonixmnt/tmp\n"\
                "mount -t iso9660 /dev/sr0 /cloonixmnt/cnf_fs\n"\
                "mount -o remount,exec /dev/sr0\n"\
                "/cloonixmnt/cnf_fs/cloonix-agent"\
                " >/cloonixmnt/tmp/cloonix-agent.log 2>&1 &\n"\
                "/cloonixmnt/cnf_fs/cloonix-cxp-srv"\
                " >/cloonixmnt/tmp/cloonix-cxp-srv.log 2>&1 &\n"\
                "exit 0\n"

/* /dev/cd0 and cd9660, not /dev/sr0 and iso9660. */
#define QGA_BSD_SCRIPT_LAUNCH \
                "#!/bin/sh\n"\
                "mkdir -p /cloonixmnt/cnf_fs\n"\
                "mkdir -p /cloonixmnt/tmp\n"\
                "start_them() {\n"\
                "  /cloonixmnt/cnf_fs/cloonix-agent"\
                " >/cloonixmnt/tmp/cloonix-agent.log 2>&1 &\n"\
                "  /cloonixmnt/cnf_fs/cloonix-cxp-srv"\
                " >/cloonixmnt/tmp/cloonix-cxp-srv.log 2>&1 &\n"\
                "}\n"\
                "if [ -x /cloonixmnt/cnf_fs/cloonix-agent ]; then\n"\
                "  echo found cloonix-agent already mounted\n"\
                "  start_them\n"\
                "  exit 0\n"\
                "fi\n"\
                "for d in cd0 cd1 cd2 cd3; do\n"\
                "  [ -c /dev/$d ] || continue\n"\
                "  mount -t cd9660 /dev/$d /cloonixmnt/cnf_fs 2>/dev/null\n"\
                "  if [ -x /cloonixmnt/cnf_fs/cloonix-agent ]; then\n"\
                "    echo found cloonix-agent on $d\n"\
                "    start_them\n"\
                "    exit 0\n"\
                "  fi\n"\
                "  umount /cloonixmnt/cnf_fs 2>/dev/null\n"\
                "done\n"\
                "echo no cloonix-agent on any cd from cd0 to cd3\n"\
                "exit 1\n"

#define QGA_WIN_LAUNCHER "C:\\\\agent_cloonix_launcher.bat"

#define QGA_WIN_OPEN_FILE "{\"execute\":\"guest-file-open\", \"arguments\":"\
                          "{\"path\":\"" QGA_WIN_LAUNCHER "\","\
                          "\"mode\":\"wb\"}}\n"

#define QGA_WIN_EXEC_FILE "{\"execute\": \"guest-exec\", \"arguments\": "\
                          "{ \"path\": \"cmd.exe\", \"arg\": [\"/c\", \""\
                          QGA_WIN_LAUNCHER "\"], \"capture-output\": true }}\n"

#define QGA_WIN_SCRIPT_LAUNCH \
                "@echo off\r\n"\
                "for %%d in (D E F G H I J K) do (\r\n"\
                "  if exist %%d:\\cloonix-agent.exe (\r\n"\
                "    echo found cloonix-agent.exe on %%d:\r\n"\
                "    start /b %%d:\\cloonix-cxp-srv.exe >nul 2>&1\r\n"\
                "    start /b %%d:\\cloonix-agent.exe >nul 2>&1\r\n"\
                "    goto done\r\n"\
                "  )\r\n"\
                ")\r\n"\
                "echo no cloonix-agent.exe on any drive from D to K\r\n"\
                "exit /b 1\r\n"\
                ":done\r\n"\
                "exit /b 0\r\n"

int g_dbg_win_agent = 0;

static void b64_txt(char *in, char *out, int outmax)
{
  static const char *tbl =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int acc = 0, bits = 0, len = 0;
  char *p;
  out[0] = 0;
  while (*in && (len < outmax - 2))
    {
    if (*in == '=')
      break;
    p = strchr((char *) tbl, *in);
    if (!p)
      {
      in += 1;
      continue;
      }
    acc = (acc << 6) | (int) (p - tbl);
    bits += 6;
    if (bits >= 8)
      {
      bits -= 8;
      out[len] = (char) ((acc >> bits) & 0xFF);
      if ((out[len] == '\n') || (out[len] == '\r'))
        out[len] = ' ';
      len += 1;
      }
    in += 1;
    }
  out[len] = 0;
}

static int vm_is_windows(char *name)
{
  t_vm *vm = cfg_get_vm(name);
  if (!vm)
    return 0;
  return ((vm->kvm.vm_config_flags & VM_CONFIG_FLAG_WINDOWS) != 0);
}

static int vm_is_freebsd(char *name)
{
  t_vm *vm = cfg_get_vm(name);
  if (!vm)
    return 0;
  return ((vm->kvm.vm_config_flags & VM_CONFIG_FLAG_FREEBSD) != 0);
}

static char *qga_no_ping_hint(char *name)
{
  if (vm_is_windows(name))
    return "The guest may still be booting; once it is up, check the "
           "QEMU Guest Agent service and the virtio-serial driver in "
           "device manager";
  if (vm_is_freebsd(name))
    return "From the guest console: kldstat | grep virtio_console, then "
           "ls /dev/vtcon/ /dev/ttyV*, then ps auxww | grep qemu-ga";
  return "From the guest console: ls /dev/virtio-ports/, then "
         "ps auxww | grep qemu-ga";
}

static void qga_reboot_stuck_guest(char *name)
{
  if (vm_is_windows(name))
    KERR("WARNING %s: agent stuck, NOT rebooting, guest is windows. Look at "
         "C:\\cloonix-agent.log in the guest: it says which disc the agent "
         "was started from and whether the host ping ever arrived", name);
  else
    qmp_request_qemu_reboot(name);
}

typedef struct t_timeout_resp
{
  char name[MAX_NAME_LEN];
  int ref_id;
} t_timeout_resp;

typedef struct t_qrec
{
  char name[MAX_NAME_LEN];
  int  llid;
  int  reboot_count;
  int wait_before_first_call;
  int count_no_response;
  int request_reboot_done;
  int ping_launch;
  int ping_outstanding;
  int ping_prod_stopped;
  int sync_launch;
  int sync_rand;
  int file_open;
  int file_ref;
  int file_write;
  int file_close;
  int file_wait_count;
  int chmod_exec;
  int chmod_pid;
  int chmod_status;
  int chmod_status_poll;
  int chmod_wait_count;
  int file_exec;
  int sync_launch_count;
  int file_open_count;
  int file_exec_count;
  int chmod_exec_count;
  int file_pid;
  char last_where[256];
  int  stall_beats;
  int file_status;
  int file_status_poll;
  int up_vport_sent;
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

static int automate_rx_qga_msg(t_qrec *cur, char *msg);
static t_qrec *g_head_qrec;
static t_qrec *g_llid_qrec[CLOWNIX_MAX_CHANNELS];

#define QGA_STATUS_POLL_MAX 30
#define QGA_SCRIPT_POLL_MAX 400

static char *qga_where(t_qrec *q)
{
  static char txt[256];
  memset(txt, 0, sizeof(txt));
  snprintf(txt, sizeof(txt)-1,
           "ping=%d sync=%d(%d) open=%d(%d) write=%d close=%d "
           "chmod=%d(%d) exec=%d(%d)",
           q->ping_launch, q->sync_launch, q->sync_launch_count,
           q->file_open, q->file_open_count, q->file_write, q->file_close,
           q->chmod_exec, q->chmod_exec_count,
           q->file_exec, q->file_exec_count);
  return txt;
}

/* Not the answer of the current step: late answers to prods are counted
   off, not reported. */
static void qga_unexpected(t_qrec *q, char *msg)
{
  if ((q->ping_outstanding > 0) && (strstr(msg, "\"return\": {}")))
    {
    q->ping_outstanding -= 1;
    q->ping_prod_stopped = 0;
    KTRC("%s: a late answer to a ping sent again, %d still out",
         q->name, q->ping_outstanding);
    }
  else
    KERR("ERROR %s: qga answer does not fit the step (%s): %s",
         q->name, qga_where(q), msg);
}

static void reset_diag(t_qrec *q)
{
  q->ref_id += 1;
  memset(q->req, 0, MAX_QGA_MSG_LEN);
  memset(q->resp, 0, MAX_QGA_MSG_LEN);
  q->resp_offset = 0;
}

static t_qrec *get_qrec_with_llid(int llid)
{
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  return (g_llid_qrec[llid]);
}

static void set_qrec_with_llid(int llid, t_qrec *q)
{
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  g_llid_qrec[llid] = q;
}

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

static void flag_ping_to_cloonix_agent_ko(char *name)
{
  t_vm *vm;
  vm = cfg_get_vm(name);
  if ((vm) && (vm->kvm.vm_config_flags & VM_FLAG_CLOONIX_AGENT_PING_OK))
    {
    vm->kvm.vm_config_flags &= ~VM_FLAG_CLOONIX_AGENT_PING_OK;
    vm->kvm.ping_ok = 0;
    cfg_send_vm_evt_cloonix_ga_ping_ko(name);
    }
}

static void flag_ping_to_cloonix_agent_ok(char *name)
{
  t_vm *vm;
  vm = cfg_get_vm(name);
  if ((vm) &&
      (!(vm->kvm.vm_config_flags & VM_FLAG_CLOONIX_AGENT_PING_OK)))
    {
    vm->kvm.vm_config_flags |= VM_FLAG_CLOONIX_AGENT_PING_OK;
    vm->kvm.ping_ok = 1;
    cfg_send_vm_evt_cloonix_ga_ping_ok(name);
    }
}

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

static void timer_reload_qmp(void *data)
{
  char *name = (char *) data;
  /* The retry itself, four seconds after reload_upon_problem armed it and
     said so. */
  KTRC("%s: reconnecting qmp now", name);
  qmp_begin_qemu_unix(name, 0);
  clownix_free(name, __FUNCTION__);
}

static void qga_dialog_begin(char *name);

static void timer_redo_qga_dialog(void *data)
{
  char *pname = (char *) data;
  qga_dialog_begin(pname);
  clownix_free(pname, __FUNCTION__);
}

static void reload_upon_problem(char *name)
{
  char *pname;
  t_qrec *cur;
  pname = (char *) clownix_malloc(MAX_NAME_LEN, 6);
  memset(pname, 0, MAX_NAME_LEN);
  strncpy(pname, name, MAX_NAME_LEN-1);
  clownix_timeout_add(400, timer_reload_qmp, (void *) pname, NULL, NULL);
  KTRC("%s: qmp and qga will be reconnected in four seconds", name);
  cur = get_qrec_with_name(pname);
  if (cur)
    qrec_free(cur);
}

static void qemu_ga_fail(char *name)
{
  t_qrec *cur = get_qrec_with_name(name);
  t_qrec *prev, *next;
  char tmp_name[MAX_NAME_LEN];
  int time_end, llid, reboots;
  if (!cur)
    KOUT("ERROR %s", name);
  KERR("ERROR %s: the agent dialog is given up and the record dropped at "
       "step (%s)", name, qga_where(cur));
  memset(tmp_name, 0, MAX_NAME_LEN);
  strncpy(tmp_name, name, MAX_NAME_LEN-1);
  prev = cur->prev;
  next = cur->next;
  time_end = cur->time_end;
  llid = cur->llid;
  reboots = cur->reboot_count;
  memset(cur, 0, sizeof(t_qrec));
  strncpy(cur->name, tmp_name, MAX_NAME_LEN-1);
  cur->time_end = time_end;
  cur->prev = prev;
  cur->next = next;
  cur->llid = llid;
  cur->reboot_count = reboots;
}

static int qga_rx_cb(int llid, int fd)
{
  int len = -1, max;
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
      t_vm *vm = cfg_get_vm(cur->name);
      char *pname;
      /* Ordinary while qemu-ga settles or the guest goes: only running out
         of retries is an ERROR. */
      KTRC("%s: the qga socket read failed", cur->name);
      if ((vm) && (vm->qga_retry < 10))
        {
        vm->qga_retry += 1;
        KTRC("%s: reconnecting to qga, try %d of 10",
             cur->name, vm->qga_retry);
        pname = (char *) clownix_malloc(MAX_NAME_LEN, 6);
        memset(pname, 0, MAX_NAME_LEN);
        strncpy(pname, cur->name, MAX_NAME_LEN-1);
        clownix_timeout_add(100, timer_redo_qga_dialog, (void *) pname,
                            NULL, NULL);
        }
      else if (vm)
        KERR("ERROR %s: the qga socket failed %d times, giving up on the "
             "agent for this guest", cur->name, vm->qga_retry);
      qrec_free(cur);
      }
    else
      {
      if (len == max)
        {
        KERR("ERROR %s: a qga answer of %d bytes fills the buffer with no "
             "complete message, dropping it and resyncing", cur->name, len);
        reset_diag(cur);
        }
      else
        {
        /* Every complete message of this read (a waking agent answers many at
                   once); the rest waits for the next read. */
        char work[MAX_QGA_MSG_LEN+1];
        char kname[MAX_NAME_LEN];
        int used = cur->resp_offset + len;
        int kept;
        memcpy(work, cur->resp, used);
        work[used] = 0;
        memset(kname, 0, MAX_NAME_LEN);
        strncpy(kname, cur->name, MAX_NAME_LEN-1);
        ptr = used ? work : NULL;
        while ((ptr) && (qmp_braces_complete(ptr, &next_ptr)))
          {
          if (automate_rx_qga_msg(cur, ptr) == 0)
            reset_diag(cur);
          /* a step that gives up frees the record: nothing left to keep */
          cur = get_qrec_with_name(kname);
          if (!cur)
            return len;
          ptr = next_ptr;
          }
        memset(cur->resp, 0, MAX_QGA_MSG_LEN);
        cur->resp_offset = 0;
        if (ptr)
          {
          kept = used - (int) (ptr - work);
          if ((kept > 0) && (kept < MAX_QGA_MSG_LEN))
            {
            memcpy(cur->resp, ptr, kept);
            cur->resp_offset = kept;
            }
          else
            KERR("ERROR %s: %d bytes of an unfinished qga answer cannot be "
                 "kept, resyncing", cur->name, kept);
          }
        }
      }
    }
  return len;
}

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

static void timer_connect_qga(void *data)
{
  char *pname = (char *) data;
  char *qga_path;
  t_vm *vm = cfg_get_vm(pname);
  t_qrec *cur = get_qrec_with_name(pname);
  int fd, timer_restarted = 0;
  if (!cur)
    KTRC("connect timer for %s: its dialog record is already gone", pname);
  else if (cur->llid)
    KERR("ERROR %s: a connect timer fired while llid %d is already open",
         pname, cur->llid);
  else if (!vm)
    {
    KTRC("connect timer for %s: the vm is already gone", pname);
    qrec_free(cur);
    }
  else
    {
    qga_path = utils_get_qga_path(vm->kvm.vm_id);
    if (util_nonblock_client_socket_unix(qga_path, &fd))
      {
      /* The qga socket appears seconds into the boot: 60 tries of 500 ms. */
      cur->count_conn_timeout += 1;
      if ((cur->count_conn_timeout % 20) == 1)
        KTRC("%s: no qga socket yet, try %d of 60 (the guest is probably "
             "still booting)", pname, cur->count_conn_timeout);
      if (cur->count_conn_timeout > 60)
        {
        KERR("ERROR %s: no qga socket after thirty seconds, giving up on "
             "the agent for this guest", pname);
        qrec_free(cur);
        }
      else
        {
        clownix_timeout_add(50, timer_connect_qga,(void *) pname,
                            NULL, NULL);
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

static void qga_dialog_begin(char *name)
{
  t_vm *vm;
  char *pname;
  t_qrec *qrec;

  if (!name)
    KOUT("ERROR");
  if (strlen(name) < 1)
    KERR("ERROR qga dialog asked for an empty name");
  else
    {
    vm = cfg_get_vm(name);
    if (vm == NULL)
      KERR("WARNING %s: qga dialog asked for a kvm that is gone", name);
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
      t_vm *vm = cfg_get_vm(name);
      flag_ping_to_cloonix_agent_ko(name);
      llid = get_doorways_llid();
      doors_send_command(llid, 0, name, CLOONIX_DOWN_AND_NOT_RUNNING);
      if ((vm) && (cloonix_get_msec() - vm->reboot_asked_ms < REBOOT_QUIET_MS))
        KTRC("%s: the agent is silent after the reboot asked for, "
             "reconnecting qmp and qga", name);
      else
        KERR("WARNING %s: the agent inside the guest stopped answering, "
             "reconnecting qmp and qga", name);
      reload_upon_problem(name);
      }
    else
      KOUT("%d", backdoor_evt);
}

static void create_qga_script(char *str_qga, char *name)
{
  t_vm *vm = cfg_get_vm(name);
  if (!vm)
    KERR("ERROR %s", name);
  else
    {
    memset(str_qga, 0, MAX_QGA_MSG_LEN);
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_WINDOWS)
      strncpy(str_qga, QGA_WIN_SCRIPT_LAUNCH, MAX_QGA_MSG_LEN-1);
    else if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_FREEBSD)
      strncpy(str_qga, QGA_BSD_SCRIPT_LAUNCH, MAX_QGA_MSG_LEN-1);
    else
      strncpy(str_qga, QGA_SCRIPT_LAUNCH, MAX_QGA_MSG_LEN-1);
    }
}

static int automate_tx_qga_msg(t_qrec *cur)
{
  char req[MAX_QGA_B64_LEN + MAX_QGA_MSG_LEN];
  char out[MAX_QGA_B64_LEN];
  char str_qga[MAX_QGA_MSG_LEN];
  int llid;

  if ((cur->ping_launch == 0) || (cur->ping_launch == 2))
    {
    cur->count_no_response = 0;
    strncpy(req, QGA_PING, sizeof(req)-1);
    watch_tx(cur->llid, strlen(req), req);
    cur->ping_outstanding += 1;
    cur->ping_launch += 1;
    }
  else if ((cur->ping_launch == 1) || (cur->ping_launch == 3))
    {
    cur->count_no_response += 1;
    if ((cur->count_no_response%20) == 0 )
      {
      if (cur->ping_outstanding < QGA_PING_MAX_OUT)
        {
        strncpy(req, QGA_PING, sizeof(req)-1);
        watch_tx(cur->llid, strlen(req), req);
        cur->ping_outstanding += 1;
        }
      else if (cur->ping_prod_stopped == 0)
        {
        cur->ping_prod_stopped = 1;
        KERR("WARNING %s: %d guest-ping prods unanswered, no more sent "
             "until one comes back", cur->name, cur->ping_outstanding);
        }
      }
    if (cur->count_no_response > QGA_PING_BEATS)
      {
      if ((cur->request_reboot_done == 0) &&
          (cur->reboot_count < QGA_MAX_REBOOTS))
        {
        cur->count_no_response = 0;
        cur->request_reboot_done = 1;
        cur->reboot_count += 1;
        KERR("WARNING %s: no answer to the guest-ping at step %d, rebooting "
             "(attempt %d of %d)", cur->name, cur->ping_launch,
             cur->reboot_count, QGA_MAX_REBOOTS);
        qga_reboot_stuck_guest(cur->name);
        qemu_ga_fail(cur->name);
        }
      else if (cur->request_reboot_done == 0)
        {
        cur->count_no_response = 0;
        cur->request_reboot_done = 1;
        KERR("ERROR %s: no answer to the guest-ping after %d reboots, so it "
             "is not a slow boot. Leaving the guest up to be looked at: "
             "%s", cur->name, cur->reboot_count, qga_where(cur));
        }
      }
    }
  else if (cur->sync_launch == 0)
    {
    cur->sync_rand = rand();
    snprintf(req, sizeof(req)-1, QGA_SYNC, cur->sync_rand);
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
        KERR("WARNING %s: the agent never answered the sync, rebooting the "
             "guest", cur->name);
        qga_reboot_stuck_guest(cur->name);
        qemu_ga_fail(cur->name);
        }
      else
        KERR("ERROR %s: the agent still does not answer the sync and a "
             "reboot has already been asked for", cur->name);
      }
    if ((cur->sync_launch_count >= 50) &&
        ((cur->sync_launch_count % 50) == 0))
      KERR("WARNING SYNC WAIT %s: %s", cur->name, qga_where(cur));
    }
  else if ((cur->file_open == 0) && (cur->ping_launch == 4))
    {
    if (vm_is_windows(cur->name))
      strncpy(req, QGA_WIN_OPEN_FILE, sizeof(req)-1);
    else
      strncpy(req, QGA_OPEN_FILE, sizeof(req)-1);
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
        qga_reboot_stuck_guest(cur->name);
        qemu_ga_fail(cur->name);
        }
      else
        KERR("ERROR REBOOT %s", cur->name);
      }
    /* The whole state, not just the name */
    /* Not on the first beat. */
    if ((cur->file_open_count >= 50) && ((cur->file_open_count % 50) == 0))
      KERR("WARNING OPEN WAIT %s: %s", cur->name, qga_where(cur));
    }
  else if ((cur->file_open == 2) && (cur->file_write == 0))
    {
    memset(out, 0, MAX_QGA_B64_LEN);
    create_qga_script(str_qga, cur->name);
    base64_encode(str_qga, strlen(str_qga), out);
    snprintf(req, sizeof(req)-1, QGA_WRITE_FILE, cur->file_ref, out);
    watch_tx(cur->llid, strlen(req), req);
    cur->file_write = 1;
    cur->file_wait_count = 0;
    }
  else if ((cur->file_write == 1) || (cur->file_close == 1))
    {
    /* One request on the wire at a time: the agent answers in order and
       nothing here tells a late answer from the awaited one. */
    cur->file_wait_count += 1;
    if (cur->file_wait_count > 400)
      {
      if (cur->request_reboot_done == 0)
        {
        cur->file_wait_count = 0;
        cur->request_reboot_done = 1;
        KERR("WARNING %s: no answer to the guest-file-%s, rebooting the "
             "guest", cur->name, (cur->file_write == 1) ? "write" : "close");
        qga_reboot_stuck_guest(cur->name);
        qemu_ga_fail(cur->name);
        }
      else
        KERR("ERROR %s: still no answer to the guest-file-%s and a reboot "
             "has already been asked for", cur->name,
             (cur->file_write == 1) ? "write" : "close");
      }
    }
  else if (cur->file_close == 0)
    {
    snprintf(req, sizeof(req)-1, QGA_CLOSE_FILE, cur->file_ref);
    watch_tx(cur->llid, strlen(req), req);
    cur->file_close = 1;
    cur->file_wait_count = 0;
    }
  else if (cur->chmod_exec == 0)
    {
    if (vm_is_windows(cur->name))
      {
      cur->chmod_exec = 2;
      cur->chmod_status = 2;
      }
    else
      {
      snprintf(req, sizeof(req)-1, QGA_EXEC_CHMOD);
      watch_tx(cur->llid, strlen(req), req);
      cur->chmod_exec = 1;
      }
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
        qga_reboot_stuck_guest(cur->name);
        qemu_ga_fail(cur->name);
        }
      else
        KERR("ERROR REBOOT %s", cur->name);
      }
    if ((cur->chmod_exec_count >= 50) && ((cur->chmod_exec_count % 50) == 0))
      KERR("WARNING CHMOD WAIT %s: %s", cur->name, qga_where(cur));
    cur->file_status = 2;
    }
  else if ((cur->chmod_exec == 2) && (cur->chmod_status == 0))
    {
    snprintf(req, sizeof(req)-1, QGA_EXEC_STATUS_FILE, cur->chmod_pid);
    watch_tx(cur->llid, strlen(req), req);
    cur->chmod_status = 1;
    }
  else if ((cur->chmod_status == 1) && (cur->file_exec == 0) &&
           (cur->chmod_wait_count < 30))
    {
    cur->chmod_wait_count += 1;
    if (cur->chmod_wait_count == 30)
      {
      cur->chmod_status = 2;
      KERR("WARNING %s: the chmod was never confirmed by the guest, going "
           "ahead with the exec anyway", cur->name);
      }
    }
  else if (((cur->chmod_status == 2) ||
            (cur->chmod_wait_count >= 30)) && (cur->file_exec == 0))
    {
    if (vm_is_windows(cur->name))
      snprintf(req, sizeof(req)-1, QGA_WIN_EXEC_FILE);
    else
      snprintf(req, sizeof(req)-1, QGA_EXEC_FILE);
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
        qga_reboot_stuck_guest(cur->name);
        qemu_ga_fail(cur->name);
        }
      else
        KERR("ERROR REBOOT %s", cur->name);
      }
    /* One in fifty. */
    if ((cur->file_exec_count >= 50) && ((cur->file_exec_count % 50) == 0))
      KERR("WARNING %s: the launcher script has not finished after %d beats "
           "(reboot at 400). If this keeps happening the agent is not "
           "returning -- look at /cloonixmnt/tmp/cloonix-agent.log inside "
           "the guest", cur->name, cur->file_exec_count);
    }
  else if ((cur->file_exec == 2) && (cur->file_status == 0))
    {
    /* On windows the pings are released HERE and not at file_status 2. */
    if ((vm_is_windows(cur->name)) && (cur->up_vport_sent == 0))
      {
      cur->up_vport_sent = 1;
      llid = get_doorways_llid();
      doors_send_command(llid, 0, cur->name, CLOONIX_UP_VPORT_AND_RUNNING);
      }
    snprintf(req, sizeof(req)-1, QGA_EXEC_STATUS_FILE, cur->file_pid);
    watch_tx(cur->llid, strlen(req), req);
    cur->file_status = 1;
    }
  else if (cur->file_status == 2)
    {
    if (cur->up_vport_sent == 0)
      {
      cur->up_vport_sent = 1;
      llid = get_doorways_llid();
      doors_send_command(llid, 0, cur->name, CLOONIX_UP_VPORT_AND_RUNNING);
      }
    qrec_free(cur);
    return 1;
    }
  return 0;
}

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
        qga_unexpected(cur, msg);
      cur->file_status = 2;
      }
    else if (sscanf(ptr, "err-data\": \"%2999s\"", txt) == 1)
      {
      ptr = strchr(txt, '"');
      if (!ptr)
        qga_unexpected(cur, msg);
      cur->file_status = 2;
      }
    else
      qga_unexpected(cur, msg);
    }
  else if (sscanf(ptr, "out-data\": \"%2999s\"", txt) == 1)
    {
    ptr = strchr(txt, '"');
    if (!ptr)
      qga_unexpected(cur, msg);
    cur->file_status = 2;
    }
  else
    qga_unexpected(cur, msg);
}

static int automate_rx_qga_msg(t_qrec *cur, char *msg)
{
  char *ptr;
  int val, result = 0;
  
  if ((cur->ping_launch == 1) || (cur->ping_launch == 3))
    {
    ptr = strstr(msg, "return\":");
    if (!ptr)
      qga_unexpected(cur, msg);
    else
      {
      if (cur->ping_outstanding > 0)
        cur->ping_outstanding -= 1;
      cur->ping_prod_stopped = 0;
      cur->ping_launch += 1;
      }
    }

  else if ((cur->ping_launch == 4) && (cur->sync_launch == 1))
    {
    ptr = strstr(msg, "return\":");
    if (!ptr)
      qga_unexpected(cur, msg);
    else if (sscanf(ptr, "return\": %d", &val) == 1)
      {
      if (val != cur->sync_rand)
        KERR("ERROR NOT SYNC %s %s %d", cur->name, msg, cur->sync_rand);
      else
        cur->sync_launch += 1;
      }
    else
      qga_unexpected(cur, msg);
    }

  else if (cur->file_open == 1)
    {
    ptr = strstr(msg, "return\":");
    if (!ptr)
      qga_unexpected(cur, msg); 
    else if (sscanf(ptr, "return\": %d", &val) == 1)
      {
      cur->file_open = 2;
      cur->file_ref = val;
      }
    else
      qga_unexpected(cur, msg);
    }

  else if (cur->file_write == 1)
    {
    if (strstr(msg, "count\":"))
      cur->file_write = 2;
    else
      qga_unexpected(cur, msg);
    }

  else if (cur->file_close == 1)
    {
    if (strstr(msg, "return\": {}"))
      cur->file_close = 2;
    else
      qga_unexpected(cur, msg);
    }

  else if (cur->chmod_exec == 1)
    {
    ptr = strstr(msg, "pid\":");
    if (!ptr)
      {
      if (strstr(msg, "return\": {}"))
        KTRC("%s: an empty ack arrived while waiting for the exec pid",
             cur->name);
      else
        KERR("ERROR %s: no pid in the answer to guest-exec: %s",
             cur->name, msg);
      }
    else if (sscanf(ptr, "pid\": %d", &val) == 1)
      {
      cur->chmod_exec = 2;
      cur->chmod_pid = val;
      }
    else
      KERR("ERROR %s: the pid in the guest-exec answer is unreadable: %s",
           cur->name, msg);
    }

  else if (cur->chmod_status == 1)
    {
    ptr = strstr(msg, "exitcode\":");
    if (!ptr)
      {
      if (strstr(msg, "exited\": false"))
        {
        cur->chmod_status_poll += 1;
        if (cur->chmod_status_poll > QGA_STATUS_POLL_MAX)
          {
          KERR("WARNING %s: the chmod is still running after %d polls, "
               "going ahead with the exec anyway",
               cur->name, cur->chmod_status_poll);
          cur->chmod_status = 2;
          }
        else
          {
          cur->chmod_status = 0;
          KTRC("%s: the chmod has not exited yet, asking again", cur->name);
          }
        }
      else if (strstr(msg, "pid\":"))
        KERR("ERROR %s: an exec answer arrived while polling a status, the "
             "dialog is out of step: %s", cur->name, msg);
      else
        KERR("ERROR %s: no exitcode in the answer to guest-exec-status: %s",
             cur->name, msg);
      }
    else if (sscanf(ptr, "exitcode\": %d", &val) == 1)
      {
      if (val != 0)
        KERR("ERROR %s: the chmod inside the guest exited with %d: %s",
             cur->name, val, msg);
      else
        cur->chmod_status = 2;
      }
    else
      qga_unexpected(cur, msg);
    }

  else if (cur->file_exec == 1)
    {
    ptr = strstr(msg, "pid\":");
    if (!ptr)
      qga_unexpected(cur, msg);
    else if (sscanf(ptr, "pid\": %d", &val) == 1)
      {
      cur->file_exec = 2;
      cur->file_pid = val;
      }
    else
      qga_unexpected(cur, msg);
    }

  else if (cur->file_status == 1)
    {
    ptr = strstr(msg, "exitcode\":");
    if (!ptr)
      {
      if (strstr(msg, "exited\": false"))
        {
        cur->file_status_poll += 1;
        if ((vm_is_windows(cur->name)) &&
            (cur->file_status_poll > QGA_SCRIPT_POLL_MAX))
          {
          /* cmd.exe can take a minute to come back on windows and there
             is no reboot to fall back on: its exit is the only answer. */
          if ((cur->file_status_poll % QGA_SCRIPT_POLL_MAX) == 0)
            KERR("WARNING %s: the launcher script is still running after "
                 "%d polls, waiting for its exit",
                 cur->name, cur->file_status_poll);
          cur->file_status = 0;
          }
        else if (cur->file_status_poll > QGA_SCRIPT_POLL_MAX)
          {
          /* The script really is not coming back. */
          KERR("ERROR %s: the launcher script is still running after %d "
               "polls, giving up on it", cur->name, cur->file_status_poll);
          cur->file_status = 2;
          if (cur->request_reboot_done == 0)
            {
            cur->request_reboot_done = 1;
            qga_reboot_stuck_guest(cur->name);
            qemu_ga_fail(cur->name);
            }
          }
        else
          {
          cur->file_status = 0;
          KTRC("%s: the launcher script has not exited yet, asking again",
               cur->name);
          }
        }
      else if (cur->request_reboot_done == 0)
        {
        cur->request_reboot_done = 1;
        cur->file_status = 2;
        if (!strstr(msg, "exited\":"))
          KERR("ERROR %s: unusable answer to guest-exec-status and a reboot "
               "has already been asked for: %s", cur->name, msg);
        }
      else
        KERR("ERROR %s: a second unusable answer to guest-exec-status: %s",
             cur->name, msg);
      }
    else if (sscanf(ptr, "exitcode\": %d", &val) == 1)
      {
      if (val != 0)
        {
        char said[MAX_NAME_LEN];
        char b64[MAX_NAME_LEN];
        said[0] = 0;
        ptr = strstr(msg, "out-data\": \"");
        if (ptr && (sscanf(ptr, "out-data\": \"%63[^\"]", b64) == 1))
          b64_txt(b64, said, sizeof(said));
        if (vm_is_freebsd(cur->name))
          KERR("ERROR %s: the agent launcher exited %d saying \"%s\". Exit 1 "
               "is no cloonix-agent on any disc from cd0 to cd3: the insider "
               "iso was built with no freebsd agent on it",
               cur->name, val, said);
        else
          KERR("ERROR %s: the agent launcher exited %d saying \"%s\". Exit 1 "
               "is no cloonix-agent.exe on any disc from D to K: the insider "
               "iso was built with no windows agent on it, or windows sees "
               "its 8.3 name because the iso has no joliet",
               cur->name, val, said);
        }
      else
        {
        char said[MAX_NAME_LEN];
        char b64[MAX_NAME_LEN];
        char *o;
        said[0] = 0;
        o = strstr(msg, "out-data\": \"");
        if (o && (sscanf(o, "out-data\": \"%63[^\"]", b64) == 1))
          b64_txt(b64, said, sizeof(said));
        KTRC_ON(g_dbg_win_agent,
                "%s: the agent launcher exited 0 saying \"%s\"",
                cur->name, said);
        rx_qga_status(cur, msg);
        }
      }
    else
      qga_unexpected(cur, msg);
    }
  return result;
}

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

/* Report vms whose agent never woke. */
static void timer_no_backdoor_watch(void)
{
  static int beats;
  int nb = 0;
  t_vm *vm;
  beats += 1;
  if ((beats % 100) != 0)
    return;
  vm = cfg_get_first_vm(&nb);
  while (vm)
    {
    /* A guest started with --no_qemu_ga has no backdoor channel, so it can
       never connect and there is nothing to report. */
    if (vm->kvm.vm_config_flags & VM_CONFIG_FLAG_NO_QEMU_GA)
      {
      vm = vm->next;
      continue;
      }
    /* A finished dialog frees its qrec: the flag says the agent answered. */
    if ((!get_qrec_with_name(vm->kvm.name)) &&
        (!(vm->kvm.vm_config_flags & VM_FLAG_CLOONIX_AGENT_PING_OK)))
      {
      vm->qga_silent_beats += 1;
      /* 18 beats is three minutes, not one. */
      if ((vm->qga_silent_beats == 18) ||
          ((vm->qga_silent_beats > 18) &&
           ((vm->qga_silent_beats % 30) == 0)))
        KERR("WARNING kvm %s up %d seconds and its agent has not "
             "connected. %s",
             vm->kvm.name, vm->qga_silent_beats * 10,
             qmp_greeting_seen(vm->kvm.name)?
             "qemu answers on qmp, so the guest is running: the image has "
             "no cloonix agent, or the agent is not starting inside it":
             "qemu has not answered on qmp either, so the guest may still "
             "be booting");
      }
    else
      vm->qga_silent_beats = 0;
    vm = vm->next;
    }
}

static void timer_heartbeat_qga(void *data)
{
 t_qrec *next, *cur = g_head_qrec;
 timer_no_backdoor_watch();
  while(cur)
    {
    next = cur->next;
    if (qga_dialog_is_ok(cur->name))
      {
      if (cur->wait_before_first_call > 10)
        {
        /* The last step of a successful dialog frees this record, so the
           stall watchdog below must not touch cur afterwards. */
        if (automate_tx_qga_msg(cur))
          {
          cur = next;
          continue;
          }
        }
      else
        cur->wait_before_first_call += 1;
      }
    {
    char *where = qga_where(cur);
    if (strcmp(cur->last_where, where))
      {
      snprintf(cur->last_where, sizeof(cur->last_where)-1, "%s", where);
      cur->stall_beats = 0;
      }
    else
      {
      cur->stall_beats += 1;
      /* A guest booting under load answers guest-ping after a minute or
         two: the first word is at 60 s, then one a minute. */
      if ((cur->stall_beats == 600) ||
          ((cur->stall_beats > 600) && ((cur->stall_beats % 600) == 0)))
        {
        if ((cur->ping_launch == 1) && (cur->sync_launch == 0))
          KERR("WARNING agent startup for %s has not moved for %ds, stuck "
               "at: %s. guest-ping got no answer: the guest is still "
               "booting, or its qemu guest agent is not on this port. %s",
               cur->name, cur->stall_beats / 10, where,
               qga_no_ping_hint(cur->name));
        else
          KERR("WARNING agent startup for %s has not moved for %ds, stuck "
               "at: %s", cur->name, cur->stall_beats / 10, where);
        }
      }
    }
    cur = next;
    }
  clownix_timeout_add(10, timer_heartbeat_qga, NULL, NULL, NULL);
}

void qga_dialog_init(void)
{
  g_head_qrec = NULL;
  memset(g_llid_qrec, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_qrec *));
  clownix_timeout_add(200, timer_heartbeat_qga, NULL, NULL, NULL);
}
