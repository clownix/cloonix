/****************************************************************************/
/* Copy-pasted-modified for cloon                License GPL-3.0+         */
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
#include "ssh.h"
#include "runopts.h"
#include "termcodes.h"
#include "chansession.h"
#include "io_clownix.h"


int call_child_death_detection(void);
char *get_cloonix_name_prompt(void);
char *get_cloonix_display(void);
static void cli_closechansess(struct Channel *channel);
static int cli_initchansess(struct Channel *channel);
static void cli_chansessreq(struct Channel *channel);
static void send_chansess_pty_req(struct Channel *channel);
static void send_chansess_shell_req(struct Channel *channel);

static void cli_tty_setup();

const struct ChanType clichansess = {
	"session", /* name */
	cli_initchansess, /* inithandler */
	cli_chansessreq, /* reqhandler */
	cli_closechansess, /* closehandler */
};


/*****************************************************************************/
int call_child_death_detection(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cli_chansessreq(struct Channel *channel)
{
  char* type = NULL;
  int wantreply;
  type = buf_getstring(ses.payload, NULL);
  wantreply = buf_getbool(ses.payload);
  if (cli_ses.retval != 42)
    KERR("%d", cli_ses.retval);
  if (strcmp(type, "exit-status") == 0)
    cli_ses.retval = buf_getint(ses.payload);
  else if (strcmp(type, "exit-signal") == 0)
    KERR(" ");
  else
    {
    if (wantreply)
      send_msg_channel_failure(channel);
    }
  m_free(type);
}
/*---------------------------------------------------------------------------*/
	
/*****************************************************************************/
static void cli_closechansess(struct Channel *channel) 
{
  (void) channel;
  if (channel->init_done)
    cli_tty_cleanup(); 
  wrapper_exit(cli_ses.retval, (char *)__FILE__, __LINE__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void cli_tty_setup(void) 
{
  struct termios tio;
  if (cli_ses.tty_raw_mode != 1)
    {
    if (tcgetattr(STDIN_FILENO, &tio) != -1)
      {
      cli_ses.saved_tio = tio;
      tio.c_iflag |= IGNPAR;
      tio.c_iflag &= ~(ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXANY | IXOFF);
      tio.c_iflag &= ~IUCLC;
      tio.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL);
      tio.c_lflag &= ~IEXTEN;
      tio.c_oflag &= ~OPOST;
      tio.c_cc[VMIN] = 1;
      tio.c_cc[VTIME] = 0;
      if (tcsetattr(STDIN_FILENO, TCSADRAIN, &tio) == -1)
        KOUT("Failed to set raw TTY mode");
      }
    cli_ses.tty_raw_mode = 1;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cli_tty_cleanup(void)
{
  if (cli_ses.tty_raw_mode != 0) 
    { 
    tcsetattr(STDIN_FILENO, TCSADRAIN, &cli_ses.saved_tio);
    cli_ses.tty_raw_mode = 0; 
    }
  unsetnonblocking(STDOUT_FILENO);
  unsetnonblocking(STDIN_FILENO);
  unsetnonblocking(STDERR_FILENO);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void put_winsize(void)
{
  struct winsize ws;
  if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) < 0)
    {
    ws.ws_row = 25;
    ws.ws_col = 80;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    }
  buf_putint(ses.writepayload, ws.ws_col); /* Cols */
  buf_putint(ses.writepayload, ws.ws_row); /* Rows */
  buf_putint(ses.writepayload, ws.ws_xpixel); /* Width */
  buf_putint(ses.writepayload, ws.ws_ypixel); /* Height */
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void sigwinch_handler(int UNUSED(unused))
{
  cli_ses.winchange = 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cli_chansess_winchange(void) 
{
  struct Channel *channel = &ses.channel;
  if (channel->init_done)
    {
    buf_putbyte(ses.writepayload, SSH_MSG_CHANNEL_REQUEST);
    buf_putint(ses.writepayload, channel->remotechan);
    buf_putstring(ses.writepayload, "window-change", 13);
    buf_putbyte(ses.writepayload, 0); 
    put_winsize();
    encrypt_packet();
    cli_ses.winchange = 0;
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static char *local_read_whole_file(char *file_name, int *len)
{ 
  char *buf = NULL;
  int fd, readlen;
  struct stat statbuf;
  if (stat(file_name, &statbuf) != 0)
    KERR("Cannot get size of file %s\n", file_name);
  else if (statbuf.st_size)
    {
    *len = statbuf.st_size;
    fd = open(file_name, O_RDONLY);
    if (fd == -1)
      KERR("Cannot open file %s errno %d\n", file_name, errno);
    else
      {
      buf = (char *) malloc((*len + 1) * sizeof(char));
      memset(buf, 0, (*len + 1) * sizeof(char));
      readlen = read(fd, buf, *len);
      if (readlen != *len)
        {
        KERR("Length of file error for file %s %d\n", file_name, errno);
        free(buf);
        buf = NULL;
        }
      close (fd);
      }
    }
  return buf;
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static int my_rand(int max)
{
  unsigned int result = rand();
  result %= max;
  return (int) result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_magic_cookie_with_display(char *magic)
{
  char *ptr, *buf, *tmpmc = "/tmp/cloonix_magic_cookie";
  char randstr[5];
  char tmp[MAX_PATH_LEN];
  char cmd[MAX_PATH_LEN];
  int i, nb, len, result = -1;
  char *display = getenv("DISPLAY");
  srand(time(NULL));

  memset(randstr, 0, 5);
  memset(tmp, 0, MAX_PATH_LEN);
  memset(magic, 0, MAX_NAME_LEN);
  memset(cmd, 0, MAX_PATH_LEN);
  for (i=0; i<4; i++)
    randstr[i] = 'A'+ my_rand(26);
  snprintf(tmp, MAX_PATH_LEN-1, "%s_%s", tmpmc, randstr);
  unlink(tmp);
  snprintf(cmd, MAX_PATH_LEN-1, "%s nextract %s %s", XAUTH_BIN, tmp, display);
  if (system(cmd))
    KERR("ERROR %s", cmd);
  else
    {
    buf = local_read_whole_file(tmp, &len);
    if (buf == NULL)
      KERR("ERROR %s", tmp);
    else
      {
      ptr = strrchr(buf, ' ');
      if (ptr)
        {
        ptr += 1;
        nb = strspn(ptr, "0123456789abcdefABCDEF");
        ptr[nb] = 0;
        strncpy(magic, ptr, MAX_NAME_LEN-1);
        result = 0;
        }
      free(buf);
      }
    }
  unlink(tmp);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_chansess_pty_req(struct Channel *channel) 
{
  char magic[MAX_NAME_LEN];
  char *cloonix_name = get_cloonix_name_prompt();
  char *cloonix_display = get_cloonix_display();
  buf_putbyte(ses.writepayload, SSH_MSG_CHANNEL_REQUEST);
  buf_putint(ses.writepayload, channel->remotechan);
  buf_putstring(ses.writepayload, "pty-req", strlen("pty-req"));

  buf_putbyte(ses.writepayload, 0);
  if (!strcmp(ses.remoteident, LOCAL_IDENT))
    {

    buf_putstring(ses.writepayload, cloonix_name, strlen(cloonix_name));
    buf_putstring(ses.writepayload, cloonix_display, strlen(cloonix_display));
    if (get_magic_cookie_with_display(magic))
      buf_putstring(ses.writepayload, "no_cookie_key", strlen("no_cookie_key")); 
    else
      buf_putstring(ses.writepayload, magic, strlen(magic)); 
    }
  put_winsize();
  buf_putint(ses.writepayload, 1); 
  buf_putbyte(ses.writepayload, 0);
  encrypt_packet();
  if (signal(SIGWINCH, sigwinch_handler) == SIG_ERR) 
    KOUT("Signal error");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_chansess_shell_req(struct Channel *channel)
{
  char* reqtype = NULL;
  if (cli_opts.cmd)
    reqtype = "exec";
  else
    reqtype = "shell";
  buf_putbyte(ses.writepayload, SSH_MSG_CHANNEL_REQUEST);
  buf_putint(ses.writepayload, channel->remotechan);
  buf_putstring(ses.writepayload, reqtype, strlen(reqtype));
  buf_putbyte(ses.writepayload, 0); /* Don't want replies */
  if (cli_opts.cmd)
    buf_putstring(ses.writepayload, cli_opts.cmd, strlen(cli_opts.cmd));
  encrypt_packet();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int cli_init_stdpipe_sess(struct Channel *channel)
{
	channel->writefd = STDOUT_FILENO;
	setnonblocking(STDOUT_FILENO);

	channel->readfd = STDIN_FILENO;
	setnonblocking(STDIN_FILENO);

	channel->errfd = STDERR_FILENO;
	setnonblocking(STDERR_FILENO);

	channel->extrabuf = cbuf_new(opts.recv_window);
	return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int cli_initchansess(struct Channel *channel)
{
  cli_init_stdpipe_sess(channel);
  if (cli_opts.wantpty)
    send_chansess_pty_req(channel);
  send_chansess_shell_req(channel);
  if (cli_opts.wantpty)
    {
    cli_tty_setup();
    cli_ses.last_char = '\r';
    }	
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cli_send_chansess_request(void)
{
  if (send_msg_channel_open_init(STDIN_FILENO) == DROPBEAR_FAILURE)
    KOUT("Couldn't open initial channel");
  encrypt_packet();
}
/*---------------------------------------------------------------------------*/
