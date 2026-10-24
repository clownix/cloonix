/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <time.h>

#include "glob_common.h"
#include "io_clownix.h"
#include "json_min.h"

/* 4-byte magic prefix — must match rx_process.c */
#define JSON_MAGIC_LEN 8

static int g_pid;

static char       g_bound_list[bnd_rpct_max][MAX_CLOWNIX_BOUND_LEN];
static t_rpct_tx  g_string_tx;
static char      *g_buf_tx;

static int get_bnd_evt(char *bound)
{
  int i;
  for (i = bnd_rpct_min; i < bnd_rpct_max; i++)
    if (!strcmp(bound, g_bound_list[i]))
      return i;
  return 0;
}

uint64_t cloonix_get_msec(void)
{
  struct timespec ts;
  uint64_t result;
  if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  result = (uint64_t)(ts.tv_sec) * 1000;
  result += (uint64_t)ts.tv_nsec / 1000000;
  return result;
}

/* Wall clock, for timestamps only: it jumps, cloonix_get_usec does not. */
uint64_t cloonix_get_epoch_usec(void)
{
  struct timespec ts;
  uint64_t result;
  if (syscall(SYS_clock_gettime, CLOCK_REALTIME, &ts))
    KOUT(" ");
  result = (uint64_t)(ts.tv_sec) * 1000000;
  result += ts.tv_nsec / 1000;
  return result;
}

uint64_t cloonix_get_usec(void)
{
  struct timespec ts;
  uint64_t result;
  if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  result = (uint64_t)(ts.tv_sec) * 1000000;
  result += ts.tv_nsec / 1000;
  return result;
}

void cloonix_set_pid(int pid) { g_pid = pid; }
int  cloonix_get_pid(void)    { return g_pid; }

char *get_buf_tx(void) { return g_buf_tx; }

/* Transmit — prepend 4-byte magic prefix */

static void msg_tx(int llid, int len, char *buf)
{
  static char *wrap = NULL;
  int wlen;
  if (!wrap) wrap = (char *) malloc(MAX_SIZE_BIGGEST_MSG + JSON_MAGIC_LEN + 1);
  wrap[0] = (char)0xCA; wrap[1] = (char)0xFE;
  wrap[2] = (char)0xDE; wrap[3] = (char)0xCA;
  wrap[4] = (char)0xCA; wrap[5] = (char)0xFE;
  wrap[6] = (char)0xDE; wrap[7] = (char)0xCA;
  memcpy(wrap + JSON_MAGIC_LEN, buf, len);
  wlen = len + JSON_MAGIC_LEN;
  g_string_tx(llid, wlen, wrap);
}

void rpct_send_pid_req(int llid, int tid, char *name, int num)
{
  char *buf = get_buf_tx();
  int len;
  if (!name || !strlen(name)) KOUT(" ");
  len = sprintf(buf, "{\"fctident\":\"hop_pid_req\",\"tid\":%d,"
                "\"name\":\"%s\",\"num\":%d}", tid, name, num);
  msg_tx(llid, len, buf);
}

void rpct_send_kil_req(int llid, int tid)
{
  char *buf = get_buf_tx();
  int len = sprintf(buf, "{\"fctident\":\"hop_kil_req\",\"tid\":%d}", tid);
  msg_tx(llid, len, buf);
}

void rpct_send_pid_resp(int llid, int tid, char *name, int num,
                        int toppid, int pid)
{
  char *buf = get_buf_tx();
  int len;
  if (!name || !strlen(name)) KOUT(" ");
  len = sprintf(buf, "{\"fctident\":\"hop_pid_resp\",\"tid\":%d,"
                "\"name\":\"%s\",\"num\":%d,\"toppid\":%d,\"pid\":%d}",
                tid, name, num, toppid, pid);
  msg_tx(llid, len, buf);
}

static void rpct_send_diag_line(int llid, int tid, char *fctident, char *line)
{
  char *buf = get_buf_tx();
  int len;
  if (!line || strlen(line) < 1) KOUT("%s", fctident);
  if (strlen(line) >= MAX_RPC_MSG_LEN) line[MAX_RPC_MSG_LEN-1] = 0;
  len = sprintf(buf, "{\"fctident\":\"%s\",\"tid\":%d,\"line\":\"",
                fctident, tid);
  len += json_escape(buf + len, line, 90000 - len);
  len += sprintf(buf + len, "\"}");
  msg_tx(llid, len, buf);
}

void rpct_send_sigdiag_msg(int llid, int tid, char *line)
{
  rpct_send_diag_line(llid, tid, "sigdiag_msg", line);
}

void rpct_send_poldiag_msg(int llid, int tid, char *line)
{
  rpct_send_diag_line(llid, tid, "poldiag_msg", line);
}

static void dispatcher(int llid, int bnd_evt, char *msg)
{
  int toppid = 0, pid = 0, tid = 0, num = 0;
  char name[MAX_NAME_LEN];
  char line[MAX_RPC_MSG_LEN];

  json_get_int(msg, "tid", &tid);

  switch (bnd_evt) {

  case bnd_rpct_pid_req:
    json_get_str(msg, "name", name, MAX_NAME_LEN);
    json_get_int(msg, "num", &num);
    rpct_recv_pid_req(llid, tid, name, num);
    break;

  case bnd_rpct_kil_req:
    rpct_recv_kil_req(llid, tid);
    break;

  case bnd_rpct_pid_resp:
    json_get_str(msg, "name", name, MAX_NAME_LEN);
    json_get_int(msg, "num",    &num);
    json_get_int(msg, "toppid", &toppid);
    json_get_int(msg, "pid",    &pid);
    rpct_recv_pid_resp(llid, tid, name, num, toppid, pid);
    break;

  case bnd_rpct_sigdiag_msg:
    json_get_str(msg, "line", line, MAX_RPC_MSG_LEN);
    rpct_recv_sigdiag_msg(llid, tid, line);
    break;

  case bnd_rpct_poldiag_msg:
    json_get_str(msg, "line", line, MAX_RPC_MSG_LEN);
    rpct_recv_poldiag_msg(llid, tid, line);
    break;

  default:
    KOUT(" ");
  }
}

int rpct_decoder(int llid, int len, char *str_rx)
{
  int  result = -1;
  int  bnd_evt;
  char bound[MAX_CLOWNIX_BOUND_LEN];
  if (len < 2 || str_rx[0] != '{')
    return -1;
  json_fctident(str_rx, bound);
  bnd_evt = get_bnd_evt(bound);
  if (bnd_evt) {
    dispatcher(llid, bnd_evt, str_rx);
    result = 0;
  }
  return result;
}

void rpct_redirect_string_tx(t_rpct_tx rpc_tx) { g_string_tx = rpc_tx; }
t_rpct_tx rpct_get_string_tx(void) { return g_string_tx; }

void rpct_init(t_rpct_tx rpc_tx)
{
  g_string_tx = rpc_tx;
  g_buf_tx    = (char *) malloc(MAX_SIZE_BIGGEST_MSG + JSON_MAGIC_LEN + 1);
  memset(g_bound_list, 0, sizeof(g_bound_list));
  strncpy(g_bound_list[bnd_rpct_sigdiag_msg],    "sigdiag_msg",   MAX_CLOWNIX_BOUND_LEN-1);
  strncpy(g_bound_list[bnd_rpct_poldiag_msg],    "poldiag_msg",   MAX_CLOWNIX_BOUND_LEN-1);
  strncpy(g_bound_list[bnd_rpct_pid_req],        "hop_pid_req",   MAX_CLOWNIX_BOUND_LEN-1);
  strncpy(g_bound_list[bnd_rpct_kil_req],        "hop_kil_req",   MAX_CLOWNIX_BOUND_LEN-1);
  strncpy(g_bound_list[bnd_rpct_pid_resp],       "hop_pid_resp",  MAX_CLOWNIX_BOUND_LEN-1);
}

/* Weak default for a receiver the linking process does not define. */
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define RECV_DEFAULT(name, args) \
__attribute__((weak)) void name args \
{ \
  static int count = 0; \
  if ((count % 1000) == 0) \
    KERR("WARNING %s reached a process that does not handle it", #name); \
  count += 1; \
}
RECV_DEFAULT(rpct_recv_kil_req, (int llid, int tid))
RECV_DEFAULT(rpct_recv_pid_req, (int llid, int tid, char *name, int num))
RECV_DEFAULT(rpct_recv_pid_resp, (int llid, int tid, char *name, int num, int toppid, int pid))
RECV_DEFAULT(rpct_recv_poldiag_msg, (int llid, int tid, char *line))
RECV_DEFAULT(rpct_recv_sigdiag_msg, (int llid, int tid, char *line))
