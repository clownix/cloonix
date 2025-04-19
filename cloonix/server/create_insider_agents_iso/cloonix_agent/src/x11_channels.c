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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <asm/types.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <sys/time.h>

#include "glob_common.h"
#include "sock.h"
#include "commun.h"
#include "nonblock.h"

static int pool_fifo_free_index[X11_DISPLAY_IDX_MAX];
static int pool_read_idx;
static int pool_write_idx;

static int pool_x11_fifo_free_index[X11_DISPLAY_IDX_MAX];
static int pool_x11_read_idx;
static int pool_x11_write_idx;


char *get_g_buf(void);

typedef struct t_fd_x11_ctx
{
  int sub_dido_idx;
  int display_sock_x11;
  int dido_llid;
  int fd;
  int ready;
  struct t_fd_x11_ctx *prev;
  struct t_fd_x11_ctx *next;
} t_fd_x11_ctx;


static t_fd_x11_ctx g_fd_x11_ctx[X11_DISPLAY_IDX_MAX+1];
static t_fd_x11_ctx *g_head_fd_x11_ctx;

/*****************************************************************************/
static void pool_init(void)
{
  int i;
  for(i = 0; i < X11_DISPLAY_IDX_MAX; i++)
    pool_fifo_free_index[i] = i+1;
  pool_read_idx = 0;
  pool_write_idx =  X11_DISPLAY_IDX_MAX - 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int pool_alloc(void)
{
  int count, idx = 0;
  if(pool_read_idx != pool_write_idx)
    {
    idx = pool_fifo_free_index[pool_read_idx];
    pool_read_idx = (pool_read_idx + 1) % X11_DISPLAY_IDX_MAX;
    }
  if (pool_write_idx > pool_read_idx)
    count = pool_write_idx - pool_read_idx;
  else
    count = X11_DISPLAY_IDX_MAX + pool_write_idx - pool_read_idx;
  if (count == X11_DISPLAY_IDX_MAX/2)
    KERR("pool_alloc WARNING 1 INCREASE X11_DISPLAY_IDX_MAX");
  if (count ==  X11_DISPLAY_IDX_MAX / 3)
    KERR("pool_alloc WARNING 2 INCREASE X11_DISPLAY_IDX_MAX");
  if (count ==  X11_DISPLAY_IDX_MAX / 4)
    KERR("pool_alloc WARNING 3 INCREASE X11_DISPLAY_IDX_MAX");
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void pool_release(int idx)
{
  pool_fifo_free_index[pool_write_idx] =  idx;
  pool_write_idx=(pool_write_idx + 1)%X11_DISPLAY_IDX_MAX;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void x11_pool_init(void)
{
  int i;
  for(i = 0; i < X11_DISPLAY_IDX_MAX; i++)
    pool_x11_fifo_free_index[i] = i+1;
  pool_x11_read_idx = 0;
  pool_x11_write_idx =  X11_DISPLAY_IDX_MAX - 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int x11_pool_alloc(void)
{
  int count, idx = 0;
  if(pool_x11_read_idx != pool_x11_write_idx)
    {
    idx = pool_x11_fifo_free_index[pool_x11_read_idx];
    pool_x11_read_idx = (pool_x11_read_idx + 1)%X11_DISPLAY_IDX_MAX;
    }
  if (pool_x11_write_idx > pool_x11_read_idx)
    count = pool_x11_write_idx - pool_x11_read_idx;
  else
    count = X11_DISPLAY_IDX_MAX+pool_x11_write_idx - pool_x11_read_idx;
  if (count == X11_DISPLAY_IDX_MAX/2)
    KERR("WARNING 1 INCREASE X11_DISPLAY_IDX_MAX");
  if (count ==  X11_DISPLAY_IDX_MAX / 3)
    KERR("WARNING 2 INCREASE X11_DISPLAY_IDX_MAX");
  if (count ==  X11_DISPLAY_IDX_MAX / 4)
    KERR("WARNING 3 INCREASE X11_DISPLAY_IDX_MAX");
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void x11_pool_release(int idx)
{
  pool_x11_fifo_free_index[pool_x11_write_idx] =  idx;
  pool_x11_write_idx=(pool_x11_write_idx + 1)%X11_DISPLAY_IDX_MAX;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void send_close_x11_to_doors(int sub_dido_idx)
{
  t_fd_x11_ctx *ctx;
  int headsize = sock_header_get_size();
  char *g_buf = get_g_buf();
  char *buf = g_buf + headsize;
  memset(g_buf, 0, headsize);
  ctx = &(g_fd_x11_ctx[sub_dido_idx]);
  if (ctx->sub_dido_idx != sub_dido_idx)
    KOUT("ERROR %d %d", ctx->sub_dido_idx, sub_dido_idx);
  snprintf(buf, MAX_A2D_LEN-1, LAX11OPENKO, sub_dido_idx);
  send_to_virtio(ctx->dido_llid, strlen(buf) + 1, header_type_x11_ctrl,
                 header_val_x11_open_serv, g_buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_get_biggest_fd(void)
{
  int result = 0;
  t_fd_x11_ctx *cur = g_head_fd_x11_ctx;
  while(cur)
    {
    if (cur->fd > result)
      result = cur->fd;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int alloc_x11_fd(int dido_llid, int fd, int display_sock_x11)
{
  int result = pool_alloc();
  if (result)
    {
    g_fd_x11_ctx[result].sub_dido_idx = result;
    g_fd_x11_ctx[result].dido_llid = dido_llid;
    g_fd_x11_ctx[result].fd = fd;
    nonblock_add_fd(fd);
    g_fd_x11_ctx[result].display_sock_x11 = display_sock_x11;
    if (g_head_fd_x11_ctx)
      g_head_fd_x11_ctx->prev = &(g_fd_x11_ctx[result]);
    g_fd_x11_ctx[result].next = g_head_fd_x11_ctx;
    g_head_fd_x11_ctx = &(g_fd_x11_ctx[result]);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_fd_ctx(int sub_dido_idx)
{
  if (g_fd_x11_ctx[sub_dido_idx].sub_dido_idx)
    {
    nonblock_del_fd(g_fd_x11_ctx[sub_dido_idx].fd);
    close(g_fd_x11_ctx[sub_dido_idx].fd);
    send_close_x11_to_doors(sub_dido_idx);
    if (g_fd_x11_ctx[sub_dido_idx].prev)
      g_fd_x11_ctx[sub_dido_idx].prev->next = g_fd_x11_ctx[sub_dido_idx].next;
    if (g_fd_x11_ctx[sub_dido_idx].next)
      g_fd_x11_ctx[sub_dido_idx].next->prev = g_fd_x11_ctx[sub_dido_idx].prev;
    if (g_head_fd_x11_ctx == &(g_fd_x11_ctx[sub_dido_idx]))
      g_head_fd_x11_ctx = g_fd_x11_ctx[sub_dido_idx].next;
    memset(&(g_fd_x11_ctx[sub_dido_idx]), 0, sizeof(t_fd_x11_ctx));
    pool_release(sub_dido_idx);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_idx_with_fd(int fd)
{
  int result = 0;
  t_fd_x11_ctx *cur = g_head_fd_x11_ctx;
  while(cur)
    {
    if (cur->fd == fd)
      {
      result = cur->sub_dido_idx;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_write(int sub_dido_idx, int len, char *buf)
{
  t_fd_x11_ctx *ctx = &(g_fd_x11_ctx[sub_dido_idx]);
  nonblock_send(ctx->fd, buf, len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void x11_read_evt(int fd)
{
  t_fd_x11_ctx *ctx;
  int headsize = sock_header_get_size();
  char *g_buf = get_g_buf();
  char *buf = g_buf + headsize;
  int err, val, sub_dido_idx, len = 1, first_loop = 1;
  sub_dido_idx = get_idx_with_fd(fd);
  if (!sub_dido_idx)
    KERR("ERROR %dd", fd);
  else
    {
    ctx = &(g_fd_x11_ctx[sub_dido_idx]);
    if (ctx->sub_dido_idx != sub_dido_idx)
      KOUT("ERROR %d %d", ctx->sub_dido_idx, sub_dido_idx);
    if (ctx->fd != fd)
      KOUT("ERROR %d %d", ctx->fd, fd);
    err = ioctl(fd, SIOCINQ, &val);
    if ((err != 0) || (val<0))
      {
      val = 0;
      KERR("ERROR %d", errno);
      free_fd_ctx(sub_dido_idx);
      }
    else
      {
      memset(g_buf, 0, headsize);
      len = read (fd, buf, MAX_A2D_LEN-headsize);
      if ((len == 0) && (first_loop == 1))
        {
        free_fd_ctx(sub_dido_idx);
        }
      else if (len < 0)
        {
        len = 0;
        if ((errno != EAGAIN) && (errno != EINTR))
          {
          KERR("ERROR %d", errno);
          free_fd_ctx(sub_dido_idx);
          }
        }
      else if (len > 0)
        {
        first_loop = 0;
        send_to_virtio(ctx->dido_llid,len,header_type_x11,sub_dido_idx,g_buf);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_event_listen(int dido_llid, int display_sock_x11, int fd_x11_listen)
{
  int fd, sub_dido_idx;
  int headsize = sock_header_get_size();
  char *g_buf = get_g_buf();
  char *buf = g_buf + headsize;
  memset(g_buf, 0, headsize);
  fd = sock_fd_accept(fd_x11_listen);
  if (fd <= 0)
    KERR("ERROR %d %d %d %d", dido_llid, display_sock_x11, fd_x11_listen, errno);
  else
    {
    sub_dido_idx = alloc_x11_fd(dido_llid, fd, display_sock_x11);
    if (!sub_dido_idx)
      {
      close(fd);
      KERR("ERROR %d %d %d", dido_llid, display_sock_x11, fd_x11_listen);
      }
    else
      {
      snprintf(buf, MAX_A2D_LEN-1, LAX11OPEN, sub_dido_idx, display_sock_x11);
      send_to_virtio(dido_llid, strlen(buf) + 1, header_type_x11_ctrl, 
                     header_val_x11_open_serv, g_buf);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_process_events(fd_set *infd)
{
  t_fd_x11_ctx *cur = g_head_fd_x11_ctx;
  while(cur)
    {
    if (FD_ISSET(cur->fd, infd))
      {
      x11_read_evt(cur->fd);
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_prepare_fd_set(fd_set *infd, fd_set *outfd)
{
 t_fd_x11_ctx *cur = g_head_fd_x11_ctx;
  while(cur)
    {
    if (cur->ready)
      {
      FD_SET(cur->fd, infd); 
      nonblock_prepare_fd_set(cur->fd, outfd);
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_rx_ack_open_x11(int dido_llid, char *rx)
{
  int sub_dido_idx;
  if (sscanf(rx, LAX11OPENOK, &sub_dido_idx) == 1)
    {
    if ((sub_dido_idx < 1) || (sub_dido_idx > X11_DISPLAY_IDX_MAX))
      KERR("ERROR %d", sub_dido_idx);
    else if (g_fd_x11_ctx[sub_dido_idx].fd <= 0)
      KERR("ERROR %d", sub_dido_idx);
    else 
      {
      if (!g_fd_x11_ctx[sub_dido_idx].dido_llid)
        KOUT("ERROR");
      if (g_fd_x11_ctx[sub_dido_idx].dido_llid != dido_llid)
        KOUT("ERROR %d %d ", g_fd_x11_ctx[sub_dido_idx].dido_llid, dido_llid);
      g_fd_x11_ctx[sub_dido_idx].ready = 1;
      }
    }
  else if (sscanf(rx, LAX11OPENKO, &sub_dido_idx) == 1)
    {
    free_fd_ctx(sub_dido_idx);
    }
  else 
    KERR("ERROR");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_init(void)
{
  memset(g_fd_x11_ctx, 0, (X11_DISPLAY_IDX_MAX+1) * sizeof(t_fd_x11_ctx));
  pool_init();
  x11_pool_init();
}
/*--------------------------------------------------------------------------*/


