/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* cloonix_cxp_serv.c - the guest end of the cloonix exec pipe. There is no
   cryptography here on purpose. */

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#ifdef __FreeBSD__
#include <libutil.h>
#include <sys/types.h>
#include <termios.h>
#else
#include <pty.h>
#endif
#include <pwd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/ioctl.h>

#include "cxp_wire.h"
#include "kerr_gate.h"

/* syslog, one gate per format string; the line also to the agent, for the
   host, dropped when the agent is not there to take it */
static void cxp_log(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
static void cxp_log(const char *fmt, ...)
{
  static t_kerr_gate tab[KERR_GATE_FMT_MAX];
  struct sockaddr_un addr;
  char line[1024], note[64], all[1100];
  va_list ap;
  int fd, len;
  if (!kerr_gate(kerr_gate_of_fmt(tab, fmt), note, sizeof(note)))
    return;
  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  syslog(LOG_ERR | LOG_USER, "cloonix-cxp-srv: %s%s", line, note);
  len = snprintf(all, sizeof(all), "%s%s", line, note);
  if (len > (int) sizeof(all) - 1)
    len = sizeof(all) - 1;
  fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, UNIX_KERR_SOCK, sizeof(addr.sun_path) - 1);
  if (sendto(fd, all, len, MSG_DONTWAIT | MSG_NOSIGNAL,
             (struct sockaddr *) &addr, sizeof(addr)) != len)
    syslog(LOG_ERR | LOG_USER, "cloonix-cxp-srv: WARNING the line above did "
           "not reach the agent: %s", strerror(errno));
  close(fd);
}

#define UNIX_CXP_SOCK "/cloonixmnt/tmp/unix_cloonix_cxp_sock"
#define MAX_FRAME     (1024 * 1024)
#define CHUNK         32768

static int write_all(int fd, const void *buf, int len)
{
  const char *p = (const char *) buf;
  int done = 0, n;
  while (done < len)
    {
    n = write(fd, p + done, len - done);
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

static int send_frame(int fd, int op, const void *payload, int len)
{
  unsigned char head[5];
  int tot = len + 1;
  head[0] = (tot >> 24) & 0xFF;
  head[1] = (tot >> 16) & 0xFF;
  head[2] = (tot >> 8)  & 0xFF;
  head[3] = tot & 0xFF;
  head[4] = op & 0xFF;
  if (write_all(fd, head, 5))
    return -1;
  if (len && write_all(fd, payload, len))
    return -1;
  return 0;
}

static int send_txt(int fd, int op, const char *fmt, ...)
{
  char buf[1024];
  va_list ap;
  int len;
  va_start(ap, fmt);
  len = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (len < 0)
    return -1;
  if (len > (int) sizeof(buf) - 1)
    len = sizeof(buf) - 1;
  return send_frame(fd, op, buf, len);
}

static int recv_frame(int fd, char **payload, int *plen)
{
  unsigned char head[5];
  int got = 0, n, tot;
  char *buf;
  *payload = NULL;
  *plen = 0;
  while (got < 5)
    {
    n = read(fd, head + got, 5 - got);
    if (n == 0)
      return -1;
    if (n < 0)
      {
      if (errno == EINTR)
        continue;
      return -1;
      }
    got += n;
    }
  tot = (head[0] << 24) | (head[1] << 16) | (head[2] << 8) | head[3];
  if ((tot < 1) || (tot > MAX_FRAME))
    return -1;
  *plen = tot - 1;
  if (*plen)
    {
    buf = (char *) malloc(*plen + 1);
    if (!buf)
      {
      cxp_log("WARNING recv: no memory for %d bytes", (int) *plen);
      return -1;
      }
    got = 0;
    while (got < *plen)
      {
      n = read(fd, buf + got, *plen - got);
      if (n <= 0)
        {
        if ((n < 0) && (errno == EINTR))
          continue;
        free(buf);
        return -1;
        }
      got += n;
      }
    buf[*plen] = 0;
    *payload = buf;
    }
  return head[4];
}

static void expand_home(char *path, int sz)
{
  char out[4096];
  struct passwd *pw;
  const char *home = getenv("HOME");
  if (path[0] != '~')
    return;
  if ((path[1]) && (path[1] != '/'))
    return;
  if ((!home) || (!home[0]))
    {
    pw = getpwuid(getuid());
    home = ((pw) && (pw->pw_dir) && (pw->pw_dir[0])) ? pw->pw_dir : "/root";
    }
  snprintf(out, sizeof(out), "%.2000s%.2000s", home, path + 1);
  snprintf(path, sz, "%s", out);
}

static void path_join(char *dst, int sz, const char *dir, const char *leaf)
{
  int len = (int) strlen(dir);
  if ((len > 0) && (dir[len-1] == '/'))
    snprintf(dst, sz, "%.4000s%.1000s", dir, leaf);
  else
    snprintf(dst, sz, "%.4000s/%.1000s", dir, leaf);
}

static int mkdir_p(const char *path, int mode)
{
  char tmp[4096];
  char *p;
  snprintf(tmp, sizeof(tmp), "%s", path);
  for (p = tmp + 1; *p; p++)
    {
    if (*p == '/')
      {
      *p = 0;
      if ((mkdir(tmp, 0755)) && (errno != EEXIST))
        {
        cxp_log("WARNING mkdir %s: %s", tmp, strerror(errno));
        return -1;
        }
      *p = '/';
      }
    }
  if ((mkdir(tmp, mode & 0777)) && (errno != EEXIST))
    {
    cxp_log("WARNING mkdir %s: %s", tmp, strerror(errno));
    return -1;
    }
  return 0;
}

static void set_winsize(int fd, int cols, int rows)
{
  struct winsize ws;
  memset(&ws, 0, sizeof(ws));
  ws.ws_col = (cols > 0) ? cols : 80;
  ws.ws_row = (rows > 0) ? rows : 24;
  ioctl(fd, TIOCSWINSZ, &ws);
}

static void child_setenv(const char **out_shell)
{
  struct passwd *pw = getpwuid(getuid());
  const char *home  = "/root";
  const char *user  = "root";
  const char *shell = "/bin/sh";

  if (pw)
    {
    if ((pw->pw_dir) && (pw->pw_dir[0]))
      home = pw->pw_dir;
    if ((pw->pw_name) && (pw->pw_name[0]))
      user = pw->pw_name;
    if ((pw->pw_shell) && (pw->pw_shell[0]) && (!access(pw->pw_shell, X_OK)))
      shell = pw->pw_shell;
    }
  if ((!strcmp(shell, "/bin/sh")) && (!access("/bin/bash", X_OK)))
    shell = "/bin/bash";

  setenv("HOME", home, 1);
  setenv("USER", user, 1);
  setenv("LOGNAME", user, 1);
  setenv("SHELL", shell, 1);
  /* Only when the guest has not provided one: a command run with -c does
     not read /etc/profile. */
  setenv("PATH",
         "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 0);
  if (chdir(home))
    {
    if (chdir("/"))
      { /* nothing better to try */ }
    home = "/";
    }
  setenv("PWD", home, 1);
  *out_shell = shell;
}

/* In a container the server is protected from the oom killer and what
   it forks inherits that: what the user runs is not. */
static void oom_unprotect(void)
{
  int fd = open("/proc/self/oom_score_adj", O_WRONLY);
  if (fd >= 0)
    {
    if (write(fd, "0\n", 2) != 2)
      cxp_log("WARNING oom_score_adj not set back to 0: %s", strerror(errno));
    close(fd);
    }
}

static void child_exec(const char *cmd)
{
  const char *shell = "/bin/sh";
  setsid();
  oom_unprotect();
  child_setenv(&shell);
  if ((cmd) && (cmd[0]))
    execl(shell, shell, "-c", cmd, (char *) NULL);
  else
    execl(shell, shell, "-l", (char *) NULL);
  _exit(127);
}

/* leg_gone: kill the setsid session as ssh does; else no kill, which could
   race the exit and report 255. */
static void end_child(pid_t pid, int *status, int reaped, int leg_gone)
{
  int i;
  if (reaped)
    return;
  if (!leg_gone)
    {
    while ((waitpid(pid, status, 0) < 0) && (errno == EINTR))
      ;
    return;
    }
  kill(-pid, SIGHUP);
  kill(pid, SIGHUP);
  for (i = 0; i < 20; i++)
    {
    if (waitpid(pid, status, WNOHANG) == pid)
      return;
    usleep(100000);
    }
  kill(-pid, SIGKILL);
  kill(pid, SIGKILL);
  while ((waitpid(pid, status, 0) < 0) && (errno == EINTR))
    ;
}

static void do_exec(int fd, const char *body)
{
  char cmd[8192];
  int outp[2], errp[2], inp[2], status = 0;
  int reaped = 0, leg_gone = 0;
  pid_t pid;
  cxp_json_str(body, "cmd", cmd, sizeof(cmd));
  if (pipe(outp))
    {
    cxp_log("WARNING exec: pipe: %s", strerror(errno));
    send_txt(fd, OP_ERROR, "{\"txt\":\"pipe failed\"}");
    return;
    }
  if (pipe(errp))
    {
    cxp_log("WARNING exec: pipe: %s", strerror(errno));
    close(outp[0]); close(outp[1]);
    send_txt(fd, OP_ERROR, "{\"txt\":\"pipe failed\"}");
    return;
    }
  if (pipe(inp))
    {
    cxp_log("WARNING exec: pipe: %s", strerror(errno));
    close(outp[0]); close(outp[1]); close(errp[0]); close(errp[1]);
    send_txt(fd, OP_ERROR, "{\"txt\":\"pipe failed\"}");
    return;
    }
  pid = fork();
  if (pid < 0)
    {
    cxp_log("WARNING exec: fork: %s", strerror(errno));
    close(outp[0]); close(outp[1]); close(errp[0]); close(errp[1]);
    close(inp[0]); close(inp[1]);
    send_txt(fd, OP_ERROR, "{\"txt\":\"fork failed\"}");
    return;
    }
  if (pid == 0)
    {
    dup2(inp[0], 0);
    dup2(outp[1], 1);
    dup2(errp[1], 2);
    close(inp[1]); close(outp[0]); close(errp[0]);
    close(inp[0]); close(outp[1]); close(errp[1]);
    child_exec(cmd);
    }
  close(inp[0]); close(outp[1]); close(errp[1]);
  for (;;)
    {
    fd_set rfds;
    struct timeval tv;
    struct timeval *ptv = NULL;
    int maxfd = fd;
    char buf[CHUNK];
    int n;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    if (outp[0] >= 0) { FD_SET(outp[0], &rfds); if (outp[0] > maxfd) maxfd = outp[0]; }
    if (errp[0] >= 0) { FD_SET(errp[0], &rfds); if (errp[0] > maxfd) maxfd = errp[0]; }
    if ((outp[0] < 0) && (errp[0] < 0))
      break;
    if ((!reaped) && (waitpid(pid, &status, WNOHANG) == pid))
      reaped = 1;
    /* The wait is always bounded, never indefinite. */
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    ptv = &tv;
    n = select(maxfd + 1, &rfds, NULL, NULL, ptv);
    if (n < 0)
      {
      if (errno == EINTR)
        continue;
      break;
      }
    if ((n == 0) && (reaped))
      break;
    if ((outp[0] >= 0) && (FD_ISSET(outp[0], &rfds)))
      {
      n = read(outp[0], buf, sizeof(buf));
      if (n > 0)
        { if (send_frame(fd, OP_STDOUT, buf, n)) { leg_gone = 1; break; } }
      else
        { close(outp[0]); outp[0] = -1; }
      }
    if ((errp[0] >= 0) && (FD_ISSET(errp[0], &rfds)))
      {
      n = read(errp[0], buf, sizeof(buf));
      if (n > 0)
        { if (send_frame(fd, OP_STDERR, buf, n)) { leg_gone = 1; break; } }
      else
        { close(errp[0]); errp[0] = -1; }
      }
    if (FD_ISSET(fd, &rfds))
      {
      char *pl = NULL;
      int pl_len = 0;
      int op = recv_frame(fd, &pl, &pl_len);
      if (op < 0)
        { if (pl) free(pl); leg_gone = 1; break; }
      if ((op == OP_STDIN) && (pl_len))
        write_all(inp[1], pl, pl_len);
      else if (op == OP_EOF)
        { close(inp[1]); inp[1] = -1; }
      if (pl)
        free(pl);
      }
    }
  if (inp[1] >= 0)
    close(inp[1]);
  if (outp[0] >= 0)
    close(outp[0]);
  if (errp[0] >= 0)
    close(errp[0]);
  end_child(pid, &status, reaped, leg_gone);
  /* A command killed by a signal is reported the way a shell reports it,
     128 plus the signal, rather than as a flat 255 that says nothing. */
  send_txt(fd, OP_EXIT, "{\"code\":%d}",
           WIFEXITED(status) ? WEXITSTATUS(status) :
           (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 255));
}

static void do_pty(int fd, const char *body)
{
  char cmd[8192];
  int master = -1, status = 0, leg_gone = 0;
  pid_t pid;
  int cols = cxp_json_int(body, "cols", 80);
  int rows = cxp_json_int(body, "rows", 24);
  cxp_json_str(body, "cmd", cmd, sizeof(cmd));
  pid = forkpty(&master, NULL, NULL, NULL);
  if (pid < 0)
    {
    cxp_log("WARNING pty: forkpty: %s", strerror(errno));
    send_txt(fd, OP_ERROR, "{\"txt\":\"forkpty failed\"}");
    return;
    }
  if (pid == 0)
    {
    setenv("TERM", "xterm-256color", 1);
    child_exec(cmd);
    }
  set_winsize(master, cols, rows);
  /* As in do_exec: a shell that ended and a client that vanished are not
     the same ending, and only the second justifies killing the session. */
  leg_gone = 0;
  for (;;)
    {
    fd_set rfds;
    int maxfd = (fd > master) ? fd : master;
    char buf[CHUNK];
    int n;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    FD_SET(master, &rfds);
    if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0)
      {
      if (errno == EINTR)
        continue;
      break;
      }
    if (FD_ISSET(master, &rfds))
      {
      n = read(master, buf, sizeof(buf));
      if (n <= 0)
        break;
      if (send_frame(fd, OP_STDOUT, buf, n))
        { leg_gone = 1; break; }
      }
    if (FD_ISSET(fd, &rfds))
      {
      char *pl = NULL;
      int pl_len = 0;
      int op = recv_frame(fd, &pl, &pl_len);
      if (op < 0)
        { if (pl) free(pl); leg_gone = 1; break; }
      if ((op == OP_STDIN) && (pl_len))
        write_all(master, pl, pl_len);
      else if (op == OP_WINCH)
        set_winsize(master, cxp_json_int(pl, "cols", cols),
                            cxp_json_int(pl, "rows", rows));
      else if (op == OP_EOF)
        {
        char eot = 4;
        write_all(master, &eot, 1);
        }
      if (pl)
        free(pl);
      }
    }
  close(master);
  end_child(pid, &status, 0, leg_gone);
  send_txt(fd, OP_EXIT, "{\"code\":%d}",
           WIFEXITED(status) ? WEXITSTATUS(status) :
           (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 0));
}

static void swallow_rest(int fd)
{
  char *pl;
  int pl_len, op;
  for (;;)
    {
    op = recv_frame(fd, &pl, &pl_len);
    if (pl)
      free(pl);
    if ((op < 0) || (op == OP_END))
      return;
    }
}

static void do_put(int fd, const char *body)
{
  char path[4096];
  char name[1024];
  char joined[4096];
  struct stat st;
  int mode = cxp_json_int(body, "mode", 0644);
  int out;
  cxp_json_str(body, "path", path, sizeof(path));
  expand_home(path, sizeof(path));
  cxp_json_str(body, "name", name, sizeof(name));
  if (!path[0])
    {
    swallow_rest(fd);
    send_txt(fd, OP_ERROR, "{\"txt\":\"no path\"}");
    return;
    }
  if ((!stat(path, &st)) && (S_ISDIR(st.st_mode)))
    {
    char *base = strrchr(name, '/');
    base = base ? base + 1 : name;
    if ((!base[0]) || (!strcmp(base, ".")) || (!strcmp(base, "..")))
      {
      swallow_rest(fd);
      send_txt(fd, OP_ERROR, "{\"txt\":\"%s is a directory and no usable "
                             "file name was given\"}", path);
      return;
      }
    path_join(joined, sizeof(joined), path, base);
    memcpy(path, joined, sizeof(joined));
    }
  out = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode & 0777);
  if (out < 0)
    {
    int why = errno;
    cxp_log("WARNING put %s: %s", path, strerror(why));
    swallow_rest(fd);
    send_txt(fd, OP_ERROR, "{\"txt\":\"%s: %s\"}", path, strerror(why));
    return;
    }
  for (;;)
    {
    char *pl = NULL;
    int pl_len = 0;
    int op = recv_frame(fd, &pl, &pl_len);
    if (op < 0)
      { if (pl) free(pl); close(out); return; }
    if (op == OP_DATA)
      {
      if ((pl_len) && (write_all(out, pl, pl_len)))
        {
        int why = errno;
        free(pl);
        close(out);
        swallow_rest(fd);
        send_txt(fd, OP_ERROR, "{\"txt\":\"write failed: %s\"}",
                 strerror(why));
        return;
        }
      }
    else if (op == OP_END)
      { if (pl) free(pl); break; }
    if (pl)
      free(pl);
    }
  close(out);
  chmod(path, mode & 0777);
  send_txt(fd, OP_EXIT, "{\"code\":0,\"path\":\"%s\"}", path);
}

static void do_get(int fd, const char *body)
{
  char path[4096];
  char buf[CHUNK];
  struct stat st;
  int in, n;
  long sent = 0;
  cxp_json_str(body, "path", path, sizeof(path));
  expand_home(path, sizeof(path));
  if (!path[0])
    {
    send_txt(fd, OP_ERROR, "{\"txt\":\"no path\"}");
    return;
    }
  if (stat(path, &st))
    {
    send_txt(fd, OP_ERROR, "{\"txt\":\"%s: %s\"}", path, strerror(errno));
    return;
    }
  if (S_ISDIR(st.st_mode))
    {
    send_txt(fd, OP_ERROR, "{\"txt\":\"%s is a directory\"}", path);
    return;
    }
  in = open(path, O_RDONLY);
  if (in < 0)
    {
    int why = errno;
    cxp_log("WARNING get %s: %s", path, strerror(why));
    send_txt(fd, OP_ERROR, "{\"txt\":\"%s: %s\"}", path, strerror(why));
    return;
    }
  send_txt(fd, OP_META, "{\"size\":%ld,\"mode\":%d}",
           (long) st.st_size, (int) (st.st_mode & 0777));
  for (;;)
    {
    n = read(in, buf, sizeof(buf));
    if (n < 0)
      {
      if (errno == EINTR)
        continue;
      break;
      }
    if (n == 0)
      break;
    if (send_frame(fd, OP_GDATA, buf, n))
      {
      cxp_log("WARNING get %s: the pipe to the agent broke after %ld of %ld "
              "bytes: %s", path,
             (long) sent, (long) st.st_size, strerror(errno));
      close(in);
      return;
      }
    sent += n;
    }
  close(in);
  send_frame(fd, OP_GEND, NULL, 0);
}

/* PUTTREE: a whole directory arrives, one ENTRY per member. */
static void do_put_tree(int fd, const char *body)
{
  char root[4096], name[1024], type[8], link[1024], full[5200];
  int cur = -1, mode;
  char *pl;
  int pl_len, op;

  cxp_json_str(body, "path", root, sizeof(root));
  expand_home(root, sizeof(root));
  if (!root[0])
    {
    swallow_rest(fd);
    send_txt(fd, OP_ERROR, "{\"txt\":\"no path\"}");
    return;
    }
  if (mkdir_p(root, 0755))
    {
    int why = errno;
    swallow_rest(fd);
    send_txt(fd, OP_ERROR, "{\"txt\":\"%s: %s\"}", root, strerror(why));
    return;
    }
  for (;;)
    {
    pl = NULL;
    pl_len = 0;
    op = recv_frame(fd, &pl, &pl_len);
    if (op < 0)
      break;
    if (op == OP_ENTRY)
      {
      if (cur >= 0)
        {
        close(cur);
        cur = -1;
        }
      cxp_json_str(pl, "name", name, sizeof(name));
      cxp_json_str(pl, "type", type, sizeof(type));
      mode = cxp_json_int(pl, "mode", 0644);
      if (cxp_safe_rel_path(name))
        {
        free(pl);
        swallow_rest(fd);
        send_txt(fd, OP_ERROR, "{\"txt\":\"refused member name\"}");
        return;
        }
      path_join(full, sizeof(full), root, name);
      if (!strcmp(type, "d"))
        {
        if (mkdir_p(full, mode))
          {
          int why = errno;
          free(pl);
          swallow_rest(fd);
          send_txt(fd, OP_ERROR, "{\"txt\":\"%s: %s\"}", full,
                   strerror(why));
          return;
          }
        }
      else if (!strcmp(type, "l"))
        {
        cxp_json_str(pl, "link", link, sizeof(link));
        unlink(full);
        if (symlink(link, full))
          { /* a symlink that cannot be made is not fatal for the tree */ }
        }
      else
        {
        char dir[5200];
        char *slash;
        snprintf(dir, sizeof(dir), "%s", full);
        slash = strrchr(dir, '/');
        if (slash)
          {
          *slash = 0;
          mkdir_p(dir, 0755);
          }
        cur = open(full, O_WRONLY | O_CREAT | O_TRUNC, mode & 0777);
        if (cur < 0)
          {
          int why = errno;
          cxp_log("WARNING put %s: %s", full, strerror(why));
          free(pl);
          swallow_rest(fd);
          send_txt(fd, OP_ERROR, "{\"txt\":\"%s: %s\"}", full,
                   strerror(why));
          return;
          }
        }
      }
    else if (op == OP_DATA)
      {
      if ((cur >= 0) && (pl_len) && (write_all(cur, pl, pl_len)))
        {
        int why = errno;
        free(pl);
        close(cur);
        swallow_rest(fd);
        send_txt(fd, OP_ERROR, "{\"txt\":\"write failed: %s\"}",
                 strerror(why));
        return;
        }
      }
    else if (op == OP_END)
      {
      if (pl)
        free(pl);
      break;
      }
    if (pl)
      free(pl);
    }
  if (cur >= 0)
    close(cur);
  send_txt(fd, OP_EXIT, "{\"code\":0}");
}

static int send_one_entry(int fd, const char *parent, const char *rel)
{
  char full[5200], esc[2100], target[1024], buf[CHUNK];
  struct stat st;
  int in, n;

  path_join(full, sizeof(full), parent, rel);
  if (lstat(full, &st))
    return 0;
  cxp_json_escape(rel, esc, sizeof(esc));

  if (S_ISDIR(st.st_mode))
    {
    DIR *d;
    struct dirent *de;
    char child[2100];
    if (send_txt(fd, OP_GENTRY, "{\"name\":\"%s\",\"type\":\"d\","
                                "\"mode\":%d}", esc, (int)(st.st_mode & 0777)))
      return -1;
    d = opendir(full);
    if (!d)
      {
      cxp_log("WARNING get %s: %s", full, strerror(errno));
      return 0;
      }
    while ((de = readdir(d)) != NULL)
      {
      if ((!strcmp(de->d_name, ".")) || (!strcmp(de->d_name, "..")))
        continue;
      snprintf(child, sizeof(child), "%.1000s/%.1000s", rel, de->d_name);
      if (send_one_entry(fd, parent, child))
        {
        closedir(d);
        return -1;
        }
      }
    closedir(d);
    return 0;
    }

  if (S_ISLNK(st.st_mode))
    {
    char tesc[2100];
    n = readlink(full, target, sizeof(target) - 1);
    if (n < 0)
      return 0;
    target[n] = 0;
    cxp_json_escape(target, tesc, sizeof(tesc));
    return send_txt(fd, OP_GENTRY, "{\"name\":\"%s\",\"type\":\"l\","
                                   "\"link\":\"%s\"}", esc, tesc);
    }

  if (!S_ISREG(st.st_mode))
    return 0;                  /* sockets, fifos and devices are skipped */

  if (send_txt(fd, OP_GENTRY, "{\"name\":\"%s\",\"type\":\"f\","
                              "\"mode\":%d,\"size\":%ld}",
               esc, (int)(st.st_mode & 0777), (long) st.st_size))
    return -1;
  in = open(full, O_RDONLY);
  if (in < 0)
    {
    cxp_log("WARNING get %s: %s", full, strerror(errno));
    return 0;
    }
  for (;;)
    {
    n = read(in, buf, sizeof(buf));
    if (n < 0)
      {
      if (errno == EINTR)
        continue;
      break;
      }
    if (n == 0)
      break;
    if (send_frame(fd, OP_GDATA, buf, n))
      {
      cxp_log("WARNING gettree %s: the pipe to the agent broke: %s", full,
              strerror(errno));
      close(in);
      return -1;
      }
    }
  close(in);
  return 0;
}

static void do_get_tree(int fd, const char *body)
{
  char path[4096], parent[4096], base[1024];
  char *slash;
  struct stat st;

  cxp_json_str(body, "path", path, sizeof(path));
  expand_home(path, sizeof(path));
  if (!path[0])
    {
    send_txt(fd, OP_ERROR, "{\"txt\":\"no path\"}");
    return;
    }
  while ((strlen(path) > 1) && (path[strlen(path) - 1] == '/'))
    path[strlen(path) - 1] = 0;
  if (stat(path, &st))
    {
    send_txt(fd, OP_ERROR, "{\"txt\":\"%s: %s\"}", path, strerror(errno));
    return;
    }
  snprintf(parent, sizeof(parent), "%s", path);
  slash = strrchr(parent, '/');
  if ((!slash) || (!slash[1]))
    {
    send_txt(fd, OP_ERROR, "{\"txt\":\"%s has no parent to copy from\"}",
             path);
    return;
    }
  *slash = 0;
  snprintf(base, sizeof(base), "%s", slash + 1);
  if (!parent[0])
    snprintf(parent, sizeof(parent), "/");
  if (send_one_entry(fd, parent, base))
    return;
  send_frame(fd, OP_GEND, NULL, 0);
}

static void do_list(int fd, const char *body)
{
  char path[4096], full[5200], esc[2100], target[1024], tesc[2100];
  char pesc[8200];
  struct dirent *de;
  struct stat st;
  DIR *d;
  int n, len;

  cxp_json_str(body, "path", path, sizeof(path));
  expand_home(path, sizeof(path));
  if (!path[0])
    snprintf(path, sizeof(path), "/");
  len = (int) strlen(path);
  while ((len > 1) && (path[len-1] == '/'))
    path[--len] = 0;
  d = opendir(path);
  if (!d)
    {
    int why = errno;
    cxp_log("WARNING list %s: %s", path, strerror(why));
    send_txt(fd, OP_ERROR, "{\"txt\":\"%s: %s\"}", path, strerror(why));
    return;
    }
  while ((de = readdir(d)) != NULL)
    {
    if ((!strcmp(de->d_name, ".")) || (!strcmp(de->d_name, "..")))
      continue;
    path_join(full, sizeof(full), path, de->d_name);
    if (lstat(full, &st))
      continue;
    cxp_json_escape(de->d_name, esc, sizeof(esc));
    if (S_ISDIR(st.st_mode))
      n = send_txt(fd, OP_GENTRY, "{\"name\":\"%s\",\"type\":\"d\",\"mode\":%d}",
                   esc, (int)(st.st_mode & 0777));
    else if (S_ISLNK(st.st_mode))
      {
      int ln = readlink(full, target, sizeof(target) - 1);
      target[(ln > 0) ? ln : 0] = 0;
      cxp_json_escape(target, tesc, sizeof(tesc));
      n = send_txt(fd, OP_GENTRY, "{\"name\":\"%s\",\"type\":\"l\",\"link\":\"%s\"}",
                   esc, tesc);
      }
    else
      n = send_txt(fd, OP_GENTRY, "{\"name\":\"%s\",\"type\":\"f\",\"mode\":%d,"
                                  "\"size\":%ld}",
                   esc, (int)(st.st_mode & 0777), (long) st.st_size);
    if (n)
      {
      closedir(d);
      return;
      }
    }
  closedir(d);
  if (send_frame(fd, OP_GEND, NULL, 0))
    return;
  cxp_json_escape(path, pesc, sizeof(pesc));
  send_txt(fd, OP_EXIT, "{\"code\":0,\"path\":\"%s\"}", pesc);
}

static void serve_one(int fd)
{
  char *pl = NULL;
  int pl_len = 0;
  int op = recv_frame(fd, &pl, &pl_len);
  if (op < 0)
    {
    if (pl)
      free(pl);
    return;
    }
  switch (op)
    {
    case OP_EXEC: do_exec(fd, pl); break;
    case OP_PTY:  do_pty(fd, pl);  break;
    case OP_PUT:  do_put(fd, pl);  break;
    case OP_GET:  do_get(fd, pl);  break;
    case OP_PUTTREE: do_put_tree(fd, pl); break;
    case OP_GETTREE: do_get_tree(fd, pl); break;
    case OP_LIST: do_list(fd, pl); break;
    default:
      cxp_log("WARNING unexpected first opcode %d", op);
      send_txt(fd, OP_ERROR, "{\"txt\":\"unexpected first opcode %d\"}", op);
      break;
    }
  if (pl)
    free(pl);
}

static int listen_unix(const char *path)
{
  struct sockaddr_un addr;
  int fd;
  unlink(path);
  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    {
    cxp_log("WARNING listen socket: %s", strerror(errno));
    return -1;
    }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, "%s", path);
  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)))
    { close(fd); return -1; }
  if (chmod(path, 0666))
    { /* best effort: the agent may run as another user */ }
  if (listen(fd, 50))
    { close(fd); return -1; }
  return fd;
}

static void sigchld_handler(int sig)
{
  int saved = errno;
  (void) sig;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
  errno = saved;
}

int main(int argc, char **argv)
{
  const char *path = UNIX_CXP_SOCK;
  int listen_fd, conn_fd;
  int foreground = 0;
  int i;

  for (i = 1; i < argc; i++)
    {
    if (!strcmp(argv[i], "-f"))
      foreground = 1;
    else
      path = argv[i];
    }

  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, sigchld_handler);

  listen_fd = listen_unix(path);
  if (listen_fd < 0)
    {
    fprintf(stderr, "cloonix-cxp-srv: cannot listen on %s: %s\n",
            path, strerror(errno));
    cxp_log("ERROR cannot listen on %s: %s", path, strerror(errno));
    return 1;
    }
  if ((!foreground) && (daemon(0, 0) < 0))
    {
    fprintf(stderr, "cloonix-cxp-srv: cannot daemonize: %s\n",
            strerror(errno));
    return 1;
    }
  for (;;)
    {
    conn_fd = accept(listen_fd, NULL, NULL);
    if (conn_fd < 0)
      {
      if ((errno == EINTR) || (errno == ECONNABORTED))
        continue;
      /* out of descriptors or memory: wait, the server must live */
      if ((errno == EMFILE) || (errno == ENFILE) ||
          (errno == ENOBUFS) || (errno == ENOMEM))
        {
        cxp_log("WARNING accept: %s, retrying", strerror(errno));
        usleep(100000);
        continue;
        }
      cxp_log("ERROR the server stops, accept: %s", strerror(errno));
      break;
      }
    i = fork();
    if (i < 0)
      cxp_log("WARNING fork for a connection: %s, closed", strerror(errno));
    else if (i == 0)
      {
      close(listen_fd);
      signal(SIGCHLD, SIG_DFL);
      serve_one(conn_fd);
      close(conn_fd);
      _exit(0);
      }
    close(conn_fd);
    }
  close(listen_fd);
  return 0;
}
