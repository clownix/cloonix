/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The wifi medium apart, cloonix-suid-power-air: create on the sync pipe,
   the rest on the async one; frames never wait for crun commands. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "pthexec.h"
#include "utils.h"
#include "vwifi.h"

char *get_net_name(void);
char *get_root_path(void);
int get_cloonix_rank(void);

/* a create is answered after its iw commands: seconds under load */
#define CREATE_WAIT_MS 30000

static int g_sync = -1;
static int g_async = -1;
static int g_async_llid;
static char g_why[MAX_NAME_LEN];

int air_lines(int fd, char *buf, int *len, void (*cb)(char *line))
{
  char *nl, *start;
  int n = read(fd, buf + *len, AIR_LINE_BUF - 1 - *len);
  if (n == 0)
    return -1;
  if (n < 0)
    return ((errno == EAGAIN) || (errno == EINTR)) ? 0 : -1;
  *len += n;
  buf[*len] = 0;
  start = buf;
  while ((nl = strchr(start, '\n')) != NULL)
    {
    *nl = 0;
    cb(start);
    start = nl + 1;
    }
  *len -= (start - buf);
  memmove(buf, start, *len);
  if (*len == AIR_LINE_BUF - 1)
    {
    KERR("ERROR air: a line of %d bytes with no end, dropped", *len);
    *len = 0;
    }
  return 0;
}

void air_line_send(int fd, const char *fmt, ...)
{
  char line[AIR_LINE_BUF];
  va_list ap;
  int len, done = 0, n;
  va_start(ap, fmt);
  len = vsnprintf(line, sizeof(line) - 1, fmt, ap);
  va_end(ap);
  if (len > (int) sizeof(line) - 2)
    len = sizeof(line) - 2;
  line[len++] = '\n';
  while (done < len)
    {
    n = write(fd, line + done, len - done);
    if (n > 0)
      done += n;
    else if ((n < 0) && (errno == EINTR))
      continue;
    else
      {
      KERR("ERROR air: a line to the other process lost: %s",
           n < 0 ? strerror(errno) : "closed");
      return;
      }
    }
}

static void spawn(void);

static void resp_line(char *line)
{
  int llid, tid, n;
  if (sscanf(line, "resp %d %d %n", &llid, &tid, &n) == 2)
    {
    if (msg_exist_channel(llid))
      rpct_send_sigdiag_msg(llid, tid, line + n);
    }
  else
    KERR("ERROR air: from the air process: %s", line);
}

static void medium_gone(void)
{
  KERR("ERROR air: the process of the medium ended, its radios with it: "
       "started again, the containers made before have no radio");
  if (g_async_llid && msg_exist_channel(g_async_llid))
    msg_delete_channel(g_async_llid);
  g_async_llid = 0;
  if (g_sync >= 0)
    close(g_sync);
  g_sync = g_async = -1;
  spawn();
}

static int async_rx(int llid, int fd)
{
  static char buf[AIR_LINE_BUF];
  static int len;
  if (air_lines(fd, buf, &len, resp_line))
    {
    len = 0;
    medium_gone();
    }
  return 0;
}

static void async_err(int llid, int err, int from)
{
  medium_gone();
}

static void spawn(void)
{
  int sy[2], as[2], fd;
  pid_t pid;
  char rank[16], a5[16], a6[16];
  char *argv[8];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sy) ||
      socketpair(AF_UNIX, SOCK_STREAM, 0, as))
    KOUT("ERROR air: socketpair: %s", strerror(errno));
  pid = fork();
  if (pid < 0)
    KOUT("ERROR air: fork: %s", strerror(errno));
  if (pid == 0)
    {
    for (fd = 3; fd < 1024; fd++)
      if ((fd != sy[1]) && (fd != as[1]))
        close(fd);
    snprintf(rank, sizeof(rank), "%d", get_cloonix_rank());
    snprintf(a5, sizeof(a5), "%d", sy[1]);
    snprintf(a6, sizeof(a6), "%d", as[1]);
    argv[0] = "cloonix-suid-power-air";
    argv[1] = get_net_name();
    argv[2] = get_root_path();
    argv[3] = pthexec_usr_libexec_cloonix();
    argv[4] = rank;
    argv[5] = a5;
    argv[6] = a6;
    argv[7] = NULL;
    execv("/proc/self/exe", argv);
    _exit(1);
    }
  close(sy[1]);
  close(as[1]);
  g_sync = sy[0];
  g_async = as[0];
  fcntl(g_sync, F_SETFD, FD_CLOEXEC);
  fcntl(g_async, F_SETFD, FD_CLOEXEC);
  g_async_llid = msg_watch_fd(g_async, async_rx, async_err, "air");
  if (g_async_llid <= 0)
    KOUT("ERROR air: the pipe of the air process is not watched");
}

static int run(char *cmd)
{
  FILE *fp;
  int pid, st, result = 0;
  char line[MAX_PATH_LEN];
  fp = cmd_lio_popen(cmd, &pid, 0);
  if (fp == NULL)
    {
    KERR("ERROR %s", cmd);
    return -1;
    }
  if (fgets(line, MAX_PATH_LEN-1, fp))
    {
    KERR("ERROR %s: %s", cmd, line);
    result = -1;
    }
  fclose(fp);
  if (force_waitpid(pid, &st))
    {
    KERR("ERROR %s: exit status %d", cmd, st);
    result = -1;
    }
  return result;
}

/* the radio made by the medium, moved into the container as wlan<idx> */
static int radio_move(int wiphy, char *nspacecrun, int idx)
{
  char cmd[2*MAX_PATH_LEN];
  snprintf(cmd, sizeof(cmd), "%s --net=%s%s_%s %s phy#%d set netns name %s",
           pthexec_nsenter_bin(), PATH_NAMESPACE, BASE_NAMESPACE,
           get_net_name(), pthexec_iw_bin(), wiphy, nspacecrun);
  if (run(cmd))
    return -1;
  snprintf(cmd, sizeof(cmd),
           "%s --net=%s%s %s phy#%d interface add wlan%d type managed",
           pthexec_nsenter_bin(), PATH_NAMESPACE, nspacecrun,
           pthexec_iw_bin(), wiphy, idx);
  return run(cmd);
}

char *vwifi_why_no_radio(void)
{
  return g_why;
}

/* The medium makes the radios, suid-power moves them: crun goes on with
   the radios in place or none. */
int vwifi_radios_create(char *cnt, int vm_id, char *nspacecrun, int nb,
                        uint8_t *mac)
{
  static char buf[AIR_LINE_BUF];
  struct pollfd pfd;
  char *nl, *p;
  int i, n, wiphy, len = 0;
  uint64_t end = cloonix_get_msec() + CREATE_WAIT_MS;
  memset(g_why, 0, MAX_NAME_LEN);
  strncpy(g_why, "air_process_no_answer", MAX_NAME_LEN-1);
  if (g_sync < 0)
    return -1;
  air_line_send(g_sync, "create %s %d %d %02x%02x%02x%02x%02x%02x", cnt,
                vm_id, nb, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  while (cloonix_get_msec() < end)
    {
    pfd.fd = g_sync;
    pfd.events = POLLIN;
    if (poll(&pfd, 1, 100) <= 0)
      continue;
    n = read(g_sync, buf + len, sizeof(buf) - 1 - len);
    if ((n < 0) && (errno == EINTR))
      continue;
    if (n <= 0)
      {
      KERR("ERROR air: %s: the air process closed during the create", cnt);
      return -1;
      }
    len += n;
    buf[len] = 0;
    nl = strchr(buf, '\n');
    if (!nl)
      continue;
    *nl = 0;
    if (!strncmp(buf, "ok", 2))
      {
      p = buf + 2;
      for (i = 0; i < nb; i++)
        {
        if ((sscanf(p, " %d%n", &wiphy, &n) != 1) ||
            radio_move(wiphy, nspacecrun, i))
          {
          memset(g_why, 0, MAX_NAME_LEN);
          strncpy(g_why, "radio_not_moved_into_the_container",
                  MAX_NAME_LEN-1);
          vwifi_radios_delete(cnt);
          return -1;
          }
        p += n;
        }
      return 0;
      }
    if (!strncmp(buf, "ko ", 3))
      {
      memset(g_why, 0, MAX_NAME_LEN);
      strncpy(g_why, buf + 3, MAX_NAME_LEN-1);
      }
    else
      KERR("ERROR air: %s: the air process answered %s", cnt, buf);
    return -1;
    }
  KERR("ERROR air: %s: no answer from the air process in %d s", cnt,
       CREATE_WAIT_MS / 1000);
  return -1;
}

void vwifi_radios_delete(char *name)
{
  if (g_async >= 0)
    air_line_send(g_async, "delete %s", name);
}

void vwifi_air_msg(int llid, int tid, char *line)
{
  if (g_async >= 0)
    air_line_send(g_async, "air %d %d %s", llid, tid, line);
}

void vwifi_after_suidroot_init(void)
{
  if (g_async >= 0)
    air_line_send(g_async, "suidroot");
}

void vwifi_init(void)
{
  spawn();
}
