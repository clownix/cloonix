/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* An idmapped mount of a container image: every user sees it as his own. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/mount.h>

#include "io_clownix.h"
#include "idmap.h"

#ifndef MOUNT_ATTR_IDMAP
#define MOUNT_ATTR_IDMAP 0x00100000
#endif
#ifndef MOUNT_ATTR_RDONLY
#define MOUNT_ATTR_RDONLY 0x00000001
#endif
#ifndef OPEN_TREE_CLONE
#define OPEN_TREE_CLONE 1
#endif
#ifndef OPEN_TREE_CLOEXEC
#define OPEN_TREE_CLOEXEC O_CLOEXEC
#endif
#ifndef MOVE_MOUNT_F_EMPTY_PATH
#define MOVE_MOUNT_F_EMPTY_PATH 0x00000004
#endif
#ifndef MOUNT_ATTR_SIZE_VER0
struct mount_attr {
  uint64_t attr_set;
  uint64_t attr_clr;
  uint64_t propagation;
  uint64_t userns_fd;
};
#define MOUNT_ATTR_SIZE_VER0 32
#endif

#define SUBID_FIRST 100000
#define SUBID_QTY   65536

static int read_subid(char *file, int id, int *start, int *qty)
{
  int i, found = 0, uid, gid;
  char line[MAX_PATH_LEN];
  char name[MAX_PATH_LEN];
  char no_use[MAX_PATH_LEN];
  char user[MAX_PATH_LEN];
  FILE *fh;
  char *ptr;
  memset(user, 0, MAX_PATH_LEN);
  fh = fopen("/etc/passwd", "r");
  if (fh)
    {
    while(fgets(line, MAX_PATH_LEN-1, fh))
      {
      for (i=0; i<4; i++)
        {
        ptr = strchr(line, ':');
        if (ptr)
          *ptr = ' ';
        }
      if ((sscanf(line, "%299s %299s %d %d", name, no_use, &uid, &gid) == 4)
          && (uid == id))
        {
        strncpy(user, name, MAX_PATH_LEN-1);
        break;
        }
      }
    fclose(fh);
    }
  if (user[0] == 0)
    return 0;
  fh = fopen(file, "r");
  if (fh)
    {
    while(fgets(line, MAX_PATH_LEN-1, fh))
      {
      for (i=0; i<2; i++)
        {
        ptr = strchr(line, ':');
        if (ptr)
          *ptr = ' ';
        }
      if ((sscanf(line, "%299s %d %d", name, start, qty) == 3) &&
          (!strcmp(name, user)))
        {
        found = 1;
        break;
        }
      }
    fclose(fh);
    }
  return found;
}

static int write_map(int pid, char *kind, char *file, int owner, int me,
                     int me_start, int me_qty)
{
  int fd, len, o_start, o_qty, qty;
  char path[MAX_PATH_LEN];
  char txt[MAX_PATH_LEN];
  if (owner == 0)
    {
    o_start = 1;
    o_qty = me_qty;
    }
  else if (!read_subid(file, owner, &o_start, &o_qty))
    {
    KERR("WARNING image owner %d has no range in %s, assuming %d:%d",
         owner, file, SUBID_FIRST, SUBID_QTY);
    o_start = SUBID_FIRST;
    o_qty = SUBID_QTY;
    }
  qty = (o_qty < me_qty) ? o_qty : me_qty;
  memset(txt, 0, MAX_PATH_LEN);
  len = snprintf(txt, MAX_PATH_LEN-1, "%d %d 1\n%d %d %d\n",
                 owner, me, o_start, me_start, qty);
  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/%s", pid, kind);
  fd = open(path, O_WRONLY);
  if (fd < 0)
    {
    KERR("ERROR %s: %s", path, strerror(errno));
    return -1;
    }
  if (write(fd, txt, len) != len)
    {
    KERR("ERROR %s <- \"%s\": %s", path, txt, strerror(errno));
    close(fd);
    return -1;
    }
  close(fd);
  return 0;
}

static int make_userns(int owner_uid, int owner_gid, int uid, int gid,
                       int subuid_start, int subuid_qty,
                       int subgid_start, int subgid_qty)
{
  int pid, fd = -1, sync[2];
  char path[MAX_PATH_LEN];
  char ok = 0;
  struct pollfd pfd;
  if (pipe(sync))
    {
    KERR("ERROR pipe: %s", strerror(errno));
    return -1;
    }
  pid = fork();
  if (pid < 0)
    {
    KERR("ERROR fork: %s", strerror(errno));
    close(sync[0]);
    close(sync[1]);
    return -1;
    }
  if (pid == 0)
    {
    close(sync[0]);
    ok = unshare(CLONE_NEWUSER) ? 'k' : 'u';
    if (write(sync[1], &ok, 1) != 1)
      _exit(1);
    if (ok != 'u')
      _exit(1);
    for (;;)
      pause();
    }
  /* the maps only once the child is in its new user namespace: a fixed
     sleep lost that race under load, the map then refused with EPERM */
  close(sync[1]);
  pfd.fd = sync[0];
  pfd.events = POLLIN;
  if ((poll(&pfd, 1, 10000) != 1) || (read(sync[0], &ok, 1) != 1) ||
      (ok != 'u'))
    KERR("ERROR the child %d did not enter a new user namespace (%c)",
         pid, ok ? ok : '-');
  else if ((write_map(pid, "uid_map", "/etc/subuid", owner_uid, uid,
                 subuid_start, subuid_qty) == 0) &&
      (write_map(pid, "gid_map", "/etc/subgid", owner_gid, gid,
                 subgid_start, subgid_qty) == 0))
    {
    snprintf(path, MAX_PATH_LEN-1, "/proc/%d/ns/user", pid);
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
      KERR("ERROR %s: %s", path, strerror(errno));
    }
  close(sync[0]);
  kill(pid, SIGKILL);
  waitpid(pid, NULL, 0);
  return fd;
}

/* The owner whose ids get translated is the owner of the TREE, not of the
   directory holding it */
static int tree_owner(char *image, struct stat *st)
{
  struct stat top;
  char path[MAX_PATH_LEN];
  char *sub[] = {"usr", "etc", NULL};
  int i;
  if (stat(image, &top))
    {
    KERR("ERROR %s: %s", image, strerror(errno));
    return -1;
    }
  *st = top;
  for (i=0; sub[i]; i++)
    {
    snprintf(path, MAX_PATH_LEN-1, "%s/%s", image, sub[i]);
    if (stat(path, st) == 0)
      break;
    }
  if (st->st_uid != top.st_uid)
    KERR("WARNING %s: top owned by %d, the tree under it by %d, mapping "
         "the tree's owner", image, top.st_uid, st->st_uid);
  return 0;
}

static int mount_with(unsigned long long attr_set, char *image, char *target,
                      int uid, int gid, int subuid_start, int subuid_qty,
                      int subgid_start, int subgid_qty)
{
  struct stat st;
  struct mount_attr attr;
  int nsfd, tree, result = -1;
  if (tree_owner(image, &st))
    return -1;
  nsfd = make_userns(st.st_uid, st.st_gid, uid, gid, subuid_start,
                     subuid_qty, subgid_start, subgid_qty);
  if (nsfd < 0)
    return -1;
  tree = syscall(SYS_open_tree, AT_FDCWD, image,
                 OPEN_TREE_CLONE | OPEN_TREE_CLOEXEC);
  if (tree < 0)
    KERR("ERROR open_tree %s: %s", image, strerror(errno));
  else
    {
    memset(&attr, 0, sizeof(attr));
    attr.attr_set = attr_set;
    attr.userns_fd = nsfd;
    if (syscall(SYS_mount_setattr, tree, "", AT_EMPTY_PATH, &attr,
                MOUNT_ATTR_SIZE_VER0))
      KERR("ERROR mount_setattr idmap on %s: %s (kernel 5.12 or later "
           "needed, 5.19 for an overlay on it)", image, strerror(errno));
    else if (syscall(SYS_move_mount, tree, "", AT_FDCWD, target,
                     MOVE_MOUNT_F_EMPTY_PATH))
      KERR("ERROR move_mount %s onto %s: %s", image, target, strerror(errno));
    else
      {
      KTRC("%s idmapped onto %s%s: owner %d:%d seen as %d:%d", image, target,
           (attr_set & MOUNT_ATTR_RDONLY) ? " read only" : "",
           st.st_uid, st.st_gid, uid, gid);
      result = 0;
      }
    close(tree);
    }
  close(nsfd);
  return result;
}

/* Persistent: the mount is the root file-system, written. */
int idmap_mount(char *image, char *target, int uid, int gid,
                int subuid_start, int subuid_qty,
                int subgid_start, int subgid_qty)
{
  return mount_with(MOUNT_ATTR_IDMAP, image, target, uid, gid,
                    subuid_start, subuid_qty, subgid_start, subgid_qty);
}

/* Lower of an overlay: the kernel refuses any write through the mount. */
int idmap_mount_ro(char *image, char *target, int uid, int gid,
                   int subuid_start, int subuid_qty,
                   int subgid_start, int subgid_qty)
{
  return mount_with(MOUNT_ATTR_IDMAP | MOUNT_ATTR_RDONLY, image, target,
                    uid, gid, subuid_start, subuid_qty,
                    subgid_start, subgid_qty);
}
