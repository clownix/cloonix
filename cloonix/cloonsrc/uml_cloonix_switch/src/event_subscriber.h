/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
enum
{
  sub_evt_min,
  sub_evt_topo,
  topo_small_event,
  sub_evt_trace_qemu,
  sub_evt_trace_json_cli,
  sub_evt_trace_crun,
  sub_evt_trace_nspace,
  sub_evt_eventfull,
  sub_evt_slowperiodic,
  sub_evt_max,
};

typedef struct t_trace_evt
{
  char *name;
  char *txt;
  int kind;
}t_trace_evt;

typedef struct t_small_evt
{
  char name[MAX_NAME_LEN];
  char param1[MAX_PATH_LEN];
  char param2[MAX_PATH_LEN];
  int evt;
}t_small_evt;

int  event_trace_sub_evt(int kind);
void event_trace(int kind, char *name, char *txt);
void event_trace_json_init(void);
void event_subscriber_init(void);
void event_subscribe(int sub_evt, int llid, int tid);
void event_unsubscribe(int sub_evt, int llid);
void event_subscriber_send(int sub_evt, void *info);

