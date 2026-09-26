/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Runs a demo on the host for the gui server: the toy crun has no python. */
/* A unix socket in the proxyshare directory carries the job. What travels  */
/* is the demo as the gui server read it from its own disk, never the bytes */
/* the browser sent.                                                        */
/*   ->  RUN <net> <len>\n  followed by <len> bytes of script                */
/*   <-  O <line>\n         one per line of stdout and stderr               */
/*   <-  E <code>\n         last, then the connection is closed             */
/* The listener does setsid, closes inherited descriptors, PDEATHSIG is     */
/* SIGTERM so it unlinks its socket. Environment switches for bisecting:    */
/*   CLOONIX_DEMO_EXEC=0     do not start the runner                        */
/*   CLOONIX_DEMO_NOFORK=1   bind the socket but do not fork the listener   */
/*   CLOONIX_DEMO_KEEPFD=1   keep the inherited descriptors                 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "pthexec.h"
#include <sys/prctl.h>
#include <poll.h>

#include "io_clownix.h"
#include "util_sock.h"
#include "rpc_clownix.h"
#include "proxy_crun.h"
#include "demo_exec.h"

#define DEMO_TRACE 0

#if DEMO_TRACE
#define DLOG(fmt, ...) KERR("DEMOEXEC " fmt, ##__VA_ARGS__)
#else
#define DLOG(fmt, ...) do { if (0) KERR(fmt, ##__VA_ARGS__); } while(0)
#endif

#define DEMO_MAX_SCRIPT (4 * 1024 * 1024)
#define DEMO_MAX_LINE   4096
#define DEMO_MAX_FD     1024

static char g_sock_path[MAX_PATH_LEN];

static int env_on(const char *name, int dflt)
{
  char *v = getenv(name);
  if (!v)
    return dflt;
  if ((!strcmp(v, "0")) || (!strcmp(v, "no")) || (!strcmp(v, "NO")))
    return 0;
  return 1;
}

static int read_all(int fd, char *buf, int len)
{
  int done = 0, n;
  while (done < len)
    {
    n = (int) read(fd, buf + done, len - done);
    if (n <= 0)
      {
      if ((n < 0) && (errno == EINTR))
        continue;
      return -1;
      }
    done += n;
    }
  return 0;
}

/* Read one byte at a time: the script bytes follow the line.              */
static int read_line(int fd, char *buf, int max)
{
  int i = 0;
  char c;
  while (i < max - 1)
    {
    int n = (int) read(fd, &c, 1);
    if (n == 0)
      return -1;
    if (n < 0)
      {
      if (errno == EINTR)
        continue;
      return -1;
      }
    if (c == '\n')
      {
      buf[i] = 0;
      return 0;
      }
    buf[i++] = c;
    }
  return -1;
}

static int net_name_is_sane(const char *net)
{
  int i;
  if ((!net[0]) || (strlen(net) >= MAX_NAME_LEN))
    return 0;
  for (i = 0; net[i]; i++)
    {
    if ((!isalnum((unsigned char) net[i])) &&
        (net[i] != '_') && (net[i] != '-'))
      return 0;
    }
  return 1;
}

static void send_exit(int fd, int code)
{
  char msg[64];
  snprintf(msg, sizeof(msg), "E %d\n", code);
  (void) util_write_all(fd, msg, (int) strlen(msg));
}

static int job_spawn(char *cmd, int *in_fd, int *out_fd)
{
  int pin[2], pout[2], pid;
  char *argv[4];
  if (pipe(pin))
    return -1;
  if (pipe(pout))
    {
    close(pin[0]);
    close(pin[1]);
    return -1;
    }
  pid = fork();
  if (pid < 0)
    {
    KERR("ERROR demo: fork: %s", strerror(errno));
    close(pin[0]); close(pin[1]); close(pout[0]); close(pout[1]);
    return -1;
    }
  if (pid == 0)
    {
    setpgid(0, 0);
    dup2(pin[0], 0);
    dup2(pout[1], 1);
    close(pin[0]); close(pin[1]); close(pout[0]); close(pout[1]);
    setenv("CLOONIX_DEMO_KEY", "1", 1);
    /* The demo marks its history lines, the panel shows them in a popup. */
    setenv("CLOONIX_DEMO_STORY", "1", 1);
    argv[0] = pthexec_bash_bin();
    argv[1] = "-c";
    argv[2] = cmd;
    argv[3] = NULL;
    execv(argv[0], argv);
    _exit(127);
    }
  setpgid(pid, pid);
  close(pin[0]);
  close(pout[1]);
  *in_fd = pin[1];
  *out_fd = pout[0];
  return pid;
}

/* The gui server gone or asking to stop: the demo and all it started. */
static int job_stop(int pid)
{
  if (kill(-pid, SIGKILL) && (errno != ESRCH))
    KERR("ERROR DEMOEXEC JOB kill %d %d", pid, errno);
  return 1;
}

static int job_relay(int fd, int in_fd, int out_fd, int pid)
{
  char obuf[DEMO_MAX_LINE], kbuf[DEMO_MAX_LINE], out[DEMO_MAX_LINE + 8];
  int oused = 0, kused = 0, nb_lines = 0, n, i, start, peer_gone = 0;
  struct pollfd pfd[2];

  while (1)
    {
    pfd[0].fd = out_fd;
    pfd[0].events = POLLIN;
    pfd[1].fd = fd;
    pfd[1].events = peer_gone ? 0 : POLLIN;
    if (poll(pfd, 2, -1) < 0)
      {
      if (errno == EINTR)
        continue;
      break;
      }
    if (pfd[0].revents & (POLLIN | POLLHUP | POLLERR))
      {
      if (oused >= (int) sizeof(obuf) - 1)
        oused = 0;
      n = (int) read(out_fd, obuf + oused, sizeof(obuf) - 1 - oused);
      if (n <= 0)
        break;
      oused += n;
      start = 0;
      for (i = 0; i < oused; i++)
        {
        if (obuf[i] != '\n')
          continue;
        obuf[i] = 0;
        if ((i > start) && (obuf[i-1] == '\r'))
          obuf[i-1] = 0;
        snprintf(out, sizeof(out), "O %s\n", obuf + start);
        if ((!peer_gone) && (util_write_all(fd, out, (int) strlen(out))))
          {
          KERR("WARNING DEMOEXEC JOB peer gone after %d lines", nb_lines);
          peer_gone = job_stop(pid);
          }
        nb_lines++;
        start = i + 1;
        }
      if (start > 0)
        {
        memmove(obuf, obuf + start, oused - start);
        oused -= start;
        }
      }
    if ((!peer_gone) && (pfd[1].revents & (POLLIN | POLLHUP | POLLERR)))
      {
      if (kused >= (int) sizeof(kbuf) - 1)
        kused = 0;
      n = (int) read(fd, kbuf + kused, sizeof(kbuf) - 1 - kused);
      if (n <= 0)
        {
        peer_gone = job_stop(pid);
        continue;
        }
      kused += n;
      start = 0;
      for (i = 0; i < kused; i++)
        {
        if (kbuf[i] != '\n')
          continue;
        if ((i - start >= 2) && (!strncmp(kbuf + start, "K ", 2)))
          (void) util_write_all(in_fd, kbuf + start + 2, i - start - 1);
        start = i + 1;
        }
      if (start > 0)
        {
        memmove(kbuf, kbuf + start, kused - start);
        kused -= start;
        }
      }
    }
  return nb_lines;
}

static void handle_job(int fd)
{
  char line[DEMO_MAX_LINE], net[MAX_NAME_LEN], bindir[MAX_PATH_LEN];
  char tmp[] = "/tmp/cloonix_demo_XXXXXX";
  char cmd[2 * MAX_PATH_LEN];
  char *script = NULL;
  int len = 0, tmpfd, rc = 0, code, nb_lines = 0, child_pid, in_fd, out_fd;

  signal(SIGCHLD, SIG_DFL);
  DLOG("JOB start pid=%d", (int) getpid());
  if (read_line(fd, line, sizeof(line)))
    {
    KERR("ERROR DEMOEXEC JOB no request line");
    return;
    }
  /* Width 63: %64s would write 65 bytes into MAX_NAME_LEN. */
  if (sscanf(line, "RUN %63s %d", net, &len) != 2)
    {
    KERR("ERROR DEMOEXEC JOB bad request [%s]", line);
    return;
    }
  if (!net_name_is_sane(net))
    {
    KERR("ERROR DEMOEXEC JOB bad net [%s]", net);
    return;
    }
  if ((len <= 0) || (len > DEMO_MAX_SCRIPT))
    {
    KERR("ERROR DEMOEXEC JOB bad len %d", len);
    return;
    }
  DLOG("JOB net=%s len=%d", net, len);
  script = (char *) malloc(len + 1);
  if (!script)
    {
    KERR("ERROR DEMOEXEC JOB malloc %d", len);
    return;
    }
  if (read_all(fd, script, len))
    {
    KERR("ERROR DEMOEXEC JOB short body");
    free(script);
    return;
    }
  script[len] = 0;

  tmpfd = mkstemp(tmp);
  if (tmpfd < 0)
    {
    KERR("ERROR DEMOEXEC JOB mkstemp %d", errno);
    free(script);
    send_exit(fd, 254);
    return;
    }
  if (util_write_all(tmpfd, script, len))
    {
    KERR("ERROR DEMOEXEC JOB write %d", errno);
    close(tmpfd);
    unlink(tmp);
    free(script);
    send_exit(fd, 254);
    return;
    }
  close(tmpfd);
  free(script);
  if (chmod(tmp, 0700))
    KERR("ERROR DEMOEXEC JOB chmod %s %d", tmp, errno);

  snprintf(bindir, sizeof(bindir), "%s", pthexec_self_bin_dir());
  if (bindir[0])
    snprintf(cmd, sizeof(cmd), "PATH=%s:$PATH %s %s 2>&1", bindir, tmp, net);
  else
    snprintf(cmd, sizeof(cmd), "%s %s 2>&1", tmp, net);
  DLOG("JOB spawn %s", cmd);

  child_pid = job_spawn(cmd, &in_fd, &out_fd);
  if (child_pid <= 0)
    {
    KERR("ERROR DEMOEXEC JOB spawn failed %d", errno);
    unlink(tmp);
    send_exit(fd, 254);
    return;
    }
  nb_lines = job_relay(fd, in_fd, out_fd, child_pid);
  close(in_fd);
  close(out_fd);
  while ((waitpid(child_pid, &rc, 0) < 0) && (errno == EINTR))
    ;
  unlink(tmp);
  code = WIFEXITED(rc) ? WEXITSTATUS(rc) : 255;
  DLOG("JOB done lines=%d code=%d", nb_lines, code);
  send_exit(fd, code);
}

/* PDEATHSIG is SIGTERM so the socket file gets unlinked.                   */
static void listener_bye(int sig)
{
  DLOG("LISTENER signal %d, unlinking %s", sig, g_sock_path);
  unlink(g_sock_path);
  killpg(getpgrp(), SIGKILL);
  _exit(0);
}

static void drop_inherited_fds(int keep)
{
  int i, fd;
  closelog();
  for (i = 3; i < DEMO_MAX_FD; i++)
    {
    if (i != keep)
      close(i);
    }
  fd = open("/dev/null", O_RDWR);
  if (fd >= 0)
    {
    dup2(fd, 0);
    dup2(fd, 1);
    dup2(fd, 2);
    if (fd > 2)
      close(fd);
    }
  openlog("cloonix-proxymous", LOG_PID, LOG_DAEMON);
}

static void accept_loop(int srv)
{
  int fd, nb = 0;

  if (setsid() < 0)
    KERR("ERROR DEMOEXEC LISTENER setsid %d", errno);
  prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0);
  signal(SIGTERM, listener_bye);
  signal(SIGINT, listener_bye);
  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  if (env_on("CLOONIX_DEMO_KEEPFD", 0))
    DLOG("LISTENER keeping inherited fds (asked for)");
  else
    drop_inherited_fds(srv);
  DLOG("LISTENER up pid=%d sid=%d on %s",
       (int) getpid(), (int) getsid(0), g_sock_path);

  for (;;)
    {
    fd = accept(srv, NULL, NULL);
    if (fd < 0)
      {
      if ((errno == EINTR) || (errno == ECONNABORTED))
        continue;
      KERR("ERROR DEMOEXEC LISTENER accept %d, leaving", errno);
      unlink(g_sock_path);
      _exit(1);
      }
    nb++;
    DLOG("LISTENER connection %d", nb);
    if (fork() == 0)
      {
      close(srv);
      handle_job(fd);
      close(fd);
      _exit(0);
      }
    close(fd);
    }
}

int demo_exec_init(char *proxyshare)
{
  struct sockaddr_un addr;
  int srv, pid;

  if (!env_on("CLOONIX_DEMO_EXEC", 1))
    {
    DLOG("disabled by CLOONIX_DEMO_EXEC");
    return 0;
    }
  DLOG("init pid=%d proxyshare=%s", (int) getpid(), proxyshare);

  memset(g_sock_path, 0, MAX_PATH_LEN);
  snprintf(g_sock_path, MAX_PATH_LEN-1, "%s/%s", proxyshare, DEMO_EXEC_SOCK);
  if (strlen(g_sock_path) >= sizeof(addr.sun_path))
    {
    KERR("ERROR DEMOEXEC path too long %s", g_sock_path);
    return -1;
    }
  unlink(g_sock_path);
  srv = socket(AF_UNIX, SOCK_STREAM, 0);
  if (srv < 0)
    {
    KERR("ERROR DEMOEXEC socket %d", errno);
    return -1;
    }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, g_sock_path, sizeof(addr.sun_path) - 1);
  if (bind(srv, (struct sockaddr *) &addr, sizeof(addr)))
    {
    KERR("ERROR DEMOEXEC bind %s %d", g_sock_path, errno);
    close(srv);
    return -1;
    }
  if (chmod(g_sock_path, 0600))
    KERR("ERROR DEMOEXEC chmod %s %d", g_sock_path, errno);
  if (listen(srv, 8))
    {
    KERR("ERROR DEMOEXEC listen %s %d", g_sock_path, errno);
    close(srv);
    unlink(g_sock_path);
    return -1;
    }
  DLOG("bound and listening on %s fd=%d", g_sock_path, srv);

  if (env_on("CLOONIX_DEMO_NOFORK", 0))
    {
    DLOG("no listener forked (asked for), socket left bound");
    return 0;
    }
  pid = fork();
  if (pid < 0)
    {
    KERR("ERROR DEMOEXEC fork %d", errno);
    close(srv);
    unlink(g_sock_path);
    return -1;
    }
  if (pid == 0)
    {
    accept_loop(srv);
    _exit(0);
    }
  close(srv);
  DLOG("listener forked pid=%d, proxymous carries on", pid);
  return 0;
}
