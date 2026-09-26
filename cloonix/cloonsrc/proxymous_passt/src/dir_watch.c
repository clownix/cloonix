/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Notice the installation directory (from /proc/self/exe) being deleted   */
/* under a running net, toy or bundle alike. Then, both needed: the main    */
/* server is asked to stop the "kill <net>" way, and a few seconds later    */
/* the container init is killed, since pid 1 of the crun never returns and  */
/* crun delete needs the state directory that has just gone.                */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "proxy_crun.h"
#include "passt_spawn.h"
#include "dir_watch.h"

static void fct_timeout_dir_watch_bye(void *data);

#define DIR_WATCH_MS 2000

/* Two readings before believing it: a single stat fails for reasons that are
   not a deletion. */
#define DIR_WATCH_STRIKES 2

#define KILL_AFTER_SEC 10

static char g_watched[MAX_PATH_LEN];
static int  g_strikes;
static int  g_fired;

/* The container init, host side, and the pid namespace it belongs to. */
static int  g_cnt_init;
static char g_cnt_ns[MAX_NAME_LEN];

static int read_ns(int pid, char *out, int sz)
{
  char path[MAX_PATH_LEN];
  int len;

  if (pid == 0)
    snprintf(path, sizeof(path), "/proc/self/ns/pid");
  else
    snprintf(path, sizeof(path), "/proc/%d/ns/pid", pid);
  memset(out, 0, sz);
  len = (int) readlink(path, out, sz - 1);
  if (len <= 0)
    return -1;
  out[len] = 0;
  return 0;
}

static int read_ppid(int pid)
{
  char path[MAX_PATH_LEN], line[MAX_PATH_LEN];
  FILE *fh;
  int ppid = 0;

  snprintf(path, sizeof(path), "/proc/%d/status", pid);
  fh = fopen(path, "r");
  if (!fh)
    return 0;
  while (fgets(line, sizeof(line), fh) != NULL)
    {
    if (sscanf(line, "PPid: %d", &ppid) == 1)
      break;
    }
  fclose(fh);
  return ppid;
}

/* The directory the running binary sits in, from /proc/self/exe: argv[0] is
   whatever the caller passed. */
static int find_own_dir(char *dir, int sz)
{
  char path[MAX_PATH_LEN];
  int len;

  memset(path, 0, sizeof(path));
  len = (int) readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len <= 0)
    return -1;
  path[len] = 0;
  if (snprintf(dir, sz, "%s", dirname(path)) >= sz)
    return -1;
  return 0;
}

void dir_watch_note_server(int llid)
{
  struct ucred cred;
  socklen_t len = sizeof(cred);
  char mine[MAX_NAME_LEN], his[MAX_NAME_LEN], up[MAX_NAME_LEN];
  int fd = get_fd_with_llid(llid);
  int pid, ppid;

  g_cnt_init = 0;
  memset(g_cnt_ns, 0, sizeof(g_cnt_ns));
  if (fd < 0)
    {
    KERR("WARNING llid %d of the server has no fd", llid);
    return;
    }
  if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len))
    {
    KERR("WARNING SO_PEERCRED of the server: %s", strerror(errno));
    return;
    }
  pid = (int) cred.pid;
  if (pid <= 1)
    {
    KERR("WARNING the server peer has pid %d", pid);
    return;
    }
  if (read_ns(0, mine, sizeof(mine)))
    {
    KERR("WARNING cannot read the net namespace of this process");
    return;
    }
  if (read_ns(pid, his, sizeof(his)))
    {
    KERR("WARNING cannot read the net namespace of the server %d", pid);
    return;
    }
  if (!strcmp(mine, his))
    return;
  ppid = read_ppid(pid);
  while ((ppid > 1) && (!read_ns(ppid, up, sizeof(up))) && (!strcmp(up, his)))
    {
    pid = ppid;
    ppid = read_ppid(pid);
    }
  g_cnt_init = pid;
  snprintf(g_cnt_ns, sizeof(g_cnt_ns), "%s", his);
}

static void ask_the_net_to_stop(void)
{
  char *req = "transcrun_req_self_destruct";
  int llid = get_llid_sig();

  if ((llid) && (msg_exist_channel(llid)))
    proxy_sig_tx(llid, strlen(req) + 1, req);
  else
    KERR("ERROR no channel to the main server, leaving without it");
}

/* SIGKILL, and only SIGKILL: pid 1 of a pid namespace drops every signal it
   has no handler for. */
static void kill_the_container(void)
{
  char now[MAX_NAME_LEN];

  if (g_cnt_init <= 1)
    return;
  if (read_ns(g_cnt_init, now, sizeof(now)))
    return;
  if (strcmp(now, g_cnt_ns))
    return;
  if (kill(g_cnt_init, SIGKILL))
    KERR("ERROR cannot end the container init pid %d, errno=%d, the net has "
         "to be stopped by hand", g_cnt_init, errno);
}

static void fork_the_follow_up(void)
{
  pid_t pid = fork();

  if (pid < 0)
    {
    KERR("ERROR cannot fork the follow-up, errno=%d, the container has to be "
         "stopped by hand", errno);
    return;
    }
  if (pid > 0)
    return;
  setsid();
  signal(SIGTERM, SIG_DFL);
  sleep(KILL_AFTER_SEC);
  kill_the_container();
  _exit(0);
}

static void fct_timeout_dir_watch(void *data)
{
  struct stat st;
  (void) data;

  if (g_fired)
    return;

  if ((stat(g_watched, &st) == 0) && (S_ISDIR(st.st_mode)))
    {
    g_strikes = 0;
    }
  else
    {
    g_strikes += 1;
    if (g_strikes >= DIR_WATCH_STRIKES)
      {
      g_fired = 1;
      KERR("ERROR %s IS GONE. The installation this net runs from has been "
           "deleted while it was running. Stopping the net and ending the "
           "container init pid %d in %d seconds.",
           g_watched, g_cnt_init, KILL_AFTER_SEC);
      /* The order matters: fork the follow-up BEFORE asking, because asking
         is what gets this process killed. */
      fork_the_follow_up();
      ask_the_net_to_stop();
      clownix_timeout_add(200, fct_timeout_dir_watch_bye, NULL, NULL, NULL);
      return;
      }
    }
  clownix_timeout_add(DIR_WATCH_MS / 10, fct_timeout_dir_watch,
                      NULL, NULL, NULL);
}

static void fct_timeout_dir_watch_bye(void *data)
{
  (void) data;
  passt_spawn_stop();
  KOUT("EXIT: the installation directory was deleted");
}

int dir_watch_init(void)
{
  memset(g_watched, 0, sizeof(g_watched));
  if (find_own_dir(g_watched, sizeof(g_watched)))
    {
    KERR("WARNING cannot find my own directory, no watch on it");
    return -1;
    }
  g_strikes = 0;
  g_fired = 0;
  g_cnt_init = 0;
  memset(g_cnt_ns, 0, sizeof(g_cnt_ns));
  clownix_timeout_add(DIR_WATCH_MS / 10, fct_timeout_dir_watch,
                      NULL, NULL, NULL);
  return 0;
}
