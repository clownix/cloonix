/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sched.h>

#include "io_clownix.h"
#include "ovs_execv.h"
#include "ovs_drv.h"

static char *make_cmd_str(char **argv)
{
  int i, len = 0;
  static char result[NB_ARG*MAX_ARG_LEN];
  memset(result, 0, sizeof(result));
  for (i=0;  (argv[i] != NULL); i++)
    {
    if (len + (int)strlen(argv[i]) + 2 > (int)sizeof(result))
      {
      KERR("NOT POSSIBLE");
      break;
      }
    len += snprintf(result+len, sizeof(result)-len, "%s ", argv[i]);
    }
  return result;
}

static void add_cmd_to_log(char *ovs_dir, char *argv[])
{
  FILE *fp_log;
  char pth[MAX_ARG_LEN];
  snprintf(pth, MAX_ARG_LEN-1, "%s/log/%s", ovs_dir, DEBUG_LOG_OVS_CMD);
  fp_log = fopen(pth, "a+");
  if (fp_log) 
    {
    fprintf(fp_log, "\n\n");
    fprintf(fp_log, "%s", make_cmd_str(argv));
    fflush(fp_log);
    fclose(fp_log);
    }
}

/* The non-empty lines the last command printed, trimmed, at most max. */
int ovs_popen_read_lines(char *ovs_dir, char lines[][MAX_NAME_LEN], int max)
{
  char errfile[MAX_PATH_LEN];
  char line[512];
  FILE *fp;
  int nb = 0, len;
  char *ptr;
  memset(errfile, 0, MAX_PATH_LEN);
  snprintf(errfile, MAX_PATH_LEN-1, "%s/ovs_cmd_stderr.txt", ovs_dir);
  fp = fopen(errfile, "r");
  if (!fp)
    return -1;
  while ((nb < max) && (fgets(line, sizeof(line), fp)))
    {
    line[strcspn(line, "\r\n")] = 0;
    ptr = line;
    while ((*ptr == ' ') || (*ptr == '\t'))
      ptr += 1;
    len = strlen(ptr);
    while ((len > 0) && (ptr[len-1] == ' '))
      ptr[--len] = 0;
    if (len == 0)
      continue;
    memset(lines[nb], 0, MAX_NAME_LEN);
    strncpy(lines[nb], ptr, MAX_NAME_LEN-1);
    nb += 1;
    }
  fclose(fp);
  return nb;
}

int ovs_popen_last_said(char *ovs_dir, char *needle)
{
  char errfile[MAX_PATH_LEN];
  char line[512];
  FILE *fp;
  int found = 0;
  memset(errfile, 0, MAX_PATH_LEN);
  snprintf(errfile, MAX_PATH_LEN-1, "%s/ovs_cmd_stderr.txt", ovs_dir);
  fp = fopen(errfile, "r");
  if (!fp)
    return 0;
  while (fgets(line, sizeof(line), fp))
    {
    if (strstr(line, needle))
      {
      found = 1;
      break;
      }
    }
  fclose(fp);
  return found;
}

static void log_what_the_command_said(char *ovs_dir)
{
  char errfile[MAX_PATH_LEN];
  char line[512];
  FILE *fp;
  int nb = 0;
  memset(errfile, 0, MAX_PATH_LEN);
  snprintf(errfile, MAX_PATH_LEN-1, "%s/ovs_cmd_stderr.txt", ovs_dir);
  fp = fopen(errfile, "r");
  if (!fp)
    return;
  while ((fgets(line, sizeof(line), fp)) && (nb < 4))
    {
    line[strcspn(line, "\n")] = 0;
    if (line[0])
      {
      KERR("ERROR   ovs said: %s", line);
      nb += 1;
      }
    }
  fclose(fp);
  if (nb == 0)
    KERR("ERROR   ovs said nothing at all");
}

/* ip -netns also switches the mount namespace (refused in a container):
   the child enters the net namespace itself and runs plain ip. */
static int enter_netns(char *netns)
{
  char path[MAX_PATH_LEN];
  int fd, result;
  snprintf(path, MAX_PATH_LEN-1, "/var/run/netns/%s", netns);
  fd = open(path, O_RDONLY);
  if (fd < 0)
    {
    KERR("ERROR open %s: %s", path, strerror(errno));
    return -1;
    }
  result = setns(fd, CLONE_NEWNET);
  if (result)
    KERR("ERROR setns %s: %s", path, strerror(errno));
  close(fd);
  return result;
}

static int ovs_popen(char *ovs_dir, char *argv[], char *netns)
{
  int exited_pid, timeout_pid, worker_pid, chld_state, pid, status=97;
  pid_t rc_pid;
  add_cmd_to_log(ovs_dir, argv);
  if ((pid = fork()) < 0)
    KOUT(" ");
  if (pid == 0)
    {
    close(get_cloonix_listen_fd());
    close(get_cloonix_fd());
    worker_pid = fork();

    if (worker_pid == 0)
      {
      char errfile[MAX_PATH_LEN];
      int efd;
      memset(errfile, 0, MAX_PATH_LEN);
      snprintf(errfile, MAX_PATH_LEN-1, "%s/ovs_cmd_stderr.txt", ovs_dir);
      efd = open(errfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
      if (efd >= 0)
        {
        dup2(efd, 1);
        dup2(efd, 2);
        close(efd);
        }
      if (netns && enter_netns(netns))
        exit(96);
      execv(argv[0], argv);
      KOUT("FORK error %s", strerror(errno));
      }
    timeout_pid = fork();
    if (timeout_pid == 0)
      {
      sleep(10);
      KERR("WARNING TIMEOUT SLOW CMD 1 %s", make_cmd_str(argv));
      sleep(8);
      KERR("WARNING TIMEOUT SLOW CMD 2 %s", make_cmd_str(argv));
      sleep(8);
      KERR("WARNING TIMEOUT SLOW CMD 3 %s", make_cmd_str(argv));
      sleep(8);
      KERR("ERROR TIMEOUT CMD %s", make_cmd_str(argv));
      exit(1);
      }
    exited_pid = wait(&chld_state);
    if (exited_pid == worker_pid)
      {
      if (WIFEXITED(chld_state))
        status = WEXITSTATUS(chld_state);
      if (WIFSIGNALED(chld_state))
        KERR("Child exited via signal %d\n",WTERMSIG(chld_state));
      kill(timeout_pid, SIGKILL);
      }
    else
      {
      kill(worker_pid, SIGKILL);
      }
    wait(NULL);
    exit(status);
    }
  rc_pid = waitpid(pid, &chld_state, 0);
  if (rc_pid > 0)
    {
    if (WIFEXITED(chld_state))
      status = WEXITSTATUS(chld_state);
    if (WIFSIGNALED(chld_state))
      KERR("Child exited via signal %d\n",WTERMSIG(chld_state));
    }
  else
    {
    if (errno != ECHILD)
      KERR("Unexpected error %s", strerror(errno));
    }
  return status;
}

int call_ovs_popen(char *ovs_dir, char **argv, int quiet,
                   const char *fct, int line)
{
  return call_ovs_popen_ns(ovs_dir, argv, quiet, fct, line, NULL);
}

/* netns: the cloonix net namespace the command runs in, NULL for none.    */
int call_ovs_popen_ns(char *ovs_dir, char **argv, int quiet,
                      const char *fct, int line, char *netns)
{
  static int protect_reentry = 0;
  int result = 0, status;
  if (protect_reentry)
    KERR("ERROR REENTRY ovs_popen %s %d", fct, line);
  protect_reentry = 1;
  status = ovs_popen(ovs_dir, argv, netns);
  if (status)
    {
    if (quiet == 0)
      {
      KERR("ERROR  %s %d %s", fct, line, make_cmd_str(argv));
      log_what_the_command_said(ovs_dir);
      }
    else if (quiet == 2)
      {
      KERR("ERROR OVS COMMAND %s %d status %d%s%s %s", fct, line, status,
           netns ? " in netns " : "", netns ? netns : "", make_cmd_str(argv));
      log_what_the_command_said(ovs_dir);
      }
    result = -1;
    }
  protect_reentry = 0;
  return result;
}

void init_environ(char *net, char *ovs_bin, char *ovs_dir)
{
  setenv("OVS_BINDIR",  ovs_bin, 1);
  setenv("OVS_RUNDIR",  ovs_dir, 1);
  setenv("OVS_LOGDIR",  ovs_dir, 1);
  setenv("OVS_DBDIR",   ovs_dir, 1);
  setenv("CLOONIX_NET", net, 1);
}

