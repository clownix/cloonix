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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <net/if.h>
#include <errno.h>
#include <linux/if_tun.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>

#include "ioc.h"
#include "clo_tcp.h"
#include "sock_fd.h"
#include "main.h"
#include "machine.h"
#include "utils.h"
#include "bootp_input.h"
#include "packets_io.h"
#include "llid_slirptux.h"
#include "tcp_tux.h"


typedef struct t_llid_fd
{
  int llid;
  int fd;
} t_llid_fd;


typedef struct t_connect_ctx
{
  t_connect cb;
  t_tcp_id tcpid; 
  int fd;
  int llid;
  int count;
  int end_ctx_call;
  long long timer_abs_beat;
  int timer_ref;
  struct sockaddr addr;
  int addr_len;
  struct t_connect_ctx *prev;
  struct t_connect_ctx *next;
} t_connect_ctx;

static t_connect_ctx *head_ctx = NULL;

/*****************************************************************************/
static t_connect_ctx *alloc_ctx(t_connect cb, t_tcp_id *tcpid, int fd,
	                        struct sockaddr *addr, int addr_len)

{
  t_connect_ctx *ctx;
  ctx = (t_connect_ctx *) malloc(sizeof(t_connect_ctx));
  memset(ctx, 0, sizeof(t_connect_ctx));
  memcpy(&(ctx->addr), addr, sizeof(struct sockaddr));
  ctx->addr_len = addr_len;
  ctx->cb = cb;
  memcpy(&(ctx->tcpid), tcpid, sizeof(t_tcp_id)); 
  ctx->fd = fd;
  if (head_ctx)
    head_ctx->prev = ctx;
  ctx->next = head_ctx;
  head_ctx = ctx;
  return ctx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void fatal_err_llid_slirptux(int llid, int reset)
{
  int is_blkd;
  if (llid_slirptux_tcp_close_llid(llid, reset))
    KERR("%d", llid);
  if (msg_exist_channel(get_all_ctx(), llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(get_all_ctx(), llid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_ctx(t_connect_ctx *ctx)
{
  if (ctx->prev)
    ctx->prev->next = ctx->next;
  if (ctx->next)
    ctx->next->prev = ctx->prev;
  if (ctx == head_ctx)
    head_ctx = ctx->next;
  free(ctx);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_connect_ctx *find_ctx(t_tcp_id *tcpid)
{
  t_connect_ctx *cur  = head_ctx;
  while (cur && util_tcpid_comp(&(cur->tcpid), tcpid))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void end_ctx(t_connect_ctx *ctx, int llid)
{
  if (ctx->end_ctx_call == 0)
    {
    ctx->end_ctx_call = 1;
    if (ctx->timer_abs_beat)
      clownix_timeout_del(get_all_ctx(), ctx->timer_abs_beat, ctx->timer_ref,
                          __FILE__, __LINE__);
    if (llid > 0)
      ctx->cb(&(ctx->tcpid), llid, 0);
    else
      ctx->cb(&(ctx->tcpid), 0, -1);
    if (ctx->llid == 0)
      close(ctx->fd);
    free_ctx(ctx);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void do_rx_from_out(t_clo *clo, int fd)
{
  int err, val, len;
  char *rx_buf = get_glob_rx_buf();
  err = ioctl(fd, SIOCINQ, &val);
  if ((err != 0) || (val<0))
    {
    KERR("SIOCINQ ERR: %d %X %X  %d  %d", val,
                      clo->tcpid.local_ip, clo->tcpid.remote_ip,
                      clo->tcpid.local_port & 0xFFFF,
                      clo->tcpid.remote_port & 0xFFFF);
    fatal_err_llid_slirptux(clo->tcpid.llid, 0);
    }
  else
    {
    len = read(fd, rx_buf, MAX_SLIRP_RX_PROCESS);
    if (len < 0)
      {
      if ((errno != EAGAIN) && (errno != EINTR))
        {
        KERR("READ ERR: %d %X %X  %d  %d", val,
                      clo->tcpid.local_ip, clo->tcpid.remote_ip,
                      clo->tcpid.local_port & 0xFFFF,
                      clo->tcpid.remote_port & 0xFFFF);
        fatal_err_llid_slirptux(clo->tcpid.llid, 0);
        }
      }
    else if (len == 0)
      {
      fatal_err_llid_slirptux(clo->tcpid.llid, 0);
      }
    else 
      llid_slirptux_tcp_rx_from_llid(clo->tcpid.llid, len, rx_buf);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timed_rx_from_out(long delta_ns, void *data)
{
  t_llid_fd *ptr_llid_fd = (t_llid_fd *) data;
  int is_blkd, llid = ptr_llid_fd->llid;
  int fd = ptr_llid_fd->fd;
  t_clo *clo = NULL;
  if (!msg_exist_channel(get_all_ctx(), llid, &is_blkd, __FUNCTION__))
    {
    }
  else if (!llid_slirptux_tcp_tx_to_slirptux_possible(llid))
    {
    channel_set_red_to_stop_reading(get_all_ctx(), llid);
    clownix_real_timer_add(0, 50000000, timed_rx_from_out, 
                             (void *) ptr_llid_fd, NULL);
    }
  else
    clo = util_get_clo(llid);
  if (clo)
    {
    if (clo->tcpid.llid != llid)
      KOUT(" ");
    if (clo->tcpid.llid_unlocked)
      {
      channel_unset_red_to_stop_reading(get_all_ctx(), llid);
      do_rx_from_out(clo, fd);
      free(data);
      }
    else
      {
      KERR("NOT READY YET");
      channel_set_red_to_stop_reading(get_all_ctx(), llid);
      clownix_real_timer_add(0, 50000000, timed_rx_from_out, 
                               (void *) ptr_llid_fd, NULL);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int tcp_rx_from_out (void *ptr, int llid, int fd)
{
  int is_blkd;
  t_llid_fd *ptr_llid_fd;
  t_clo *clo = util_get_clo(llid);
  if (clo)
    {
    ptr_llid_fd = (t_llid_fd *) malloc(sizeof(t_llid_fd));
    memset(ptr_llid_fd, 0, sizeof(t_llid_fd));
    ptr_llid_fd->llid = llid;
    ptr_llid_fd->fd = fd;
    timed_rx_from_out(0, (void *)ptr_llid_fd);
    }
  else
    {
    if (msg_exist_channel(get_all_ctx(), llid, &is_blkd, __FUNCTION__))
      msg_delete_channel(get_all_ctx(), llid);
    }
  return 0;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void tcp_err_from_out (void *ptr, int llid, int err, int from)
{
  KERR("%d %d %d", llid, err, from);
  fatal_err_llid_slirptux(llid, 1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int quick_select_ok(int fd)
{
  int result = 0;
  fd_set fdset;
  struct timeval tv;
  FD_ZERO(&fdset);
  FD_SET(fd, &fdset);
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  if (select(fd + 1, NULL, &fdset, NULL, &tv) == 1)
    {
    result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_connect_wait(t_all_ctx *all_ctx, void *data)
{
  t_connect_ctx *ctx = (t_connect_ctx *) data;
  t_connect_ctx *stctx;
  int so_error, llid = -1;
  socklen_t len = sizeof so_error;
  stctx = find_ctx(&(ctx->tcpid));
  if (!stctx)
    KOUT(" ");
  if (stctx != ctx)
    KOUT(" ");
  ctx->timer_abs_beat = 0;
  ctx->timer_ref = 0;
  ctx->count++;
  if (ctx->count > 40)
    {
    KERR(" FAIL CONNECT TO PORT %d", ctx->tcpid.local_port);
    end_ctx(ctx, -1);
    }
  else
    {
    if (!quick_select_ok(ctx->fd))
      {
      if ((ctx->count == 8) ||
          (ctx->count == 16) ||
          (ctx->count == 24))
        {
        connect(ctx->fd, &(ctx->addr), ctx->addr_len);
        }
      clownix_timeout_add(get_all_ctx(), 5, timer_connect_wait, 
                          data, &(ctx->timer_abs_beat), &(ctx->timer_ref));
      }
    else
      {
      if ((ctx->fd < 0) || (ctx->fd >= MAX_SELECT_CHANNELS-1))
        KOUT("%d", ctx->fd);
      getsockopt(ctx->fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
      if (so_error == 0) 
        {
        llid = msg_watch_fd(get_all_ctx(), ctx->fd, 
                            tcp_rx_from_out, tcp_err_from_out);
        if (llid <= 0)
          KOUT(" ");
        ctx->llid = llid;
        }
      end_ctx(ctx, llid);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void tcp_connect_wait_management(t_connect cb, t_tcp_id *tcpid, int fd,
                                 struct sockaddr *addr, int addr_len)
{
  t_connect_ctx *ctx;
  ctx = find_ctx(tcpid);
  if (ctx)
    {
    KERR(" !!!!!!! NOT EXPECTED REPEAT CONNECT TO PORT %d %08X", 
           ctx->tcpid.local_port, ctx->tcpid.local_ip);
    if (ctx->timer_abs_beat)
      clownix_timeout_del(get_all_ctx(), ctx->timer_abs_beat, ctx->timer_ref,
                          __FILE__, __LINE__);
    end_ctx(ctx, ctx->llid);
    ctx = alloc_ctx(cb, tcpid, fd, addr, addr_len);
    connect(fd, addr, addr_len);
    clownix_timeout_add(get_all_ctx(), 1, timer_connect_wait, (void *) ctx, 
                        &(ctx->timer_abs_beat), &(ctx->timer_ref));
    }
  else
    {
    ctx = alloc_ctx(cb, tcpid, fd, addr, addr_len);
    connect(fd, addr, addr_len);
    clownix_timeout_add(get_all_ctx(), 1, timer_connect_wait, (void *) ctx, 
                        &(ctx->timer_abs_beat), &(ctx->timer_ref));
    }
}
/*---------------------------------------------------------------------------*/

