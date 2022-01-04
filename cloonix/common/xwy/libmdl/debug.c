/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <pthread.h>
#include <stdarg.h>


#include "mdl.h"
#include "debug.h"
#include "fd_spy.h"
#include "wrap.h"

/*****************************************************************************/
char *debug_get_thread_type_txt(int type)
{
  char *result = "not_decoded";
  switch(type)
    {
    case thread_type_tx:
    result = "thread_type_tx";
    break;
    case thread_type_x11:
    result = "thread_type_x11";
    break;
    default:
      KERR("%d", type);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *debug_get_fd_type_txt(int type)
{
  char *result = "not_decoded";
  switch(type)
    {
    case fd_type_fork_pty:
    result = "fork_pty";
    break;
    case fd_type_pipe_win_chg:
    result = "pipe_win_chg";
    break;
    case fd_type_pipe_sig:
    result = "pipe_sig";
    break;
    case fd_type_dialog_thread:
    result = "dialog_thread";
    break;
    case fd_type_listen_inet_srv:
    result = "listen_inet_srv";
    break;
    case fd_type_listen_unix_srv:
    result = "listen_unix_srv";
    break;
    case fd_type_srv:
    result = "srv";
    break;
    case fd_type_srv_ass:
    result = "srv_ass";
    break;
    case fd_type_cli:
    result = "cli";
    break;
    case fd_type_scp:
    result = "scp";
    break;
    case fd_type_pty:
    result = "pty";
    break;
    case fd_type_sig:
    result = "sig";
    break;
    case fd_type_zero:
    result = "zeo";
    break;
    case fd_type_one:
    result = "one";
    break;
    case fd_type_win:
    result = "win";
    break;
    case fd_type_tx_epoll:
    result = "tx_epoll";
    break;
    case fd_type_x11_epoll:
    result = "x11_epoll";
    break;
    case fd_type_x11_listen:
    result = "x11_listen";
    break;
    case fd_type_x11_connect:
    result = "x11_connect";
    break;
    case fd_type_x11_accept:
    result = "x11_accept";
    break;
    case fd_type_x11_rd_x11:
    result = "x11_rd_x11";
    break;
    case fd_type_x11_rd_soc:
    result = "x11_rd_soc";
    break;
    case fd_type_x11_wr_x11:
    result = "x11_wr_x11";
    break;
    case fd_type_x11_wr_soc:
    result = "x11_wr_soc";
    break;
    case fd_type_x11:
    result = "x11";
    break;
    case fd_type_cloonix:
    result = "cloonix";
    break;
    case fd_type_listen_cloonix:
    result = "listen_cloonix";
    break;
    default:
      KERR("%d", type);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int debug_get_trunc_usec(void)
{
  struct timespec ts;
  long long date;
  int result;
  if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC_RAW, &ts))
    KOUT(" ");
  date = (long long) (ts.tv_sec);
  date *= 1000000;
  date += ((long long) ts.tv_nsec) / 1000;
  result = date % 10000000;
  return result;
}
/*---------------------------------------------------------------------------*/




#ifdef DEBUG

static FILE *g_debug_logfile;

/****************************************************************************/
void debug_dump_thread(int sock_fd_ass, int x11_fd, int srv_idx, int cli_idx)
{
  debug_evt("THREAD: sock_fd_ass: %d  x11_fd: %d  (%d-%d)",
             sock_fd_ass, x11_fd, srv_idx, cli_idx);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void debug_dump_rxmsg(t_msg *msg)
{
  int type, from, srv_idx, cli_idx;
  uint32_t randid;
  mdl_get_header_vals(msg, &randid, &type, &from, &srv_idx, &cli_idx);
  debug_evt("RXMSG: %s %s (%d-%d) len:%d  %08X",
                                        debug_get_evt_type_txt(type),
                                        debug_get_fd_type_txt(from),
                                        srv_idx, cli_idx, msg->len,
                                        msg->seqnum_checksum);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void debug_dump_enqueue(int fd_dst, t_msg *msg, int all, int th)
{
  int type, from, srv_idx, cli_idx;
  uint32_t randid;
  char *lib, *thlib;
  if (all)
    lib = "TXMSG";
  else
    lib = "TXRAW";
  if (th)
    thlib="thread";
  else
    thlib="main";
  mdl_get_header_vals(msg, &randid, &type, &from, &srv_idx, &cli_idx);

  if (all)
    debug_evt("%s %s: %s %s fd_dst:%d srv:%d cli:%d len:%05d  %08X",
                                                lib, thlib,
                                                debug_get_evt_type_txt(type),
                                                debug_get_fd_type_txt(from),
                                                fd_dst, srv_idx,
                                                cli_idx, msg->len, 
                                                msg->seqnum_checksum);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void debug_dump_enqueue_levels(int fd_dst, int slots, int bytes)
{
  debug_evt("CIRCLE: fd_dst:%d slots:%d bytes:%d", fd_dst, slots, bytes);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int debug_get_ioctl_queue_len(int fd, int type, int *used)
{
  int result = 0;
  if (!ioctl(fd, SIOCOUTQ, used))
    {
    if (*used > 1000)
      {
      result = 1;
      debug_evt("LOAD  %s used:%d", debug_get_fd_type_txt(type), *used);
      }
    }
  else
     debug_evt("LOAD  %s SIOCOUTQ ioctl error", debug_get_fd_type_txt(type));
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
char *debug_get_evt_type_txt(int type)
{
  char *result = "not_decoded";
  switch(type)
    {
    case msg_type_randid:
    result = "msg_type_randid";
    break;
    
    case msg_type_randid_associated:
    result = "msg_type_randid_associated";
    break;
    
    case msg_type_data_pty:
    result = "msg_type_data_pty";
    break;
    
    case msg_type_open_bash:
    result = "msg_type_open_bash";
    break;
    
    case msg_type_open_dae:
    result = "msg_type_open_dae";
    break;
    
    case msg_type_open_cmd:
    result = "msg_type_open_cmd";
    break;
    
    case msg_type_win_size:
    result = "msg_type_win_size";
    break;
    
    case msg_type_data_cli_pty:
    result = "msg_type_data_cli_pty";
    break;
    
    case msg_type_end_cli_pty:
    result = "msg_type_end_cli_pty";
    break;
    
    case msg_type_end_cli_pty_ack:
    result = "msg_type_end_cli_pty_ack";
    break;
    
    case msg_type_x11_info_flow:
    result = "msg_type_x11_info_flow";
    break;
    
    case msg_type_x11_init:
    result = "msg_type_x11_init";
    break;
    
    case msg_type_x11_connect:
    result = "msg_type_x11_connect";
    break;
    
    case msg_type_x11_connect_ack:
    result = "msg_type_x11_connect_ack";
    break;
    
    case msg_type_scp_open_snd:
    result = "msg_type_scp_open_snd";
    break;
    
    case msg_type_scp_open_rcv:
    result = "msg_type_scp_open_rcv";
    break;
    
    case msg_type_scp_ready_to_snd:
    result = "msg_type_scp_ready_to_snd";
    break;
    
    case msg_type_scp_ready_to_rcv:
    result = "msg_type_scp_ready_to_rcv";
    break;
    
    case msg_type_scp_data:
    result = "msg_type_scp_data";
    break;
    
    case msg_type_scp_data_end:
    result = "msg_type_scp_data_end";
    break;
    
    case msg_type_scp_data_begin:
    result = "msg_type_scp_data_begin";
    break;
    
    case msg_type_scp_data_end_ack:
    result = "msg_type_scp_data_end_ack";
    break;

    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *debug_pty_txt(t_msg *msg)
{
  char *result = NULL;
  static char txt[101]; 
  int i, j, type, from, cli_idx, srv_idx;
  uint32_t randid;

  mdl_get_header_vals(msg, &randid, &type, &from, &srv_idx, &cli_idx);
  if (from == fd_type_pty)
    {
    for (i=0, j=0; (i < msg->len) && (j < 97); i++, j += 3)
      sprintf(&(txt[j]), "%02X ", msg->buf[i]);
    result = txt;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void debug_wrap_read_write(int is_read, int fd, int len, int from,
                           const void *buf)
{
  char *label = debug_get_fd_type_txt(from);
  char *rd_wr="READ ";
  uint16_t sum;
  uint32_t randid;
  int type=0, secfrom=0, srv_idx=0, cli_idx=0;
  t_msg *msg = (t_msg *) buf;

  if (!is_read)
    rd_wr="WRITE";
  if (msg->cafe == 0xCAFEDECA)
    {
    mdl_get_header_vals(msg, &randid, &type, &secfrom, &srv_idx, &cli_idx);
    sum = mdl_sum_calc(msg->len, msg->buf);
    }
  else if ((from == fd_type_x11_rd_soc) ||
           (from == fd_type_x11_rd_x11) ||
           (from == fd_type_x11_wr_soc) ||
           (from == fd_type_x11_wr_x11))
    sum = mdl_sum_calc(len, (uint8_t *)buf);
  else
    sum = 0;
  if (msg->cafe == 0xCAFEDECA)
    debug_evt("%s %s len:%05d fd:%02d srv:%d cli:%d sum:%04X", 
              rd_wr, label, len, fd, srv_idx, cli_idx, sum & 0xFFFF);
  else
    {
    fd_spy_get_type(fd, &srv_idx, &cli_idx);
    debug_evt("%s x11rdwr %s len:%05d fd:%02d (%d-%d) sum:%04X",
              rd_wr, label, len, fd, srv_idx, cli_idx, sum & 0xFFFF);
    }
  if (len < 0)
    debug_evt("IO NEG LEN fd: %d ERR: %s", fd, strerror(errno));
  else if (len == 0)
    debug_evt("IO ZERO LEN fd: %d", fd);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void debug_wrap_fd_ok(char *act, int fd, char *wrapped, int fd_type)
{
  char *type = debug_get_fd_type_txt(fd_type);
  debug_evt("%s fd:%d syst:%s type:%s", act, fd, wrapped, type);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void debug_wrap_fd_ko(char *act, char *wrapped, int fd_type,
                      char *err, const char *fct)
{
  char *type = debug_get_fd_type_txt(fd_type);
  debug_evt("%s syst:%s type:%s err:%s fct:%s", act, wrapped, type, err, fct);
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void debug_evt(const char * format, ...)
{
  va_list arguments;
  va_start (arguments, format);
  fprintf(g_debug_logfile, "%07d  ", debug_get_trunc_usec());
  vfprintf(g_debug_logfile, format, arguments); 
  fprintf(g_debug_logfile, "\n"); 
  fflush(g_debug_logfile);
  va_end (arguments);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void debug_init(int is_srv)
{
  char *user = getenv("USER");
  char log_file[MAX_TXT_LEN];
  if (is_srv) 
    strcpy(log_file, DEBUG_LOG_FILE_SRV);
  else
    strcpy(log_file, DEBUG_LOG_FILE_CLI);
  if (user)
    strcat(log_file, user);
  g_debug_logfile = fopen(log_file, "w");
  wrap_chmod_666(log_file);
  if (g_debug_logfile == NULL)
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

#endif
