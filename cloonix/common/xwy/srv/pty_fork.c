/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#include <sys/prctl.h>

#include "mdl.h"
#include "low_write.h"
#include "wrap.h"


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


static int g_sig_write_fd;
static int g_sig_read_fd;
static int g_listen_sock_cloonix;
static char g_home[MAX_TXT_LEN];
static char g_user[MAX_TXT_LEN];
static char g_xdg_runtime_dir[MAX_TXT_LEN];
static char g_path[MAX_TXT_LEN];
static char g_term[MAX_TXT_LEN];
static char g_shell[MAX_TXT_LEN];
static char g_xauthority[MAX_TXT_LEN];
static char g_xauthority_file[MAX_TXT_LEN];
static struct timeval g_last_cloonix_tv;


/****************************************************************************/
static void pty_cli_alloc(int pty_fd, int pid, int sock_fd, uint32_t randid)
{
  int i;
  t_pty_cli *cli = (t_pty_cli *) wrap_malloc(sizeof(t_pty_cli));
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
static t_pty_cli *find_with_sock_fd(int sock_fd)
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
  char cmd[2*MAX_TXT_LEN];
  char buf[MAX_TXT_LEN];
  char *xaf = g_xauthority_file;
  FILE *fp;
  memset(cmd, 0, 2*MAX_TXT_LEN);
  memset(buf, 0, MAX_TXT_LEN);
  memset(dpyname, 0, MAX_TXT_LEN);
  snprintf(dpyname, MAX_TXT_LEN-1, UNIX_X11_DPYNAME, display_val);
  snprintf(cmd, 2*MAX_TXT_LEN-1,
           "xauth -f %s add %s %s %s",xaf,dpyname,MAGIC_COOKIE,magic_cookie);
  fp = popen(cmd, "r");
  if (fp == NULL)
    KERR("%s", cmd);
  else
    {
    if (fgets(buf, MAX_TXT_LEN-1, fp))
      KERR("%s %s", cmd, buf);
    if (pclose(fp))
      KERR("%s", cmd);
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
    KOUT("XAUTHORITY file lock  %s", xauth_locks);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void init_all_env(void)
{
  char *home = getenv("HOME");
  char *user = getenv("USER");
  char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  memset(g_home, 0, MAX_TXT_LEN);
  memset(g_xdg_runtime_dir, 0, MAX_TXT_LEN);
  memset(g_path, 0, MAX_TXT_LEN);
  memset(g_term, 0, MAX_TXT_LEN);
  memset(g_shell, 0, MAX_TXT_LEN);
  memset(g_xauthority, 0, MAX_TXT_LEN);
  memset(g_xauthority_file, 0, MAX_TXT_LEN);
  memset(g_user, 0, MAX_TXT_LEN);
  if (user)
    strncpy(g_user, user, MAX_TXT_LEN-1);
  if (!home)
    {
    snprintf(g_home, MAX_TXT_LEN-1, "HOME=%s", "/root");
    sprintf(g_xauthority_file, "/root/.CloonixXauthority_root"); 
    snprintf(g_xauthority, MAX_TXT_LEN-1, "XAUTHORITY=%s", g_xauthority_file); 
    }
  else
    {
    snprintf(g_home, MAX_TXT_LEN-1, "HOME=%s", home);
    snprintf(g_xdg_runtime_dir, MAX_TXT_LEN-1,
             "XDG_RUNTIME_DIR=%s", xdg_runtime_dir);
    snprintf(g_xauthority_file, MAX_TXT_LEN-1, 
             "/tmp/.CloonixXauthority_%s", user); 
    snprintf(g_xauthority, MAX_TXT_LEN-1, "XAUTHORITY=%s", g_xauthority_file); 
    }
  unlink(g_xauthority_file);
  clean_xauthority(g_xauthority_file, "-c");
  clean_xauthority(g_xauthority_file, "-l");
  clean_xauthority(g_xauthority_file, "-n");
  if (wrap_touch(g_xauthority_file))
    KOUT("XAUTHORITY file %s", g_xauthority_file);
  snprintf(g_path,  MAX_TXT_LEN-1, "PATH=/usr/sbin:/usr/bin:/sbin:/bin");
  snprintf(g_term,  MAX_TXT_LEN-1, "TERM=xterm");
  snprintf(g_shell, MAX_TXT_LEN-1, "SHELL=/bin/bash");
  if (clearenv())
    KERR("Bad Clear ENV ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static char **create_env(int display_val, char *ttyname)
{
  int i;
  static char envttyname[MAX_TXT_LEN];
  static char disp_str[MAX_TXT_LEN];
  static char *env[] = {g_path, g_home, g_term, g_shell, NULL,
                        NULL , NULL, NULL, NULL, NULL, NULL,
                        NULL , NULL, NULL, NULL, NULL, NULL,
                        NULL , NULL, NULL, NULL, NULL, NULL};
  if (display_val > 0)
    {
    memset(disp_str, 0, MAX_TXT_LEN);
    snprintf(disp_str, MAX_TXT_LEN-1, LOCALHOST_X11_DISPLAY, display_val);
    if (strlen(ttyname))
      { 
      strcpy (envttyname, "SSH_TTY=");
      strcat (envttyname, ttyname);
      }
    i = 4;
    env[i++] = g_xauthority;
    env[i++] = g_xdg_runtime_dir;
    env[i++] = disp_str;
    env[i++] = "LIBGL_ALWAYS_INDIRECT=1";
    env[i++] = "LIBGL_ALWAYS_SOFTWARE=1";
    env[i++] = "LIBGL_DRI3_DISABLE=1";
    env[i++] = "QT_X11_NO_MITSHM=1";
    env[i++] = "QT_XCB_NO_MITSHM=1"; 
    if (strlen(ttyname))
      env[i++] = envttyname;
    }
  else
    KERR("Problem setting DISPLAY");
  return env;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_argv_from_cmd(char *cmd, char **argv)
{
  int len, i, nb;
  char *cmd_ptr, *ptr[MAX_ARGC];
  memset(ptr, 0, MAX_ARGC * sizeof(char *));
  cmd_ptr = cmd;
  for (i=0; i<MAX_ARGC-1; i++)
    {
    if (strlen(cmd_ptr) == 0)
      break;
    len = strspn(cmd_ptr, " \t"); 
    if (len)
      *cmd_ptr = 0;
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
      len = strcspn(ptr[i], " \t"); 
      cmd_ptr = ptr[i] + len;
      }
    argv[i] = ptr[i];
    nb = i;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void helper_wrap_forkpty(int action, uint32_t randid, int sock_fd,
                                char *cmd, int display_val)
{
  char ttyname[MAX_TXT_LEN], *argv[MAX_ARGC];
  int i, pty_fd=-1, ttyfd, pid;
  char **env;
  memset(argv, 0, MAX_ARGC * sizeof(char *));
  memset(ttyname, 0, MAX_TXT_LEN);
  if ((action == action_bash) || (action == action_cmd))
    {
    if (wrap_openpty(&pty_fd, &ttyfd, ttyname, fd_type_fork_pty))
      KOUT(" ");
    }
  env = create_env(display_val, ttyname); 
  prctl(PR_SET_PDEATHSIG, SIGHUP);
  pid = fork();
  if (pid == 0)
    {
    signal(SIGPIPE, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    prctl(PR_SET_CHILD_SUBREAPER);
    prctl(PR_SET_PDEATHSIG, SIGHUP);
    if (setsid() < 0)
      KOUT("setsid: %s", strerror(errno));

    if (action == action_dae)
      {
      for (i=0; i<MAX_FD_NUM; i++)
        close(i);
      }
    else
      {
      for (i=0; i<MAX_FD_NUM; i++)
        {
        if (i != ttyfd)
          close(i);
        }
      wrap_pty_make_controlling_tty(ttyfd, ttyname);

      if (dup2(ttyfd, 0) < 0)
        KERR("dup2 stdin: %s", strerror(errno));
      if (dup2(ttyfd, 1) < 0)
        KERR("dup2 stdout: %s", strerror(errno));
      if (dup2(ttyfd, 2) < 0)
        KERR("dup2 stderr: %s", strerror(errno));
      close(ttyfd);
      wrap_nonnonblock(0);
      wrap_nonnonblock(1);
      wrap_nonnonblock(2);
      }
    if (action == action_dae)
      {
      create_argv_from_cmd(cmd, argv);
      }
    else if (action == action_bash)
      {
      argv[0] = "/bin/bash";
      argv[1] = NULL;
      }
    else
      {
      argv[0] = "/bin/bash";
      argv[1] = "-c";
      argv[2] = cmd;
      argv[3] = NULL;
      }
    execve(argv[0], argv, env);
    for (i=0; (argv[i] != NULL) && (i < MAX_ARGC); i++)
      KERR("ARG %d : %s", i, argv[i]);
    KOUT("%s", strerror(errno));
    }
  else if (pid < 0)
    KOUT("%s", strerror(errno));
  else
    {
    pty_cli_alloc(pty_fd, pid, sock_fd, randid);
    if (action != action_dae)
      {
      wrap_close(ttyfd, __FUNCTION__);
      mdl_open(pty_fd, fd_type_pty, wrap_write_pty, wrap_read_kout);
      wrap_nonblock(pty_fd);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cli_pty_fd_action(t_pty_cli *cli)
{
  t_msg *msg;
  int len = MAX_MSG_LEN+g_msg_header_len, result = -1;
  msg = (t_msg *) wrap_malloc(len);
  len = wrap_read_pty(cli->pty_fd, msg->buf, MAX_MSG_LEN);
  if (len > 0)
    {
    mdl_set_header_vals(msg, cli->randid, msg_type_data_cli_pty,
                        fd_type_pty, 0, 0);
    msg->len = len;
    result = mdl_queue_write_msg(cli->sock_fd, msg);
    }
  if (result)
    {
    wrap_close(cli->pty_fd, __FUNCTION__);
    mdl_close(cli->pty_fd);
    cli->pty_fd = -1;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void child_death_detection(void)
{
  int status, pid;
  char exstat;
  t_pty_cli *cur = g_pty_cli_head;
  if ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
    while(cur)
      {
      if (cur->pid == pid)
        {
        if (WIFEXITED(status))
          exstat = WEXITSTATUS(status);
        else
          exstat = 1;
        send_msg_type_end(cur->randid, cur->sock_fd, exstat);
        break;
        } 
      cur = cur->next;
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
    KOUT("%d %s", len, buf);
  else
    {
    child_death_detection();
    }
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
  return xauth_add_magic_cookie(display_val, magic_cookie);
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
    cur = cur->next;
    }
  FD_SET(g_sig_read_fd, readfds);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pty_fork_fdisset(fd_set *readfds, fd_set *writefds)
{
 t_pty_cli *cur = g_pty_cli_head;
  while(cur)
    {
    if (cur->pty_fd != -1)
      {
      if (FD_ISSET(cur->pty_fd, readfds))
        {
        cli_pty_fd_action(cur);
        }
      if (FD_ISSET(cur->pty_fd, writefds))
        {
        if (low_write_fd(cur->pty_fd))
          KERR(" ");
        }
      }
    cur = cur->next;
    }
  if (FD_ISSET(g_sig_read_fd, readfds))
    sig_evt_action(g_sig_read_fd);
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
void pty_fork_bin_bash(int action, uint32_t randid, int sock_fd,
                         char *cmd, int display_val)
{
 helper_wrap_forkpty(action, randid, sock_fd, cmd, display_val);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pty_fork_msg_type_win_size(int sock_fd, int len, char *buf)
{
  char win_size_buf[16];
  t_pty_cli *cli = find_with_sock_fd(sock_fd);
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
  t_pty_cli *cli = find_with_sock_fd(sock_fd);
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
int pty_fork_free_with_sock_fd(int sock_fd)
{
  int result = -1;
  t_pty_cli *cli = find_with_sock_fd(sock_fd);
  if (cli)
    {
    result = 0;
    pty_cli_free(cli);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pty_fork_init(void)
{
  int pipe_fd[2];

  if (signal(SIGCHLD, child_exit) == SIG_ERR)
    KERR("%s", strerror(errno));
  init_all_env();
  if (wrap_pipe(pipe_fd, fd_type_pipe_sig, __FUNCTION__) < 0)
    KOUT(" ");
  g_sig_write_fd = pipe_fd[1];
  g_sig_read_fd = pipe_fd[0];
}
/*--------------------------------------------------------------------------*/
