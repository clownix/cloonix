/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fsuid.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "pthexec.h"
#include "guard.h"

/* The rule of this file: a request reaching suid_power comes from the net
   owner, and the net owner is not root. */

static uid_t g_uid;
static gid_t g_gid;
static char  g_root[MAX_PATH_LEN];
static char  g_net[MAX_NAME_LEN];

void guard_init(uid_t uid, gid_t gid, char *net, char *root_path)
{
  g_uid = uid;
  g_gid = gid;
  memset(g_net, 0, MAX_NAME_LEN);
  memset(g_root, 0, MAX_PATH_LEN);
  strncpy(g_net, net, MAX_NAME_LEN-1);
  strncpy(g_root, root_path, MAX_PATH_LEN-1);
}

int guard_group_allows(uid_t uid)
{
  struct group *gr;
  struct passwd *pw;
  gid_t *list;
  int i, n = 64, ok = 0;
  if (uid == 0)
    return 1;
  gr = getgrnam(CLOONIX_GROUP);
  if (gr == NULL)
    return 1;
  pw = getpwuid(uid);
  if (pw == NULL)
    return 0;
  if (pw->pw_gid == gr->gr_gid)
    return 1;
  list = (gid_t *) malloc(n * sizeof(gid_t));
  if (list == NULL)
    KOUT("ERROR malloc");
  if (getgrouplist(pw->pw_name, pw->pw_gid, list, &n) == -1)
    {
    free(list);
    list = (gid_t *) malloc(n * sizeof(gid_t));
    if (list == NULL)
      KOUT("ERROR malloc");
    if (getgrouplist(pw->pw_name, pw->pw_gid, list, &n) == -1)
      n = 0;
    }
  for (i=0; i<n; i++)
    if (list[i] == gr->gr_gid)
      ok = 1;
  free(list);
  return ok;
}

int guard_path_in_root(char *path)
{
  int len = strlen(g_root);
  if ((!path) || (path[0] != '/'))
    return 0;
  if (strstr(path, ".."))
    return 0;
  if (strncmp(path, g_root, len))
    return 0;
  if (path[len] != '/')
    return 0;
  return 1;
}

/* Can the caller reach this path with their own credentials? */
int guard_caller_can_access(char *path, int mode)
{
  struct stat st;
  char dir[MAX_PATH_LEN];
  char *ptr;
  int result;
  if ((!path) || (path[0] != '/') || (strstr(path, "..")))
    return 0;
  if (g_uid == 0)
    return 1;
  if (setfsuid(g_uid) < 0)
    {
    KERR("WARNING setfsuid %d: %s", g_uid, strerror(errno));
    return 0;
    }
  setfsgid(g_gid);
  if (stat(path, &st) == 0)
    result = (access(path, mode) == 0);
  else
    {
    memset(dir, 0, MAX_PATH_LEN);
    strncpy(dir, path, MAX_PATH_LEN-1);
    ptr = strrchr(dir, '/');
    if ((ptr) && (ptr != dir))
      *ptr = 0;
    else
      strcpy(dir, "/");
    result = (access(dir, W_OK | X_OK) == 0);
    }
  setfsuid(0);
  setfsgid(0);
  return result;
}

static int guard_qemu_token(char *tok)
{
  char item[MAX_PATH_LEN];
  char *p, *q, *save;
  int mode;
  if (!strchr(tok, '/'))
    return 1;
  memset(item, 0, MAX_PATH_LEN);
  strncpy(item, tok, MAX_PATH_LEN-1);
  p = strtok_r(item, ",", &save);
  while (p)
    {
    q = strrchr(p, '=');
    if (q)
      q += 1;
    else
      q = p;
    if (q[0] == '/')
      {
      mode = R_OK;
      if ((strstr(p, "path=")) || (strstr(p, "pidfile")) ||
          (strstr(p, "unix:")))
        mode = W_OK;
      if (!guard_caller_can_access(q, mode))
        {
        KERR("ERROR REFUSED launch, %s not readable by uid %d", q, (int)g_uid);
        return 0;
        }
      }
    p = strtok_r(NULL, ",", &save);
    }
  return 1;
}

int guard_launch_script(char *script)
{
  char head[2*MAX_PATH_LEN];
  char *buf, *line2, *line3, *tok, *save, *nl;
  struct stat st;
  int fd, len, n, result = 0;
  char bad[] = ";|&`$<()\n\\\"'";
  if (!guard_path_in_root(script))
    {
    KERR("ERROR REFUSED launch script outside the net dir: %s", script);
    return 0;
    }
  if ((lstat(script, &st)) || (!S_ISREG(st.st_mode)) || (st.st_uid != g_uid))
    {
    KERR("ERROR REFUSED launch script not a file of uid %d: %s",
         (int)g_uid, script);
    return 0;
    }
  if (st.st_size >= MAX_TRACE_LEN)
    return 0;
  buf = (char *) malloc(st.st_size + 1);
  memset(buf, 0, st.st_size + 1);
  fd = open(script, O_RDONLY);
  if (fd < 0)
    {
    KERR("WARNING %s: %s", script, strerror(errno));
    free(buf);
    return 0;
    }
  len = read(fd, buf, st.st_size);
  close(fd);
  if (len != st.st_size)
    goto out;
  snprintf(head, sizeof(head)-1, "#!%s\n", pthexec_bash_bin());
  if (strncmp(buf, head, strlen(head)))
    {
    KERR("ERROR REFUSED launch script shebang: %s", script);
    goto out;
    }
  line2 = buf + strlen(head);
  nl = strchr(line2, '\n');
  if (!nl)
    {
    KERR("ERROR REFUSED launch script is one line: %s", script);
    goto out;
    }
  *nl = 0;
  line3 = nl + 1;
  nl = strchr(line3, '\n');
  if ((!nl) || (nl[1] != 0))
    {
    KERR("ERROR REFUSED launch script is not three lines: %s", script);
    goto out;
    }
  *nl = 0;
  snprintf(head, sizeof(head)-1,
           "%s --net=%s%s_%s %s/cloonfs/bin/cloonix-qemu-system -L ",
           pthexec_nsenter_bin(), PATH_NAMESPACE, BASE_NAMESPACE, g_net,
           pthexec_usr_libexec_cloonix());
  if (strncmp(line2, head, strlen(head)))
    {
    KERR("ERROR REFUSED launch line head: %s", line2);
    goto out;
    }
  tok = strrchr(line2, '>');
  if ((!tok) || (tok[-1] != '2') || (tok[-2] != ' ') ||
      (!guard_path_in_root(tok+1)))
    {
    KERR("ERROR REFUSED launch line stderr redirection: %s", line2);
    goto out;
    }
  /* Line 3, the one text allowed: qemu's exit status into the same file. */
  snprintf(head, sizeof(head)-1, "echo qemu exit $? >>%s", tok+1);
  if (strcmp(line3, head))
    {
    KERR("ERROR REFUSED launch script third line: %s", line3);
    goto out;
    }
  tok[-2] = 0;
  for (n=0; bad[n]; n++)
    {
    if (strchr(line2, bad[n]))
      {
      KERR("ERROR REFUSED launch line shell character %c", bad[n]);
      goto out;
      }
    }
  tok = strtok_r(line2 + strlen(head), " ", &save);
  while (tok)
    {
    if (!guard_qemu_token(tok))
      goto out;
    tok = strtok_r(NULL, " ", &save);
    }
  result = 1;
out:
  free(buf);
  return result;
}

int guard_launch_line(char *line)
{
  char copy[MAX_PRINT_LEN];
  char *tok[8];
  char *save, *t;
  int nb = 0;
  memset(copy, 0, MAX_PRINT_LEN);
  strncpy(copy, line, MAX_PRINT_LEN-1);
  t = strtok_r(copy, " ", &save);
  while ((t) && (nb < 8))
    {
    tok[nb++] = t;
    t = strtok_r(NULL, " ", &save);
    }
  if (nb != 5)
    {
    KERR("ERROR REFUSED launch line has %d words", nb);
    return 0;
    }
  if (strcmp(tok[0], pthexec_dtach_bin()))
    {
    KERR("ERROR REFUSED launch not the pinned dtach: %s", tok[0]);
    return 0;
    }
  if (strcmp(tok[1], "-n"))
    return 0;
  if (!guard_path_in_root(tok[2]))
    {
    KERR("ERROR REFUSED launch dtach sock outside net dir: %s", tok[2]);
    return 0;
    }
  if (strcmp(tok[3], pthexec_bash_bin()))
    {
    KERR("ERROR REFUSED launch not the pinned bash: %s", tok[3]);
    return 0;
    }
  return guard_launch_script(tok[4]);
}

/* Killable: a process of the caller, or a root process of this net (its
   work directory is on the command line). */
int guard_pid_is_callers(int pid)
{
  char path[MAX_PATH_LEN];
  char buf[MAX_BIG_BUF];
  char *ptr;
  int fd, len, i;
  unsigned int ruid;
  if (pid <= 1)
    return 0;
  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/status", pid);
  fd = open(path, O_RDONLY);
  if (fd < 0)
    return 0;
  memset(buf, 0, MAX_BIG_BUF);
  len = read(fd, buf, MAX_BIG_BUF-1);
  close(fd);
  if (len <= 0)
    return 0;
  ptr = strstr(buf, "\nUid:");
  if ((ptr) && (sscanf(ptr, "\nUid: %u", &ruid) == 1) && (ruid == g_uid))
    return 1;
  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/cmdline", pid);
  fd = open(path, O_RDONLY);
  if (fd < 0)
    return 0;
  memset(buf, 0, MAX_BIG_BUF);
  len = read(fd, buf, MAX_BIG_BUF-2);
  close(fd);
  for (i=0; i<len; i++)
    if (buf[i] == 0)
      buf[i] = ' ';
  return (strstr(buf, g_root) != NULL);
}
