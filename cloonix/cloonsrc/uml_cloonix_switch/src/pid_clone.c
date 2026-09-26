/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sched.h>
#include <malloc.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pty.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "pid_clone.h"
#include "uml_clownix_switch.h"

  #include "commun_daemon.h"
  #include "cfg_store.h"
  #include "llid_trace.h"
  #include "event_subscriber.h"
  #include "qmp.h"
void suid_power_fork_closed(void);

#define XML_OPEN  "{<ascii_free_format_text>"
#define XML_CLOSE "</ascii_free_format_text>}"
#define XML_SYS_OPEN  "{<msg_for_main_pid_mngt>"
#define XML_SYS_CLOSE "</msg_for_main_pid_mngt>}"
#define START_FORK_PID "START_FORK_PID:"
#define END_FORK_PID   "END_FORK_PID:"
#define SENDER_PID     "SENDER_PID:"

#define MAX_FORK_IDENT 5000
#define PROCESS_STACK 500*1024

void cloonix_lock_fd_close(void);

typedef struct t_clone_ctx
{
  int used;
  int pid; 
  int fd_not_to_close;
  int no_kerr;
  void *stack;
  t_fct_to_launch fct;
  t_launched_death death;
  t_from_child_to_main fct_msg;
  void *param_data_start;
  void *param_data_end;
  void *param_data_msg;
  char vm_name[MAX_NAME_LEN];
} t_clone_ctx;

static int pid_dead_clone(int pid, int status);
static void pid_clone_harvest_death(void);

static void send_to_pid_mngt(int llid, char *str);
static int current_max_pid;
static t_clone_ctx clone_ctx[MAX_FORK_IDENT];
static int client_llid;

static char clowncom[MAX_PATH_LEN];

extern int g_i_am_a_clone;
extern int g_i_am_a_clone_no_kerr;

typedef struct t_jfs_tab
{
  int pid;
  int status;
} t_jfs_tab;

static void *g_jfs;

char *pid_get_clone_internal_com(void)
{
  return clowncom;
}

static int check_pid_exists(int pid)
{
  int fd, result = 0;
  char path[MAX_PATH_LEN];
  snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
  fd = open(path, O_RDONLY);
  if (fd >= 0)
    {
    result = 1;
    if (close(fd))
      KOUT("%d", errno);
    }
  return result;
}

static void timer_clones_check(void *data)
{
  int i;
  pid_clone_harvest_death();
  for (i=1; i <= current_max_pid; i++)
    {
    if (clone_ctx[i].used == i)
      {
      if (!check_pid_exists(clone_ctx[i].pid))
        {
        if (!pid_dead_clone(clone_ctx[i].pid, -1))
          KOUT(" ");
        } 
      }
    }
  clownix_timeout_add(50, timer_clones_check, NULL, NULL, NULL); 
}

static int pid_find_ident(int pid)
{
  int i, ident = 0;
  for (i=1; i <= current_max_pid; i++)
    if (clone_ctx[i].used == i)
      {
      if (clone_ctx[i].pid == pid)
        {
        ident = i;
        break;
        }
      }
  return ident;
}

static t_clone_ctx *pid_find_clone_ctx(int pid)
{
  t_clone_ctx *result = NULL;
  int ident = pid_find_ident(pid);
  if (ident)
    result = &(clone_ctx[ident]);
  return result;
}

static void err_cb(int llid, int err, int from)
{
  exit(-1);
}

static void rx_from_main(int llid, int len, char *buf)
{
  int rx_pid, status;
  char *ptr_start = buf;
  char *ptr_end;
  if (!msg_exist_channel(llid))
    KOUT(" ");
  if  (!strncmp(buf, XML_SYS_OPEN, strlen(XML_SYS_OPEN)))
    {
    ptr_start += strlen(XML_SYS_OPEN);
    ptr_start += strspn(ptr_start, " \r\n\t");
    ptr_end = strstr(ptr_start, XML_SYS_CLOSE);
    if (!ptr_end)
      KOUT(" ");
    *ptr_end = 0;
    ptr_end = ptr_start + strcspn(ptr_start, " \r\n\t");
    *ptr_end = 0;
    if (!strncmp(ptr_start, END_FORK_PID, strlen(END_FORK_PID)))
      {
      ptr_start += strlen(END_FORK_PID);
      if (sscanf(ptr_start, "%d,%d", &rx_pid, &status) != 2)
        KOUT("%s ", buf);
      if (rx_pid != getpid())
        KOUT("%d %d", rx_pid, getpid());
      if (status)
        exit(-1);
      else
        exit(0);
      }
    else
      KOUT("%s", buf);
    }
  else
    KOUT("%s", buf);
}

/* Only the switch runs the clone server that installs this callback. */
static void rx_from_child(int llid, int len, char *buf)
{
  char pid_str[MAX_NAME_LEN];
  int rx_pid, status;
  char *ptr_start = buf;
  char *ptr_end;
  t_clone_ctx *ctx;
  if (!msg_exist_channel(llid))
    KOUT(" ");
  if ((size_t)len != strlen(buf) && (size_t)len != strlen(buf)+1)
    KOUT("%d %d %d", llid, len, (int)strlen(buf));
  if (!strncmp(buf, XML_OPEN SENDER_PID, strlen(XML_OPEN SENDER_PID)))
    {
    ptr_start += strlen(XML_OPEN SENDER_PID);
    if (sscanf(ptr_start, "%d", &rx_pid) != 1)
      KOUT("%s ", buf);
    ctx = pid_find_clone_ctx(rx_pid);
    if (ctx)
      {
      ptr_start += strcspn(ptr_start, " \r\n\t"); 
      ptr_start += strspn(ptr_start, " \r\n\t"); 
      ptr_end = strstr(ptr_start, XML_CLOSE);
      if (!ptr_end)
        KOUT(" ");
      *ptr_end = 0;
      if ((strlen(ptr_start)) && (ctx->fct_msg))
        ctx->fct_msg(ctx->param_data_msg, ptr_start);
      }
    }
  else if  (!strncmp(buf, XML_SYS_OPEN, strlen(XML_SYS_OPEN)))
    {
    ptr_start += strlen(XML_SYS_OPEN);
    ptr_start += strspn(ptr_start, " \r\n\t");
    ptr_end = strstr(ptr_start, XML_SYS_CLOSE);
    if (!ptr_end)
      KOUT(" ");
    *ptr_end = 0;
    if (!strncmp(ptr_start, START_FORK_PID, strlen(START_FORK_PID)))
      {
      ptr_start += strlen(START_FORK_PID);
      if (sscanf(ptr_start, "%d", &rx_pid) != 1)
        KOUT("%s ", buf);
      }
    else if (!strncmp(ptr_start, END_FORK_PID, strlen(END_FORK_PID)))
      {
      ptr_start += strlen(END_FORK_PID);
      if (sscanf(ptr_start, "%d,%d", &rx_pid, &status) != 2)
        KOUT("%s ", buf);
      snprintf(pid_str, sizeof(pid_str), "%s%d,%d", END_FORK_PID, rx_pid, status);
      send_to_pid_mngt(llid, pid_str);
      }
    else
      KOUT("%s", buf);
    }
  else
    KOUT("llid: %d  Pid:%d %s", llid, getpid(), buf);
}

static t_clone_ctx *pid_clone_alloc_ident(void *stack, 
                                          t_fct_to_launch fct,
                                          t_launched_death death, 
                                          t_from_child_to_main fct_msg,
                                          void *data_start, 
                                          void *data_end,
                                          void *data_msg,
                                          char *vm_name,
                                          int fd_not_to_close,
                                          int no_kerr)
{
  int i, free_ident = 0;
  t_clone_ctx *ctx;
  for (i=1; i<MAX_FORK_IDENT; i++)
    if (clone_ctx[i].used == 0)
      {
      free_ident = i;
      clone_ctx[i].used = i;
      if (i > current_max_pid)
        current_max_pid = i;
      break;
      }
  if (!free_ident)
    KOUT(" ");
  ctx = &(clone_ctx[free_ident]);
  ctx->stack = stack;
  ctx->fct   = fct;
  ctx->death = death;
  ctx->fct_msg  = fct_msg;
  ctx->param_data_start = data_start;
  ctx->param_data_end   = data_end;
  ctx->param_data_msg   = data_msg;
  ctx->fd_not_to_close = fd_not_to_close;
  ctx->no_kerr = no_kerr;
  if (vm_name)
    strcpy(ctx->vm_name, vm_name);
  return (ctx);
}

static void pid_clone_free_ident(int ident)
{
  int i;
  t_clone_ctx *ctx = &(clone_ctx[ident]);
  if (ident != ctx->used)
    KOUT(" ");
  clownix_free(ctx->stack, __FUNCTION__);
  memset(ctx, 0, sizeof(t_clone_ctx));
  if (ident == current_max_pid)
    {
    for (i=1; i<MAX_FORK_IDENT; i++)
      if (clone_ctx[i].used)
        current_max_pid = i;
    }
}

static int pid_dead_clone(int pid, int status)
{
  char name[MAX_NAME_LEN];
  int  result;
  t_clone_ctx *ctx;
  result = pid_find_ident(pid);
  if (result)
    {
    ctx = &(clone_ctx[result]);
    if (ctx->death)
      {
      strcpy(name, ctx->vm_name);
      if (strlen(ctx->vm_name))
        ctx->death(ctx->param_data_end, status, ctx->vm_name);
      else
        ctx->death(ctx->param_data_end, status, NULL);
      }
    pid_clone_free_ident(result);
    }
  return result;
}

void pid_clone_harvest_death(void)
{
  int i, res, status=0;
  int options = WNOHANG | WEXITED | WSTOPPED | WCONTINUED;
  siginfo_t infop;
  for (i=1; i <= current_max_pid; i++)
    {
    if (clone_ctx[i].used == i)
      {
      memset(&infop, 0, sizeof(siginfo_t));
      res = waitid(P_PID, clone_ctx[i].pid, &infop, options);
      if ((res==0) && (clone_ctx[i].pid == infop.si_pid))
        {
        status = infop.si_status;
        if (!pid_dead_clone(clone_ctx[i].pid, status))
          KERR("ERROR");
        }
      }
    }
}

static void fct_job_for_select(int ref_id, void *data)
{
  if (ref_id != 0xCAB)
    KOUT(" ");
  pid_clone_harvest_death();
}

static void child_death_catcher_signal(int n)
{
  if (job_for_select_request(g_jfs, fct_job_for_select, NULL))
    KERR(" ");
}

int pid_clone_kill_single(int pid)
{
  int ident = pid_find_ident(pid);
  int result = -1;
  kill(pid, SIGKILL);
  if (ident)
    result = 0;
  else
    KERR("WARNING BAD KILL %d", pid);
  return result;
}

void pid_clone_kill_all(void)
{
  int i;
  for (i=1; i <= current_max_pid; i++)
    if (clone_ctx[i].used == i)
      pid_clone_kill_single(clone_ctx[i].pid);
}

static int forked_fct(void *ptr)
{
  t_clone_ctx *ctx = (t_clone_ctx *) ptr;
  int i, nb_chan, llid;

  char pid_str[MAX_NAME_LEN];
  int status;
  closelog();
  g_i_am_a_clone = 1;
  if (ctx->no_kerr)
    g_i_am_a_clone_no_kerr = 1;
  else
    g_i_am_a_clone_no_kerr = 0;
  openlog("forked_fct", 0, LOG_USER);
  signal(SIGCHLD, SIG_DFL);

  suid_power_fork_closed();
  llid_free_all_llid();
  cloonix_lock_fd_close();
  job_for_select_close_fd();
  clownix_timeout_del_all();
  qmp_clean_all_data();
  nb_chan = get_clownix_channels_nb();
  for (i=0; i<=nb_chan; i++)
    {
    llid = channel_get_llid_with_cidx(i);
    if (llid)
      llid_trace_free_if_exists(llid);
    }
  msg_mngt_init("clone", IO_MAX_BUF_LEN);
  client_llid = string_client_unix(pid_get_clone_internal_com(), 
                                   err_cb, rx_from_main, "clone_client");
  if (client_llid != 0)
    {
    llid_trace_alloc(client_llid,"CLONE_CLI",0,0,type_llid_trace_clone);
    snprintf(pid_str, sizeof(pid_str), "%s%d", START_FORK_PID, getpid());
    send_to_pid_mngt(client_llid, pid_str);
    status = ctx->fct(ctx->param_data_start);
    snprintf(pid_str, sizeof(pid_str), "%s%d,%d", END_FORK_PID, getpid(), status);
    send_to_pid_mngt(client_llid, pid_str);
    msg_mngt_loop();
    }
  else
    KERR("%s", pid_get_clone_internal_com());

  return 0;
}

int pid_clone_launch(t_fct_to_launch fct, t_launched_death death,
                     t_from_child_to_main fct_msg,
                     void *data_start, void *data_end, void *data_msg,
                     char *vm_name, int fd_not_to_close, int no_kerr)
{
  int pid;
  void* stack;
  t_clone_ctx *ctx;
  int flags = SIGCHLD;
  stack = clownix_malloc(PROCESS_STACK, 17);
  if ( stack == 0 )
    KOUT(" ");
  ctx = pid_clone_alloc_ident(stack, fct, death, fct_msg,
                              data_start, data_end, data_msg, 
                              vm_name, fd_not_to_close, no_kerr);
  pid = clone(forked_fct, (char*)stack+PROCESS_STACK,flags, (void *)(ctx));
  if ( pid == -1 )
    KOUT("%d", errno);
  ctx->pid = pid;
  return pid;
}

/* Reports only when clones are left: called at the last step of a shutdown,
   where an empty list is the normal case. */
void kerr_running_pids(void)
{
  int i, nb = 0;
  for (i=1; i<MAX_FORK_IDENT; i++)
    {
    if (clone_ctx[i].used)
      nb += 1;
    }
  if (nb == 0)
    return;
  KERR("WARNING %d clone(s) still running at the last step:", nb);
  for (i=1; i<MAX_FORK_IDENT; i++)
    {
    if (clone_ctx[i].used)
      KERR("WARNING   clone %s", clone_ctx[i].vm_name);
    }
}

static void send_to_pid_mngt(int llid, char *str)
{
  int len;
  char *buf;
  len = strlen(str) + strlen(XML_SYS_OPEN) + strlen(XML_SYS_CLOSE) + 10;
  len += 8;
  buf  = (char *)clownix_malloc(len, 17);
  memset (buf, 0, len);
  buf[0] = 0xCA; buf[1] = 0xFE; buf[2] = 0xDE; buf[3] = 0xCA;
  buf[4] = 0xCA; buf[5] = 0xFE; buf[6] = 0xDE; buf[7] = 0xCA;
  len = sprintf(buf+8, "%s %s %s", XML_SYS_OPEN, str, XML_SYS_CLOSE);
  len += 1 + 8;  /* +1 for null terminator, +8 for magic prefix */
  if (!msg_exist_channel(llid))
    KERR("%s", str);
  else
    string_tx_now(llid, len, buf);
  clownix_free(buf, __FUNCTION__);
}

void send_to_daddy (char *str)
{
  int len;
  char *buf;
  len = strlen(str)+strlen(XML_OPEN)+strlen(XML_CLOSE)+strlen(SENDER_PID)+10;
  len += 8;
  buf  = (char *)clownix_malloc(len, 17);
  memset (buf, 0, len);
  buf[0] = 0xCA; buf[1] = 0xFE; buf[2] = 0xDE; buf[3] = 0xCA;
  buf[4] = 0xCA; buf[5] = 0xFE; buf[6] = 0xDE; buf[7] = 0xCA;
  len = sprintf(buf+8,"%s%s%d %s %s",XML_OPEN,SENDER_PID,getpid(),str,XML_CLOSE);
  len += 1 + 8;  /* +1 for null terminator, +8 for magic prefix */
  if (!msg_exist_channel(client_llid))
    KERR("%s", str);
  else
    string_tx_now(client_llid, len, buf);
  clownix_free(buf, __FUNCTION__);
}

static void connect_from_client_clone(int llid, int llid_new)
{
  if (!msg_exist_channel(llid))
    KOUT(" ");
  llid_trace_alloc(llid_new,"CLONE_SRV",0,0,type_llid_trace_clone);
  msg_mngt_set_callbacks(llid_new,uml_clownix_switch_error_cb,rx_from_child);
}

void pid_clone_init(void)
{
  int llid1, llid2;
  int llid;

  signal(SIGCHLD, child_death_catcher_signal);
  memset(clone_ctx, 0, MAX_FORK_IDENT * sizeof(t_clone_ctx));
  current_max_pid = 0;
  clownix_timeout_add(100, timer_clones_check, NULL, NULL, NULL); 

  sprintf(clowncom, "%s/%s",  cfg_get_root_work(), CLOONIX_INTERNAL_COM);
  llid = string_server_unix(pid_get_clone_internal_com(), 
                            connect_from_client_clone, "clone_serv");
  if (llid > 0)
    llid_trace_alloc(llid,"CLONE_LISTEN",0,0,type_llid_trace_listen_clone);
  else
    KOUT(" ");
  g_jfs = job_for_select_alloc(0xCAB, &llid1, &llid2);
  llid_trace_alloc(llid1,"JFS_ACK",0,0,type_llid_trace_jfs);
  llid_trace_alloc(llid2,"JFS_REQ",0,0,type_llid_trace_jfs);

}

