/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "mdl.h"
#include "low_write.h"
#include "wrap.h"
#include "debug.h"
#include "fd_spy.h"

/*****************************************************************************/
static int scp_rx_open_snd(char *src, char *complete_dst, char *resp)
{
  struct stat sb;
  char dupdst[MAX_TXT_LEN];
  char *dir_path;
  int result = -1;
  memset(dupdst, 0, MAX_TXT_LEN);
  memset(resp, 0, MAX_TXT_LEN);
  snprintf(dupdst, MAX_TXT_LEN-1, "%s", complete_dst); 
  if (stat(complete_dst, &sb) == -1)
    {
    dir_path = dirname(dupdst);
    if (stat(dir_path, &sb) != -1)
      {
      snprintf(resp, MAX_TXT_LEN-1, "OK %s %s", src, complete_dst); 
      result = 0;
      }
    else 
      snprintf(resp, MAX_TXT_LEN-1, 
               "KO distant %s directory does not exist\n", dir_path);
    }
  else
    snprintf(resp, MAX_TXT_LEN-1, 
             "KO distant %s exists already\n", complete_dst);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int scp_rx_open_rcv(char *src, char *complete_dst, char *resp)
{
  struct stat sb;
  int result = -1;
  memset(resp, 0, MAX_TXT_LEN);
  if (stat(src, &sb) == -1)
    snprintf(resp, MAX_TXT_LEN-1, "KO distant %s does not exist\n", src);
  else if ((sb.st_mode & S_IFMT) != S_IFREG)
    snprintf(resp, MAX_TXT_LEN-1, "KO distant %s not regular file\n", src);
  else
    {
    snprintf(resp, MAX_TXT_LEN-1, "OK %s %s", src, complete_dst); 
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static void scp_cli_snd(int *fd, char *src, char *complete_dst, char *resp)
{
  if (!scp_rx_open_snd(src, complete_dst, resp))
    {
    *fd = open(complete_dst, O_CREAT|O_EXCL|O_WRONLY, 0644);
    if (*fd == -1)
      {
      resp[0] = 'K';
      resp[1] = 'O';
      KERR("%s %s", complete_dst, strerror(errno));
      }
    else if (fd_spy_add(*fd, fd_type_scp, __LINE__))
      KERR("%s", complete_dst);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void scp_cli_rcv(int *fd, char *src, char *complete_dst, char *resp)
{ 
  if (!scp_rx_open_rcv(src, complete_dst, resp))
    {
    *fd = open(src, O_RDONLY);
    if (*fd == -1)
      {
      resp[0] = 'K';
      resp[1] = 'O';
      KERR("%s %s", src, strerror(errno));
      }
    else if (fd_spy_add(*fd, fd_type_scp, __LINE__))
      KERR("%s", complete_dst);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int send_resp_cli(uint32_t randid, int type, int sock_fd, char *resp)
{
  int len = MAX_MSG_LEN+g_msg_header_len, result;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg, randid, type, fd_type_srv, 0, 0);
  msg->len = sprintf(msg->buf, "%s", resp) + 1;
  result = mdl_queue_write_msg(sock_fd, msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int send_scp_data_end(uint32_t randid, int sock_fd)
{
  int len = MAX_MSG_LEN+g_msg_header_len, result;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  mdl_set_header_vals(msg, randid, msg_type_scp_data_end, fd_type_srv,0,0);
  msg->len = 0;
  result = mdl_queue_write_msg(sock_fd, msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int send_scp_data(uint32_t randid, int scp_fd, int sock_fd)
{
  int len, result = -1;
  t_msg *msg;
  len = MAX_MSG_LEN+g_msg_header_len;
  msg = (t_msg *) wrap_malloc(len);
  len = wrap_read_scp(scp_fd, msg->buf, MAX_MSG_LEN);
  if (len < 0)
    KERR("%s", strerror(errno));
  else
    {
    if (len == MAX_MSG_LEN)
      result = 0;
    else
      result = 1;
    mdl_set_header_vals(msg, randid, msg_type_scp_data, fd_type_scp, 0, 0);
    msg->len = len;
    if (mdl_queue_write_msg(sock_fd, msg))
      result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int recv_scp_data_end(uint32_t randid, int scp_fd, int sock_fd)
{
  int len = MAX_MSG_LEN+g_msg_header_len, result;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  if (scp_fd == -1)
    KERR(" ");
  wrap_close(scp_fd, __FUNCTION__);
  scp_fd = -1;
  mdl_set_header_vals(msg, randid, msg_type_scp_data_end_ack,
                      fd_type_srv, 0, 0);
  msg->len = 0;
  result = mdl_queue_write_msg(sock_fd, msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int recv_scp_data(int scp_fd, t_msg *msg)
{
  int len, result = -1;
  if (scp_fd == -1)
    KERR(" ");
  else
    {
    len = wrap_write_scp(scp_fd, msg->buf, msg->len);
    if ((len < 0) || (len != msg->len))
      KERR("%d %ld %s", len, msg->len, strerror(errno));
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int recv_scp_open(uint32_t randid, int type, int sock_fd,
                  int *cli_scp_fd, char *buf)
{
  int result;
  char resp[MAX_TXT_LEN];
  char src[MAX_TXT_LEN];
  char complete_dst[MAX_TXT_LEN];
  strcpy(resp, "KO FATAL ERROR\r\n");
  if (sscanf(buf, "%s %s", src, complete_dst) != 2)
    {
    KERR("%s",  buf);
    result = send_resp_cli(randid, msg_type_scp_ready_to_rcv, sock_fd, resp);
    }
  else
    {
    if (type == msg_type_scp_open_snd)
      {
      scp_cli_snd(cli_scp_fd, src, complete_dst, resp);
      result = send_resp_cli(randid, msg_type_scp_ready_to_rcv,sock_fd,resp);
      }
    else if (type == msg_type_scp_open_rcv)
      {
      scp_cli_rcv(cli_scp_fd, src, complete_dst, resp);
      result = send_resp_cli(randid, msg_type_scp_ready_to_snd,sock_fd,resp);
      }
    else
      {
      KERR("ERROR %d", type);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int send_scp_to_cli(uint32_t randid, int scp_fd, int sock_fd)
{
  int result;
  result = send_scp_data(randid, scp_fd, sock_fd);
  if (result == 1)
    {
    if (send_scp_data_end(randid, sock_fd) == 0)
      result = 1;
    else
      result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/


