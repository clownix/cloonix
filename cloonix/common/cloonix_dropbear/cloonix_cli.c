#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <asm/types.h>
#include <signal.h>




#include "io_clownix.h"
#include "doorways_sock.h"
#include "util_sock.h"
#include "queue.h"
#include "buffer.h"
#include "epoll_hooks.h"
#include "header_sock.h"

#include "includes.h"
#include "session.h"
#include "cloonix_x11.h"


void read_packet(void);
void cli_session(int sock_in, int sock_out); 
int send_msg_channel_data(struct Channel *channel, int isextended);
void process_packet(void);
void cli_sessionloop(void);
int writechannel(struct Channel* channel, int fd, circbuffer *cbuf);
void session_cleanup(void);
void cli_sessionloop_winchange();



extern int exitflag;

extern struct sshsession ses; 
#define MAX_CHARBUF_SIZE 100
static char g_cloonix_name_prompt[2*MAX_CHARBUF_SIZE];
static char g_cloonix_name[MAX_CHARBUF_SIZE];
static char g_cloonix_display[MAX_CHARBUF_SIZE];
static int g_door_llid;
static int g_connect_llid;

static char g_vm_name[MAX_NAME_LEN];
static char g_password[MSG_DIGEST_LEN];
static char g_cloonix_doors[MAX_PATH_LEN];

#define RX_BUF_MAX (MAX_DOORWAYS_BUF_LEN)
static char g_rx_buf[RX_BUF_MAX];
static int g_max_rx_buf_len;
static int g_current_read_buf_offset;
static int g_current_write_buf_offset;
static int g_current_rx_buf_len;

static struct epoll_event g_epev_readfd;
static struct epoll_event g_epev_writefd;
static struct epoll_event g_epev_errfd;


/*****************************************************************************/
/*
static void trace_read_write(char *act, int len, char *buf)
{
  KERR("%s: %d: %02X %02X %02X %02X %02X %02X %02X %02X   "
       "%02X %02X %02X %02X %02X %02X %02X %02X", act, len,
        buf[0] & 0xFF, buf[1] & 0xFF, buf[2] & 0xFF, buf[3] & 0xFF,
        buf[4] & 0xFF, buf[5] & 0xFF, buf[6] & 0xFF, buf[7] & 0xFF,
        buf[0+8] & 0xFF, buf[1+8] & 0xFF, buf[2+8] & 0xFF, buf[3+8] & 0xFF,
        buf[4+8] & 0xFF, buf[5+8] & 0xFF, buf[6+8] & 0xFF, buf[7+8] & 0xFF);
}
*/
/*--------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
void rpct_recv_sigdiag_msg(int llid, int tid, char *line){KOUT(" ");}
void rpct_recv_poldiag_msg(int llid, int tid, char *line){KOUT(" ");}
void rpct_recv_pid_req(int llid, int tid, char *name, int num){KOUT(" ");}
void rpct_recv_kil_req(int llid, int tid){KOUT();}
void rpct_recv_pid_resp(int llid, int tid, char *name, int num,
                        int toppid, int pid){KOUT(" ");}
void rpct_recv_hop_sub(int llid, int tid, int flags_hop){KOUT(" ");}
void rpct_recv_hop_unsub(int llid, int tid){KOUT(" ");}
void rpct_recv_hop_msg(int llid, int tid, int flags_hop, char *txt){KOUT(" ");}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_cli_finished(void *data)
{
  struct Channel *channel;
  channel = &ses.channel;
  if (((channel->writefd >= 0) && (cbuf_getused(channel->writebuf) > 0)) ||
      ((channel->errfd >= 0) && (cbuf_getused(channel->extrabuf) > 0)))
    {
    clownix_timeout_add(1, timeout_cli_finished, data, NULL, NULL);
    }
  else
    {
    session_cleanup();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cli_finished(int line) 
{
  unsigned long uline = (unsigned long) line;
  clownix_timeout_add(1, timeout_cli_finished, (void *) uline, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_cloonix_name_prompt(void)
{
  return g_cloonix_name_prompt;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_cloonix_display(void)
{
  return g_cloonix_display;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void cb_doors_end(int llid)
{
  (void) llid;
  cli_finished(__LINE__); 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void add_to_read_buf(int len, char *buf)
{
  int i;
  if ((g_current_rx_buf_len + len) >= g_max_rx_buf_len)
    KOUT("%d %d %d", g_current_rx_buf_len, len, g_max_rx_buf_len);
  for (i=0; i<len; i++)
    {
    g_rx_buf[g_current_write_buf_offset] = buf[i];
    g_current_write_buf_offset += 1;
    g_current_rx_buf_len += 1;
    if (g_current_write_buf_offset == g_max_rx_buf_len)
      g_current_write_buf_offset = 0;
    if (g_current_rx_buf_len >= g_max_rx_buf_len)
      KOUT(" ");
    if (g_current_write_buf_offset == g_current_read_buf_offset)
      KOUT(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_from_read_buf(int max, char *buf)
{
  int i, len;
  len = g_current_rx_buf_len;
  if (len > max)
    len = max;
  for (i=0; i<len; i++)
    {
    buf[i] = g_rx_buf[g_current_read_buf_offset];
    g_current_read_buf_offset += 1;
    g_current_rx_buf_len -= 1;
    if (g_current_read_buf_offset == g_max_rx_buf_len)
      g_current_read_buf_offset = 0;
    if (g_current_rx_buf_len < 0)
      KOUT(" ");
    }
  return len; 
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int set_remoteident(char *buf)
{
  int result;
  char *ptr, *end;
  ptr = buf;
  while (memcmp(ptr, "SSH-", 4))
    {
    ptr = strchr(ptr, '\n');
    if (!ptr)
      break;
    ptr+= 1;
    }
  if (!memcmp(ptr, "SSH-", 4))
    {
    end = strchr(ptr, '\n');
    if (end)
      {
      result = end - ptr + 1;
      *end = 0;
      end = strchr(ptr, '\r');
      if (end)
        *end = 0;
      ses.remoteident = m_malloc(strlen(ptr) + 1);
      memcpy(ses.remoteident, ptr, strlen(ptr) + 1);
      }
    else
      KOUT("%s", buf);
    }
  else
    KOUT("%s", buf);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fct_before_epoll(int epollfd)
{
  struct Channel *channel;
  if (!(get_sessinitdone()))
    KOUT(" ");
  channel = &ses.channel;
  memset(&g_epev_readfd, 0, sizeof(struct epoll_event));
  memset(&g_epev_writefd, 0, sizeof(struct epoll_event));
  memset(&g_epev_errfd, 0, sizeof(struct epoll_event));
  g_epev_readfd.events = 0;
  g_epev_readfd.data.fd = channel->readfd;
  g_epev_writefd.events = 0;
  g_epev_writefd.data.fd = channel->writefd;
  g_epev_errfd.events = 0;
  g_epev_errfd.data.fd = channel->errfd;
  epoll_ctl(epollfd, EPOLL_CTL_DEL, channel->readfd, NULL);
  epoll_ctl(epollfd, EPOLL_CTL_DEL, channel->writefd, NULL);
  epoll_ctl(epollfd, EPOLL_CTL_DEL, channel->errfd, NULL);
  if (channel->init_done)
    {
    if (channel->transwindow > 0)
      {
      if (channel->readfd >= 0)
        {
        if (channel->readfd != 0)
          KOUT("%d", channel->readfd);
        g_epev_readfd.events |= EPOLLIN;
        epoll_ctl(epollfd, EPOLL_CTL_ADD, channel->readfd, &g_epev_readfd);
        }
      }
    if (channel->writefd >= 0 &&
        cbuf_getused(channel->writebuf) > 0)
      {
      if (channel->writefd != 1)
        KOUT("%d", channel->writefd);
      g_epev_writefd.events |= EPOLLOUT;
      epoll_ctl(epollfd, EPOLL_CTL_ADD, channel->writefd, &g_epev_writefd);
      }
    if ((channel->errfd >= 0) && (cbuf_getused(channel->extrabuf) > 0))
      {
      if (channel->errfd != 2)
        KOUT("%d", channel->errfd);
      g_epev_errfd.events |= EPOLLOUT;
      if (epoll_ctl(epollfd, EPOLL_CTL_ADD, channel->errfd, &g_epev_errfd))
        KOUT(" ");
      }
  }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int fct_after_epoll(int nb, struct epoll_event *events)
{
  struct Channel *channel = &ses.channel;
  int i, fd;
  uint32_t evt;
  int result = 0;
  if (exitflag)
    {
    cli_finished(__LINE__); 
    }
  else if (channel->init_done)
    {
    for(i=0; i<nb; i++)
      {
      fd = events[i].data.fd;
      evt = events[i].events;
      if (evt & EPOLLIN)
        {
        if (fd == channel->readfd)
          {
          if (channel->readfd != 0)
            KOUT("%d", channel->readfd);
          if (send_msg_channel_data(channel, 0) == -1)
            {
            cli_finished(__LINE__); 
            }
          }
        if (fd == channel->errfd)
          {
          if (channel->errfd != 2)
            KOUT("%d", channel->errfd);
          if (send_msg_channel_data(channel, 1) == -1)
            {
            cli_finished(__LINE__); 
            }
          }
        result += 1;
        }
      if (evt & EPOLLOUT)
        {
        if (fd == channel->writefd)
          {
          if (channel->writefd != 1)
            KOUT("%d", channel->writefd);
          if (writechannel(channel, channel->writefd, channel->writebuf) == -1)
            {
            cli_finished(__LINE__); 
            }
          }
        if (fd == channel->errfd)
          {
          if (channel->errfd != 2)
            KOUT("%d", channel->errfd);
          if (writechannel(channel, channel->errfd, channel->extrabuf) ==  -1)
            {
            cli_finished(__LINE__); 
            }
          }
        result += 1;
        }
      if (evt & EPOLLERR)
        {
        cli_finished(__LINE__); 
        result += 1;
        }
      if (evt & EPOLLHUP)
        {
        cli_finished(__LINE__); 
        result += 1;
        }
      else if (evt)
        {
        if ((!(evt & EPOLLOUT)) && (!(evt & EPOLLIN)))
          KOUT("%X", evt);
        }
      }
    }
  else
    KERR(" ");
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void cb_doors_rx_nominal(int len, char *buf)
{
  int nb;
  if (!ses.remoteident)
    {
    nb = set_remoteident(buf);
    if (len < nb)
      KOUT("%d %d", len, nb);
    add_to_read_buf(len-nb, buf+nb);
    channel_add_epoll_hooks(fct_before_epoll,fct_after_epoll);
    }
  else
    {
    add_to_read_buf(len, buf);
    }

    
  while (g_current_rx_buf_len)
    {
    read_packet();
    if (ses.payload != NULL)
      process_packet();
    }

  cli_sessionloop();
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doorways_rx_bufraw(int llid, int tid,
                        int len_bufraw, char *doors_bufraw,
                        int type, int val, int len, char *buf)
{
  int display, dido_llid, sub_dido_idx;
  char ok[100];

  (void) llid;

  if (type == doors_type_dbssh_x11_ctrl)  
    {
    dido_llid = tid;
    sub_dido_idx = val;
    receive_ctrl_x11_from_doors(dido_llid, sub_dido_idx, len, buf);
    }
  else if (type == doors_type_dbssh_x11_traf)  
    {
    dido_llid = tid;
    sub_dido_idx = val;
    receive_traf_x11_from_doors(dido_llid, sub_dido_idx, len, buf);
    }
  else if (type == doors_type_dbssh)  
    {
    if ((val == doors_val_link_ok) || (val == doors_val_link_ko))
      {
      if (!strncmp(buf, "OK", 2))
        {
        if (sscanf(buf,"%s DBSSH_CLI_DOORS_RESP name=%s display_sock_x11=%d", 
            ok, g_cloonix_name, &display)!= 3) 
          {
          KOUT("%s", buf);
          }
        else
          {
          memset(g_cloonix_name_prompt, 0, 2*MAX_CHARBUF_SIZE);
          snprintf(g_cloonix_name_prompt, 2*MAX_CHARBUF_SIZE-1,
                   "PS1=\"%s# \"", g_cloonix_name); 
          snprintf(g_cloonix_display, MAX_CHARBUF_SIZE-1, ":%d", 
                   display + X11_DISPLAY_OFFSET);
          cli_session(-1, -1); 
          }
        }
      else if (!strncmp(buf, "KO", 2))
        {
        if (get_sessinitdone())
          {
          cli_finished(__LINE__); 
          }
        else
          {
          if (sscanf(buf,"KO_ping_to_agent DBSSH_CLI_DOORS_RESP name=%s",
              g_cloonix_name)== 1)
            {
            fprintf(stderr, "NOT REACHABLE: %s\n", g_cloonix_name);
            wrapper_exit(1, (char *)__FILE__, __LINE__);
            }
          else if (sscanf(buf,"KO_name_not_found DBSSH_CLI_DOORS_RESP name=%s",
              g_cloonix_name)== 1)
            {
            fprintf(stderr, "NOT FOUND: %s\n", g_cloonix_name);
            wrapper_exit(1, (char *)__FILE__, __LINE__);
            }
          else if (sscanf(buf,"KO_bad_connection DBSSH_CLI_DOORS_RESP name=%s",
              g_cloonix_name)== 1)
            {
            fprintf(stderr, "BAD CONNECT: %s\n", g_cloonix_name);
            wrapper_exit(1, (char *)__FILE__, __LINE__);
            }
          else
            {
            fprintf(stderr, "ERROR: %s %s\n",  buf, g_cloonix_name);
            wrapper_exit(1, (char *)__FILE__, __LINE__);
            }
          }
        }
      else
        {
        KERR("%s", buf);
        cli_finished(__LINE__);
        }
      }
    else if (val == doors_val_none)
      {
      cb_doors_rx_nominal(len, buf);
      }
    }
  else
    KOUT("%d %d", type, val);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void heartbeat(int delta)
{
  static int count = 0;
  (void) delta;
  count++;
  if (count == 500)
    {
    if (!g_door_llid)
      {
      close(get_fd_with_llid(g_connect_llid));
      doorways_sock_client_inet_delete(g_connect_llid);
      printf("\nTimeout during connect: %s\n\n", g_cloonix_doors);
      wrapper_exit(1, (char *)__FILE__, __LINE__);
      }
    }
  if (!(count%10))
    cli_sessionloop_winchange();
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int tst_port(char *str_port, int *port)
{
  int result = 0;
  unsigned long val;
  char *endptr;
  val = strtoul(str_port, &endptr, 10);
  if ((endptr == NULL)||(endptr[0] != 0))
    result = -1;
  else
    {
    if ((val < 1) || (val > 65535))
      result = -1;
    *port = (int) val;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void test_param(char *param, uint32_t *ip, int *port)
{
  char pm[MAX_PATH_LEN];
  char *ptr_ip, *ptr_port;
  struct stat stat_file;

  *ip = 0;
  *port = 0;
  if (strlen(param) >= MAX_PATH_LEN)
    {
    printf("\nparam LENGTH Problem %s\n\n", param);
    KOUT("param LENGTH Problem");
    }
  else
    {
    memset(pm, 0, MAX_PATH_LEN);
    strncpy(pm, param, MAX_PATH_LEN-1);
    ptr_ip = pm;
    ptr_port = strchr(pm, ':');
    if (ptr_port)
      {
      *ptr_port = 0;
      ptr_port++;
      if (ip_string_to_int (ip, ptr_ip))
        {
        printf("\nIP param Problem %s\n\n", param);
        KOUT("IP param Problem %s", param);
        }
      if (tst_port(ptr_port, port))
        {
        printf("\nPORT param Problem %s\n\n", param);
        KOUT("PORT param Problem %s", param);
        }
      }
    else if (stat(param, &stat_file))
      {
      printf("\nBad address 1 %s\n\n", param);
      KOUT("Bad address: %s", param);
      }
    else if (!S_ISSOCK(stat_file.st_mode))
      {
      printf("\nBad address 2 %s\n\n", param);
      KOUT("Bad address: %s", param);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int callback_connect(int llid, int fd)
{
  char buf[MAX_PATH_LEN];
  if (g_door_llid == 0)
    {
    g_door_llid = doorways_sock_client_inet_end(doors_type_dbssh, llid, fd, 
                                                g_password, 
                                                cb_doors_end,
                                                doorways_rx_bufraw);
    if (!g_door_llid)
      {
      printf("\nConnect not possible: %s\n\n", g_cloonix_doors);
      wrapper_exit(1, (char *)__FILE__, __LINE__);
      }
    memset(buf, 0, MAX_PATH_LEN);
    snprintf(buf, MAX_PATH_LEN, 
             "DBSSH_CLI_DOORS_REQ name=%s cookie=no_cookies", g_vm_name);
  
    if (doorways_tx_bufraw(g_door_llid, 0, doors_type_dbssh, 
                           doors_val_init_link, strlen(buf)+1, buf))
      {
      printf("ERROR INIT SEQ:\n%d, %s\n\n", (int) strlen(buf), buf);
      wrapper_exit(1, (char *)__FILE__, __LINE__);
      }
    }
  else
    KERR("TWO CONNECTS FOR ONE REQUEST");
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cloonix_connect_remote(char *cloonix_doors, 
                           char *vmname,
                           char *password)
{
  uint32_t  ip=0;
  int  port=0;
  memset(g_cloonix_name, 0, 100);
  memset(g_cloonix_display, 0, 100);
  memset(g_rx_buf, 0, RX_BUF_MAX);
  memset(g_vm_name, 0, MAX_NAME_LEN);
  memset(g_password, 0, MSG_DIGEST_LEN);
  memset(g_cloonix_doors, 0, MAX_PATH_LEN);
  strncpy(g_cloonix_doors, cloonix_doors, MAX_PATH_LEN-1);
  strncpy(g_vm_name, vmname, MAX_NAME_LEN-1);
  strncpy(g_password, password, MSG_DIGEST_LEN-1);
  g_max_rx_buf_len = RX_BUF_MAX;
  g_current_read_buf_offset = 0;
  g_current_write_buf_offset = 0;
  g_current_rx_buf_len = 0;
  doorways_sock_init(0);
  msg_mngt_init("dropbear", IO_MAX_BUF_LEN);
  msg_mngt_heartbeat_init(heartbeat);
  test_param(cloonix_doors, &ip, &port);
  if (ip)
    {
    g_door_llid = 0;
    g_connect_llid = doorways_sock_client_inet_start(ip, port, 
                                                     callback_connect);
    }
  else
    {
    g_door_llid = 0;
    g_connect_llid = doorways_sock_client_unix_start(cloonix_doors,
                                                     callback_connect);
    }
  if (!g_connect_llid)
    {
    printf("\nCannot reach doorways %s\n\n", cloonix_doors);
    wrapper_exit(1, (char *)__FILE__, __LINE__);
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
size_t cloonix_read(int fd, void *ibuf, size_t count)
{
  char *buf = (char *) ibuf;
  size_t len;
  if (fd == -1)
    {
    len = (size_t) get_from_read_buf((int) count, buf);
    }
  else
    len = read(fd, ibuf, count);
  return len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
size_t cloonix_write(int fd, const void *ibuf, size_t count)
{
  int result = -1;
  int len_done, len_to_do, chosen_len;
  int headsize = doorways_header_size();
  int len_max = MAX_DOORWAYS_BUF_LEN - headsize;
  char *buf = (char *) ibuf;

  if (fd == -1)
    {
    if (msg_exist_channel(g_door_llid))
      {
      len_to_do = count;
      len_done = 0;
      while (len_to_do)
        {
        if (len_to_do >= len_max)
          chosen_len = len_max;
        else
          chosen_len = len_to_do;
        if (doorways_tx_bufraw(g_door_llid, 0, doors_type_dbssh, 
                               doors_val_none, chosen_len, buf+len_done))
          {
          KERR("%d %d %d %d", len_to_do, chosen_len, len_max, errno);
          break;
          }
        else
          {
          len_done += chosen_len;
          len_to_do -= chosen_len;
          }
        }
      if (len_done == (int) count)
        result = count;
      }
    }
  else
    {
    result = write(fd, buf, count);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cloonix_dequeue(struct Queue* queue)
{
  dequeue(queue);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cloonix_enqueue(struct Queue* queue, void* item)
{
  enqueue(queue, item);
  while (!write_packet());
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cloonix_session_loop(void) 
{
  msg_mngt_loop();
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void send_traf_x11_to_doors(int dido_llid,int sub_dido_idx,int len,char *buf)
{
  if (doorways_tx_bufraw(g_door_llid, dido_llid,
                         doors_type_dbssh_x11_traf, 
                         sub_dido_idx, len, buf))
    KERR("WARNING %d %d %d %d", g_door_llid, dido_llid, sub_dido_idx, len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void send_ctrl_x11_to_doors(int dido_llid,int sub_dido_idx,int len,char *buf)
{
  if (doorways_tx_bufraw(g_door_llid, dido_llid,
                         doors_type_dbssh_x11_ctrl,
                         sub_dido_idx, len, buf))
    KERR("WARNING %d %d %d %d", g_door_llid, dido_llid, sub_dido_idx, len);
}
/*--------------------------------------------------------------------------*/


