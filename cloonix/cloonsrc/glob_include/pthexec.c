/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <libgen.h>

#include "glob_common.h"

static int g_init_done=0;
static int g_is_in_crun;

#define PTHEXEC_LIST \
  X(pthexec_agent_iso_amd64,      "/insider_agents/insider_agent_x86_64.iso") \
  X(pthexec_agent_dir_cnt,        "/insider_agents/insider_agent_cnt") \
  X(pthexec_agent_iso_win,        "/insider_agents/insider_agent_win_x86_64.iso") \
  X(pthexec_agent_iso_bsd,        "/insider_agents/insider_agent_bsd_x86_64.iso") \
  X(pthexec_iw_bin,               "/cloonfs/sbin/iw") \
  X(pthexec_crun_bin,             "/cloonfs/bin/cloonix-crun") \
  X(pthexec_ovsdb_tool_bin,       "/cloonfs/bin/cloonix-ovsdb-tool") \
  X(pthexec_cp_bin,               "/cloonfs/bin/cp") \
  X(pthexec_umount_bin,           "/cloonfs/bin/umount") \
  X(pthexec_mount_bin,            "/cloonfs/bin/mount") \
  X(pthexec_bash_bin,             "/cloonfs/bin/bash") \
  X(pthexec_nsenter_bin,          "/cloonfs/bin/nsenter") \
  X(pthexec_ip_bin,               "/cloonfs/sbin/ip") \
  X(pthexec_awk_bin,              "/cloonfs/bin/awk") \
  X(pthexec_grep_bin,             "/cloonfs/bin/grep") \
  X(pthexec_ps_bin,               "/cloonfs/bin/ps") \
  X(pthexec_mknod_bin,            "/cloonfs/bin/mknod") \
  X(pthexec_chmod_bin,            "/cloonfs/bin/chmod") \
  X(pthexec_tar_bin,              "/cloonfs/bin/tar") \
  X(pthexec_gzip_bin,             "/cloonfs/bin/gzip") \
  X(pthexec_cloonix_cfg,          "/cloonfs/etc/cloonix.cfg") \
  X(pthexec_cxp_cli,              "/cloonfs/bin/cloonix-cxp-cli") \
  X(pthexec_dtach_bin,            "/cloonfs/bin/cloonix-dtach") \
  X(pthexec_usr_libexec_cloonix,  "") \
  X(pthexec_cloonfs_lib64,        "/cloonfs/lib64") \
  X(pthexec_cloonfs_ovs_vswitchd, "/cloonfs/bin/cloonix-ovs-vswitchd") \
  X(pthexec_cloonfs_ovs_drv,      "/cloonfs/bin/cloonix-ovs-drv") \
  X(pthexec_cloonfs_ovs_ovsdb,    "/cloonfs/bin/cloonix-ovsdb-server") \
  X(pthexec_cloonfs_ovs_vsctl,    "/cloonfs/bin/cloonix-ovs-vsctl") \
  X(pthexec_cloonfs_suid_power,   "/cloonfs/bin/cloonix-suid-power") \
  X(pthexec_cloonfs_doorways,     "/cloonfs/bin/cloonix-doorways") \
  X(pthexec_cloonfs_gui_server,   "/cloonfs/bin/jscloonix/cloonix-gui-server") \
  Y(pthexec_qemu_bin,             "/cloonfs/bin/cloonix-qemu-system") \
  Y(pthexec_ovmf_code,            "/cloonfs/bin/qemu/edk2-x86_64-code.fd") \
  Y(pthexec_ovmf_vars,            "/cloonfs/bin/qemu/edk2-i386-vars.fd")

enum {
#define X(fn, rel) idx_##fn,
#define Y(fn, rel) idx_##fn,
PTHEXEC_LIST
#undef X
#undef Y
  pthexec_nb_paths
};

static const char *g_rel[] = {
#define X(fn, rel) rel,
#define Y(fn, rel) rel,
PTHEXEC_LIST
#undef X
#undef Y
};

static char g_path[pthexec_nb_paths][MAX_PATH_LEN];

int pthexec_running_in_crun(char *name)
{
  int result = 0;
  int fd, len, i, start, last = -1;
  char buf[2*MAX_PATH_LEN];

  /* No /proc on freebsd: the answer is no, a freebsd guest is never a crun. */
  fd = open("/proc/1/cmdline", O_RDONLY);
  if (fd < 0)
    return 0;
  memset(buf, 0, 2*MAX_PATH_LEN);
  len = read(fd, buf, 2*MAX_PATH_LEN - 1);
  close(fd);
  if (len <= 0)
    return 0;

  start = 0;
  for (i = 0; i < len; i++)
    {
    if (buf[i])
      continue;
    if (strstr(buf + start, CLOONIX_TOY_INIT))
      result = 1;
    if (buf[start])
      last = start;
    start = i + 1;
    }
  if ((result) && (name))
    {
    memset(name, 0, MAX_NAME_LEN);
    if (last < 0)
      KERR("ERROR pid 1 is %s with no net name after it", buf);
    else if (strstr(buf + last, "__"))
      {
      KERR("ERROR pid 1 gives the net as %s: setup.sh left a placeholder "
           "unsubstituted", buf + last);
      result = 0;
      }
    else
      strncpy(name, buf + last, MAX_NAME_LEN-1);
    }
  return result;
}

static int g_rank = -1;

void pthexec_set_rank(int rank)
{
  g_rank = rank;
}

char *pthexec_ovs_port(int vm_id, int eth)
{
  static char name[MAX_NAME_LEN];
  if (g_rank < 0)
    KOUT("ERROR pthexec_set_rank not called");
  memset(name, 0, MAX_NAME_LEN);
  snprintf(name, MAX_NAME_LEN-1, "%s%d_%d_%d",
           OVS_BRIDGE_PORT, g_rank, vm_id, eth);
  return name;
}

char *pthexec_ovs_peer(int vm_id, int eth)
{
  static char name[MAX_NAME_LEN];
  if (g_rank < 0)
    KOUT("ERROR pthexec_set_rank not called");
  memset(name, 0, MAX_NAME_LEN);
  snprintf(name, MAX_NAME_LEN-1, "%s%d_%d_%d",
           OVS_PEER_PORT, g_rank, vm_id, eth);
  return name;
}

static char *readable(char *path)
{
  return (access(path, R_OK) == 0) ? path : NULL;
}

#define X(fn, rel) \
char *fn(void) \
{ \
  if (g_init_done != 1) \
    KOUT("ERROR NO INIT"); \
  return g_path[idx_##fn]; \
}
#define Y(fn, rel) \
char *fn(void) \
{ \
  if (g_init_done != 1) \
    KOUT("ERROR NO INIT"); \
  return readable(g_path[idx_##fn]); \
}
PTHEXEC_LIST
#undef X
#undef Y

/* The directory of the running binary: a helper the toy carries beside */
/* proxymous is found there, in the bundle it is cloonfs/bin. */
char *pthexec_self_bin_dir(void)
{
  static char dir[MAX_PATH_LEN];
  char path[MAX_PATH_LEN];
  int len;
  memset(dir, 0, MAX_PATH_LEN);
  len = (int) readlink("/proc/self/exe", path, MAX_PATH_LEN - 1);
  if (len <= 0)
    KERR("ERROR readlink /proc/self/exe %s", strerror(errno));
  else
    {
    path[len] = 0;
    snprintf(dir, MAX_PATH_LEN-1, "%s", dirname(path));
    }
  return dir;
}

void pthexec_init(void)
{
  const char *prefix;
  int i;
  if (g_init_done == 0)
    {
    g_init_done = 1;
    g_is_in_crun = pthexec_running_in_crun(NULL) ? 1 : 0;
    prefix = g_is_in_crun ? "/cloonix" : "/usr/libexec/cloonix";
    for (i = 0; i < pthexec_nb_paths; i++)
      snprintf(g_path[i], MAX_PATH_LEN-1, "%s%s", prefix, g_rel[i]);
    }
}

FILE *pthexec_popen(char *cmd, int *child_pid)
{
  int pdes[2], pid;
  FILE *iop;
  char *argv[4];
  if ((!cmd) || (!cmd[0]) || (!child_pid))
    KOUT("ERROR");
  *child_pid = 0;
  if (pipe(pdes))
    {
    KERR("ERROR pipe %s", strerror(errno));
    return NULL;
    }
  if ((pid = fork()) < 0)
    {
    KERR("ERROR fork %s", strerror(errno));
    close(pdes[0]);
    close(pdes[1]);
    return NULL;
    }
  if (pid == 0)
    {
    close(pdes[0]);
    if (pdes[1] != STDOUT_FILENO)
      {
      if (dup2(pdes[1], STDOUT_FILENO) < 0)
        KOUT("ERROR dup2 %s", strerror(errno));
      close(pdes[1]);
      }
    argv[0] = pthexec_bash_bin();
    argv[1] = "-c";
    argv[2] = cmd;
    argv[3] = NULL;
    execv(argv[0], argv);
    KOUT("ERROR execv %s %s", argv[0], strerror(errno));
    }
  close(pdes[1]);
  fcntl(pdes[0], F_SETFD, FD_CLOEXEC);
  iop = fdopen(pdes[0], "r");
  if (iop == NULL)
    {
    KERR("ERROR fdopen %s", strerror(errno));
    close(pdes[0]);
    kill(pid, SIGKILL);
    waitpid(pid, NULL, 0);
    return NULL;
    }
  *child_pid = pid;
  return iop;
}

int pthexec_pclose(FILE *iop, int child_pid)
{
  int wstatus = 0;
  if (iop)
    fclose(iop);
  if (child_pid <= 0)
    {
    KERR("WARNING pclose of a popen that never forked");
    return -1;
    }
  while (waitpid(child_pid, &wstatus, 0) < 0)
    {
    if (errno != EINTR)
      {
      KERR("ERROR waitpid %d %s", child_pid, strerror(errno));
      return -1;
      }
    }
  return wstatus;
}
