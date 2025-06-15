/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <pty.h>
#include <sched.h>
#include <sys/prctl.h>

#include "mdl.h"
#include "low_write.h"
#include "wrap.h"
#include "glob_common.h"
#include "thread_tx.h"

void hide_real_machine_serv(void);
int display_add_rank(int net_rank, int srv_idx);

#define MAX_ARGC 100

typedef struct t_pty_cli
{
  int pty_fd;
  int pid;
  int sock_fd;
  uint32_t randid;
  struct t_pty_cli *prev;
  struct t_pty_cli *next;
}t_pty_cli;
static t_pty_cli *g_pty_cli_head;
static int g_net_rank;
static int g_sig_write_fd;
static int g_sig_read_fd;
static int g_listen_sock_cloon;
static char g_user[MAX_TXT_LEN];
static char g_xauthority[MAX_TXT_LEN];
static char g_net_name[MAX_TXT_LEN];
static struct timeval g_last_cloonix_tv;


/****************************************************************************/
static t_pty_cli *find_with_fd(int sock_fd)
{
  t_pty_cli *cur = g_pty_cli_head;
  while(cur)
    {
    if (cur->sock_fd == sock_fd)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void pty_cli_alloc(int pty_fd, int pid, int sock_fd, uint32_t randid)
{
  int i;
  t_pty_cli *cli = find_with_fd(sock_fd);
  if (cli)
    KERR("ERROR  pty_fd:%d pid:%d sock_fd:%d", pty_fd, pid, sock_fd);
  else
    {
    cli = (t_pty_cli *) wrap_malloc(sizeof(t_pty_cli));
    memset(cli, 0, sizeof(t_pty_cli));
    cli->pty_fd = pty_fd;
    cli->pid = pid;
    cli->sock_fd = sock_fd;
    cli->randid = randid;
    if (g_pty_cli_head)
      g_pty_cli_head->prev = cli;
    cli->next = g_pty_cli_head;
    g_pty_cli_head = cli;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void pty_cli_free(t_pty_cli *cli)
{
  if (cli->prev)
    cli->prev->next = cli->next;
  if (cli->next)
    cli->next->prev = cli->prev;
  if (cli == g_pty_cli_head)
    g_pty_cli_head = cli->next;
  if (cli->pty_fd != -1)
    {
    wrap_close(cli->pty_fd, __FUNCTION__);
    mdl_close(cli->pty_fd);
    }
  wrap_free(cli, __LINE__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int send_msg_type_end(uint32_t randid, int sock_fd, char status)
{
  int len = MAX_MSG_LEN+g_msg_header_len, result;
  t_msg *msg;
  if (sock_fd == -1)
    KERR(" ");
  else
    {
    msg = (t_msg *) wrap_malloc(len);
    mdl_set_header_vals(msg, randid, msg_type_end_cli_pty, fd_type_srv, 0, 0);
    msg->len = 1;
    msg->buf[0] = status;
    result = mdl_queue_write_msg(sock_fd, msg);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int xauth_add_magic_cookie(int display_val, char *magic_cookie)
{
  int result = -1;
  char dpyname[MAX_TXT_LEN];
  char buf[MAX_TXT_LEN];
  char *xaf = g_xauthority;
  FILE *fp;
  char *argv[10];
  char *acmd;
  memset(argv, 0, 10*sizeof(char *));
  memset(buf, 0, MAX_TXT_LEN);
  memset(dpyname, 0, MAX_TXT_LEN);
  snprintf(dpyname, MAX_TXT_LEN-1, X11_DISPLAY_NAME, display_val);
  argv[0] = XAUTH_BIN;
  argv[1] = "-f";
  argv[2] = xaf;
  argv[3] = "add";
  argv[4] = dpyname;
  argv[5] = MAGIC_COOKIE;
  argv[6] = magic_cookie;
  argv[7] = NULL;
  acmd = mdl_argv_linear(argv);
  fp = mdl_argv_popen(argv);
  if (fp == NULL)
    KERR("ERROR %s", acmd);
  else
    {
    if (fgets(buf, MAX_TXT_LEN-1, fp))
      KERR("ERROR %s %s", acmd, buf);
    if (pclose(fp))
      KERR("ERROR %s", acmd);
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void clean_xauthority(char *xauthority_file, char *end_file)
{
  char xauth_locks[2*MAX_TXT_LEN];
  strcpy(xauth_locks, xauthority_file);
  strcat(xauth_locks, end_file);
  unlink(xauth_locks);
  if (wrap_file_exists(xauth_locks))
    KOUT("ERROR XAUTHORITY file lock  %s", xauth_locks);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_env_display(int net_rank, int display_val, char *ttyname)
{
  char rdir[MAX_PATH_LEN];
  char disp_xwy[MAX_TXT_LEN];
  char disp_X11[MAX_TXT_LEN];
  char *net = g_net_name;
  char *px86_64="/usr/libexec/cloonix/cloonfs/lib/x86_64-linux-gnu/qt6/plugins";
  int display_val_rank;
  memset(disp_xwy, 0, MAX_TXT_LEN);
  memset(disp_X11, 0, MAX_TXT_LEN);
  setenv("PATH",  "/usr/libexec/cloonix/cloonfs/bin:/usr/libexec/cloonix/cloonfs/sbin", 1);
  setenv("LC_ALL", "C", 1);
  setenv("LANG", "C", 1);
  setenv("XAUTHORITY", g_xauthority, 1);
  setenv("SHELL", "/usr/libexec/cloonix/cloonfs/bin/cloonix-bash", 1);
  setenv("TERM", "xterm", 1);
  if (display_val > 0)
    {
    display_val_rank = display_add_rank(net_rank, display_val);
    snprintf(disp_xwy, MAX_TXT_LEN-1, "%s_%d", XWY_X11_SOCK, display_val);
    snprintf(disp_X11, MAX_TXT_LEN-1, ":%d", display_val_rank);
    setenv("XWY_DISPLAY", disp_xwy, 1);
    setenv("DISPLAY", disp_X11, 1);
    if (strlen(ttyname))
      setenv("SSH_TTY", ttyname, 1);
    }
  else
    KERR("ERROR Problem setting DISPLAY");
  memset(rdir, 0, MAX_PATH_LEN);
  snprintf(rdir, MAX_PATH_LEN-1, "/var/lib/cloonix/%s/run", net);
  setenv("XDG_RUNTIME_DIR", rdir, 1);
  setenv("XDG_CACHE_HOME", rdir, 1);
  setenv("XDG_DATA_HOME", rdir, 1);
  memset(rdir, 0, MAX_PATH_LEN);
  snprintf(rdir, MAX_PATH_LEN-1, "/var/lib/cloonix/%s/run/.config", net);
  setenv("XDG_CONFIG_HOME", rdir, 1);
  setenv("NO_AT_BRIDGE", "1", 1);
  setenv("QT_X11_NO_MITSHM", "1", 1);
  setenv("QT_XCB_NO_MITSHM", "1", 1);
  if (wrap_file_exists(px86_64))
    {
    setenv("QT_PLUGIN_PATH", px86_64, 1);
    setenv("PIPEWIRE_MODULE_DIR", "/usr/libexec/cloonix/cloonfs/lib/x86_64-linux-gnu/pipewire-0.3", 1);
    }
  setenv("GST_PLUGIN_SCANNER", "/usr/libexec/cloonix/cloonfs/lib/x86_64-linux-gnu/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner", 1);
  setenv("GST_PLUGIN_PATH", "/usr/libexec/cloonix/cloonfs/lib/x86_64-linux-gnu/gstreamer-1.0", 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_argv_from_cmd(char *cmd, char **argv)
{
  static char comp_cmd[MAX_MSG_LEN + MAX_PATH_LEN];
  memset(comp_cmd, 0, MAX_MSG_LEN + MAX_PATH_LEN);
  snprintf(comp_cmd, MAX_MSG_LEN + MAX_PATH_LEN -1,
           "%s ; /usr/libexec/cloonix/cloonfs/bin/sleep 1", cmd);
  argv[0] = "/usr/libexec/cloonix/cloonfs/bin/cloonix-bash";
  argv[1] = "-c";
  argv[2] = comp_cmd;
  argv[3] = NULL;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_argv_from_crun(char *cmd, char **argv)
{
  int len, nb, i;
  char *cmd_ptr=cmd, *ptr[MAX_ARGC];
  memset(ptr, 0, MAX_ARGC * sizeof(char *));
  for (i=0; i<MAX_ARGC-1; i++)
    {
    if (strlen(cmd_ptr) == 0)
      break;
    len = strspn(cmd_ptr, " \t"); 
    if (len)
      {
      cmd_ptr = cmd_ptr + len;;
      }
    ptr[i] = cmd_ptr + len;
    if (ptr[i][0] == '"')
      {
      cmd_ptr = strchr(ptr[i]+1, '"');
      if (!cmd_ptr)
        {
        KERR("%s", cmd);
        break;
        }
      ptr[i] += 1;
      *cmd_ptr = 0;
      cmd_ptr += 1;
      }
    else
      {
      len = strcspn(ptr[i], " \t\n"); 
      cmd_ptr = ptr[i] + len;
      *cmd_ptr = 0;
      cmd_ptr += 1;
      }
    argv[i] = ptr[i];
    nb = i;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void sigusr1_child_handler(int dummy)                           
{                                          
  KERR("WARNING  Parent died, child now exiting\n");    
  exit(0);                                  
}                            
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pty_fork_bin_bash(int action, uint32_t randid, int sock_fd,
                         char *cmd, int net_rank, int display_val)
{
  char ttyname[MAX_TXT_LEN], *argv[MAX_ARGC];
  int i, pty_fd=-1, ttyfd=-1, pid;
  char *ptr, *ptre;
  memset(argv, 0, MAX_ARGC * sizeof(char *));
  memset(ttyname, 0, MAX_TXT_LEN);
  if (action != action_dae)
    {
    if (wrap_openpty(&pty_fd, &ttyfd, ttyname, fd_type_fork_pty))
      KOUT(" ");
    if (mdl_exists(pty_fd))
      {
      KERR("ERROR pty_fd %d exists in mdl", pty_fd);
      return;
      }
    wrap_pty_make_controlling_tty(ttyfd, ttyname);
    }
  pid = fork();
  if (pid < 0)
    KOUT("%s", strerror(errno));
  if (pid == 0)
    {
    clearenv();
    create_env_display(net_rank, display_val, ttyname); 
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_DFL);
    if (signal(SIGUSR1, sigusr1_child_handler) == SIG_ERR)          
       KERR("ERROR signal failed");                  
    prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);
    if (setsid() < 0)
      KOUT("setsid: %s", strerror(errno));
    if (action != action_dae)
      {
      close(0);
      close(1);
      close(2);
      if (dup2(ttyfd, 0) < 0)
        KERR("dup2 stdin: %s", strerror(errno));
      if (dup2(ttyfd, 1) < 0)
        KERR("dup2 stdout: %s", strerror(errno));
      if (dup2(ttyfd, 2) < 0)
        KERR("dup2 stderr: %s", strerror(errno));
      wrap_nonnonblock(0);
      wrap_nonnonblock(1);
      wrap_nonnonblock(2);
      }
    for (i=3; i<MAX_FD_NUM; i++)
      close(i);
    if (action == action_bash)
      {
      argv[0] = "/usr/libexec/cloonix/cloonfs/bin/cloonix-bash";
      argv[1] = NULL;
      }
    else if (action == action_crun)
      {
      if (cmd[0] == '"')
        {
        ptr = cmd+1;
        ptre = strchr(ptr, '"');
        if (!ptre)
          KERR("ERROR action_crun %s", cmd);
        else
          *ptre = 0;
        }
      else
        ptr = cmd;
      create_argv_from_crun(ptr, argv);
      }
    else  if ((action == action_cmd) ||
              (action == action_dae) ||
              (action == action_ovs) ||
              (action == action_slf))
      {
      if ((action == action_cmd) || (action == action_dae))
        hide_real_machine_serv();
      create_argv_from_cmd(cmd, argv);
      }
    else
      KOUT("ERROR %d", action);
    execv(argv[0], argv);
    KOUT("ERROR execv %s", argv[0]);
    }
  else
    {
    if (action == action_dae)
      pty_cli_alloc(-1, pid, sock_fd, randid);
    else
      {
      pty_cli_alloc(pty_fd, pid, sock_fd, randid);
      wrap_close(ttyfd, __FUNCTION__);
      mdl_open(pty_fd, fd_type_pty, wrap_write_pty, wrap_read_kout);
      wrap_nonblock(pty_fd);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int cli_pty_fd_action(t_pty_cli *cli)
{
  int result = -1;
  t_msg *msg;
  int len = MAX_MSG_LEN+g_msg_header_len;
  msg = (t_msg *) wrap_malloc(len);
  len = wrap_read_pty(cli->pty_fd, msg->buf, MAX_MSG_LEN);
  if (len < 0)
    {
    if ((errno == EINTR) || (errno == EAGAIN))
      result = 0;
    else
      KERR("ERROR %d %d %d", cli->sock_fd, cli->pty_fd, errno); 
    }
  else if (len == 0)
    KERR("ERROR %d %d", cli->sock_fd, cli->pty_fd); 
  else
    {
    mdl_set_header_vals(msg, cli->randid, msg_type_data_cli_pty,
                        fd_type_pty, 0, 0);
    msg->len = len;
    result = mdl_queue_write_msg(cli->sock_fd, msg);
    if (result)
      KERR("ERROR %d %d", cli->sock_fd, cli->pty_fd); 
    }
  if (result)
    {
    wrap_close(cli->pty_fd, __FUNCTION__);
    mdl_close(cli->pty_fd);
    cli->pty_fd = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void child_death_detection(void)
{
  int status, pid;
  char exstat;
  t_pty_cli *next, *cur = g_pty_cli_head;
  if ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
    while(cur)
      {
      next = cur->next;
      if (cur->pid == pid)
        {
        if (WIFEXITED(status))
          exstat = WEXITSTATUS(status);
        else
          exstat = 1;
        if (cur->pty_fd == -1)
          {
          if (thread_tx_get_stored_bytes(cur->sock_fd) != 0)
            KERR("ERROR sock_fd not flushed");
          close(cur->sock_fd);
          pty_cli_free(cur);
          }
        else
          {
          send_msg_type_end(cur->randid, cur->sock_fd, exstat);
          }
        break;
        } 
      cur = next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void sig_evt_action(int sig_read_fd)
{
  int len, status, pid;
  char buf[1], exstat;
  len = wrap_read_sig(sig_read_fd, buf, 1);
  if ((len != 1) || (buf[0] != 's'))
    KOUT("ERROR %d %s", len, buf);
  else
    child_death_detection();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void child_exit(int sig)
{
  int len;
  char buf[1];
  buf[0] = 's';
  len = write(g_sig_write_fd, buf, 1);
  if (len != 1)
    KOUT("%d %s", len, strerror(errno));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int pty_fork_xauth_add_magic_cookie(int display_val, char *magic_cookie)
{
  int display_val_rank = display_add_rank(g_net_rank, display_val);
  return xauth_add_magic_cookie(display_val_rank, magic_cookie);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pty_fork_fdset(fd_set *readfds, fd_set *writefds)
{
  t_pty_cli *cur = g_pty_cli_head;
  int ret;
  while(cur)
    {
    if (cur->pty_fd != -1)
      {
      if (!mdl_fd_is_valid(cur->pty_fd))
        KERR("ERROR pty_fd");
      else
        {
        ret = low_write_levels_above_thresholds(cur->sock_fd);
        if (ret == 0)
          {
          FD_SET(cur->pty_fd, readfds);
          }
        if (low_write_not_empty(cur->pty_fd))
          {
          FD_SET(cur->pty_fd, writefds);
          }
        }
      }
    cur = cur->next;
    }
  FD_SET(g_sig_read_fd, readfds);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int pty_fork_fdisset(fd_set *readfds, fd_set *writefds)
{
  int result = 0;
  t_pty_cli *cur = g_pty_cli_head;
  while(cur)
    {
    if (cur->pty_fd != -1)
      {
      if (FD_ISSET(cur->pty_fd, readfds))
        {
        result = cli_pty_fd_action(cur);
        }
      if (result == 0)
        {
        if (FD_ISSET(cur->pty_fd, writefds))
          {
          if (low_write_fd(cur->pty_fd))
            KERR("ERROR %d", cur->pty_fd);
          }
        }
      }
    cur = cur->next;
    }
  if (result == 0)
    {
    if (FD_ISSET(g_sig_read_fd, readfds))
      sig_evt_action(g_sig_read_fd);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int pty_fork_get_max_fd(int val)
{
  int result = val;
  t_pty_cli *cur = g_pty_cli_head;
  while(cur)
    {
    if (cur->pty_fd != -1)
      {
      if (cur->pty_fd > result)
        result = cur->pty_fd;
      }
    cur = cur->next;
    }
  if (g_sig_read_fd > result)
    result = g_sig_read_fd;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pty_fork_msg_type_win_size(int sock_fd, int len, char *buf)
{
  char win_size_buf[16];
  t_pty_cli *cli = find_with_fd(sock_fd);
  if (cli)
    {
    if (len > 16)
      KOUT("%d", len);
    if (cli->pty_fd != -1)
      {
      memcpy(win_size_buf, buf, 16);
      ioctl(cli->pty_fd, TIOCSWINSZ, win_size_buf);
      ioctl(cli->pty_fd, TIOCSIG, SIGWINCH);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pty_fork_msg_type_data_pty(int sock_fd, t_msg *msg)
{
  t_pty_cli *cli = find_with_fd(sock_fd);
  if (cli)
    {
    if (cli->pty_fd != -1)
      {
      low_write_raw(cli->pty_fd, msg, 0);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int pty_fork_free_with_fd(int sock_fd)
{
  int result = -1;
  t_pty_cli *cli = find_with_fd(sock_fd);
  if (cli)
    {
    result = 0;
    pty_cli_free(cli);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pty_fork_init(char *net_name, int net_rank)
{
  char cmd[2 * MAX_PATH_LEN];
  int pipe_fd[2];
  memset(g_net_name, 0, MAX_TXT_LEN);
  memset(g_xauthority, 0, MAX_TXT_LEN);
  memset(cmd, 0, 2 * MAX_PATH_LEN);
  strncpy(g_net_name, net_name, MAX_TXT_LEN-1);
  g_net_rank = net_rank;
  if (signal(SIGCHLD, child_exit) == SIG_ERR)
    KERR("ERROR %s", strerror(errno));
  snprintf(g_xauthority, MAX_TXT_LEN-1,
           "/var/lib/cloonix/%s/.Xauthority", g_net_name); 
  sprintf(cmd, "/usr/libexec/cloonix/cloonfs/bin/touch %s", g_xauthority);
  system(cmd);
  if (wrap_pipe(pipe_fd, fd_type_pipe_sig, __FUNCTION__) < 0)
    KOUT(" ");
  g_sig_write_fd = pipe_fd[1];
  g_sig_read_fd = pipe_fd[0];
}
/*--------------------------------------------------------------------------*/
