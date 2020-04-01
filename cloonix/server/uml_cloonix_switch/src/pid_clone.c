/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
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

#if defined(IS_DOORWAYS)
  #include "doorways_sock.h"
#else
  #include "commun_daemon.h"
  #include "cfg_store.h"
  #include "llid_trace.h"
  #include "event_subscriber.h"
  #include "qmp.h"
#endif

#define XML_OPEN  "<ascii_free_format_text>"
#define XML_CLOSE "</ascii_free_format_text>"
#define XML_SYS_OPEN  "<msg_for_main_pid_mngt>"
#define XML_SYS_CLOSE "</msg_for_main_pid_mngt>"
#define START_FORK_PID "START_FORK_PID:"
#define END_FORK_PID   "END_FORK_PID:"
#define SENDER_PID     "SENDER_PID:"


#define MAX_FORK_IDENT 500
#define MAX_DEAD_PIDS_CIRCLE 50
#define PROCESS_STACK 500*1024

void cloonix_lock_fd_close(void);
void blkd_sub_clean_all(void);
void blkd_data_clean_all(void);


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
static int nb_running_pids;
static t_clone_ctx clone_ctx[MAX_FORK_IDENT];
static int client_llid;

#if !defined(IS_DOORWAYS)
static char clowncom[MAX_PATH_LEN];
#endif

extern int g_i_am_a_clone;
extern int g_i_am_a_clone_no_kerr;

typedef struct t_jfs_tab
{
  int pid;
  int status;
} t_jfs_tab;

static void *g_jfs;


/*---------------------------------------------------------------------------*/

/*****************************************************************************/
#if !defined(IS_DOORWAYS)

char *pid_get_clone_internal_com(void)
{
  return clowncom;
}

#endif
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int check_pid_exists(int pid)
{
  int fd, result = 0;
  char path[MAX_PATH_LEN];
  sprintf(path, "/proc/%d/cmdline", pid);
  fd = open(path, O_RDONLY);
  if (fd >= 0)
    {
    result = 1;
    if (close(fd))
      KOUT("%d", errno);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
        KERR(" %d %s ", clone_ctx[i].pid, clone_ctx[i].vm_name);
        if (!pid_dead_clone(clone_ctx[i].pid, -1))
          KOUT(" ");
        } 
      }
    }
  clownix_timeout_add(50, timer_clones_check, NULL, NULL, NULL); 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_clone_ctx *pid_find_clone_ctx(int pid)
{
  t_clone_ctx *result = NULL;
  int ident = pid_find_ident(pid);
  if (ident)
    result = &(clone_ctx[ident]);
  return result;
}


/*****************************************************************************/
#if !defined(IS_DOORWAYS)
static void err_cb(void *ptr, int llid, int err, int from)
{
  exit(-1);
}
#endif
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
#if !defined(IS_DOORWAYS)
static void rx_from_main(int llid, int len, char *buf)
{
  int rx_pid, status;
  char *ptr_start = buf;
  char *ptr_end;
  if (!msg_exist_channel(llid))
    KOUT(" ");
  if (((int) strlen(buf) + 1) != (size_t)len)
    KOUT("%d %d %d", llid, len, (int)strlen(buf));
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
#endif
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void rx_from_child (int llid, int len, char *buf)
{
  char pid_str[MAX_NAME_LEN];
  int rx_pid, status;
  char *ptr_start = buf;
  char *ptr_end;
  static t_clone_ctx *clone_ctx;
  if (!msg_exist_channel(llid))
    KOUT(" ");
  if ((strlen(buf)+1) != (size_t)len)
    KOUT("%d %d %d", llid, len, (int)strlen(buf));
  if (!strncmp(buf, XML_OPEN SENDER_PID, strlen(XML_OPEN SENDER_PID)))
    {
    ptr_start += strlen(XML_OPEN SENDER_PID);
    if (sscanf(ptr_start, "%d", &rx_pid) != 1)
      KOUT("%s ", buf);
    clone_ctx = pid_find_clone_ctx(rx_pid);
    if (clone_ctx)
      {
      ptr_start += strcspn(ptr_start, " \r\n\t"); 
      ptr_start += strspn(ptr_start, " \r\n\t"); 
      ptr_end = strstr(ptr_start, XML_CLOSE);
      if (!ptr_end)
        KOUT(" ");
      *ptr_end = 0;
      if ((strlen(ptr_start)) && (clone_ctx->fct_msg))
        clone_ctx->fct_msg(clone_ctx->param_data_msg, ptr_start);
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
      sprintf(pid_str, "%s%d,%d", END_FORK_PID, rx_pid, status);
      send_to_pid_mngt(llid, pid_str);
      }
    else
      KOUT("%s", buf);
    }
  else
    KOUT("llid: %d  Pid:%d %s", llid, getpid(), buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
  nb_running_pids++;
  return (ctx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void pid_clone_free_ident(int ident)
{
  int i;
  t_clone_ctx *ctx = &(clone_ctx[ident]);
  if (ident != ctx->used)
    KOUT(" ");
  clownix_free(ctx->stack, __FUNCTION__);
  memset(ctx, 0, sizeof(t_clone_ctx));
  nb_running_pids--;
  if (ident == current_max_pid)
    {
    for (i=1; i<MAX_FORK_IDENT; i++)
      if (clone_ctx[i].used)
        current_max_pid = i;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void pid_clone_harvest_death(void)
{
  int res, pid=0, status=0;
  siginfo_t infop;
  memset(&infop, 0, sizeof(siginfo_t));
  res = waitid(P_ALL, 0, &infop, WNOHANG | WEXITED | WSTOPPED | WCONTINUED); 
  if (res==0)
    {
    pid = infop.si_pid;
    status = infop.si_status;
    }
  if (pid > 0)
    {
    if (!pid_dead_clone(pid, status))
      KERR(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fct_job_for_select(int ref_id, void *data)
{
  if (ref_id != 0xCAB)
    KOUT(" ");
  pid_clone_harvest_death();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void child_death_catcher_signal(int n)
{
  job_for_select_request(g_jfs, fct_job_for_select, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void pid_clone_check_and_kill(int pid)
{
  if (pid_find_ident(pid))
    kill(pid, SIGTERM);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void pid_clone_kill_single(int pid)
{
  int ident = pid_find_ident(pid);
  if (ident)
    kill(pid, SIGTERM);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void pid_clone_kill_all(void)
{
  int i;
  for (i=1; i <= current_max_pid; i++)
    if (clone_ctx[i].used == i)
      pid_clone_check_and_kill(clone_ctx[i].pid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int forked_fct(void *ptr)
{
  t_clone_ctx *ctx = (t_clone_ctx *) ptr;
  int i, nb_chan, llid;

#if !defined(IS_DOORWAYS)
  char pid_str[MAX_NAME_LEN];
  int sock, status;
#endif
  closelog();
  g_i_am_a_clone = 1;
  if (ctx->no_kerr)
    g_i_am_a_clone_no_kerr = 1;
  else
    g_i_am_a_clone_no_kerr = 0;
  openlog("forked_fct", 0, LOG_USER);
  signal(SIGCHLD, SIG_DFL);

#if !defined(IS_DOORWAYS)

  blkd_data_clean_all();
  blkd_sub_clean_all();
  rpct_clean_all(NULL);
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
  sock = socket(AF_UNIX, SOCK_STREAM, 0);
  client_llid = string_client_unix(pid_get_clone_internal_com(), 
                                   err_cb, rx_from_main, "clone_client");
  close(sock);
  if (client_llid != 0)
    {
    llid_trace_alloc(client_llid,"CLONE_CLI",0,0,type_llid_trace_clone);
    sprintf(pid_str, "%s%d", START_FORK_PID, getpid());
    send_to_pid_mngt(client_llid, pid_str);
    status = ctx->fct(ctx->param_data_start);
    sprintf(pid_str, "%s%d,%d", END_FORK_PID, getpid(), status);
    send_to_pid_mngt(client_llid, pid_str);
    msg_mngt_loop();
    }
  else
    KERR("%s", pid_get_clone_internal_com());

#else

  clownix_timeout_del_all();
  nb_chan = get_clownix_channels_nb();
  for (i=0; i<=nb_chan; i++)
    {
    llid = channel_get_llid_with_cidx(i);
    if (llid)
      {
      if (msg_exist_channel(llid))
        {
        if ((ctx->fd_not_to_close != -1) &&
            (ctx->fd_not_to_close == get_fd_with_llid(llid)))
          {
          set_fd_not_to_close(ctx->fd_not_to_close);
          }
        else
          {
          msg_delete_channel(llid);
          }
        }
      }
    }
  ctx->fct(ctx->param_data_start);

#endif

  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void kerr_running_pids(void)
{
  int i;
  for (i=1; i<MAX_FORK_IDENT; i++)
    {
    if (clone_ctx[i].used)
      {
      KERR("CLONE:%s", clone_ctx[i].vm_name);
      }
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int get_nb_running_pids(void)
{
  return(nb_running_pids);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_to_pid_mngt(int llid, char *str)
{
  int len;
  char *buf;
  len = strlen(str) + strlen(XML_SYS_OPEN) + strlen(XML_SYS_CLOSE) + 10;
  buf  = (char *)clownix_malloc(len, 17);
  memset (buf, 0, len);
  len = sprintf(buf, "%s %s %s", XML_SYS_OPEN, str, XML_SYS_CLOSE);
  if (!msg_exist_channel(llid))
    KERR("%s", str);
  else
    string_tx_now(llid, len, buf);
  clownix_free(buf, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void send_to_daddy (char *str)
{
  int len;
  char *buf;
  len = strlen(str)+strlen(XML_OPEN)+strlen(XML_CLOSE)+strlen(SENDER_PID)+10;
  buf  = (char *)clownix_malloc(len, 17);
  memset (buf, 0, len);
  len = sprintf(buf,"%s%s%d %s %s",XML_OPEN,SENDER_PID,getpid(),str,XML_CLOSE);
  if (!msg_exist_channel(client_llid))
    KERR("%s", str);
  else
    string_tx_now(client_llid, len, buf);
  clownix_free(buf, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int popenRWE(int *rp, char *exe, char *argv[])
{
  int out[2];
  int pid;
  if (pipe(out) < 0)
    KOUT(" ");
  if ((pid = fork()) < 0)
    KOUT(" ");
  if (pid > 0)
    {
    close(out[1]);
    *rp = out[0];
    }
  else if (pid == 0)
    {
    close(out[0]);
    close(1);
    dup(out[1]);
    execvp(exe, (char**)argv);
    }
  return pid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int my_popen(char *exe, char *argv[])
{
  static int count = 0;
  int rpipe, pid, status;
  FILE *fp;
  char line[MAX_PRINT_LEN];
  pid = popenRWE(&rpipe, exe, argv);
  fp = fdopen(rpipe, "r");
  if (fp)
    {
    while (fgets(line, MAX_PRINT_LEN, fp))
      {
      if (strlen(line))
        {
        send_to_daddy (line);
        }
      }
    fclose(fp);
    }
  close(rpipe);
  do
    {
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status))
      {
      count++;
      if (count == 10)
        KOUT(" ");
      usleep(10000);
      KERR("my_popen slip");
      }
    } while(!WIFEXITED(status));

  return status;
}
/*---------------------------------------------------------------------------*/


#if !defined(IS_DOORWAYS)
/*****************************************************************************/
static void connect_from_client_clone(void *ptr, int llid, int llid_new)
{
  if (!msg_exist_channel(llid))
    KOUT(" ");
  llid_trace_alloc(llid_new,"CLONE_SRV",0,0,type_llid_trace_clone);
  msg_mngt_set_callbacks(llid_new,uml_clownix_switch_error_cb,rx_from_child);
}
/*---------------------------------------------------------------------------*/
#endif


/*****************************************************************************/
void pid_clone_init(void)
{
#if !defined(IS_DOORWAYS)
  int llid1, llid2;
  int llid;
#endif

  signal(SIGCHLD, child_death_catcher_signal);
  memset(clone_ctx, 0, MAX_FORK_IDENT * sizeof(t_clone_ctx));
  current_max_pid = 0;
  nb_running_pids = 0;
  clownix_timeout_add(100, timer_clones_check, NULL, NULL, NULL); 

#if !defined(IS_DOORWAYS)
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
#endif

}
/*---------------------------------------------------------------------------*/







