/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "json_min.h"
#include "doors_rpc.h"

#define DOORS_MAGIC_LEN 8
#define MAX_CLOWNIX_BOUND_LEN 64

enum {
  bnd_min = 0,
  bnd_info,
  bnd_event,
  bnd_command,
  bnd_status,
  bnd_add_vm,
  bnd_del_vm,
  bnd_max,
};

static char *sndbuf = NULL;
static char *wrapbuf = NULL;
static char  bound_list[bnd_max][MAX_CLOWNIX_BOUND_LEN];

/* Transmit with magic prefix */
static void my_msg_mngt_tx(int llid, int len, char *buf)
{
  int wlen;
  if (len > MAX_SIZE_BIGGEST_MSG - 1000)
    KOUT("%d %d", len, MAX_SIZE_BIGGEST_MSG);
  if (len > MAX_SIZE_BIGGEST_MSG/2)
    KERR("WARNING LEN MSG %d %d", len, MAX_SIZE_BIGGEST_MSG);
  if (!msg_exist_channel(llid))
    return;
  wrapbuf[0] = (char)0xCA; wrapbuf[1] = (char)0xFE;
  wrapbuf[2] = (char)0xDE; wrapbuf[3] = (char)0xCA;
  wrapbuf[4] = (char)0xCA; wrapbuf[5] = (char)0xFE;
  wrapbuf[6] = (char)0xDE; wrapbuf[7] = (char)0xCA;
  memcpy(wrapbuf + DOORS_MAGIC_LEN, buf, len);
  wlen = len + DOORS_MAGIC_LEN;
  string_tx(llid, wlen, wrapbuf);
}

static int get_bnd_event(char *bound)
{
  int i;
  for (i=bnd_min; i<bnd_max; i++)
    if (!strcmp(bound, bound_list[i])) return i;
  return 0;
}

/* All four doors messages have the same shape: one name and one escaped   */
/* text field. */
static void doors_send_named_text(int llid, int tid, char *fctident,
                                  char *nkey, char *name,
                                  char *tkey, char *text)
{
  int len;
  if (!name||!strlen(name)||strlen(name)>=MAX_NAME_LEN) KOUT("%s", fctident);
  if (!text||!strlen(text)) KOUT("%s", fctident);
  len = sprintf(sndbuf, "{\"fctident\":\"%s\",\"tid\":%d,\"%s\":\"%s\",\"%s\":\"",
                fctident, tid, nkey, name, tkey);
  len += json_escape(sndbuf+len, text, MAX_SIZE_BIGGEST_MSG-len);
  len += sprintf(sndbuf+len, "\"}");
  my_msg_mngt_tx(llid, len, sndbuf);
}

void doors_send_info(int llid, int tid, char *type, char *info)
{
  doors_send_named_text(llid, tid, "doors_any_info", "type", type,
                        "info", info);
}

void doors_send_event(int llid, int tid, char *name, char *line)
{
  doors_send_named_text(llid, tid, "doors_event", "name", name, "line", line);
}

void doors_send_command(int llid, int tid, char *name, char *cmd)
{
  doors_send_named_text(llid, tid, "doors_command", "name", name, "cmd", cmd);
}

void doors_send_status(int llid, int tid, char *name, char *status)
{
  doors_send_named_text(llid, tid, "doors_status", "name", name,
                        "status", status);
}

void doors_send_add_vm(int llid, int tid, char *name, char *way2agent)
{
  int len;
  if (!name||!strlen(name)||strlen(name)>=MAX_NAME_LEN) KOUT(" ");
  if (!way2agent||!strlen(way2agent)||strlen(way2agent)>=MAX_PATH_LEN)
    KOUT(" ");
  len = sprintf(sndbuf,
    "{\"fctident\":\"doors_add_vm\",\"tid\":%d,\"name\":\"%s\","
    "\"way2agent\":\"%s\"}",
    tid, name, way2agent);
  my_msg_mngt_tx(llid, len, sndbuf);
}

void doors_send_del_vm(int llid, int tid, char *name)
{
  int len;
  if (!name||!strlen(name)||strlen(name)>=MAX_NAME_LEN) KOUT(" ");
  len = sprintf(sndbuf,
    "{\"fctident\":\"doors_del_vm\",\"tid\":%d,\"name\":\"%s\"}",
    tid, name);
  my_msg_mngt_tx(llid, len, sndbuf);
}

static void json_dispatcher(int llid, int bnd_evt, char *msg)
{
  int  tid = 0;
  char name[MAX_NAME_LEN];
  char way2agent[MAX_PATH_LEN];
  char *payload = (char *) clownix_malloc(MAX_SIZE_BIGGEST_MSG, 1);
  if (!payload)
    KOUT("clownix_malloc failed for payload");

  json_get_int(msg, "tid", &tid);

  switch (bnd_evt) {
  case bnd_info:
    json_get_str(msg, "type", name,    MAX_NAME_LEN);
    json_get_str(msg, "info", payload, MAX_SIZE_BIGGEST_MSG);
    doors_recv_info(llid, tid, name, payload);
    break;
  case bnd_event:
    json_get_str(msg, "name", name,    MAX_NAME_LEN);
    json_get_str(msg, "line", payload, MAX_SIZE_BIGGEST_MSG);
    doors_recv_event(llid, tid, name, payload);
    break;
  case bnd_command:
    json_get_str(msg, "name", name,    MAX_NAME_LEN);
    json_get_str(msg, "cmd",  payload, MAX_SIZE_BIGGEST_MSG);
    doors_recv_command(llid, tid, name, payload);
    break;
  case bnd_status:
    json_get_str(msg, "name",   name,    MAX_NAME_LEN);
    json_get_str(msg, "status", payload, MAX_SIZE_BIGGEST_MSG);
    doors_recv_status(llid, tid, name, payload);
    break;
  case bnd_add_vm:
    json_get_str(msg, "name",      name,      MAX_NAME_LEN);
    json_get_str(msg, "way2agent", way2agent, MAX_PATH_LEN);
    doors_recv_add_vm(llid, tid, name, way2agent);
    break;
  case bnd_del_vm:
    json_get_str(msg, "name", name, MAX_NAME_LEN);
    doors_recv_del_vm(llid, tid, name);
    break;
  default:
    clownix_free(payload, __FUNCTION__);
    KOUT(" ");
  }
  clownix_free(payload, __FUNCTION__);
}

int doors_json_decoder(int llid, int len, char *str_rx)
{
  int  result = -1;
  int  bnd_evt;
  char bound[MAX_CLOWNIX_BOUND_LEN];
  if (len < 2 || str_rx[0] != '{') return -1;
  if (str_rx[len-1] != '}')
    KOUT("bad doors json len=%d last=%c", len, str_rx[len-1]);
  json_fctident(str_rx, bound);
  bnd_evt = get_bnd_event(bound);
  if (bnd_evt) {
    json_dispatcher(llid, bnd_evt, str_rx);
    result = 0;
  }
  return result;
}

void doors_json_init(void)
{
  sndbuf  = get_bigbuf();
  wrapbuf = (char *) clownix_malloc(MAX_SIZE_BIGGEST_MSG + DOORS_MAGIC_LEN + 1, 1);
  memset(bound_list, 0, sizeof(bound_list));
#define SB(idx,str) strncpy(bound_list[idx],str,MAX_CLOWNIX_BOUND_LEN-1)
  SB(bnd_info,    "doors_any_info");
  SB(bnd_event,   "doors_event");
  SB(bnd_command, "doors_command");
  SB(bnd_status,  "doors_status");
  SB(bnd_add_vm,  "doors_add_vm");
  SB(bnd_del_vm,  "doors_del_vm");
#undef SB
}

/* Weak default for a receiver the linking process does not define.        */
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define RECV_DEFAULT(name, args) \
__attribute__((weak)) void name args \
{ \
  static int count = 0; \
  if ((count % 1000) == 0) \
    KERR("WARNING %s reached a process that does not handle it", #name); \
  count += 1; \
}
RECV_DEFAULT(doors_recv_add_vm, (int llid, int tid, char *name, char *way2agent))
RECV_DEFAULT(doors_recv_command, (int llid, int tid, char *name, char *cmd))
RECV_DEFAULT(doors_recv_del_vm, (int llid, int tid, char *name))
RECV_DEFAULT(doors_recv_event, (int llid, int tid, char *name, char *line))
RECV_DEFAULT(doors_recv_info, (int llid, int tid, char *type, char *info))
RECV_DEFAULT(doors_recv_status, (int llid, int tid, char *name, char *status))
