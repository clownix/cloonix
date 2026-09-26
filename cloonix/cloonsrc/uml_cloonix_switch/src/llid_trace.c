/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "llid_trace.h"
#include "eventfull.h"
#include "slowperiodic.h"
#include "file_read_write.h"
#include "qmp.h"
#include "hop_event.h"
#include "suid_power.h"
#include "gui_server_mngt.h"
#include "ovs_snf.h"
#include "ovs_c2c.h"
#include "ovs_a2b.h"
#include "ovs_nat_main.h"
#include "proxymous.h"
#include "ovs.h"
typedef struct t_event_to_llid
{
  int llid;
  int tid;
  struct t_event_to_llid *prev;
  struct t_event_to_llid *next;
} t_event_to_llid;
typedef struct t_vm_to_llid
{
  int llid;
  struct t_vm_to_llid *prev;
  struct t_vm_to_llid *next;
} t_vm_to_llid;
typedef struct t_llid_trace_data
{
  char name[MAX_PATH_LEN];
  int vm_id;
  int guest_fd; 
  int type;
  int num;
  int son_connected; 
  int count_beat;
  int sub_event_tab[sub_evt_max];
  struct t_llid_trace_data *prev;
  struct t_llid_trace_data *next;
} t_llid_trace_data;
static t_llid_trace_data *head_llid_trace_data;
static int32_t trace_fd_qty[type_llid_max];
static t_llid_trace_data *llid_trace_data[CLOWNIX_MAX_CHANNELS];
static t_vm_to_llid *vm_to_llid[MAX_VM];
static t_event_to_llid *event_to_llid[sub_evt_max];

static void insert_llid_in_event_chain(int event, int llid, int tid,
                                       t_llid_trace_data *llid_data)
{
  t_event_to_llid *cur;
  cur = (t_event_to_llid *) clownix_malloc(sizeof(t_event_to_llid), 24);
  memset(cur, 0, sizeof(t_event_to_llid));
  cur->llid = llid;
  cur->tid = tid;
  cur->next = event_to_llid[event];
  if (event_to_llid[event])
    event_to_llid[event]->prev = cur;
  event_to_llid[event] = cur;
  llid_data->sub_event_tab[event] = 1;
}

static void extract_llid_from_event_chain(int event, int llid, 
                                          t_llid_trace_data *llid_data)
{
  t_event_to_llid *cur;
  cur = event_to_llid[event];
  while(cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;
    }
  if (!cur)
    KOUT("%d %d", event, llid);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == event_to_llid[event])
    {
    if (cur->next)
      event_to_llid[event] = cur->next;
    else
      {
      event_to_llid[event] = NULL;
      llid_data->sub_event_tab[event] = 0; 
      }
    }
  clownix_free(cur, __FUNCTION__);
}

static t_event_to_llid *get_event_to_llid(int llid, int event)
{
  t_event_to_llid *cur = event_to_llid[event];
  while(cur)
    {
    if (cur->llid == llid)
      break;
     cur = cur->next;
     }
  return cur;
}

void llid_set_event2llid(int llid, int event, int tid)
{
  t_llid_trace_data *cur;
  if ((event<=sub_evt_min) || (event>=sub_evt_max))
    KOUT("%d %d", event, llid);
  if ((llid <1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  if (!get_event_to_llid(llid, event))
    {
    cur = llid_trace_data[llid];
    if (!cur)
      KOUT("%d", llid);
    insert_llid_in_event_chain(event, llid, tid, cur);
    }
}

static t_event_to_llid *get_evt2llid(t_event_to_llid *head, int prev_llid)
{
  t_event_to_llid *cur = head;
  t_event_to_llid *result = NULL;
  if (cur)
    {
    if (prev_llid == 0)
      result = cur;
    else
      {
      while(cur)
        {
        if (cur->llid == prev_llid)
          break;
        cur = cur->next;
        }
      if (!cur)
        KOUT("%d",  prev_llid);
      if (cur->next)
        result = cur->next;
      }
    }
  return result;
}

int llid_get_event2llid(int event, int prev_llid, int *llid, int *tid)
{
  int result = 0;
  t_llid_trace_data *llid_data;
  t_event_to_llid *cur;
  *llid = 0;
  *tid = 0;
  if ((event<=sub_evt_min) || (event>=sub_evt_max))
    KOUT("%d", event);
  if ((prev_llid <0) || (prev_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", prev_llid);
  if (prev_llid)
    {
    llid_data = llid_trace_data[prev_llid];
    if (!llid_data)
      KOUT("%d", prev_llid);
    }
  cur = event_to_llid[event];
  cur = get_evt2llid(cur, prev_llid);
  if (cur)
      {
      *llid = cur->llid;
      *tid = cur->tid;
      result = 1;
      }
  return result;
}

void llid_unset_event2llid(int llid, int event)
{
  t_llid_trace_data *cur;
  if ((event<=sub_evt_min) || (event>=sub_evt_max))
    KOUT("%d %d", event, llid);
  if ((llid <1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  cur = llid_trace_data[llid];
  if (!cur)
    KOUT("%d", llid);
  extract_llid_from_event_chain(event, llid, cur);
}

static void insert_llid_in_vm_id_chain(int vm_id, int llid)
{
  t_vm_to_llid *cur;
  cur = (t_vm_to_llid *) clownix_malloc(sizeof(t_vm_to_llid), 24);
  memset(cur, 0, sizeof(t_vm_to_llid));
  cur->llid = llid;
  cur->next = vm_to_llid[vm_id];
  if (vm_to_llid[vm_id])
    vm_to_llid[vm_id]->prev = cur;
  vm_to_llid[vm_id] = cur;
}

static void extract_llid_from_vm_id_chain(int vm_id, int llid)
{
  t_vm_to_llid *cur;
  if ((vm_id <1) || (vm_id >= MAX_VM))
    KOUT("%d", vm_id);
  cur = vm_to_llid[vm_id];
  while(cur)
    {
    if (cur->llid == llid)
      break;
    cur = cur->next;          
    }
  if (!cur)
    KOUT("%d %d", vm_id, llid);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == vm_to_llid[vm_id])    
    vm_to_llid[vm_id] = cur->next;
  clownix_free(cur, __FUNCTION__);
}

int32_t cfg_get_trace_fd_qty(int type)
{
  return trace_fd_qty[type];
}

static void trace_fd_insert(int type)
{
  trace_fd_qty[type_llid_trace_all] += 1;
  trace_fd_qty[type] += 1;
}

static void trace_fd_extract(int type)
{
  trace_fd_qty[type_llid_trace_all] -= 1;
  trace_fd_qty[type] -= 1;
  if (trace_fd_qty[type] < 0)
    KOUT(" ");
  if (trace_fd_qty[type_llid_trace_all] < 0)
    KOUT(" ");
}

void llid_trace_alloc(int llid, char *name, int vm_id, int guest_fd, int type)
{
  if ((llid <1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  if(llid_trace_data[llid])
    KOUT("%d %d %d", llid, guest_fd, type);
  llid_trace_data[llid] = 
  (t_llid_trace_data *) clownix_malloc(sizeof(t_llid_trace_data), 23);
  memset(llid_trace_data[llid], 0, sizeof(t_llid_trace_data));
  strcpy(llid_trace_data[llid]->name, name);
  llid_trace_data[llid]->type = type;
  llid_trace_data[llid]->vm_id = vm_id;
  llid_trace_data[llid]->guest_fd = guest_fd;
  trace_fd_insert(type);
  if (vm_id)
    insert_llid_in_vm_id_chain(vm_id, llid);

  if (head_llid_trace_data)  
    head_llid_trace_data->prev = llid_trace_data[llid];
  llid_trace_data[llid]->next = head_llid_trace_data;
  head_llid_trace_data = llid_trace_data[llid]; 

}

void llid_trace_free(int llid, int from_clone, const char* fct)
{
  int i;
  t_llid_trace_data *cur;

  if ((llid <1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%s %d", fct, llid);

  ovs_llid_closed(llid, from_clone);
  proxymous_llid_closed(llid, from_clone);
  ovs_c2c_llid_closed(llid, from_clone);
  ovs_nat_main_llid_closed(llid, from_clone);
  ovs_a2b_llid_closed(llid, from_clone);

  suid_power_llid_closed(llid, from_clone);
  gui_server_llid_closed(llid, from_clone);
  ovs_snf_llid_closed(llid, from_clone);

  cur = llid_trace_data[llid];
  if ((!from_clone) && (!cur))
    KERR("WARNING UNKNOWN LLID TRACE FREE %d %s", llid, fct);
  if (cur)
    {
    trace_fd_extract(cur->type);
    if (cur->vm_id)
      extract_llid_from_vm_id_chain(cur->vm_id, llid);
    for (i=sub_evt_min; i<sub_evt_max; i++)
      if (cur->sub_event_tab[i])
        llid_unset_event2llid(llid, i);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);  

    if (llid_trace_data[llid]->prev)
      llid_trace_data[llid]->prev->next = llid_trace_data[llid]->next;
    if (llid_trace_data[llid]->next)
      llid_trace_data[llid]->next->prev = llid_trace_data[llid]->prev;
    if (head_llid_trace_data == llid_trace_data[llid])  
      head_llid_trace_data = llid_trace_data[llid]->next;

    llid_trace_data[llid] = NULL;
    clownix_free(cur, __FUNCTION__);
    }
  else
    KERR("WARNING LLID NOT KNOWN %d %s", llid, fct);

  if (!from_clone)
    {
    for (i=0; i<CLOWNIX_MAX_CHANNELS; i++)
      {
      if (msg_exist_channel(i))
        {
        cur = llid_trace_data[i];
        if (!cur)
          KERR("WARNING LLID NOT KNOWN %d", i);
        }
      }
    }
}

int llid_trace_exists(int llid)
{
  int result = 0;
  if ((llid > 0) && (llid < CLOWNIX_MAX_CHANNELS))
    {
    if (llid_trace_data[llid])
      result = 1;
    }
  return result;
}

void llid_count_beat(void)
{
  t_llid_trace_data *cur = head_llid_trace_data;
  while(cur)
    {
    cur->count_beat += 1;
    cur = cur->next;
    }
}

void llid_free_all_llid(void)
{
  int i;
  t_llid_trace_data *cur;
  for (i=0; i<CLOWNIX_MAX_CHANNELS; i++)
    {
    cur = llid_trace_data[i];
    if (cur)
      llid_trace_free(i, 1, __FUNCTION__);
    }
}

void llid_trace_vm_delete(int vm_id)
{
  t_vm_to_llid *cur, *next; 
  if ((vm_id <1) || (vm_id >= MAX_VM))
    KOUT("%d", vm_id);
  cur = vm_to_llid[vm_id];
  while(cur)
    {
    next = cur->next;
    llid_trace_free(cur->llid, 0, __FUNCTION__);
    cur = next;
    } 
}

void llid_trace_free_if_exists(int llid)
{
  t_llid_trace_data *cur;
  if ((llid <1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  cur = llid_trace_data[llid];
  if (cur)
    llid_trace_free(llid, 1, __FUNCTION__);
}

void llid_trace_init(void)
{
  int i;
  for (i=0; i<type_llid_max; i++)
    trace_fd_qty[i] = 0;
  memset(llid_trace_data,0,CLOWNIX_MAX_CHANNELS*sizeof(t_llid_trace_data *));
  memset(vm_to_llid, 0, MAX_VM*sizeof(t_vm_to_llid *));
  memset(event_to_llid, 0, sub_evt_max*sizeof(t_event_to_llid *));
}

