/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include <dirent.h>

#include "mdl.h"
#include "low_write.h"
#include "wrap.h"
#include "debug.h"
#include "cli_lib.h"
#include "scp.h"

static int g_llid;
static int g_tid;
static int g_type;
static int g_scp_fd;
static int g_mdl_src;
static t_sock_fd_tx g_sock_fd_tx;

static int g_write_seqnum;

char *read_whole_file(char *src_file, int *len, char *err)
{
  return NULL;
}

int write_whole_file(char *dst_file, char *buf, int len, char *err)
{
  return 0;
}


/****************************************************************************/
static uint16_t get_write_seqnum(void)
{
  uint16_t result = g_write_seqnum;
  g_write_seqnum += 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void my_cp_link(char *dir_src, char *dir_dst, char *name)
{
  char link_name[2*MAX_TXT_LEN];
  char link_val[2*MAX_TXT_LEN];
  int len;
  memset(link_val, 0, 2*MAX_TXT_LEN);
  snprintf(link_name, 2*MAX_TXT_LEN, "%s/%s", dir_src, name);
  link_name[MAX_TXT_LEN-1] = 0;
  len = readlink(link_name, link_val, MAX_TXT_LEN-1);
  if ((len != -1) && (len < MAX_TXT_LEN))
    {
    sprintf(link_name, "%s/%s", dir_dst, name);
    if (symlink (link_val, link_name))
      XERR("ERROR writing link:  %s  %s", link_name, strerror(errno));
    }
  else
    XERR("ERROR reading link: %s  %s", link_name, strerror(errno));
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void my_cp_file(char *dsrc, char *ddst, char *name)
{
  char err[MAX_TXT_LEN];
  char src_file[2*MAX_TXT_LEN];
  char dst_file[2*MAX_TXT_LEN];
  struct stat stat_file;
  int len;
  char *buf;
  err[0] = 0;
  snprintf(src_file, 2*MAX_TXT_LEN, "%s/%s", dsrc, name);
  snprintf(dst_file, 2*MAX_TXT_LEN, "%s/%s", ddst, name);
  src_file[2*MAX_TXT_LEN-1] = 0;
  dst_file[2*MAX_TXT_LEN-1] = 0;
  if (!stat(src_file, &stat_file))
    {
    buf = read_whole_file(src_file, &len, err);
    if (buf)
      {
      unlink(dst_file);
      if (!write_whole_file(dst_file, buf, len, err))
        chmod(dst_file, stat_file.st_mode);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void my_cp_dir(char *src_dir, char *dst_dir, char *src_name, char *dst_name)
{
  DIR *dirptr;
  struct dirent *ent;
  char src_sub_dir[2*MAX_TXT_LEN];
  char dst_sub_dir[2*MAX_TXT_LEN];
  snprintf(src_sub_dir, 2*MAX_TXT_LEN, "%s/%s", src_dir, src_name);
  snprintf(dst_sub_dir, 2*MAX_TXT_LEN, "%s/%s", dst_dir, dst_name);
  wrap_mkdir(dst_sub_dir);
  dirptr = opendir(src_sub_dir);
  if (dirptr)
    {
    while ((ent = readdir(dirptr)) != NULL)
      {
      if (!strcmp(ent->d_name, "."))
        continue;
      if (!strcmp(ent->d_name, ".."))
        continue;
      if (ent->d_type == DT_REG)
        my_cp_file(src_sub_dir, dst_sub_dir, ent->d_name);
      else if(ent->d_type == DT_DIR)
        my_cp_dir(src_sub_dir, dst_sub_dir, ent->d_name, ent->d_name);
      else if(ent->d_type == DT_LNK)
        my_cp_link(src_sub_dir, dst_sub_dir, ent->d_name);
      else
        XERR("Wrong type of file %s/%s", src_sub_dir, ent->d_name);
      }
    if (closedir(dirptr))
      XOUT("%s", strerror(errno));
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_scp_data_begin(void)
{
  int len = MAX_MSG_LEN+g_msg_header_len;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  uint32_t randid = xcli_get_randid();
  mdl_set_header_vals(msg,randid,msg_type_scp_data_begin,fd_type_scp,0,0);
  msg->len = 0;
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void recv_scp_data_end(void)
{
  int len = MAX_MSG_LEN+g_msg_header_len;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  uint32_t randid = xcli_get_randid();
  if (g_scp_fd == -1)
    XOUT(" ");
  close(g_scp_fd);
  g_scp_fd = -1;
  mdl_set_header_vals(msg,randid,msg_type_scp_data_end_ack,fd_type_scp,0,0);
  msg->len = 0;
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void recv_scp_data(t_msg *msg)
{
  int len;
  if (g_scp_fd == -1)
    XOUT(" ");
  len = wrap_write_scp(g_scp_fd, msg->buf, msg->len);
  if ((len < 0) || (len != msg->len))
    XOUT("%d %ld %s", len, msg->len, strerror(errno));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void recv_scp_ready(int llid, int type, t_msg *msg)
{
  char src[MAX_TXT_LEN];
  char complete_dst[MAX_TXT_LEN];
  if (msg->buf[0] == 'K')
    {
    printf("%s", msg->buf);
    exit(1);
    }
  if (sscanf(msg->buf, "OK %s %s", src, complete_dst) != 2)
    XOUT("%s", msg->buf);
  if (type == msg_type_scp_ready_to_rcv)
    {
    g_scp_fd = open(src, O_RDONLY);
    if (g_scp_fd == -1)
      XOUT("%s %s", src, strerror(errno));
    g_mdl_src = g_scp_fd;
    }
  else
    {
    g_scp_fd = open(complete_dst, O_CREAT|O_EXCL|O_WRONLY, 0644);
    if (g_scp_fd == -1)
      XOUT("%s %s", complete_dst, strerror(errno));
    g_mdl_src = g_scp_fd;
    send_scp_data_begin();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rx_scp_err_cb (void *ptr, int llid, int fd, char *err)
{
  (void) ptr;
  XOUT("%d  %s", fd,  err);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rx_scp_msg_cb(void *ptr, int llid, int sock_fd, t_msg *msg)
{
  int type, from, srv_idx, cli_idx;
  uint32_t randid;
  mdl_get_header_vals(msg, &randid, &type, &from, &srv_idx, &cli_idx);
  DEBUG_DUMP_RXMSG(msg);
  (void) ptr;
  if (randid != xcli_get_randid())
    XERR("%08X %08X", randid, xcli_get_randid());
  else switch(type)
    {
    case msg_type_end_cli_pty:
      exit(msg->buf[0]);
      break;
    case msg_type_scp_ready_to_snd:
    case msg_type_scp_ready_to_rcv:
      recv_scp_ready(llid, type, msg);
      break;
    case msg_type_scp_data:
      recv_scp_data(msg);
      break;
    case msg_type_scp_data_end:
      recv_scp_data_end();
      break;
    case msg_type_scp_data_end_ack:
      exit(0);
      break;
    default:
      XOUT("%ld", type);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int scp_get_fd(void)
{
  return g_scp_fd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void scp_close_fd(void)
{
  close(g_scp_fd);
  g_scp_fd = -1;
  g_mdl_src = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void scp_send_data()
{
  int len;
  t_msg *msg;
  uint32_t randid = xcli_get_randid();
  len = MAX_MSG_LEN+g_msg_header_len;
  msg = (t_msg *) wrap_malloc(len);
  len = wrap_read_scp(g_scp_fd, msg->buf, MAX_MSG_LEN);
  if (len < 0)
    XOUT("%s", strerror(errno));
  if (len == 0)
     {
     scp_send_data_end();
     wrap_free(msg, __LINE__);
     }
  else
    {
    mdl_set_header_vals(msg,randid,msg_type_scp_data,fd_type_scp,0,0);
    msg->len = len;
    mdl_prepare_header_msg(get_write_seqnum(), msg);
    len = msg->len + g_msg_header_len;
    g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void scp_send_data_end()
{
  int len = MAX_MSG_LEN+g_msg_header_len;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  uint32_t randid = xcli_get_randid();
  mdl_set_header_vals(msg,randid,msg_type_scp_data_end,fd_type_scp,0,0);
  msg->len = 0;
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void scp_send_get(int llid, int tid, int type,
                  t_sock_fd_tx soc_tx, char *src, char *dst)
{
  int len = MAX_MSG_LEN+g_msg_header_len;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  uint32_t randid = xcli_get_randid();
  g_write_seqnum = 1;
  g_sock_fd_tx = soc_tx;
  g_llid = llid;
  g_tid = llid;
  g_type = type;
  g_scp_fd = -1;
  g_mdl_src = 0;
  mdl_set_header_vals(msg,randid,msg_type_scp_open_rcv,fd_type_scp,0,0);
  msg->len = sprintf(msg->buf, "%s %s", src, dst) + 1;
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void scp_send_put(int llid, int tid, int type,
                  t_sock_fd_tx soc_tx, char *src, char *dst)
{
  int len = MAX_MSG_LEN+g_msg_header_len;
  t_msg *msg = (t_msg *) wrap_malloc(len);
  uint32_t randid = xcli_get_randid();
  g_write_seqnum = 1;
  g_sock_fd_tx = soc_tx;
  g_llid = llid;
  g_tid = llid;
  g_type = type;
  g_scp_fd = -1;
  g_mdl_src = 0;
  mdl_set_header_vals(msg,randid,msg_type_scp_open_snd,fd_type_scp,0,0);
  msg->len = sprintf(msg->buf, "%s %s", src, dst) + 1;
  mdl_prepare_header_msg(get_write_seqnum(), msg);
  len = msg->len + g_msg_header_len;
  g_sock_fd_tx(g_llid, g_tid, g_type, len, (char *) msg);
}
/*--------------------------------------------------------------------------*/
