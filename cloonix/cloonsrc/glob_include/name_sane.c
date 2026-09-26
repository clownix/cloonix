#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <grp.h>
#include <errno.h>

#include "glob_common.h"

unsigned long cloonix_now_ms(void)
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    {
    KERR("ERROR clock_gettime %s", strerror(errno));
    return 0;
    }
  return (unsigned long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int cloonix_caller_uid(void)
{
  char *v = getenv("CLOONIX_CALLER_UID");
  if ((v) && (v[0]))
    return (int) strtol(v, NULL, 10);
  return (int) getuid();
}

void cloonix_drop_root(void)
{
  uid_t uid = (uid_t) cloonix_caller_uid();
  gid_t gid = (gid_t) cloonix_caller_gid();
  if ((getuid() != 0) && (geteuid() != 0))
    return;
  if (uid == 0)
    return;
  if (setgroups(0, NULL))
    KERR("ERROR setgroups %s", strerror(errno));
  if (setresgid(gid, gid, gid))
    KOUT("ERROR setresgid %s", strerror(errno));
  if (setresuid(uid, uid, uid))
    KOUT("ERROR setresuid %s", strerror(errno));
}

int cloonix_caller_gid(void)
{
  char *v = getenv("CLOONIX_CALLER_GID");
  if ((v) && (v[0]))
    return (int) strtol(v, NULL, 10);
  return (int) getgid();
}

int name_is_sane(char *name)
{
  int i, len;
  if ((!name) || (!name[0]))
    return 0;
  len = strlen(name);
  if (len >= MAX_NAME_LEN)
    return 0;
  if ((name[0] == '-') || (name[0] == '.'))
    return 0;
  for (i=0; i<len; i++)
    {
    if ((name[i] >= 'a') && (name[i] <= 'z'))
      continue;
    if ((name[i] >= 'A') && (name[i] <= 'Z'))
      continue;
    if ((name[i] >= '0') && (name[i] <= '9'))
      continue;
    if ((name[i] == '_') || (name[i] == '-') || (name[i] == '.'))
      continue;
    return 0;
    }
  if (strstr(name, ".."))
    return 0;
  return 1;
}

/* A path is not a name: the satellites get one as argv[2]. */
int path_is_sane(char *path)
{
  int len;
  if ((!path) || (path[0] != '/'))
    return 0;
  len = strlen(path);
  if (len >= MAX_PATH_LEN)
    return 0;
  if (strstr(path, ".."))
    return 0;
  return 1;
}
