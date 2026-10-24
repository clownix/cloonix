/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "heartbeat.h"
#include "system_callers.h"
#include "pid_clone.h"
#include "llid_trace.h"
#include "lib_topo.h"

static void subscribing_llid_send(int sub_evt, void *info)
{
  int llid, prev_llid, tid;
  if (llid_get_event2llid(sub_evt, 0, &llid, &tid))
    {
    if (!msg_exist_channel(llid))
      KOUT("%d", sub_evt);
    do
      {
      switch (sub_evt)
        {
        case sub_evt_topo:
          send_event_topo(llid, tid, (t_topo_info *)info);
        break;
        case topo_small_event:
          send_topo_small_event(llid, tid, ((t_small_evt *)info)->name, 
                                ((t_small_evt *)info)->param1, 
                                ((t_small_evt *)info)->param2,
                                ((t_small_evt *)info)->evt);
        break;
        case sub_evt_trace_qemu:
        case sub_evt_trace_json_cli:
        case sub_evt_trace_crun:
        case sub_evt_trace_nspace:
          send_trace(llid, tid, ((t_trace_evt *)info)->kind,
                     ((t_trace_evt *)info)->name,
                     ((t_trace_evt *)info)->txt);
        break;
        default:
          KOUT(" ");
        }
      prev_llid = llid;
      } while(llid_get_event2llid(sub_evt, prev_llid, &llid, &tid));
    }
  if (sub_evt == sub_evt_topo)
    topo_free_topo((t_topo_info *)info);
}   

int event_trace_sub_evt(int kind)
{
  int result;
  switch (kind)
    {
    case trace_kind_qemu:
      result = sub_evt_trace_qemu;
    break;
    case trace_kind_json_cli:
      result = sub_evt_trace_json_cli;
    break;
    case trace_kind_crun:
      result = sub_evt_trace_crun;
    break;
    case trace_kind_nspace:
      result = sub_evt_trace_nspace;
    break;
    default:
      result = -1;
    }
  return result;
}

void event_trace(int kind, char *name, char *txt)
{
  t_trace_evt trace;
  int sub_evt = event_trace_sub_evt(kind);
  if (sub_evt == -1)
    KERR("ERROR unknown trace kind %d", kind);
  else if (!i_am_a_clone())
    {
    trace.kind = kind;
    trace.name = name;
    trace.txt = txt;
    subscribing_llid_send(sub_evt, (void *) &trace);
    }
}

static void json_trace_cb(int llid, int is_tx, char *buf, int len)
{
  static char txt[MAX_TRACE_LEN];
  static int busy = 0;
  char name[MAX_NAME_LEN];
  int sub_llid, tid;
  if (busy)
    return;
  if (!llid_get_event2llid(sub_evt_trace_json_cli, 0, &sub_llid, &tid))
    return;
  if (len >= MAX_TRACE_LEN)
    {
    len = MAX_TRACE_LEN - 1;
    memcpy(txt, buf, len);
    memcpy(txt + len - 11, "<truncated>", 11);
    }
  else
    memcpy(txt, buf, len);
  txt[len] = 0;
  snprintf(name, MAX_NAME_LEN-1, "%s llid %d", is_tx?"tx":"rx", llid);
  busy = 1;
  event_trace(trace_kind_json_cli, name, txt);
  busy = 0;
}

void event_trace_json_init(void)
{
  doors_io_basic_json_trace_set(json_trace_cb);
}

void event_subscribe(int sub_evt, int llid, int tid)
{
  llid_set_event2llid(llid, sub_evt, tid);
}

void event_unsubscribe(int sub_evt, int llid)
{ 
  llid_unset_event2llid(llid, sub_evt);
}

void event_subscriber_send(int sub_evt, void *info)
{
  if ((sub_evt <= 0) || (sub_evt >= sub_evt_max))
    KOUT(" ");
  if (!i_am_a_clone())           
    {
    subscribing_llid_send(sub_evt, info);
    }
}

void event_subscriber_init(void)
{
}

