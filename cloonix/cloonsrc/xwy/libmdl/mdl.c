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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>



#include "mdl.h"
#include "low_write.h"
#include "wrap.h"
#include "debug.h"
#include "thread_tx.h"
#include "thread_x11.h"
#include "fd_spy.h"
#include "thread_spy.h"
#include "glob_common.h"

#define MAX_QUEUE_MDL_RX 5*MAX_MSG_LEN

typedef struct t_mdl
{
  uint16_t write_seqnum;
  uint16_t read_seqnum;
  t_inflow inflow;
  int  rxoffst;
  char bufrx[MAX_QUEUE_MDL_RX];
  int type;
} t_mdl;

static t_mdl *g_mdl[MAX_FD_NUM];

/*****************************************************************************/
char *mdl_argv_linear(char **argv)
{ 
  int i; 
  static char result[3*MAX_TXT_LEN];
  memset(result, 0, 3*MAX_TXT_LEN);
  for (i=0;  (argv[i] != NULL); i++)
    {
    strcat(result, argv[i]);
    if (strlen(result) >= 2*MAX_TXT_LEN)
      {
      KERR("NOT POSSIBLE");
      break;
      }
    strcat(result, " ");
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
FILE *mdl_argv_popen(char *argv[])
{
  int exited_pid, timeout_pid, worker_pid, chld_state, pid, status=97;
  int i, pdes[2];
  FILE *iop = NULL;

  if (pipe(pdes))
    KOUT("ERROR");
  if ((pid = fork()) < 0)
    KOUT("ERROR");
  if (pid == 0)
    {
    close(pdes[0]);
    if (pdes[1] != STDOUT_FILENO)
      {
      (void)dup2(pdes[1], STDOUT_FILENO);
      (void)close(pdes[1]);
      }
    worker_pid = fork();
    if (worker_pid == 0)
      {
      execv(argv[0], argv);
      KOUT("ERROR FORK error %s", strerror(errno));
      }
    timeout_pid = fork();
    if (timeout_pid == 0)
      {
      sleep(5);
      KERR("WARNING TIMEOUT SLOW CMD 1 %s", mdl_argv_linear(argv));
      exit(1);
      }
    exited_pid = wait(&chld_state);
    if (exited_pid == worker_pid)
      {
      if (WIFEXITED(chld_state))
        status = WEXITSTATUS(chld_state);
      if (WIFSIGNALED(chld_state))
        KERR("WARNING Child exited via signal %d\n", WTERMSIG(chld_state));
      kill(timeout_pid, SIGKILL);
      }
    else
      {
      kill(worker_pid, SIGKILL);
      }
    wait(NULL);
    exit(status);
    }
  wait(NULL);
  iop = fdopen(pdes[0], "r");
  (void)close(pdes[1]);
  return iop;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
uint16_t mdl_sum_calc(int len, uint8_t *buff)
{
  int i, sum = 0;
  for (i=0; i<len; i=i+2)
    {
    if (i+1 == len)
      {
      sum += ((buff[i] << 8) & 0xFF00);
      break;
      }
    else
      sum += ((buff[i] << 8) & 0xFF00) + (buff[i+1] & 0xFF);
    }
  return sum;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
char *mdl_int_to_ip_string (int addr)
{
  static char ip_string[30];
  memset(ip_string, 0, 30);
  sprintf(ip_string, "%d.%d.%d.%d", (int) ((addr >> 24) & 0xff),
          (int)((addr >> 16) & 0xff), (int)((addr >> 8) & 0xff),
          (int) (addr & 0xff));
  return ip_string;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int mdl_ip_string_to_int (unsigned long *inet_addr, char *ip_string)
{
  int result = -1;
  unsigned int part[4];
  if (strlen(ip_string) != 0)
    {
    if ((sscanf(ip_string,"%u.%u.%u.%u",
                          &part[0], &part[1], &part[2], &part[3]) == 4) &&
        (part[0]<=0xFF) && (part[1]<=0xFF) &&
        (part[2]<=0xFF) && (part[3]<=0xFF))
      {
      result = 0;
      *inet_addr = 0;
      *inet_addr += part[0]<<24;
      *inet_addr += part[1]<<16;
      *inet_addr += part[2]<<8;
      *inet_addr += part[3];
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int mdl_parse_val(const char *str_val)
{
  int result = -1;
  char *end = NULL;
  long val = strtol(str_val, &end, 10);
  if ((str_val != end) && (*end == '\0'))
    result = (int) val;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mdl_open(int s, int type, t_outflow outflow, t_inflow inflow)
{
  if ((s < 0) || (s >= MAX_FD_NUM))
    KOUT("ERROR %d", s);
  if (g_mdl[s])
    KOUT("ERROR MDL OPEN MDL EXISTS %d", s);
  wrap_nonblock(s);
  g_mdl[s] = (t_mdl *) wrap_malloc(sizeof(t_mdl));
  memset(g_mdl[s], 0, sizeof(t_mdl));
  g_mdl[s]->write_seqnum = 0;
  g_mdl[s]->read_seqnum = 0;
  g_mdl[s]->rxoffst = 0;
  g_mdl[s]->inflow = inflow;
  g_mdl[s]->type = type;

  if (type != fd_type_cli)
    low_write_open(s, type, outflow);
}
/*--------------------------------------------------------------------------*/

/**************************************************************************/
void mdl_modify(int s, int type)
{
  if ((s < 0) || (s >= MAX_FD_NUM))
    KOUT("%d", s);
  if (g_mdl[s])
    {
    DEBUG_EVT("MDL_MODIFY %s fd:%d", debug_get_fd_type_txt(type), s);
    low_write_modify(s, type);
    }

}
/*--------------------------------------------------------------------------*/

/**************************************************************************/
int  mdl_exists(int s)
{
  int result = 0;
  if ((s < 0) || (s >= MAX_FD_NUM))
    KOUT("%d", s);
  if (g_mdl[s])
    {
    result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/**************************************************************************/
void mdl_close(int s)
{
  int is_srv = 0;
  if ((s < 0) || (s >= MAX_FD_NUM))
    KOUT("%d", s);
  if (g_mdl[s])
    {
    if (g_mdl[s]->type != fd_type_cli)
      is_srv = 1;
    if (is_srv)
      {
      low_write_close(s);
      }
    wrap_free(g_mdl[s], __LINE__);
    g_mdl[s] = NULL;
    }
  else
    KERR("WARNING MDL DOES NOT EXIST %d", s);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mdl_get_header_vals(t_msg *msg, uint32_t *randid, int *type, int *from,
                                     int *srv_idx, int *cli_idx)
{
  *type    = msg->type         & 0x00FF;
  *from    = (msg->type >> 8)  & 0x00FF;
  *cli_idx = (msg->type >> 16) & 0x00FF;
  *srv_idx = (msg->type >> 24) & 0x00FF;
  *randid  = msg->randid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mdl_set_header_vals(t_msg *msg, uint32_t randid, int type, int from,
                                     int srv_idx, int cli_idx)
{
  if (srv_idx > 0)
    {
    if ((srv_idx < X11_DISPLAY_XWY_MIN) || (srv_idx > X11_DISPLAY_XWY_MAX))
      KOUT("%d %d %d", type, from, srv_idx);
    }

  msg->type = (((srv_idx << 24) & 0xFF000000) |
               ((cli_idx << 16) & 0x00FF0000) |
               ((from    << 8)  & 0x0000FF00) |
                (type & 0xFF));
  msg->randid = randid; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mdl_prepare_header_msg(uint16_t write_seqnum, t_msg *msg)
{
  uint16_t sumcheck= mdl_sum_calc(msg->len, msg->buf);
  msg->cafe = 0xCAFEDECA;
  msg->seqnum_checksum = ((write_seqnum<<16) & 0xFFFF0000);
  msg->seqnum_checksum |= (sumcheck & 0xFFFF);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mdl_queue_write_msg(int fd_dst, t_msg *msg)
{
  int result = 0;
  uint16_t sumcheck= mdl_sum_calc(msg->len, msg->buf);
  uint32_t randid;
  int type, from, srv_idx, cli_idx;
  t_mdl *mdl = g_mdl[fd_dst];
  if (!mdl)
    KOUT("%d NOT OPEN", fd_dst);
  mdl_prepare_header_msg(mdl->write_seqnum, msg);
  mdl->write_seqnum++;
  if (low_write_raw(fd_dst, msg, 1))
    {
    mdl_get_header_vals(msg, &randid, &type, &from, &srv_idx, &cli_idx);
    KERR("%d %d %d %d %d %s",type,from,srv_idx,cli_idx,msg->len,msg->buf); 
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_msg *alloc_and_copy(int tot_len, t_msg *in_msg)
{
  char *out_msg = (char *) wrap_malloc(tot_len+1);
  memcpy(out_msg, (char *) in_msg, tot_len); 
  out_msg[tot_len] = 0;
  return ((t_msg *) out_msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void do_cb(t_mdl *mdl, void *ptr, int llid, int fd, 
                  t_rx_msg_cb rx_cb, t_rx_err_cb err_cb)
{
  t_msg *rx_cb_msg, *msg = (t_msg *) mdl->bufrx;
  char *tmp;
  int done, rxoffst = mdl->rxoffst;
  uint16_t seqnum, checksum, sumcheck;
  while((rxoffst >= g_msg_header_len) &&
        (rxoffst >= msg->len + g_msg_header_len))
    {
    if (msg->cafe != 0xCAFEDECA)
      KERR("header id is %lX", msg->cafe);
    seqnum = (msg->seqnum_checksum >> 16) & 0xFFFF;
    if (seqnum != mdl->read_seqnum)
      KERR("header seqnum %d %d", seqnum&0xFFFF, 
                                  mdl->read_seqnum&0xFFFF);
    mdl->read_seqnum++;
    checksum = (msg->seqnum_checksum) & 0xFFFF;
    sumcheck = mdl_sum_calc(msg->len , msg->buf);
    if (checksum != sumcheck)
      KERR("header checksum %04X %04X", checksum&0xFFFF, sumcheck&0xFFFF); 
    if ((msg->len < 0) || (msg->len > MAX_MSG_LEN))
      KERR("header len: %ld", msg->len); 
    rx_cb_msg = alloc_and_copy(msg->len + g_msg_header_len, msg);
    rx_cb(ptr, llid, fd, rx_cb_msg);
    done = msg->len + g_msg_header_len;
    msg = (t_msg *)((char *)msg + done);
    rxoffst -= done;
    }
  if (rxoffst)
    {
    tmp = (char *) wrap_malloc(rxoffst);
    memcpy(tmp, (char *) msg, rxoffst);
    memcpy(mdl->bufrx, tmp, rxoffst);
    wrap_free(tmp, __LINE__);
    mdl->rxoffst = rxoffst;
    }
  else
    mdl->rxoffst = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_mdl *read_fd_checks(void *ptr, int llid, int fd, t_rx_err_cb err_cb)
{
  t_mdl *mdl;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    KOUT("%d", fd);
  mdl = g_mdl[fd];
  if (!mdl)
    err_cb(ptr, llid, fd, "Context mdl not found");
  else
    {
    if (MAX_QUEUE_MDL_RX - mdl->rxoffst <= 0)
      {
      mdl = NULL;
      KERR("%d %d", MAX_QUEUE_MDL_RX, mdl->rxoffst);
      err_cb(ptr, llid, fd, "Not enough space for fd read");
      }
    }
  return mdl;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_mdl *read_data_checks(void *ptr, int llid, int fd, 
                               int len, t_rx_err_cb err_cb)
{
  t_mdl *mdl = read_fd_checks(ptr, llid, fd, err_cb);
  if (mdl)
    {
    if (MAX_QUEUE_MDL_RX - mdl->rxoffst - len <= 0)
      {
      mdl = NULL;
      KERR("%d %d %d", MAX_QUEUE_MDL_RX, mdl->rxoffst, len);
      err_cb(ptr, llid, fd, "Not enough space for data read");
      }
    }
  return mdl;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mdl_read_fd(void *ptr, int fd, t_rx_msg_cb rx_cb, t_rx_err_cb err_cb)
{
  char err[MAX_TXT_LEN];
  int len, max_to_read;
  t_mdl *mdl = read_fd_checks(ptr, 0, fd, err_cb);
  if (mdl)
    {
    max_to_read = MAX_QUEUE_MDL_RX - mdl->rxoffst;
    len = mdl->inflow(fd, (void *)(mdl->bufrx + mdl->rxoffst),
                         (size_t) (max_to_read));
    if (len == 0)
      {
      err_cb(ptr, 0, fd, "read len is 0");
      }
    else if (len < 0)
      {
      if ((errno != EAGAIN) && (errno != EINTR))
        {
        snprintf(err, MAX_TXT_LEN-1, "read error: %s", strerror(errno));
        err[MAX_TXT_LEN-1] = 0;
        err_cb(ptr, 0, fd, err);
        KERR("%s", err);
        }
      }
    else
      {
      mdl->rxoffst += len;
      do_cb(mdl, ptr, 0, fd, rx_cb, err_cb);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mdl_read_data(void *ptr, int llid, int fd, int len, char *data,
                              t_rx_msg_cb rx_cb, t_rx_err_cb err_cb)
{
  t_mdl *mdl = read_data_checks(ptr, llid, fd, len, err_cb);
  if (mdl)
    {
    memcpy(mdl->bufrx + mdl->rxoffst, data, len);
    mdl->rxoffst += len;
    do_cb(mdl, ptr, llid, fd, rx_cb, err_cb);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mdl_heartbeat(void)
{
  static int count=0;
  static int count_1000=0;
  count += 1;
  count_1000 += 1;
  if (count == 10)
    {
    count = 0;
    thread_x11_heartbeat();
    }
  if (count_1000 == 1000)
    {
    count_1000 = 0;
    fd_spy_heartbeat();
    thread_spy_heartbeat();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mdl_fd_is_valid(int fd)
{
  return fcntl(fd, F_GETFD) != -1 || errno != EBADF;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int mdl_init(void)
{
  wrap_init();
  memset(g_mdl, 0, MAX_FD_NUM * sizeof(t_mdl *)); 
  thread_tx_init();
  thread_x11_init();
  fd_spy_init();
  cloonix_timer_init();
}
/*--------------------------------------------------------------------------*/
