/****************************************************************************/
/* Copy-pasted-modified for cloonix                License GPL-3.0+         */
/*--------------------------------------------------------------------------*/
/* Original code from:                                                      */
/*                            Dropbear SSH                                  */
/*                            Matt Johnston                                 */
/****************************************************************************/
#include "includes.h"
#include "packet.h"
#include "buffer.h"
#include "session.h"
#include "dbutil.h"
#include "channel.h"
#include "chansession.h"
#include "sshpty.h"
#include "termcodes.h"
#include "ssh.h"
#include "runopts.h"

#include "io_clownix.h"
#include "cloonix_timer.h"

/*---------------------------------------------------------------------------*/
void cloonix_serv_xauth_cookie_key(char *tree, char *display, char *cookie_key);
int call_child_death_detection(void);
/*---------------------------------------------------------------------------*/
static int sessioncommand(struct Channel *channel, 
                          struct ChanSess *chansess, int iscmd);
static int sessionpty(struct ChanSess * chansess);
static int sessionsignal(struct ChanSess *chansess);
static int noptycommand(struct Channel *channel, struct ChanSess *chansess);
static int ptycommand(struct Channel *channel, struct ChanSess *chansess);
static int sessionwinchange(struct ChanSess *chansess);
static void execchild(struct ChanSess *chansess);
static void addchildpid(struct ChanSess *chansess, pid_t pid);
static void closechansess(struct Channel *channel);
static int newchansess(struct Channel *channel);
static void chansessionrequest(struct Channel *channel);

static void send_exitsignalstatus(struct Channel *channel);
static void send_msg_chansess_exitstatus(struct Channel * channel,
                struct ChanSess * chansess);
static void send_msg_chansess_exitsignal(struct Channel * channel,
                struct ChanSess * chansess);
static void get_termmodes(struct ChanSess *chansess);

const struct ChanType svrchansess = {
        "session", /* name */
        newchansess, /* inithandler */
        chansessionrequest, /* reqhandler */
        closechansess, /* closehandler */
};

/* required to clear environment */
extern char** environ;


/*****************************************************************************/
int call_child_death_detection(void)
{
  int i, status, result = 0;
  pid_t pid;
  struct exxitinfo *nexxit = NULL;
  if ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
    nexxit = NULL;
    for (i = 0; i < svr_ses.childpidsize; i++)
      {
      if (svr_ses.childpids[i].pid == pid)
        {
        nexxit = &svr_ses.childpids[i].chansess->exxit;
        break;
        }
      }
    if (nexxit == NULL)
      KOUT(" ");
    nexxit->exxitpid = pid;
    result = (int) pid;
    if (WIFEXITED(status))
      {
      nexxit->exxitstatus = WEXITSTATUS(status);
      }
    if (WIFSIGNALED(status))
      {
      nexxit->exxitsignal = WTERMSIG(status);
      nexxit->exxitcore = 0;
      }
    else
      {
      nexxit->exxitsignal = -1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int spawn_command( struct ChanSess *chansess,
                          int *ret_writefd, int *ret_readfd, 
                          int *ret_errfd, pid_t *ret_pid) 
{
  int infds[2];
  int outfds[2];
  int errfds[2];
  pid_t pid;
  const int FDIN = 0;
  const int FDOUT = 1;
  int result = DROPBEAR_FAILURE;
  prctl(PR_SET_PDEATHSIG, SIGKILL);
  if (pipe(infds) != 0) 
    KERR(" ");
  else if (pipe(outfds) != 0)
    KERR(" ");
  else if (pipe(errfds) != 0)
    KERR(" ");
  else
    {
    pid = fork();
    if (pid < 0)
      KERR(" ");
    else
      {
      if (!pid) 
        {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        if ((dup2(infds[FDIN], STDIN_FILENO) < 0) ||
            (dup2(outfds[FDOUT], STDOUT_FILENO) < 0) ||
            (dup2(errfds[FDOUT], STDERR_FILENO) < 0))
          KOUT(" ");
        close(infds[FDOUT]);
        close(infds[FDIN]);
        close(outfds[FDIN]);
        close(outfds[FDOUT]);
        close(errfds[FDIN]);
        close(errfds[FDOUT]);
        execchild(chansess);
        }
      else
        {
        close(infds[FDIN]);
        close(outfds[FDOUT]);
        close(errfds[FDOUT]);
        setnonblocking(outfds[FDIN]);
        setnonblocking(infds[FDOUT]);
        setnonblocking(errfds[FDIN]);
        if (ret_pid)
          *ret_pid = pid;
        *ret_writefd = infds[FDOUT];
        *ret_readfd  = outfds[FDIN];
        *ret_errfd   = errfds[FDIN];
        result = DROPBEAR_SUCCESS;
        }
    }
  }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void run_shell_command(char *cmd, unsigned int maxfd, 
                              char *usershell, char *login) 
{
  char *argv[7];
  char *baseshell = NULL;
  unsigned int i;
  int len;
  int is_login=0;

  baseshell = basename(usershell);
  if (cmd != NULL) 
    {
    argv[0] = baseshell;
    argv[1] = "-c";
    argv[2] = (char *) cmd;
    argv[3] = NULL;
    } 
  else 
    {
    if (login)
      {
      is_login = 1;
      argv[0] = login;
      argv[1] = "-p";
      argv[2] = "-f";
      argv[3] = "root";
      argv[4] = NULL;
      }
    else
      {
      len = strlen(baseshell) + 2;
      argv[0] = (char*)m_malloc(len);
      snprintf(argv[0], len, "-%s", baseshell);
      argv[1] = NULL;
      }
    }
  if (signal(SIGPIPE, SIG_DFL) == SIG_ERR)
    KOUT("signal() error");
  for (i = 3; i <= maxfd; i++)
    {
    m_close(i);
    }
  if (is_login)
    execv(login, argv);
  else
    execv(usershell, argv);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_exitsignalstatus(struct Channel *channel)
{
  struct ChanSess *chansess = (struct ChanSess*)channel->typedata;
  if (chansess->exxit.exxitpid >= 0) 
    {
    if (chansess->exxit.exxitsignal > 0) 
      send_msg_chansess_exitsignal(channel, chansess);
    else 
      send_msg_chansess_exitstatus(channel, chansess);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_msg_chansess_exitstatus(struct Channel * channel,
                                         struct ChanSess * chansess)
{
  if (chansess->exxit.exxitpid == -1)
    KOUT(" ");
  if(chansess->exxit.exxitsignal != -1)
    KOUT(" ");
  buf_putbyte(ses.writepayload, SSH_MSG_CHANNEL_REQUEST);
  buf_putint(ses.writepayload, channel->remotechan);
  buf_putstring(ses.writepayload, "exit-status", 11);
  buf_putbyte(ses.writepayload, 0); /* boolean FALSE */
  buf_putint(ses.writepayload, chansess->exxit.exxitstatus);
  encrypt_packet();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_msg_chansess_exitsignal(struct Channel * channel,
                                         struct ChanSess * chansess)
{
  int i;
  char* signame = NULL;
  if (chansess->exxit.exxitpid == -1)
    KOUT(" ");
  if (chansess->exxit.exxitsignal <= 0)
    KOUT(" ");
  KERR("send_msg_chansess_exitsignal %d", chansess->exxit.exxitsignal);
  for (i = 0; signames[i].name != NULL; i++)
    {
    if (signames[i].signal == chansess->exxit.exxitsignal)
      {
      signame = signames[i].name;
      break;
      }
    }
  if (signame != NULL)
    {
    buf_putbyte(ses.writepayload, SSH_MSG_CHANNEL_REQUEST);
    buf_putint(ses.writepayload, channel->remotechan);
    buf_putstring(ses.writepayload, "exit-signal", 11);
    buf_putbyte(ses.writepayload, 0); /* boolean FALSE */
    buf_putstring(ses.writepayload, signame, strlen(signame));
    buf_putbyte(ses.writepayload, chansess->exxit.exxitcore);
    buf_putstring(ses.writepayload, "", 0); /* error msg */
    buf_putstring(ses.writepayload, "", 0); /* lang */
    encrypt_packet();
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int newchansess(struct Channel *channel)
{
  struct ChanSess *chansess;
  if (channel->typedata)
    KOUT("%p", channel->typedata);
  chansess = (struct ChanSess*)m_malloc(sizeof(struct ChanSess));
  chansess->cmd = NULL;
  chansess->pid = 0;
  chansess->master = -1;
  chansess->slave = -1;
  chansess->tty = NULL;
  chansess->exxit.exxitpid = -1;
  chansess->i_run_in_kvm = channel->i_run_in_kvm;
  if (channel->cloonix_tree_dir[0])
    {
    strncpy(chansess->cloonix_tree_dir, 
            channel->cloonix_tree_dir, 
            MAX_DROPBEAR_PATH_LEN-1);
    }

  channel->typedata = chansess;
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void closechansess(struct Channel *channel)
{
  struct ChanSess *chansess;
  unsigned int i;
  chansess = (struct ChanSess*)channel->typedata;
  if (chansess != NULL) 
    {
    send_exitsignalstatus(channel);
    m_free(chansess->cmd);
    if (chansess->tty) 
      {
      pty_release(chansess->tty);
      m_free(chansess->tty);
      }
    for (i = 0; i < svr_ses.childpidsize; i++) 
      {
      if (svr_ses.childpids[i].chansess == chansess) 
        {
        if (svr_ses.childpids[i].pid <= 0)
          KOUT(" ");
        svr_ses.childpids[i].pid = -1;
        svr_ses.childpids[i].chansess = NULL;
        }
      }
    memset(chansess, 0, sizeof(struct ChanSess));
    m_free(chansess);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void chansessionrequest(struct Channel *channel) 
{
  char *type = NULL;
  unsigned int typelen;
  unsigned char wantreply;
  int ret = 1;
  struct ChanSess *chansess;
  type = buf_getstring(ses.payload, &typelen);
  wantreply = buf_getbool(ses.payload);
  if (typelen > MAX_NAME_LEN) 
    KERR("leave chansessionrequest: type too long");
  else
    {
    chansess = (struct ChanSess*)channel->typedata;
    if (chansess == NULL)
      KOUT(" ");
    if (strcmp(type, "window-change") == 0)
      ret = sessionwinchange(chansess);
    else if (strcmp(type, "shell") == 0)
      ret = sessioncommand(channel, chansess, 0);
    else if (strcmp(type, "pty-req") == 0) 
      ret = sessionpty(chansess);
    else if (strcmp(type, "exec") == 0)
      ret = sessioncommand(channel, chansess, 1);
    else if (strcmp(type, "signal") == 0)
      {
      KERR(" ");
      ret = sessionsignal(chansess);
      }
    else
      KERR("%s", type);
    }
  if (wantreply)
    {
    if (ret == DROPBEAR_SUCCESS)
      send_msg_channel_success(channel);
    else
      send_msg_channel_failure(channel);
    }
  m_free(type);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int sessionsignal(struct ChanSess *chansess)
{
  int sig = 0;
  char *signame = NULL;
  int i = 0, result = DROPBEAR_FAILURE;
  if (chansess->pid == 0)
    KERR(" ");
  else
    {
    signame = buf_getstring(ses.payload, NULL);
    while (signames[i].name != 0)
      {
      if (strcmp(signames[i].name, signame) == 0)
        {
        sig = signames[i].signal;
        break;
        }
      i++;
      }
    m_free(signame);
    if (sig == 0)
      KERR(" ");
    else
      {
      if (kill(chansess->pid, sig) < 0)
        KERR(" ");
      else
        {
        KERR("%d ",  sig);
        result = DROPBEAR_SUCCESS;
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int sessionwinchange(struct ChanSess *chansess)
{
  int termc, termr, termw, termh, result = DROPBEAR_FAILURE;
  if (chansess->master < 0)
    KERR(" ");
  else
    {			
    termc = buf_getint(ses.payload);
    termr = buf_getint(ses.payload);
    termw = buf_getint(ses.payload);
    termh = buf_getint(ses.payload);
    pty_change_window_size(chansess->master, termr, termc, termw, termh);
    result = DROPBEAR_SUCCESS;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void get_termmodes(struct ChanSess *chansess)
{
  struct termios termio;
  unsigned char opcode;
  unsigned int value;
  const struct TermCode * termcode;
  unsigned int len;
  len = buf_getint(ses.payload);
  if (tcgetattr(chansess->master, &termio) == -1)
    {
    if (len != ses.payload->len - ses.payload->pos) 
      KERR("%d %d %d", len, ses.payload->len, ses.payload->pos);
    else if (len == 0)
      KERR(" ");
    else
      {
      while (((opcode = buf_getbyte(ses.payload)) != 0x00) && opcode <= 159)
        {
        value = buf_getint(ses.payload);
        if (opcode > MAX_TERMCODE)
          continue;
        termcode = &termcodes[(unsigned int)opcode];
        switch (termcode->type)
          {
          case TERMCODE_NONE:
          break;
          case TERMCODE_CONTROLCHAR:
            termio.c_cc[termcode->mapcode] = value;
          break;
          case TERMCODE_INPUT:
            if (value)
              termio.c_iflag |= termcode->mapcode;
            else
              termio.c_iflag &= ~(termcode->mapcode);
          break;
          case TERMCODE_OUTPUT:
            if (value)
              termio.c_oflag |= termcode->mapcode;
            else
              termio.c_oflag &= ~(termcode->mapcode);
          break;
          case TERMCODE_LOCAL:
            if (value)
              termio.c_lflag |= termcode->mapcode;
            else
              termio.c_lflag &= ~(termcode->mapcode);
          break;
          case TERMCODE_CONTROL:
            if (value)
              termio.c_cflag |= termcode->mapcode;
            else
              termio.c_cflag &= ~(termcode->mapcode);
            break;
          default:
            KERR(" ");
          break;
          }
        }
      if (tcsetattr(chansess->master, TCSANOW, &termio) < 0)
        KERR(" ");
      }
    }
}
/*---------------------------------------------------------------------------*/

        
/*****************************************************************************/
static int sessionpty(struct ChanSess * chansess)
{
  unsigned int len, result = DROPBEAR_FAILURE;;
  char namebuf[65];
  chansess->cloonix_name = buf_getstring(ses.payload, &len);
  chansess->cloonix_display = buf_getstring(ses.payload, &len);
  chansess->cloonix_xauth_cookie_key = buf_getstring(ses.payload, &len);
  if (chansess->master != -1)
    KOUT("Multiple pty requests");
  if (pty_allocate(&chansess->master, &chansess->slave, namebuf, 64) == 0)
    KERR("leave sessionpty: failed to allocate pty");
  else
    {
    chansess->tty = m_strdup(namebuf);
    if (!chansess->tty)
      KOUT(" ");
    sessionwinchange(chansess);
    get_termmodes(chansess);
    result = DROPBEAR_SUCCESS;
    }
return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int sessioncommand(struct Channel *channel, 
                          struct ChanSess *chansess,
                          int iscmd)
{
  unsigned int cmdlen;
  int result = DROPBEAR_FAILURE;
  if (chansess->cmd != NULL)
    KERR(" ");
  else
    {
    if (iscmd)
      {
      chansess->cmd = buf_getstring(ses.payload, &cmdlen);
      if (cmdlen > MAX_CMD_LEN)
        {
        m_free(chansess->cmd);
        KERR("%d %d", cmdlen, MAX_CMD_LEN);
        return DROPBEAR_FAILURE;
        }
      }
    if (signal(SIGCHLD, SIG_DFL) == SIG_ERR)
      KOUT("signal() error");
    if (chansess->tty == NULL)
      result = noptycommand(channel, chansess);
    else
      result = ptycommand(channel, chansess);
    m_free(chansess->cmd);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int noptycommand(struct Channel *channel, struct ChanSess *chansess)
{
  int result;
  result = spawn_command(chansess, &channel->writefd, &channel->readfd, 
                         &channel->errfd, &chansess->pid);
  if (result == DROPBEAR_SUCCESS)
    {
    ses.maxfd = MAX(ses.maxfd, channel->writefd);
    ses.maxfd = MAX(ses.maxfd, channel->readfd);
    ses.maxfd = MAX(ses.maxfd, channel->errfd);
    addchildpid(chansess, chansess->pid);
    }
return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int ptycommand(struct Channel *channel, struct ChanSess *chansess) 
{
  pid_t pid;
  int result = DROPBEAR_FAILURE;
  if (chansess->master == -1 || chansess->tty == NULL) 
    {
    KERR("No pty was allocated, couldn't execute %d %p", 
         chansess->master, chansess->tty);
    }
  else
    {
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    pid = fork();
    if (pid < 0)
      KERR("Bad fork");
    else if (pid == 0)
      {
      prctl(PR_SET_PDEATHSIG, SIGKILL);
      close(chansess->master);
      pty_make_controlling_tty(&chansess->slave, chansess->tty);
      if ((chansess->cloonix_xauth_cookie_key) && (chansess->cloonix_display))
        {
        if (strcmp(chansess->cloonix_xauth_cookie_key, "NO_X11_FORWARDING_COOKIE"))
          {
          cloonix_serv_xauth_cookie_key(chansess->cloonix_tree_dir,
                                        chansess->cloonix_display,
                                        chansess->cloonix_xauth_cookie_key);
          }
        }

      if ((dup2(chansess->slave, STDIN_FILENO) < 0) ||
          (dup2(chansess->slave, STDERR_FILENO) < 0) ||
          (dup2(chansess->slave, STDOUT_FILENO) < 0)) 
        KERR("leave ptycommand: error redirecting filedesc");
      else
        {
        close(chansess->slave);
        execchild(chansess);
        result = DROPBEAR_SUCCESS;
        }
      } 
    else 
      {
      chansess->pid = pid;
      addchildpid(chansess, pid);
      channel->writefd = chansess->master;
      channel->readfd = chansess->master;
      ses.maxfd = MAX(ses.maxfd, chansess->master);
      setnonblocking(chansess->master);
      result = DROPBEAR_SUCCESS;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void addchildpid(struct ChanSess *chansess, pid_t pid)
{
  int i;
  for (i = 0; i < svr_ses.childpidsize; i++)
    {
    if (svr_ses.childpids[i].pid == -1)
      break;
    }
  if (i == svr_ses.childpidsize)
    {
    svr_ses.childpids = (struct ChildPid*)m_realloc(svr_ses.childpids,
                         sizeof(struct ChildPid) * (svr_ses.childpidsize+1));
    svr_ses.childpidsize++;
    }
  svr_ses.childpids[i].pid = pid;
  svr_ses.childpids[i].chansess = chansess;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void execchild(struct ChanSess *chansess)
{
  char *usershell = NULL;
  char *login = NULL;
  char *pth="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin";
  unsetnonblocking(STDOUT_FILENO);
  unsetnonblocking(STDIN_FILENO);
  unsetnonblocking(STDERR_FILENO);
  if (chansess->i_run_in_kvm)
    {
    clearenv();
    addnewvar("PATH", pth); 
    addnewvar("USER", "root");
    addnewvar("HOME", "/root");
    addnewvar("TERM", "xterm");
    addnewvar("XAUTHORITY", "/root/.Xauthority");
    if (chansess->cloonix_name)
      addnewvar("PROMPT_COMMAND", chansess->cloonix_name);
    if (chansess->cloonix_display)
      addnewvar("DISPLAY", chansess->cloonix_display);
    if (chdir("/root") < 0)
      KOUT("Error changing directory");
    if (!access("/bin/login", X_OK))
      login = m_strdup("/bin/login");
    }
  else
    {
    unsetenv("PATH");
    addnewvar("PATH", pth); 
    addnewvar("TERM", "xterm");
    if (chansess->cloonix_display)
      addnewvar("DISPLAY", chansess->cloonix_display);
    }
  if (chansess->tty)
    addnewvar("SSH_TTY", (char *) chansess->tty);

  if (!access("/bin/bash", X_OK))
    {
    addnewvar("SHELL", "/bin/bash");
    usershell = m_strdup("/bin/bash");
    }
  else if (!access("/bin/ash", X_OK))
    {
    addnewvar("SHELL", "/bin/ash");
    usershell = m_strdup("/bin/ash");
    }
  else
    {
    addnewvar("SHELL", "/bin/sh");
    usershell = m_strdup("/bin/sh");
    }
  run_shell_command(chansess->cmd, ses.maxfd, usershell, login);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void svr_chansessinitialise()
{
  svr_ses.childpids = (struct ChildPid*)m_malloc(sizeof(struct ChildPid));
  svr_ses.childpids[0].pid = -1;
  svr_ses.childpids[0].chansess = NULL;
  svr_ses.childpidsize = 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void addnewvar(char* param, char* var)
{
  char* newvar = NULL;
  int plen, vlen;
  plen = strlen(param);
  vlen = strlen(var);
  newvar = m_malloc(plen + vlen + 2);
  memcpy(newvar, param, plen);
  newvar[plen] = '=';
  memcpy(&newvar[plen+1], var, vlen);
  newvar[plen+vlen+1] = '\0';
  if (putenv(newvar) < 0)
    KOUT("environ error");
}
/*---------------------------------------------------------------------------*/
