/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Container consoles: entering the namespaces needs root. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "io_clownix.h"
#include "util_sock.h"
#include "glob_common.h"
#include "pthexec.h"
#include "console.h"
#include "crun.h"
#include "crun_utils.h"
#include "utils.h"

#define MAX_CONSOLE_REQ_LEN 512

static int g_listen_fd = -1;
static int g_listen_llid;
static char g_var_root[MAX_PATH_LEN];

char *get_net_name(void);

static char *console_sock_path(char *name)
{
  static char path[MAX_PATH_LEN];
  snprintf(path, MAX_PATH_LEN-1, "%s/%s/cnt_%s", g_var_root, DTACH_SOCK, name);
  return path;
}

static int hand_sock_to_caller(char *path)
{
  int result = 0;
  if (chown(path, get_caller_uid(), get_caller_gid()))
    {
    KERR("ERROR chown console socket %s %d", path, errno);
    result = -1;
    }
  else if (chmod(path, S_IRUSR | S_IWUSR))
    {
    KERR("ERROR chmod console socket %s %d", path, errno);
    result = -1;
    }
  return result;
}

static int fork_console(char *name, char *sock, int is_privileged)
{
  int pid, status, result = -1;
  char crun_dir[MAX_PATH_LEN];
  char log[MAX_PATH_LEN];
  char *argv[13];
  char *env[] = {"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:"
                 "/sbin:/bin", "TERM=xterm-256color", "LANG=C", "LC_ALL=C",
                 NULL};
  snprintf(crun_dir, MAX_PATH_LEN-1, "--root=%s/%s", g_var_root, CRUN_DIR);
  snprintf(log, MAX_PATH_LEN-1, "--log=%s", get_var_crun_log());
  argv[0]  = pthexec_dtach_bin();
  argv[1]  = "-n";
  argv[2]  = sock;
  argv[3]  = pthexec_crun_bin();
  argv[4]  = crun_cgroup_manager(is_privileged);
  argv[5]  = log;
  argv[6]  = crun_dir;
  argv[7]  = "exec";
  argv[8]  = "-ti";
  argv[9]  = crun_id(name);
  argv[10] = "/bin/sh";  /* resolved inside the container */
  argv[11] = NULL;
  pid = fork();
  if (pid < 0)
    KERR("ERROR fork console %s %d", name, errno);
  else if (pid == 0)
    {
    signal(SIGCHLD, SIG_DFL);
    execve(argv[0], argv, env);
    KOUT("ERROR execve %s", argv[0]);
    }
  else
    {
    if (waitpid(pid, &status, 0) < 0)
      KERR("ERROR waitpid console %s %d", name, errno);
    else if (!WIFEXITED(status))
      KERR("ERROR console %s died on signal", name);
    else if (WEXITSTATUS(status))
      KERR("ERROR console %s: dtach exited %d", name, WEXITSTATUS(status));
    else
      result = 0;
    }
  return result;
}

static int session_is_alive(char *path)
{
  struct sockaddr_un addr;
  int s, alive = 0;
  s = socket(AF_UNIX, SOCK_STREAM, 0);
  if (s < 0)
    {
    KERR("WARNING console %s: socket: %s", path, strerror(errno));
    return 0;
    }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path)-1, "%s", path);
  if (!connect(s, (struct sockaddr *) &addr, sizeof(addr)))
    alive = 1;
  close(s);
  if ((!alive) && (!access(path, F_OK)))
    {
    KERR("WARNING console socket %s is stale, a new session starts", path);
    unlink(path);
    }
  return alive;
}

static void console_crun_req(int fd, char *name)
{
  char resp[MAX_CONSOLE_REQ_LEN];
  char *sock = console_sock_path(name);
  int is_privileged;
  memset(resp, 0, MAX_CONSOLE_REQ_LEN);
  if (crun_get_is_privileged(name, &is_privileged))
    {
    KERR("ERROR console asked for unknown container %s", name);
    snprintf(resp, MAX_CONSOLE_REQ_LEN-1, CONSOLE_CRUN_KO, name);
    }
  else if (session_is_alive(sock))
    {
    /* A session is already up for this container; attaching to it is the
       point, a second one would be a second shell. */
    snprintf(resp, MAX_CONSOLE_REQ_LEN-1, CONSOLE_CRUN_OK, name, sock);
    }
  else if (fork_console(name, sock, is_privileged))
    snprintf(resp, MAX_CONSOLE_REQ_LEN-1, CONSOLE_CRUN_KO, name);
  else if (hand_sock_to_caller(sock))
    snprintf(resp, MAX_CONSOLE_REQ_LEN-1, CONSOLE_CRUN_KO, name);
  else
    snprintf(resp, MAX_CONSOLE_REQ_LEN-1, CONSOLE_CRUN_OK, name, sock);
  if (write(fd, resp, strlen(resp) + 1) != (int) (strlen(resp) + 1))
    KERR("ERROR console response %s", resp);
}

static int console_rx_cb(int llid, int fd)
{
  char req[MAX_CONSOLE_REQ_LEN];
  char name[MAX_NAME_LEN];
  int len;
  memset(req, 0, MAX_CONSOLE_REQ_LEN);
  len = util_read(req, MAX_CONSOLE_REQ_LEN-1, fd);
  if (len <= 0)
    {
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    return -1;
    }
  req[len] = 0;
  if (sscanf(req, CONSOLE_CRUN_REQ, name) != 1)
    KERR("ERROR bad console request: %s", req);
  else if (!name_is_sane(name))
    KERR("ERROR console refused, bad guest name: %s", req);
  else
    console_crun_req(fd, name);
  return 0;
}

static void console_err_cb(int llid, int err, int from)
{
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}

static int console_listen_cb(int llid, int fd)
{
  int new_fd;
  util_fd_accept(g_listen_fd, &new_fd, __FUNCTION__);
  if (new_fd < 0)
    KERR("ERROR console accept %d", errno);
  else if (msg_watch_fd(new_fd, console_rx_cb, console_err_cb, "cons") <= 0)
    {
    KERR("ERROR console watch fd");
    close(new_fd);
    }
  return 0;
}

void console_init(char *var_root)
{
  char path[MAX_PATH_LEN];
  memset(g_var_root, 0, MAX_PATH_LEN);
  strncpy(g_var_root, var_root, MAX_PATH_LEN-1);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", var_root, CONSOLE_SOCK);
  unlink(path);
  g_listen_fd = util_socket_listen_unix(path);
  if (g_listen_fd < 0)
    KOUT("ERROR console listen %s", path);
  if (chown(path, get_caller_uid(), get_caller_gid()))
    KERR("ERROR chown %s %d", path, errno);
  if (chmod(path, S_IRUSR | S_IWUSR))
    KERR("ERROR chmod %s %d", path, errno);
  g_listen_llid = msg_watch_fd(g_listen_fd, console_listen_cb,
                               console_err_cb, "conl");
  if (g_listen_llid <= 0)
    KOUT("ERROR console watch listen");
}
