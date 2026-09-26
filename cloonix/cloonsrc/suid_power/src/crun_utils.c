/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/prctl.h>
#include <ftw.h>
#include <sys/xattr.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "config_json.h"
#include "crun.h"
#include "tar_img.h"
#include "net_phy.h"
#include "crun_utils.h"

#include "utils.h"
#include "vwifi.h"
#include "idmap.h"

/* A container has a handful of mounts, not hundreds. */
#define MAX_UMOUNT_TREE 64

#define CGROUP_SYS "--cgroup-manager=systemd"
#define CGROUP_FS  "--cgroup-manager=cgroupfs"
#define CGROUP_DIS "--cgroup-manager=disabled"
#define SYSTEM_BUS "/run/dbus/system_bus_socket"

char *crun_cgroup_manager(int is_privileged)
{
  static char *unprivileged = NULL;
  if (is_privileged)
    return CGROUP_DIS;
  if (unprivileged == NULL)
    {
    if (access(SYSTEM_BUS, F_OK))
      {
      KERR("WARNING no %s, crun uses the cgroupfs manager", SYSTEM_BUS);
      unprivileged = CGROUP_FS;
      }
    else
      unprivileged = CGROUP_SYS;
    }
  return unprivileged;
}

char *get_net_name(void);
char *get_root_path(void);

/* The id crun knows a container by. Net-unique: with the systemd cgroup
   manager the scope is crun-<id>.scope, one namespace for the machine,
   and two nets both holding an hA would collide on it. */
char *crun_id(char *name)
{
  static char id[MAX_NAME_LEN];
  memset(id, 0, MAX_NAME_LEN);
  snprintf(id, MAX_NAME_LEN-1, "%s_%s", get_net_name(), name);
  return id;
}

int get_cloonix_llid(void);

/* What suid_power does that a "cloonix cli trace" reports, sent up to the
   switch. */
void suid_trace(int kind, char *name, char *txt)
{
  char resp[6*MAX_PATH_LEN];
  int llid = get_cloonix_llid();
  if (llid)
    {
    memset(resp, 0, 6*MAX_PATH_LEN);
    snprintf(resp, 6*MAX_PATH_LEN-1,
             "cloonsuid_trace kind=%d name=%s txt=%s", kind, name, txt);
    rpct_send_sigdiag_msg(llid, type_hop_suid_power, resp);
    }
}

/* The namespaces a container is given, in the order crun makes them. */
static const char *g_nspace_types[] =
  {"net", "mnt", "uts", "ipc", "pid", "cgroup", "user", NULL};

static int nspace_ids(int pid, char *out, int outlen)
{
  char path[MAX_PATH_LEN], its[MAX_NAME_LEN], ours[MAX_NAME_LEN];
  int i, n, len = 0;
  for (i=0; g_nspace_types[i]; i++)
    {
    snprintf(path, MAX_PATH_LEN-1, "/proc/%d/ns/%s", pid, g_nspace_types[i]);
    n = readlink(path, its, MAX_NAME_LEN-1);
    if (n <= 0)
      continue;
    its[n] = 0;
    snprintf(path, MAX_PATH_LEN-1, "/proc/self/ns/%s", g_nspace_types[i]);
    n = readlink(path, ours, MAX_NAME_LEN-1);
    if (n <= 0)
      n = 0;
    ours[n] = 0;
    len += snprintf(out+len, outlen-len-1, " %s%s", its,
                    strcmp(its, ours) ? "" : "=host");
    }
  return len;
}

static char *nspace_deleted(char *nspacecrun)
{
  static char txt[MAX_PATH_LEN];
  snprintf(txt, MAX_PATH_LEN-1, "deleted %s", nspacecrun);
  return txt;
}

void crun_utils_nspace_created(char *name, char *nspacecrun, int crun_pid)
{
  char txt[4*MAX_PATH_LEN];
  int len;
  memset(txt, 0, 4*MAX_PATH_LEN);
  len = snprintf(txt, 4*MAX_PATH_LEN-1, "created %s crun_pid=%d",
                 nspacecrun, crun_pid);
  /* Said explicitly: an empty set would read as no namespaces. */
  if (!nspace_ids(crun_pid, txt+len, 4*MAX_PATH_LEN-len))
    snprintf(txt+len, 4*MAX_PATH_LEN-len-1, " /proc/%d/ns unreadable",
             crun_pid);
  suid_trace(trace_kind_nspace, name, txt);
}

static int mkdir_for_guest(char *path)
{
  if (my_mkdir(path))
    {
    KERR("ERROR %s", path);
    return -1;
    }
  if (chown(path, get_caller_uid(), get_caller_gid()))
    KERR("ERROR chown %s: %s", path, strerror(errno));
  chmod(path, 0755);
  return 0;
}

int dirs_agent_create_mnt_tmp(char *mountbear, char *mounttmp)
{
  char path[MAX_PATH_LEN];
  if (mkdir_for_guest(mounttmp))
    return -1;
  if (mkdir_for_guest(mountbear))
    return -1;
  snprintf(path, MAX_PATH_LEN-1, "%s/mountbear", mountbear);
  if (mkdir_for_guest(path))
    return -1;
  snprintf(path, MAX_PATH_LEN-1, "%s/cnf_fs", mountbear);
  if (mkdir_for_guest(path))
    return -1;
  snprintf(path, MAX_PATH_LEN-1, "%s/cnf_fs/lib", mountbear);
  return mkdir_for_guest(path);
}

static int dirscnt_upper(char *tmpfs)
{
  char path[MAX_PATH_LEN];
  snprintf(path, MAX_PATH_LEN-1, "%s/%s/", tmpfs, UPPER);
  return mkdir_for_guest(path);
}

static int dirscnt(char *tmpfs, char *cnt_dir, char *name)
{
  char path[MAX_PATH_LEN];
  snprintf(path, MAX_PATH_LEN-1, "%s/%s", tmpfs, WORKDIR);
  if (my_mkdir(path))
    {
    KERR("ERROR %s", path);
    return -1;
    }
  snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s", cnt_dir, name, ROOTFS);
  return mkdir_for_guest(path);
}

static int create_all_dirs(char *cnt_dir, char *name)
{
  int result = -1;
  char path[MAX_PATH_LEN];
  char cmd[2*MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s/", cnt_dir, name, TMPFS);
  if (my_mkdir(path))
    {
    KERR("WARNING %s", path);
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s %s/%s/%s/",
             pthexec_umount_bin(), cnt_dir, name, TMPFS); 
    if (execute_cmd(cmd, 1, 0))
      KERR("WARNING %s", cmd);
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s %s/%s/%s",
             pthexec_umount_bin(), cnt_dir, name, ROOTFS);
    if (execute_cmd(cmd, 1, 0))
      KERR("WARNING %s", cmd);
    if (my_mkdir(path))
      KERR("ERROR %s", path);
    else
      result = 0;
    }
  else
    result = 0;
  return result;
}

static char *get_insider_agent(void)
{
  struct stat sb;
  char *result = NULL;

  if (lstat(pthexec_cloonfs_lib64(), &sb) == 0)
    {
    if (lstat(pthexec_agent_iso_amd64(), &sb) == 0)
      result = pthexec_agent_iso_amd64();
    else
      KERR("ERROR %s NOT FOUND", pthexec_agent_iso_amd64());
    }
  else
    KERR("ERROR %s NOT FOUND", pthexec_cloonfs_lib64());
  return result;
}

static int dirs_agent_copy_starter(char *mountbear)
{
  int result = -1;
  char path[MAX_PATH_LEN];
  char cmd[2*MAX_PATH_LEN];
  char *iso = get_insider_agent();
  if (iso)
    {
    memset(path, 0, MAX_PATH_LEN);
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(path, MAX_PATH_LEN-1, "%s/cnf_fs", mountbear);
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s -osirrox on -indev %s -extract / %s",
             pthexec_osirrox_bin(), iso, path); 
    if (execute_cmd(cmd, 1, 0))
      KERR("ERROR %s", cmd);
    else
      result = 0;
    }
  return result;
}

static int read_crun_create_pid(char *name, int should_not_exist,
                                int is_privileged)
{
  FILE *fp;
  char *ptr;
  int child_pid, wstatus, count= 0, create_pid = 0;
  /* Three tool paths and a name of MAX_NAME_LEN: more than MAX_PATH_LEN. */
  char cmd[2*MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  int nb_tries = 15;
  char *cgroupman;
  cgroupman = crun_cgroup_manager(is_privileged);
  if (should_not_exist)
    nb_tries = 1;
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1,
  "%s %s --log=%s --root=/var/lib/cloonix/%s/%s state %s | "
  "%s \"\\\"pid\\\"\" | %s '{ print $2 }'",
  pthexec_crun_bin(), cgroupman, get_var_crun_log(), get_net_name(),
  CRUN_DIR, crun_id(name), pthexec_grep_bin(), pthexec_awk_bin());
  while(create_pid == 0)
    {
    count += 1;
    if (count > nb_tries)
      {
      if (should_not_exist == 0)
        KERR("ERROR %s: crun state gave no pid in %d tries", name, nb_tries);
      break;
      }
    usleep(100000);
    memset(line, 0, MAX_PATH_LEN);
    fp = cmd_lio_popen(cmd, &child_pid, 0);
    if (fp == NULL)
      {
      KERR("ERROR %s", cmd);
      log_write_req("PREVIOUS CMD KO 2");
      }
    else
      {
      if(fgets(line, MAX_PATH_LEN-1, fp))
        {
        ptr = strchr(line, ',');
        if (ptr)
          {
          *ptr = 0;
          create_pid = atoi(line);
          }
        else
          KERR("ERROR PID %s %s", name, line);
        }
      fclose(fp);
      if (force_waitpid(child_pid, &wstatus))
        KERR("ERROR");
      }
    }
  return create_pid;
}

static int read_crun_create_pid_while_delete(char *name, int is_privileged)
{ 
  FILE *fp;
  char *ptr;
  int child_pid, wstatus, create_pid = 0;
  char cmd[2*MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char *cgroupman;
  cgroupman = crun_cgroup_manager(is_privileged);
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1,
  "%s %s --log=%s --root=/var/lib/cloonix/%s/%s state %s | "
  "%s \"\\\"pid\\\"\" | %s '{ print $2 }'",
  pthexec_crun_bin(), cgroupman, get_var_crun_log(), get_net_name(),
  CRUN_DIR, crun_id(name), pthexec_grep_bin(), pthexec_awk_bin());
  memset(line, 0, MAX_PATH_LEN);
  fp = cmd_lio_popen(cmd, &child_pid, 0);
  if (fp == NULL)
    {
    KERR("ERROR %s", cmd);
    log_write_req("PREVIOUS CMD KO 2");
    }
  else
    {
    if(fgets(line, MAX_PATH_LEN-1, fp))
      {
      ptr = strchr(line, ',');
      if (ptr)
        {
        *ptr = 0;
        create_pid = atoi(line);
        KTRC("%s: crun still reports pid %s", name, line);
        }
      else
        KERR("ERROR LINE PID %s %s", name, line);
      }
    fclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR");
    }
  return create_pid;
}

/* 0: out is the image itself. 1: out is a mount point to fill. */
static int lower_place(char *cnt_dir, char *name, char *image, char *lower,
                       int is_privileged, char *out)
{
  if ((is_privileged) || (!strcmp(image, "self_rootfs")))
    {
    strncpy(out, lower, MAX_PATH_LEN-1);
    return 0;
    }
  snprintf(out, MAX_PATH_LEN-1, "%s/%s/%s", cnt_dir, name, LOWER);
  if (mkdir_for_guest(out))
    return -1;
  return 1;
}

/* A lower the toy does not own: overlayfs cannot copy a directory up */
/* /usr and the rest stay in the lower, read-only, shared. */
static const char *g_precopy_full[] = {"/etc", "/root", "/home", "/var", NULL};
static int g_precopy_lower_len;
static char g_precopy_upper[MAX_PATH_LEN];

static int precopy_is_full(const char *rel)
{
  int i, len;
  for (i=0; g_precopy_full[i]; i++)
    {
    len = strlen(g_precopy_full[i]);
    if ((!strncmp(rel, g_precopy_full[i], len)) &&
        ((rel[len] == 0) || (rel[len] == '/')))
      return 1;
    }
  return 0;
}

static int g_precopy_unreadable;
static char g_precopy_first_unreadable[MAX_PATH_LEN];

/* An unreadable file (shadow, apt locks: 0600 of an unmapped owner) gets an
   empty stand-in of the same mode. */
static int precopy_file(const char *src, const char *dst, mode_t mode)
{
  int in, out, result = -1;
  ssize_t len;
  static char buf[65536];
  in = open(src, O_RDONLY);
  if (in < 0)
    {
    if (g_precopy_unreadable == 0)
      strncpy(g_precopy_first_unreadable, src, MAX_PATH_LEN-1);
    g_precopy_unreadable += 1;
    }
  out = open(dst, O_WRONLY|O_CREAT|O_TRUNC, mode & 07777);
  if (out >= 0)
    {
    result = 0;
    while ((in >= 0) && ((len = read(in, buf, sizeof(buf))) > 0))
      {
      if (write(out, buf, len) != len)
        {
        result = -1;
        break;
        }
      }
    fchmod(out, mode & 07777);
    close(out);
    }
  if (in >= 0)
    close(in);
  return result;
}

static int precopy_walk(const char *path, const struct stat *sb,
                        int flag, struct FTW *ftw)
{
  char dst[MAX_PATH_LEN], target[MAX_PATH_LEN];
  const char *rel = path + g_precopy_lower_len;
  int len;
  if (rel[0] == 0)
    return 0;
  snprintf(dst, MAX_PATH_LEN-1, "%s%s", g_precopy_upper, rel);
  if ((flag == FTW_D) || (flag == FTW_DNR))
    {
    if (mkdir(dst, sb->st_mode & 07777) && (errno != EEXIST))
      KERR("ERROR precopy mkdir %s: %s", dst, strerror(errno));
    else
      chmod(dst, sb->st_mode & 07777);
    if ((flag == FTW_DNR) &&
        (setxattr(dst, "user.overlay.opaque", "y", 1, 0)))
      KERR("WARNING precopy %s: cannot mark opaque: %s", dst,
           strerror(errno));
    }
  else if (!precopy_is_full(rel))
    return 0;
  else if (flag == FTW_SL)
    {
    len = readlink(path, target, MAX_PATH_LEN-1);
    if (len < 0)
      KERR("ERROR precopy readlink %s: %s", path, strerror(errno));
    else
      {
      target[len] = 0;
      if (symlink(target, dst) && (errno != EEXIST))
        KERR("ERROR precopy symlink %s: %s", dst, strerror(errno));
      }
    }
  else if (flag == FTW_F)
    {
    if (S_ISREG(sb->st_mode))
      {
      if (precopy_file(path, dst, sb->st_mode))
        KERR("ERROR precopy file %s: %s", dst, strerror(errno));
      }
    else
      KTRC("precopy skips %s, not a regular file", path);
    }
  else
    KERR("WARNING precopy %s: flag %d", path, flag);
  return 0;
}

static int precopy_upper(char *lower, char *upper)
{
  static const char *mnt[] = {"/cloonixmnt", "/dev", "/proc", "/run",
                              "/sys", "/tmp", "/etc", "/var", NULL};
  char path[MAX_PATH_LEN];
  int i;
  g_precopy_lower_len = strlen(lower);
  g_precopy_unreadable = 0;
  memset(g_precopy_first_unreadable, 0, MAX_PATH_LEN);
  strncpy(g_precopy_upper, upper, MAX_PATH_LEN-1);
  if (nftw(lower, precopy_walk, 64, FTW_PHYS|FTW_MOUNT))
    {
    KERR("ERROR precopy of %s into %s: %s", lower, upper, strerror(errno));
    return -1;
    }
  for (i=0; mnt[i]; i++)
    {
    snprintf(path, MAX_PATH_LEN-1, "%s%s", upper, mnt[i]);
    if (mkdir(path, 0755) && (errno != EEXIST))
      KERR("ERROR precopy mkdir %s: %s", path, strerror(errno));
    }
  if (g_precopy_unreadable)
    KERR("WARNING image %s: %d file(s) this toy cannot read (first %s), "
         "each replaced by an empty file of the same mode",
         lower, g_precopy_unreadable, g_precopy_first_unreadable);
  return 0;
}

static int lower_is_foreign(char *lower)
{
  struct stat sb;
  if (get_caller_uid() != 0)
    return 0;
  if (stat(lower, &sb))
    return 0;
  return (sb.st_uid != 0);
}

int crun_utils_create_overlay(char *mountbear, char *mounttmp,
                              char *name, char *cnt_dir, char *lower,
                              char *image, int uid_image,
                              int is_persistent, int is_privileged,
                              char *brandtype, int subuid_start,
                              int subuid_qty, int subgid_start,
                              int subgid_qty)
{
  int place, result = -1;
  char cmd[2*MAX_PATH_LEN];
  char path[MAX_PATH_LEN];
  char seen[MAX_PATH_LEN];
  if (strcmp(brandtype, "brandcnt"))
    KOUT("ERROR %s (only brandcnt)", brandtype);
  memset(seen, 0, MAX_PATH_LEN);
  if (is_persistent)
    {
    if (dirs_agent_copy_starter(mountbear))
      KERR("ERROR %s %s", mountbear, mounttmp);
    else
      {
      place = lower_place(cnt_dir, name, image, lower, is_privileged, seen);
      if ((place == 0) ||
          ((place == 1) &&
           (idmap_mount(lower, seen, uid_image, (int) get_caller_gid(),
                        subuid_start, subuid_qty,
                        subgid_start, subgid_qty) == 0)))
        result = 0;
      else
        KERR("ERROR %s: the image %s could not be mounted for it", name,
             lower);
      }
    }
  else
    {
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s -t tmpfs -o mode=0755 tmpfs %s/%s/%s",
             pthexec_mount_bin(), cnt_dir, name, TMPFS);
    if (execute_cmd(cmd, 1, 0))
      KERR("ERROR %s", cmd);
    else
      {
      memset(path, 0, MAX_PATH_LEN);
      snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s", cnt_dir, name, TMPFS);
      if (dirscnt_upper(path))
        KERR("ERROR %s %s", path, name);
      else if (dirscnt(path, cnt_dir, name))
        KERR("ERROR %s %s", path, name);
      else if (dirs_agent_copy_starter(mountbear))
        KERR("ERROR %s %s", path, name);
      else if (((place = lower_place(cnt_dir, name, image, lower,
                                     is_privileged, seen)) < 0) ||
               ((place == 1) &&
                idmap_mount_ro(lower, seen, uid_image,
                               (int) get_caller_gid(), subuid_start,
                               subuid_qty, subgid_start, subgid_qty)))
        KERR("ERROR %s: the image %s could not be mounted for it", name,
             lower);
      else
        {
        char *userxattr = "";
        if (lower_is_foreign(seen))
          {
          snprintf(path, MAX_PATH_LEN-1, "%s/%s/%s/%s",
                   cnt_dir, name, TMPFS, UPPER);
          KTRC("%s: image %s owned by another user, copying its writable "
               "trees into the upper", name, seen);
          precopy_upper(seen, path);
          userxattr = ",userxattr";
          }
        memset(cmd, 0, 2*MAX_PATH_LEN);
        snprintf(cmd, 2*MAX_PATH_LEN-1,
          "%s -t overlay overlay "
          "-o index=off,xino=off,redirect_dir=nofollow,metacopy=off%s "
          "-o lowerdir=%s,upperdir=%s/%s/%s/%s,workdir=%s/%s/%s/%s %s/%s/%s",
          pthexec_mount_bin(), userxattr, seen, cnt_dir, name, TMPFS, UPPER,
          cnt_dir, name, TMPFS, WORKDIR, cnt_dir, name, ROOTFS);
        if (execute_cmd(cmd, 1, 0))
          KERR("ERROR %s", cmd);
        else
          result = 0;
        }
      }
    }
  return result;
}

int crun_utils_get_vswitchd_pid(char *name)
{
  FILE *fp;
  int child_pid, wstatus, pid, result = 0;
  char *ident1 = pthexec_cloonfs_ovs_vswitchd();
  char ident2[MAX_PATH_LEN];
  char cmd[2*MAX_PATH_LEN];
  char line[2*MAX_PATH_LEN];
  memset(ident2, 0, MAX_PATH_LEN);
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(ident2, MAX_PATH_LEN-1,
  "unix:/var/lib/cloonix/%s/ovsdb_server.sock", name);
  snprintf(cmd, 2*MAX_PATH_LEN-1,
           "%s axo pid,args | grep %s | grep %s | grep -v grep",
           pthexec_ps_bin(), ident1, ident2);
  fp = cmd_lio_popen(cmd, &child_pid, 0);
  if (fp == NULL)
    {
    KERR("ERROR %s", cmd);
    log_write_req("PREVIOUS CMD KO 2");
    }
  else
    {
    if(fgets(line, 2*MAX_PATH_LEN-1, fp))
      {
      if (sscanf(line, "%d", &pid) == 1)
        result = pid;
      }
    fclose(fp);
    if (force_waitpid(child_pid, &wstatus))
      KERR("ERROR %s", cmd);
    }
  return result;
}

static char *last_line_of(char *path, char *out, int outlen)
{
  FILE *fp = fopen(path, "r");
  char line[MAX_PATH_LEN];
  strncpy(out, "(no log)", outlen-1);
  out[outlen-1] = 0;
  if (!fp)
    KERR("WARNING cannot read %s: %s", path, strerror(errno));
  else
    {
    while (fgets(line, MAX_PATH_LEN, fp))
      {
      if (line[0] && (line[0] != '\n'))
        {
        line[strcspn(line, "\n")] = 0;
        strncpy(out, line, outlen-1);
        out[outlen-1] = 0;
        }
      }
    fclose(fp);
    }
  return out;
}

/* One line of a file, or "" */
static char *read_first_line(char *path, char *out, int outlen)
{
  FILE *fp = fopen(path, "r");
  memset(out, 0, outlen);
  if (fp)
    {
    if (fgets(out, outlen-1, fp))
      out[strcspn(out, "\n")] = 0;
    fclose(fp);
    }
  return out;
}

/* The cgroup of the container, and the systemd scope above it if any. */
static int cgroups_of(int crun_pid, char *cg, char *scope, char *why,
                      int why_len)
{
  char path[MAX_PATH_LEN], line[MAX_PATH_LEN];
  char *ptr, *dot;
  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/cgroup", crun_pid);
  read_first_line(path, line, MAX_PATH_LEN);
  ptr = strstr(line, "::/");
  if (ptr == NULL)
    {
    snprintf(why, why_len-1, "no_cgroup_v2_path_for_pid_%d", crun_pid);
    return -1;
    }
  memset(cg, 0, MAX_PATH_LEN);
  strncpy(cg, ptr+2, MAX_PATH_LEN-1);
  memset(scope, 0, MAX_PATH_LEN);
  dot = strstr(cg, ".scope");
  if ((dot) && (strcmp(dot, ".scope")))
    {
    strncpy(scope, cg, MAX_PATH_LEN-1);
    scope[(dot - cg) + strlen(".scope")] = 0;
    }
  return 0;
}

/* -1 (no cap) leaves the scope quota untouched: write max there too. */
static int lift_scope_cpu(char *scope, char *why, int why_len)
{
  char path[MAX_PATH_LEN];
  FILE *fp;
  if (scope[0] == 0)
    return 0;
  snprintf(path, MAX_PATH_LEN-1, "/sys/fs/cgroup%s/cpu.max", scope);
  fp = fopen(path, "w");
  if (fp == NULL)
    {
    snprintf(why, why_len-1, "cannot_write_%s:%s", path, strerror(errno));
    return -1;
    }
  fputs("max 100000\n", fp);
  fclose(fp);
  return 0;
}

/* The cap as the kernel holds it, at the leaf and at the scope above it,
   read back so the ok means something. */
static int cgroup_says_at(char *cg, int cpu, int ram, char *why, int why_len)
{
  char path[MAX_PATH_LEN], line[MAX_PATH_LEN], want[MAX_NAME_LEN];
  snprintf(path, MAX_PATH_LEN-1, "/sys/fs/cgroup%s/cpu.max", cg);
  if (access(path, R_OK))
    {
    snprintf(why, why_len-1, "no_%s_(%s):_cpu_not_in_cgroup.subtree_control"
             "_of_the_parent", path, strerror(errno));
    return -1;
    }
  read_first_line(path, line, MAX_PATH_LEN);
  if (cpu)
    snprintf(want, MAX_NAME_LEN-1, "%d 100000", cpu * 1000);
  else
    snprintf(want, MAX_NAME_LEN-1, "max 100000");
  if (strcmp(line, want))
    {
    snprintf(why, why_len-1, "cpu.max_is_'%s'_not_'%s'", line, want);
    return -1;
    }
  snprintf(path, MAX_PATH_LEN-1, "/sys/fs/cgroup%s/memory.max", cg);
  if (access(path, R_OK))
    {
    snprintf(why, why_len-1, "no_%s_(%s):_memory_not_in_cgroup."
             "subtree_control_of_the_parent", path, strerror(errno));
    return -1;
    }
  read_first_line(path, line, MAX_PATH_LEN);
  if (ram)
    snprintf(want, MAX_NAME_LEN-1, "%lld", (long long) ram * 1048576);
  else
    snprintf(want, MAX_NAME_LEN-1, "max");
  if (strcmp(line, want))
    {
    snprintf(why, why_len-1, "memory.max_is_'%s'_not_'%s'", line, want);
    return -1;
    }
  return 0;
}

static int cgroup_says(char *cg, char *scope, int cpu, int ram, char *why,
                       int why_len)
{
  if (cgroup_says_at(cg, cpu, ram, why, why_len))
    return -1;
  if (scope[0] && cgroup_says_at(scope, cpu, ram, why, why_len))
    return -1;
  return 0;
}

/* cpu in percent of one core, ram in mega bytes, 0 lifts the cap. */
int crun_utils_update(char *name, int crun_pid, int is_privileged,
                      int cpu, int ram, char *why, int why_len)
{
  char cmd[4*MAX_PATH_LEN];
  char out[MAX_PATH_LEN];
  char cg[MAX_PATH_LEN], scope[MAX_PATH_LEN];
  int i, len = 0, result = -1;
  memset(why, 0, why_len);
  if (is_privileged)
    {
    snprintf(why, why_len-1, "a container of a toy has no cgroup of its "
             "own, cpu= and ram= cannot be applied in a toy");
    return -1;
    }
  if (cgroups_of(crun_pid, cg, scope, why, why_len))
    return -1;
  memset(cmd, 0, 4*MAX_PATH_LEN);
  len += sprintf(cmd+len, "%s %s", pthexec_crun_bin(),
                 crun_cgroup_manager(is_privileged));
  len += sprintf(cmd+len, " --log=%s", get_var_crun_log());
  len += sprintf(cmd+len, " --root=/var/lib/cloonix/%s/%s",
                 get_net_name(), CRUN_DIR);
  len += sprintf(cmd+len, " update --cpu-period 100000 --cpu-quota %d",
                 cpu ? cpu * 1000 : -1);
  /* memory-swap is memory plus swap: equal to memory, the cap cannot be
     dodged by swapping. */
  len += sprintf(cmd+len, " --memory %lld --memory-swap %lld",
                 ram ? (long long) ram * 1048576 : -1LL,
                 ram ? (long long) ram * 1048576 : -1LL);
  len += sprintf(cmd+len, " %s", crun_id(name));
  suid_trace(trace_kind_crun, name, cmd);
  if (execute_cmd_out(cmd, 1, 0, out, MAX_PATH_LEN))
    {
    for (i=0; out[i]; i++)
      if ((out[i] == ' ') || (out[i] == '\n'))
        out[i] = '_';
    snprintf(why, why_len-1, "crun_update_said:%s",
             out[0] ? out : "nothing");
    }
  else if ((cpu == 0) && lift_scope_cpu(scope, why, why_len))
    result = -1;
  else
    result = cgroup_says(cg, scope, cpu, ram, why, why_len);
  return result;
}

/* Under a ram cap the kernel must take what the user runs, never pid 1 nor
   the agents that inherit this; they set their children back to 0. */
static void oom_protect(char *name, int pid)
{
  char path[MAX_PATH_LEN];
  FILE *fp;
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/proc/%d/oom_score_adj", pid);
  fp = fopen(path, "w");
  if ((fp == NULL) || (fprintf(fp, "-1000\n") < 0) || fclose(fp))
    KERR("WARNING %s: %s not written errno=%d, a ram cap can kill pid 1",
         name, path, errno);
}

int crun_utils_create_crun_create(char *cnt_dir, char *name, int ovspid,
                                  int uid_image, int is_privileged)
{
  int pid = 0, len = 0;
  char cmd[4*MAX_PATH_LEN];
  char out[MAX_PATH_LEN];
  char last[MAX_PATH_LEN];
  char *cgroupman;
  cgroupman = crun_cgroup_manager(is_privileged);
  memset(cmd, 0, 4*MAX_PATH_LEN);
  len += sprintf(cmd+len, "%s %s", pthexec_crun_bin(), cgroupman); 
  len += sprintf(cmd+len, " --debug"); 
  len += sprintf(cmd+len, " --log=%s", get_var_crun_log()); 
  len += sprintf(cmd+len, " --root=/var/lib/cloonix/%s/%s",
                 get_net_name(), CRUN_DIR); 
  len += sprintf(cmd+len, " create"); 
  len += sprintf(cmd+len, " --config=%s/%s/config.json", cnt_dir, name);
  len += sprintf(cmd+len, " %s", crun_id(name)); 
  suid_trace(trace_kind_crun, name, cmd);
  if (execute_cmd_out(cmd, 1, 0, out, MAX_PATH_LEN) == 0)
    {
    pid = read_crun_create_pid(name, 0, is_privileged);
    if (pid && (!is_privileged))
      oom_protect(name, pid);
    }
  else
    {
    KERR("ERROR %s: crun said: %s -- last of %s: %s", name,
         out[0] ? out : "(nothing)", get_var_crun_log(),
         last_line_of(get_var_crun_log(), last, MAX_PATH_LEN));
    }
  return pid;
}
   
/* One ip step of the veth build; the stderr of ip is kept in the report   */
/* because exit 2 alone covers three different faults.                      */
/* All the ip lines of one namespace in one ip -batch: one exec for any
   number of eths, not five per eth, so a 32 eth create does not hold the
   loop for seconds. netns NULL is the namespace of suid_power; force goes
   on past a failed line, for the deletes of what may not exist. */
#define IP_BATCH_LEN (MAX_ETH_VM * 3 * 160)
static int ip_batch(char *netns, char *lines, int force, char *why)
{
  char path[MAX_PATH_LEN];
  char cmd[2*MAX_PATH_LEN];
  char out[MAX_PATH_LEN];
  FILE *fp;
  int result = 0;
  if (!lines[0])
    return 0;
  snprintf(path, MAX_PATH_LEN-1, "%s/ip_batch_%d", get_root_path(), getpid());
  fp = fopen(path, "w");
  if (!fp)
    {
    KERR("ERROR %s%s: %s", why, path, strerror(errno));
    return -1;
    }
  fputs(lines, fp);
  fclose(fp);
  /* nsenter and not ip -n: ip -n remounts /sys, refused in the toy */
  if (netns)
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s --net=%s%s %s%s -batch %s",
             pthexec_nsenter_bin(), PATH_NAMESPACE, netns, pthexec_ip_bin(),
             force ? " -force" : "", path);
  else
    snprintf(cmd, 2*MAX_PATH_LEN-1, "%s%s -batch %s", pthexec_ip_bin(),
             force ? " -force" : "", path);
  if (execute_cmd_out(cmd, !force, 0, out, MAX_PATH_LEN) && (!force))
    {
    KERR("ERROR %s%s -- ip said: %s", why, cmd, out[0] ? out : "(nothing)");
    result = -1;
    }
  unlink(path);
  return result;
}

static int crun_utils_create_veth(int vm_id, char *nspacecrun,
                                  int nb_eth, t_eth_mac *eth_mac)
{
  static char host[IP_BATCH_LEN], cnt[IP_BATCH_LEN];
  char net[MAX_NAME_LEN];
  unsigned char *mac;
  int i, lh = 0, lc = 0, result;
  memset(net, 0, MAX_NAME_LEN);
  snprintf(net, MAX_NAME_LEN-1, "%s_%s", BASE_NAMESPACE, get_net_name());
  host[0] = cnt[0] = 0;
  for (i=0; i<nb_eth; i++)
    {
    mac = eth_mac[i].mac;
    lh += snprintf(host + lh, IP_BATCH_LEN - lh,
      "link add name %s type veth peer name %s "
      "address %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n"
      "link set %s netns %s\n"
      "link set %s netns %s\n",
      pthexec_ovs_port(vm_id, i), pthexec_ovs_peer(vm_id, i),
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
      pthexec_ovs_peer(vm_id, i), nspacecrun,
      pthexec_ovs_port(vm_id, i), net);
    lc += snprintf(cnt + lc, IP_BATCH_LEN - lc,
      "link set %s name eth%d\nlink set eth%d up\n",
      pthexec_ovs_peer(vm_id, i), i, i);
    }
  result = ip_batch(NULL, host, 0, "the eths could not be made and moved "
                    "into their namespaces: ");
  if (!result)
    result = ip_batch(nspacecrun, cnt, 0, "the eths could not be named and "
                      "brought up in the container: ");
  return result;
}

void crun_utils_prepare_with_delete(char *name, char *nspacecrun,
                                    int vm_id, int nb_eth,
                                    int uid_image, int is_privileged)
{
  char cmd[2*MAX_PATH_LEN];
  static char dels[IP_BATCH_LEN];
  char net[MAX_NAME_LEN];
  int len = 0;
  int i, pid = read_crun_create_pid(name, 1, is_privileged);
  char *ptr = get_var_crun_log();
  char *cgroupman;
  cgroupman = crun_cgroup_manager(is_privileged);
  if (pid)
    {
    KERR("WARNING %s: crun still had a container of that name, pid %d, "
         "killing it before the create", name, pid);
    kill(pid, 9);
    }
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, 2*MAX_PATH_LEN-1,
           "%s %s --log=%s --root=/var/lib/cloonix/%s/%s delete %s",
           pthexec_crun_bin(), cgroupman, ptr, get_net_name(), CRUN_DIR,
           crun_id(name));
  execute_cmd(cmd, 0, 0);
  memset(cmd, 0, 2*MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns del %s >/dev/null",
           pthexec_ip_bin(), nspacecrun);
  if (!execute_cmd(cmd, 0, 0))
    suid_trace(trace_kind_nspace, name, nspace_deleted(nspacecrun));
  memset(net, 0, MAX_NAME_LEN);
  snprintf(net, MAX_NAME_LEN-1, "%s_%s", BASE_NAMESPACE, get_net_name());
  dels[0] = 0;
  for (i=0; i<nb_eth; i++)
    len += snprintf(dels + len, IP_BATCH_LEN - len, "link del name %s\n",
                    pthexec_ovs_port(vm_id, i));
  /* left by an earlier run in either namespace, or nowhere */
  ip_batch(net, dels, 1, "");
  ip_batch(NULL, dels, 1, "");
}

int crun_utils_user_nspacecrun_init(char *name, char *nspacecrun,
                                    int crun_pid)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns attach %s %d",
           pthexec_ip_bin(), nspacecrun, crun_pid);
  if (execute_cmd(cmd, 1, 0))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  else
    crun_utils_nspace_created(name, nspacecrun, crun_pid);
  return result;
}

int crun_utils_user_eth_create(char *name, char *nspacecrun,
                               int cloonix_rank, int vm_id,
                               int nb_eth, t_eth_mac *eth_mac)
{
  char cmd[MAX_PATH_LEN];
  int result;

  result = crun_utils_create_veth(vm_id, nspacecrun, nb_eth, eth_mac);
  if (result == 0)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1,
             "%s --net=%s%s %s link set lo up",
             pthexec_nsenter_bin(), PATH_NAMESPACE, nspacecrun,
             pthexec_ip_bin());
    if (execute_cmd(cmd, 1, 0))
      {
      KERR("ERROR %s", cmd);
      result = -1;
      }
    }
  return result;
}

int crun_utils_create_net(char *brandtype, int is_persistent,
                          int is_privileged, char *mountbear,
                          char *image, char *name, char *cnt_dir,
                          char *nspacecrun, int cloonix_rank,
                          int vm_id, int nb_eth, t_eth_mac *eth_mac)
{
  int result;
  result = create_all_dirs(cnt_dir, name);
  if (result)
    KERR("ERROR %s: %s/%s could not be prepared", name, cnt_dir, name);
  else if (dirs_agent_copy_starter(mountbear))
    {
    KERR("ERROR %s: the agent iso could not be unpacked into %s", name,
         mountbear);
    result = -1;
    }
  return result;
}

int crun_utils_mount_exists(char *path, char *found, int found_len)
{
  FILE *fp;
  char line[2*MAX_PATH_LEN];
  char mnt[2*MAX_PATH_LEN];
  int len, result = 0;
  if (found && found_len > 0)
    found[0] = 0;
  fp = fopen("/proc/1/mounts", "r");
  if (fp == NULL)
    fp = fopen("/proc/self/mounts", "r");
  if (fp == NULL)
    {
    KERR("ERROR cannot read the mount table, cannot check %s", path);
    return 0;
    }
  len = strlen(path);
  while (fgets(line, sizeof(line), fp))
    {
    if (sscanf(line, "%*s %599s", mnt) != 1)
      continue;
    if (strncmp(mnt, path, len))
      continue;
    if ((mnt[len] != 0) && (mnt[len] != '/'))
      continue;                 /* /a/bc must not match a check on /a/b */
    result += 1;
    if (found && found_len > 0 && found[0] == 0)
      snprintf(found, found_len, "%s", mnt);
    }
  fclose(fp);
  return result;
}

int crun_utils_umount_tree(char *path)
{
  FILE *fp;
  char line[2*MAX_PATH_LEN];
  char mnt[2*MAX_PATH_LEN];
  char list[MAX_UMOUNT_TREE][2*MAX_PATH_LEN];
  int i, j, len, nb = 0;
  fp = fopen("/proc/1/mounts", "r");
  if (fp == NULL)
    fp = fopen("/proc/self/mounts", "r");
  if (fp == NULL)
    {
    KERR("ERROR cannot read the mount table, cannot unmount %s", path);
    return -1;
    }
  len = strlen(path);
  while (fgets(line, sizeof(line), fp) && (nb < MAX_UMOUNT_TREE))
    {
    if (sscanf(line, "%*s %599s", mnt) != 1)
      continue;
    if (strncmp(mnt, path, len))
      continue;
    if ((mnt[len] != 0) && (mnt[len] != '/'))
      continue;
    memcpy(list[nb], mnt, 2*MAX_PATH_LEN);
    list[nb][2*MAX_PATH_LEN-1] = 0;
    nb += 1;
    }
  fclose(fp);
  for (i = 0; i < nb; i++)
    for (j = i+1; j < nb; j++)
      if (strlen(list[j]) > strlen(list[i]))
        {
        char tmp[2*MAX_PATH_LEN];
        memcpy(tmp, list[i], 2*MAX_PATH_LEN);
        memcpy(list[i], list[j], 2*MAX_PATH_LEN);
        memcpy(list[j], tmp, 2*MAX_PATH_LEN);
        }
  for (i = 0; i < nb; i++)
    {
    char ucmd[2*MAX_PATH_LEN + MAX_NAME_LEN];
    if (strlen(pthexec_umount_bin()) + strlen(list[i]) + 8 > sizeof(ucmd))
      {
      KERR("ERROR mount path too long to unmount safely: %s", list[i]);
      continue;
      }
    memset(ucmd, 0, sizeof(ucmd));
    snprintf(ucmd, sizeof(ucmd)-1, "%s %s", pthexec_umount_bin(), list[i]);
    if (execute_cmd(ucmd, 0, 0))
      {
      memset(ucmd, 0, sizeof(ucmd));
      snprintf(ucmd, sizeof(ucmd)-1, "%s -l %s",
               pthexec_umount_bin(), list[i]);
      if (execute_cmd(ucmd, 0, 0))
        KERR("ERROR could not unmount %s, even lazily", list[i]);
      else
        KERR("WARNING %s needed a lazy unmount, something still held it",
             list[i]);
      }
    }
  return nb;
}

static int delete_one_overlay(char *name, char *cnt_dir, char *sub)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  char dir[MAX_PATH_LEN];
  memset(dir, 0, MAX_PATH_LEN);
  snprintf(dir, MAX_PATH_LEN-1, "%s/%s/%s", cnt_dir, name, sub);
  if (access(dir, F_OK))
    {
    KTRC("%s: %s does not exist, nothing to unmount or remove", name, dir);
    return 0;
    }
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s %s", pthexec_umount_bin(), dir);
  if (execute_cmd(cmd, 0, 0))
    {
    /* Not fatal by itself: the mount may have gone with the container. */
    KTRC("%s: umount of %s did not succeed", name, dir);
    }
  if (rmdir(dir))
    {
    if (errno == ENOENT)
      KTRC("%s: %s is already gone", name, dir);
    else
      {
      KERR("ERROR %s: %s could not be removed, errno=%d (%s)",
           name, dir, errno, strerror(errno));
      result = -1;
      }
    }
  return result;
}

int crun_utils_delete_overlay1(char *name, char *cnt_dir)
{
  return delete_one_overlay(name, cnt_dir, ROOTFS);
}

int crun_utils_delete_overlay2(char *name, char *cnt_dir)
{
  int result = delete_one_overlay(name, cnt_dir, TMPFS);
  if (delete_one_overlay(name, cnt_dir, LOWER))
    result = -1;
  return result;
}

#define CRUN_DELETE_TRIES 20
#define CRUN_DELETE_ESCALATE 5

static int crun_knows(char *name)
{
  char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "/var/lib/cloonix/%s/%s/%s",
           get_net_name(), CRUN_DIR, crun_id(name));
  return (access(path, F_OK) == 0);
}

void crun_utils_delete_crun_stop(char *name, int pid, int uid_image,
                                 int is_privileged, char *mountbear)
{
  char cmd[MAX_PATH_LEN];
  char said[MAX_PATH_LEN];
  char *ptr = get_var_crun_log();
  char *cgroupman;
  memset(said, 0, MAX_PATH_LEN);
  cgroupman = crun_cgroup_manager(is_privileged);
  if (!crun_knows(name))
    KTRC("%s: crun has no such container, nothing to kill", name);
  else
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1,
             "%s %s --log=%s --root=/var/lib/cloonix/%s/%s kill %s",
             pthexec_crun_bin(), cgroupman, ptr, get_net_name(), CRUN_DIR,
             crun_id(name));
    if (execute_cmd_out(cmd, 0, 0, said, sizeof(said)))
      {
      /* A container whose process already died answers this way. */
      if (strstr(said, "process not running"))
        KTRC("%s: crun kill of an already dead container", name);
      else
        KERR("ERROR %s: crun would not kill it. crun said: %s",
             name, said[0] ? said : "nothing at all");
      }
    }
  if (pid)
    kill(pid, 9);
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s %s/tmp", pthexec_umount_bin(), mountbear);
  execute_cmd(cmd, 0, 0);
}

void crun_utils_delete_crun_delete(char *name, char *nspacecrun,
                                   int vm_id, int nb_eth,
                                   int uid_image, int is_privileged)
{
  int i, pid;
  char cmd[2*MAX_PATH_LEN];
  char said[MAX_PATH_LEN];
  char *ptr = get_var_crun_log();
  char *cgroupman;
  memset(said, 0, MAX_PATH_LEN);
  cgroupman = crun_cgroup_manager(is_privileged);
  if (!crun_knows(name))
    KTRC("%s: crun has no such container, nothing to delete", name);
  else
    {
    memset(cmd, 0, 2*MAX_PATH_LEN);
    snprintf(cmd, 2*MAX_PATH_LEN-1,
             "%s %s --log=%s --root=/var/lib/cloonix/%s/%s delete %s",
             pthexec_crun_bin(), cgroupman, ptr, get_net_name(), CRUN_DIR,
             crun_id(name));
    for (i=0; i<CRUN_DELETE_TRIES; i++)
      {
      if (execute_cmd_out(cmd, 0, 0, said, sizeof(said)) == 0)
        break;
      if (!crun_knows(name))
        break;
      /* Escalated once, a quarter of a second in. */
      if (i == CRUN_DELETE_ESCALATE)
        {
        char kcmd[MAX_PATH_LEN];
        memset(kcmd, 0, MAX_PATH_LEN);
        snprintf(kcmd, MAX_PATH_LEN-1,
                 "%s %s --log=%s --root=/var/lib/cloonix/%s/%s kill %s KILL",
                 pthexec_crun_bin(), cgroupman, ptr, get_net_name(),
                 CRUN_DIR, crun_id(name));
        KTRC("%s: still running after %d delete tries, sending SIGKILL",
             name, i);
        execute_cmd(kcmd, 0, 0);
        }
      usleep(50000);
      }
    if (i)
      {
      if (i >= CRUN_DELETE_TRIES)
        KERR("ERROR %s: crun would not delete it after %d tries. crun said: "
             "%s", name, i, said[0] ? said : "nothing at all");
      else
        KTRC("%s: crun deleted it on try %d", name, i+1);
      }
    }
  pid = read_crun_create_pid_while_delete(name, is_privileged);
  if (pid > 0)
    {
    KERR("WARNING %s: still running as pid %d after crun delete, killing it",
         name, pid);
    kill(pid, 9);
    }
  memset(cmd, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "%s netns del %s >/dev/null",
           pthexec_ip_bin(), nspacecrun);
  if (!execute_cmd(cmd, 0, 0))
    suid_trace(trace_kind_nspace, name, nspace_deleted(nspacecrun));
  for (i=0; i<nb_eth; i++)
    {
    memset(cmd, 0, MAX_PATH_LEN);
    snprintf(cmd, MAX_PATH_LEN-1, "%s link del name %s",
             pthexec_ip_bin(), pthexec_ovs_port(vm_id, i));
    execute_cmd(cmd, 0, 0);
    }
}

int crun_utils_create_crun_start(char *name, int ovspid, int uid_image,
                                 int is_privileged)
{
  int result = 0;
  char cmd[MAX_PATH_LEN];
  char crun_dir[MAX_PATH_LEN];
  char line[MAX_PATH_LEN];
  char *ptr = get_var_crun_log();
  char *cgroupman;
  cgroupman = crun_cgroup_manager(is_privileged);
  memset(cmd, 0, MAX_PATH_LEN);
  memset(crun_dir, 0, MAX_PATH_LEN);
  memset(line, 0, MAX_PATH_LEN);
  snprintf(crun_dir, MAX_PATH_LEN-1,
  "/var/lib/cloonix/%s/%s", get_net_name(), CRUN_DIR);
  snprintf(cmd, MAX_PATH_LEN-1,
  "%s -t %d --net -- %s %s --debug --log=%s --root=%s start %s",
  pthexec_nsenter_bin(), ovspid, pthexec_crun_bin(),
  cgroupman, ptr, crun_dir, crun_id(name));
  suid_trace(trace_kind_crun, name, cmd);
  if (execute_cmd(cmd, 1, 0))
    {
    KERR("ERROR %s", cmd);
    result = -1;
    }
  return result;
}

