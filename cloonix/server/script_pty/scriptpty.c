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
/***************************************************************************/
/* Copied From: util-linux-2.38.1                                          */
/***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <paths.h>
#include <time.h>
#include <sys/stat.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stddef.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/signalfd.h>
#include <assert.h>
#include <inttypes.h>
#include <syslog.h>
#include <pty.h>

#define UL_BUILD_BUG_ON_ZERO(e) __extension__ (sizeof(struct { int:-!!(e); }))

#define __must_be_array(a) \
        UL_BUILD_BUG_ON_ZERO(__builtin_types_compatible_p(__typeof__(a), __typeof__(&a[0])))


#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))
#define CTL(x)		((x) ^ 0100)
#define DEF_EOF		CTL('D')

/*-------------------------------------------------------------------------*/
enum {
     POLLFD_SIGNAL = 0,
     POLLFD_MASTER,
     POLLFD_STDIN
};
/*-------------------------------------------------------------------------*/
#define KOUT(format, a...)                                    \
 do {                                                         \
    fprintf(stderr, "KOUT line:%lu " format "\n\n",            \
    __LINE__, ## a);                                          \
    syslog(LOG_ERR | LOG_USER, "KOUT line:%lu " format "\n\n", \
    __LINE__, ## a); exit(-1);                                \
    } while (0)
#define KERR(format, a...)                                    \
 do {                                                         \
    fprintf(stderr, "line:%lu " format "\n\n",            \
    __LINE__, ## a);                                          \
    syslog(LOG_ERR | LOG_USER, "KERR line:%lu " format "\n\n", \
    __LINE__, ## a);                                \
    } while (0)
/*-------------------------------------------------------------------------*/
struct ul_pty_callbacks {
  void (*child_die)(void *, pid_t, int);
  void (*child_sigstop)(void *, pid_t);
};
/*-------------------------------------------------------------------------*/
struct ul_pty_child_buffer {
  struct ul_pty_child_buffer *next;
  char buf[BUFSIZ];
  size_t size, cursor;
  int final_input;
};
/*-------------------------------------------------------------------------*/
struct ul_pty {
  struct termios  stdin_attrs;
  int             master;
  int             slave;
  int             sigfd;
  int             poll_timeout;
  struct winsize  win;
  sigset_t        orgsig;
  int             delivered_signal;
  struct ul_pty_callbacks callbacks;
  void                    *callback_data;
  int           child;
  struct timeval  next_callback_time;
  struct ul_pty_child_buffer *child_buffer_head;
  struct ul_pty_child_buffer *child_buffer_tail;
  struct ul_pty_child_buffer *free_buffers;
  int slave_echo;
};
/*-------------------------------------------------------------------------*/
struct script_control {
  uint64_t outsz;
  uint64_t maxsz;
  const char *ttyname;
  const char *ttytype;
  int ttycols;
  int ttylines;
  struct ul_pty *pty;
  int child;
  int childstatus;
  int append;
  int rc_wanted;
  int flush;
  int quiet;
  int force;
};
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static void write_all(int fd, const void *buf, size_t count)
{
  ssize_t tmp;
  while (count)
    {
    errno = 0;
    tmp = write(fd, buf, count);
    if (tmp > 0)
      {
      count -= tmp;
      if (count)
        buf = (const void *) ((const char *) buf + tmp);
      }
    else if ((errno == EINTR) || (errno == EAGAIN))
      {
      usleep(100000);
      }
    else
      KOUT("ERROR %d", errno);
    }
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static void schedule_child_write(struct ul_pty *pty,
                                 char *buf, size_t bufsz, int final)
{                       
  struct ul_pty_child_buffer *stash;
  if (pty->free_buffers)
    {
    stash = pty->free_buffers;
    pty->free_buffers = stash->next;
    memset(stash, 0, sizeof(*stash));
    }
  else
    stash = calloc(1, sizeof(*stash));
  if (!stash)
    KOUT("ERROR");
  if (bufsz > sizeof(stash->buf))
    KOUT("ERROR %d %d", bufsz, (int) sizeof(stash->buf));
  memcpy(stash->buf, buf, bufsz);
  stash->size = bufsz;
  stash->final_input = final;
  if (pty->child_buffer_head)
    pty->child_buffer_tail = pty->child_buffer_tail->next = stash;
  else
    pty->child_buffer_head = pty->child_buffer_tail = stash;
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static int handle_io(struct ul_pty *pty, int fd, int *eof)
{
  char buf[BUFSIZ];
  ssize_t bytes;
  int result = 0;
  sigset_t set;
  *eof = 0;
  sigemptyset(&set);
  sigaddset(&set, SIGTTIN);
  sigprocmask(SIG_UNBLOCK, &set, NULL);
  bytes = read(fd, buf, sizeof(buf));
  sigprocmask(SIG_BLOCK, &set, NULL);
  if (bytes == -1)
    {
    if ((errno != EAGAIN) && (errno != EINTR))
      result = -errno;
    }
  else if (bytes == 0)
    {
    *eof = 1;
    }
  else if (fd == STDIN_FILENO)
    {
    schedule_child_write(pty, buf, bytes, 0);
    }
  else if (fd == pty->master)
    {
    write_all(STDOUT_FILENO, buf, bytes);
    }
  else
    {
    KERR("WARNING: %d", fd);
    result = -1;
    }
  return result;
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static void drain_child_buffers(struct ul_pty *pty)
{
  unsigned int tries = 0;
  struct pollfd fd = { .fd = pty->slave, .events = POLLIN };
  while (poll(&fd, 1, 10) == 1 && tries < 8)
    {
    usleep(250000);
    tries++;
    }
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static int flush_child_buffers(struct ul_pty *pty, int *anything)
{
  int result = 0, any = 0;
  struct ul_pty_child_buffer *hd = pty->child_buffer_head;
  ssize_t ret;
  while (pty->child_buffer_head)
    {
    if (hd->final_input)
      drain_child_buffers(pty);
    ret = write(pty->master, hd->buf + hd->cursor, hd->size - hd->cursor);
    if (ret == -1)
      {
      if ((errno != EINTR) && (errno != EAGAIN))
        {
        result = -errno;
        break;
        }
      usleep(100000);
      }
    any = 1;
    hd->cursor += ret;
    if (hd->cursor == hd->size)
      {
      pty->child_buffer_head = hd->next;
      if (!hd->next)
        pty->child_buffer_tail = NULL;
      hd->next = pty->free_buffers;
      pty->free_buffers = hd;
      }
    }
  if (any)
    fdatasync(pty->master);
  if (anything)
    *anything = any;
  return result;
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
void ul_pty_wait_for_child(struct ul_pty *pty)
{
  int status, pid, options = 0;
  if (pty->child >= 0)
    {
    if (pty->sigfd >= 0)
      {
      options = WNOHANG;
      for (;;)
        {
        pid = waitpid(pty->child, &status, options);
        if (pid == -1)
          break;
        if (pty->callbacks.child_die)
          pty->callbacks.child_die( pty->callback_data, pty->child, status);
        pty->child = -1;
        }
      }
    else
      {
      while ((pid = waitpid(-1, &status, options)) > 0)
        {
        if (pid == pty->child)
          {
          if (pty->callbacks.child_die)
            pty->callbacks.child_die( pty->callback_data, pty->child, status);
          pty->child = -1;
          }
        }
      }
    }
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static void handle_signal(struct ul_pty *pty, int fd)
{
  struct signalfd_siginfo info;
  ssize_t bytes;
  bytes = read(fd, &info, sizeof(info));
  if (bytes != sizeof(info))
    {
    if ((bytes < 0) && (errno != EAGAIN) && (errno != EINTR))
      KERR("WARNING %d", errno);
    }
  else
    {
    switch (info.ssi_signo)
      {
      case SIGCHLD:
        if (info.ssi_code == CLD_EXITED ||
            info.ssi_code == CLD_KILLED ||
            info.ssi_code == CLD_DUMPED)
          {
          ul_pty_wait_for_child(pty);
          }
        else if ((info.ssi_status == SIGSTOP) && (pty->child > 0))
          {
          pty->callbacks.child_sigstop(pty->callback_data, pty->child);
          }
        if (pty->child <= 0)
          {
          pty->poll_timeout = 10;
          timerclear(&pty->next_callback_time);
          }
        break;
      case SIGWINCH:
//        ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&pty->win);
//        ioctl(pty->slave, TIOCSWINSZ, (char *)&pty->win);
        break;
      case SIGTERM:
      case SIGINT:
      case SIGQUIT:
        pty->delivered_signal = info.ssi_signo;
        if (pty->child > 0)
          kill(pty->child, SIGTERM);
        break;
      case SIGUSR1:
            break;
      default:
        KOUT("ERROR %d", info.ssi_signo);
      }
    }
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static void pty_signals_cleanup(struct ul_pty *pty)
{
  if (pty->sigfd != -1)
    close(pty->sigfd);
  pty->sigfd = -1;
  sigprocmask(SIG_SETMASK, &pty->orgsig, NULL);
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
void ul_pty_write_eof_to_child(struct ul_pty *pty)
{
  char c = DEF_EOF;
  schedule_child_write(pty, &c, sizeof(char), 1);
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static void callback_child_sigstop(
                        void *data __attribute__((__unused__)),
                        pid_t child)
{
  kill(getpid(), SIGSTOP);
  kill(child, SIGCONT);
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
static void callback_child_die(void *data,
                               pid_t child __attribute__((__unused__)),
                               int status)
{
  struct script_control *ctl = (struct script_control *) data;
  ctl->child = (pid_t) -1;
  ctl->childstatus = status;
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
void ul_pty_setup(struct ul_pty *pty)
{
  struct termios attrs;
  sigset_t ourset;
  memset(&attrs, 0, sizeof(struct termios));
  assert(pty->sigfd == -1);
  sigprocmask(0, NULL, &pty->orgsig);
  if (tcgetattr(STDIN_FILENO, &pty->stdin_attrs) != 0)
    KERR("ERROR tcgetattr");
  attrs = pty->stdin_attrs;
  if (pty->slave_echo)
    attrs.c_lflag |= ECHO;
  else
    attrs.c_lflag &= ~ECHO;
  ioctl(STDIN_FILENO, TIOCGWINSZ, (char *)&pty->win);
  if (openpty(&pty->master, &pty->slave, NULL, &attrs, &pty->win))
    KOUT("ERROR");
  cfmakeraw(&attrs);
  tcsetattr(STDIN_FILENO, TCSANOW, &attrs);
  fcntl(pty->master, F_SETFL, O_NONBLOCK);
  sigfillset(&ourset);
  if (sigprocmask(SIG_BLOCK, &ourset, NULL))
    KOUT("ERROR");
  sigemptyset(&ourset);
  sigaddset(&ourset, SIGCHLD);
  sigaddset(&ourset, SIGWINCH);
  sigaddset(&ourset, SIGALRM);
  sigaddset(&ourset, SIGTERM);
  sigaddset(&ourset, SIGINT);
  sigaddset(&ourset, SIGQUIT);
  if ((pty->sigfd = signalfd(-1, &ourset, SFD_CLOEXEC)) < 0)
    KOUT("ERROR");
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
void ul_pty_init_slave(struct ul_pty *pty)
{
  setsid();
  ioctl(pty->slave, TIOCSCTTY, 1);
  close(pty->master);
  dup2(pty->slave, STDIN_FILENO);
  dup2(pty->slave, STDOUT_FILENO);
  dup2(pty->slave, STDERR_FILENO);
  close(pty->slave);
  if (pty->sigfd >= 0)
    close(pty->sigfd);
  pty->slave = -1;
  pty->master = -1;
  pty->sigfd = -1;
  sigprocmask(SIG_SETMASK, &pty->orgsig, NULL);
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
int ul_pty_proxy_master(struct ul_pty *pty)
{
  int rc = 0, ret, eof = 0;
  size_t i; struct timeval now, rest;
  int errsv, timeout, anything = 1;
  struct pollfd pfd[] = {
  [POLLFD_SIGNAL] = {.fd=-1,           .events = POLLIN|POLLERR|POLLHUP},
  [POLLFD_MASTER] = {.fd=pty->master,  .events = POLLIN|POLLERR|POLLHUP},
  [POLLFD_STDIN]  = {.fd=STDIN_FILENO, .events = POLLIN|POLLERR|POLLHUP}};
  assert(pty->sigfd >= 0);
  pfd[POLLFD_SIGNAL].fd = pty->sigfd;
  pty->poll_timeout = -1;
  while (!pty->delivered_signal)
    {
    if (timerisset(&pty->next_callback_time))
      {
      gettimeofday(&now, NULL);
      timersub(&pty->next_callback_time, &now, &rest);
      timeout = (rest.tv_sec * 1000) +  (rest.tv_usec / 1000);
      }
    else
      timeout = pty->poll_timeout;
    if (pty->child_buffer_head)
      pfd[POLLFD_MASTER].events |= POLLOUT;
    else
      pfd[POLLFD_MASTER].events &= ~POLLOUT;
    ret = poll(pfd, ARRAY_SIZE(pfd), timeout);
    errsv = errno;
    if (ret < 0)
      {
      if (errsv == EAGAIN)
        continue;
      rc = -errno;
      break;
      }
               if (ret == 0) {
                        if (timerisset(&pty->next_callback_time)) {
                                        continue;
                        } else {
                                rc = 0;
                        }

                        break;
                }

    for (i = 0; i < ARRAY_SIZE(pfd); i++)
      {
      if (pfd[i].revents == 0)
        continue;
      if (i == POLLFD_SIGNAL)
        {
        handle_signal(pty, pfd[i].fd);
        }
      else
        {
        if (pfd[i].revents & POLLIN)
          rc = handle_io(pty, pfd[i].fd, &eof);
        if (pfd[i].revents & POLLOUT)
          rc = flush_child_buffers(pty, NULL);
        }
      if (rc)
        {
        ul_pty_write_eof_to_child(pty);
        for (anything = 1; anything;)
          {
          flush_child_buffers(pty, &anything);
          }
        break;
        }
      if (i == POLLFD_SIGNAL)
        continue;
      if ((pfd[i].revents & POLLHUP) || (pfd[i].revents & POLLNVAL) || eof)
        {
        if (i == POLLFD_STDIN)
          {
          pfd[i].fd = -1;
          ul_pty_write_eof_to_child(pty);
          }
        else
          pfd[i].revents &= ~POLLIN;
        }
      }
    if (rc)
      break;
    }
    if (rc && pty->child &&
       (pty->child != (pid_t) -1) &&
       (!pty->delivered_signal))
      {
      kill(pty->child, SIGTERM);
      sleep(2);
      kill(pty->child, SIGKILL);
      }
    pty_signals_cleanup(pty);
    return rc;
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
void ul_pty_cleanup(struct ul_pty *pty)
{
  struct termios rtt;
  pty_signals_cleanup(pty);
  if (pty->master != -1 )
    {
    rtt = pty->stdin_attrs;
    tcsetattr(STDIN_FILENO, TCSADRAIN, &rtt);
    }
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
void ul_free_pty(struct ul_pty *pty)
{
  struct ul_pty_child_buffer *hd;
  while ((hd = pty->child_buffer_head))
    {
    pty->child_buffer_head = hd->next;
    free(hd);
    }
  while ((hd = pty->free_buffers))
    {
    pty->free_buffers = hd->next;
    free(hd);
    }
  free(pty);
}
/*-------------------------------------------------------------------------*/

/***************************************************************************/
int main(int argc, char **argv)
{       
  struct script_control ctl;
  struct ul_pty_callbacks *cb;
  int ch, format = 0, caught_signal = 0, rc = 0, echo = 1;
  char local_path[PATH_MAX];
  char *current_dir_name = getcwd(local_path, PATH_MAX);
  char path_to_bash[PATH_MAX];

  snprintf(path_to_bash, PATH_MAX-1, "/usr/libexec/cloonix/cloonfs/bin/bash");
  if (access( path_to_bash, X_OK))
    {
    snprintf(path_to_bash, PATH_MAX-1, "%s/bin/bash", current_dir_name);
    if (access( path_to_bash, X_OK))
      KOUT("ERROR scriptpty.c failed to find /usr/libexec/cloonix/cloonfs/bin/bash or %s", path_to_bash);
    }
  if (argc != 2)
    KOUT("ERROR failed argc = %d", argc);
  ctl.pty = calloc(1, sizeof(*ctl.pty));
  ctl.pty->master = -1;
  ctl.pty->slave = -1;
  ctl.pty->sigfd = -1;
  ctl.pty->child = (pid_t) -1;
  ctl.pty->slave_echo = 1;
  ctl.pty->callback_data = (void *) &ctl;
  cb = &(ctl.pty->callbacks);
  cb->child_die = callback_child_die;
  cb->child_sigstop = callback_child_sigstop;
  ul_pty_setup(ctl.pty);
  fflush(stdout);
  ctl.child = fork();
  if (ctl.child < 0)
    KOUT("ERROR cannot create child process");
  if (ctl.child == 0)
    {
    ul_pty_init_slave(ctl.pty);
    signal(SIGTERM, SIG_DFL);
    execl(path_to_bash, "sh", "-c", argv[1], (char *)NULL);
    KOUT("ERROR failed to execute %s", "/bin/sh");
    }
  ctl.pty->child = ctl.child;
  rc = ul_pty_proxy_master(ctl.pty);
  caught_signal =  ctl.pty->delivered_signal;
  if (!caught_signal && ctl.child != -1)
    {
    ul_pty_wait_for_child(ctl.pty);
    }
  if (caught_signal && ctl.child != -1)
    {
    fprintf(stderr, "\nSession terminated, killing shell...");
    kill(ctl.child, SIGTERM);
    sleep(2);
    kill(ctl.child, SIGKILL);
    fprintf(stderr, " ...killed.\n");
    }
  ul_pty_cleanup(ctl.pty);
  ul_free_pty(ctl.pty);
}
/*-------------------------------------------------------------------------*/
