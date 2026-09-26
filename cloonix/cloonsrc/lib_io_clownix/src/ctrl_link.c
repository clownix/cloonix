/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Control link of an ovs satellite (a2b, c2c, snf, nat) to the server:    */
/* listening unix socket, one live connection, watchdog on pid_req (first   */
/* within CTRL_LINK_GRACE_MS, then one per CTRL_LINK_BEAT_MS), stop on      */
/* kil_req. Every stop is one KERR line then exit(0).                       */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "io_clownix.h"
#include "stall_detect.h"

/* 10 ms beats on the monotonic clock. A late server is a busy host (a loop
   storm in a guest can hold it for seconds): 20 s of silence means gone. */
#define CTRL_LINK_BEAT_MS  20000
#define CTRL_LINK_POLL_BEATS 20
#define CTRL_LINK_MAX_PATH 4
#define CTRL_LINK_MAX_CHILD 4

static char g_net[MAX_NAME_LEN];
static char g_kind[MAX_NAME_LEN];
static char g_name[MAX_NAME_LEN];
static int g_type_hop;
static int g_llid;
static long long g_fed_ms;
static unsigned long g_nb_pid_req;
static t_stall g_stall;
static int g_nb_path;
static char g_path[CTRL_LINK_MAX_PATH][MAX_PATH_LEN];
static int g_nb_child;
static int g_child[CTRL_LINK_MAX_CHILD];
static int g_said_killed;

int ctrl_link_llid(void)
{
  return g_llid;
}

void ctrl_link_stop(void)
{
  int i;
  for (i=0; i<g_nb_path; i++)
    unlink(g_path[i]);
  for (i=0; i<g_nb_child; i++)
    {
    if (g_child[i] > 0)
      kill(g_child[i], SIGKILL);
    }
  exit(0);
}

/* A path left behind means the previous holder was killed, not stopped.   */
void ctrl_link_add_path(char *path)
{
  if (g_nb_path >= CTRL_LINK_MAX_PATH)
    KOUT("ERROR %s %s too many paths", g_kind, g_name);
  if (!access(path, R_OK))
    {
    KERR("WARNING %s %s %s: the previous one left %s behind, so it was "
         "killed rather than stopped. Erasing and carrying on",
         g_net, g_kind, g_name, path);
    g_said_killed = 1;
    unlink(path);
    }
  memset(g_path[g_nb_path], 0, MAX_PATH_LEN);
  strncpy(g_path[g_nb_path], path, MAX_PATH_LEN-1);
  g_nb_path += 1;
}

/* A fifo left behind is kept: a reader started for this snf may already  */
/* hold it, and erasing it would leave that reader on an orphaned inode.   */
void ctrl_link_keep_path(char *path)
{
  if (g_nb_path >= CTRL_LINK_MAX_PATH)
    KOUT("ERROR %s %s too many paths", g_kind, g_name);
  /* the kill is said once, by ctrl_link_add_path */
  if ((!access(path, R_OK)) && (!g_said_killed))
    KERR("WARNING %s %s %s: the previous one left %s behind, so it was "
         "killed rather than stopped. Kept, its reader may be waiting on it",
         g_net, g_kind, g_name, path);
  memset(g_path[g_nb_path], 0, MAX_PATH_LEN);
  strncpy(g_path[g_nb_path], path, MAX_PATH_LEN-1);
  g_nb_path += 1;
}

void ctrl_link_add_child(int pid)
{
  if (g_nb_child >= CTRL_LINK_MAX_CHILD)
    KOUT("ERROR %s %s too many children", g_kind, g_name);
  g_child[g_nb_child] = pid;
  g_nb_child += 1;
}

void ctrl_link_pid_req(int llid, int tid, char *name, int num)
{
  if (llid != g_llid)
    KERR("ERROR %s %s %s %d %d", g_net, g_kind, name, llid, g_llid);
  if (tid != g_type_hop)
    KERR("ERROR %s %s %s %d %d", g_net, g_kind, name, tid, g_type_hop);
  if (strcmp(g_name, name))
    KERR("ERROR %s %s %s %s", g_net, g_kind, name, g_name);
  rpct_send_pid_resp(llid, tid, name, num, cloonix_get_pid(), getpid());
  g_fed_ms = stall_now_ms();
  stall_answered(&g_stall);
  g_nb_pid_req += 1;
}

static void err_ctrl_cb(int llid, int err, int from)
{
  KERR("WARNING %s %s %s: the ctrl connection to the main server closed, "
       "stopping", g_net, g_kind, g_name);
  ctrl_link_stop();
}

static void rx_ctrl_cb(int llid, int len, char *buf)
{
  if (rpct_decoder(llid, len, buf))
    KOUT("ERROR %s %s %s %s", g_net, g_kind, g_name, buf);
}

/* The listener is kept, a second connection replaces the first.           */
static void connect_from_ctrl_client(int llid, int llid_new)
{
  if ((g_llid) && (g_llid != llid_new) && (msg_exist_channel(g_llid)))
    {
    KERR("WARNING %s %s %s: a new connection from the main server replaces "
         "the previous one", g_net, g_kind, g_name);
    msg_delete_channel(g_llid);
    }
  msg_mngt_set_callbacks(llid_new, err_ctrl_cb, rx_ctrl_cb);
  g_llid = llid_new;
  g_fed_ms = stall_now_ms();
}

static void timeout_watchdog(void *data)
{
  long long silence = stall_now_ms() - g_fed_ms;
  int limit = (g_nb_pid_req == 0) ? CTRL_LINK_GRACE_MS : CTRL_LINK_BEAT_MS;
  int gap = stall_beat(&g_stall);
  int i;
  /* The child carries the tap side of the data path: without it this
     process must go too so the main server sees it. */
  for (i=0; i<g_nb_child; i++)
    {
    if ((g_child[i] > 0) && (kill(g_child[i], 0)) && (errno == ESRCH))
      {
      KERR("ERROR %s %s %s: its child process %d is gone, stopping so the "
           "main server starts a whole one again", g_net, g_kind, g_name,
           g_child[i]);
      g_child[i] = 0;
      ctrl_link_stop();
      }
    }
  if (gap > 0)
    KERR("WARNING %s %s %s: this loop did not beat for %d ms (%s), not "
         "counted against the main server", g_net, g_kind, g_name, gap,
         stall_why(&g_stall));
  limit += stall_credit_ms(&g_stall);
  if (silence > limit)
    {
    if (g_nb_pid_req == 0)
      KERR("WARNING %s %s %s: never heard from the main server within %d ms, "
           "so it was deleted while still starting, stopping",
           g_net, g_kind, g_name, limit);
    else
      KERR("WARNING %s %s %s: the main server said nothing for %d ms after "
           "%lu pid_req, stopping", g_net, g_kind, g_name, limit,
           g_nb_pid_req);
    ctrl_link_stop();
    }
  clownix_timeout_add(CTRL_LINK_POLL_BEATS, timeout_watchdog,
                      NULL, NULL, NULL);
}

void ctrl_link_init(char *net, char *kind, char *name, int type_hop,
                    char *ctrl_path)
{
  memset(g_net, 0, MAX_NAME_LEN);
  memset(g_kind, 0, MAX_NAME_LEN);
  memset(g_name, 0, MAX_NAME_LEN);
  strncpy(g_net, net, MAX_NAME_LEN-1);
  strncpy(g_kind, kind, MAX_NAME_LEN-1);
  strncpy(g_name, name, MAX_NAME_LEN-1);
  g_type_hop = type_hop;
  g_llid = 0;
  g_fed_ms = stall_now_ms();
  g_nb_pid_req = 0;
  stall_init(&g_stall, CTRL_LINK_POLL_BEATS * 10, CTRL_LINK_BEAT_MS);
  ctrl_link_add_path(ctrl_path);
  string_server_unix(ctrl_path, connect_from_ctrl_client, "ctrl");
  clownix_timeout_add(CTRL_LINK_POLL_BEATS, timeout_watchdog,
                      NULL, NULL, NULL);
}
