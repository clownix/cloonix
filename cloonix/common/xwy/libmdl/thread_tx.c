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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <signal.h>

#include "mdl.h"
#include "wrap.h"
#include "debug.h"
#include "circle.h"
#include "low_write.h"
#include "thread_tx.h"
#include "thread_spy.h"

/*****************************************************************************/
typedef struct t_tx
{
  int dst_fd;
  int thread_tx_on;
  int thread_waiting;
  int thread_abort;
  int count;
  pthread_t thread_tx;
  t_lw_el *current_el;
  t_circ_ctx *circ_ctx;
} t_tx;
static t_tx *g_tx[MAX_FD_NUM];
/*--------------------------------------------------------------------------*/
 
/*****************************************************************************/
static int try_send_el_successfull(t_lw_el *el)
{
  int result = 0;
  ssize_t len = el->outflow(el->fd_dst, el->data + el->offst,
                el->payload - el->offst);
  if (len < 0)
    {
    if (errno != EINTR && errno != EAGAIN)
      {
      KERR("%s", strerror(errno));
      result = -1;
      }
    }
  else if (len == 0)
    {
    KERR(" ");
    result = -1;
    }
  else
    {
    el->offst += len;
    if (el->offst == el->payload)
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void purge_el_to_free(t_tx *tx)
{
  t_lw_el *el;
  el = (t_lw_el *) circ_free_slot_get(tx->circ_ctx);
  while(el)
    {
    wrap_free(el->msg_to_free, __LINE__);
    wrap_free(el, __LINE__);
    el = (t_lw_el *) circ_free_slot_get(tx->circ_ctx);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int transmit_circ_tx(int sock_fd)
{
  int result = 0;
  t_lw_el *el;
  t_tx *tx;
  if ((sock_fd < 0) || (sock_fd >= MAX_FD_NUM))
    KOUT("%d", sock_fd);
  else if (!g_tx[sock_fd])
    KOUT("%d", sock_fd);
  else
    {
    tx = g_tx[sock_fd];
    if (tx->current_el == NULL)
      {
      tx->current_el = (t_lw_el *) circ_tx_slot_get(tx->circ_ctx);
      tx->count = 0;
      }
    if (tx->current_el)
      {
      tx->count += 1;
      if (tx->count > 50000)
        {
        tx->thread_tx_on = 0;
        KERR("%d %d", tx->current_el->offst, tx->current_el->payload);
        result = -1;
        }
      else
        {
        el = tx->current_el;
        result = try_send_el_successfull(el);
        if (result == 1)
          {
          circ_free_slot_put(tx->circ_ctx, el);
          tx->current_el = NULL;
          result = 0;
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cli_warn(int sig)
{
  KERR("%d", sig);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void *thread_tx(void *arg)
{
  t_tx *tx = (t_tx *) arg;
  if (!tx)
    KOUT(" ");
  thread_spy_add(tx->dst_fd, -1, -1, thread_type_tx);
  if (signal(SIGPIPE, cli_warn) == SIG_ERR)
    KERR("%s", strerror(errno));

  while(tx->thread_waiting)
    usleep(10000);

  if (tx->thread_abort)
    tx->thread_tx_on = 0;

  while (tx->thread_tx_on)
    {
    usleep(10000);
    while(circ_tx_used_slot_nb(tx->circ_ctx))
      {
      if (transmit_circ_tx(tx->dst_fd))
        {
        KERR(" ");
        tx->thread_tx_on = 0;
        break;
        }
      }
    }

  thread_spy_del(tx->dst_fd, thread_type_tx);
  pthread_exit(NULL);

  return NULL;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int thread_tx_add_el(t_lw_el *el, int len)
{
  int result = -1;
  t_tx *tx;
  if ((el->fd_dst < 0) || (el->fd_dst >= MAX_FD_NUM))
    KOUT("%d", el->fd_dst);
  if (!(g_tx[el->fd_dst]))
    KERR("%d", el->fd_dst);
  else
    {
    tx = g_tx[el->fd_dst];
    circ_tx_slot_put(tx->circ_ctx, (void *) el, len);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int thread_tx_get_levels(int fd_dst, int *used_slot_nb, int *stored_bytes)
{
  int result = -1;
  t_tx *tx;
  if ((fd_dst < 0) || (fd_dst >= MAX_FD_NUM))
    KOUT("%d", fd_dst);
  if (!(g_tx[fd_dst]))
    KERR("%d", fd_dst);
  else
    {
    tx = g_tx[fd_dst];
    *used_slot_nb = circ_tx_used_slot_nb(tx->circ_ctx);
    *stored_bytes = circ_tx_stored_bytes_nb(tx->circ_ctx);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int thread_tx_levels_above_thresholds(int fd_dst)
{
  int used_slot_nb, stored_bytes, result;
  result = thread_tx_get_levels(fd_dst, &used_slot_nb, &stored_bytes);
  if (result == 0)
    {
    if ((used_slot_nb > MAX_MSG_LEN/20) || (stored_bytes >  MAX_MSG_LEN))
      result = 1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void thread_tx_open(int dst_fd)
{
  t_tx *tx;
  if ((dst_fd < 0) || (dst_fd >= MAX_FD_NUM))
    KOUT("%d", dst_fd);
  if (g_tx[dst_fd])
    KOUT("%d", dst_fd);
  tx = (t_tx *) wrap_malloc(sizeof(t_tx));
  memset(tx, 0, sizeof(t_tx));
  tx->circ_ctx = (t_circ_ctx *) wrap_malloc(sizeof(t_circ_ctx));
  memset(tx->circ_ctx, 0, sizeof(t_circ_ctx));
  circ_slot_init(tx->circ_ctx);
  tx->dst_fd = dst_fd;
  tx->current_el = NULL;
  tx->thread_waiting = 1;
  tx->thread_abort = 1;
  tx->thread_tx_on = 1;
  g_tx[dst_fd] = tx;
  if (pthread_create(&tx->thread_tx, NULL, thread_tx, (void *) tx) != 0)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void thread_tx_close(int dst_fd)
{ 
  t_tx *tx;
  if (dst_fd == -1)
    KERR(" ");
  else
    {
    if ((dst_fd < 0) || (dst_fd >= MAX_FD_NUM))
      KOUT("%d", dst_fd);
    tx = g_tx[dst_fd];
    if (tx)
      {
      tx->thread_waiting = 0;
      tx->thread_tx_on = 0;
      if (pthread_join(tx->thread_tx, NULL))
        KERR(" ");
      g_tx[dst_fd] = NULL;
      wrap_free(tx, __LINE__);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void thread_tx_start(int dst_fd)
{ 
    t_tx *tx;
  if ((dst_fd < 0) || (dst_fd >= MAX_FD_NUM))
    KOUT("%d", dst_fd);
  if (!(g_tx[dst_fd]))
    KOUT("%d", dst_fd);
  tx = g_tx[dst_fd];
  tx->thread_abort = 0;
  tx->thread_waiting = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void thread_tx_abort(int dst_fd)
{
    t_tx *tx;
  if ((dst_fd < 0) || (dst_fd >= MAX_FD_NUM))
    KOUT("%d", dst_fd);
  if (!(g_tx[dst_fd]))
    KOUT("%d", dst_fd);
  tx = g_tx[dst_fd];
  tx->thread_waiting = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void thread_tx_purge_el(int dst_fd)
{
    t_tx *tx;
  if ((dst_fd < 0) || (dst_fd >= MAX_FD_NUM))
    KOUT("%d", dst_fd);
  if (!(g_tx[dst_fd]))
    KOUT("%d", dst_fd);
  tx = g_tx[dst_fd];
  purge_el_to_free(tx);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void thread_tx_init(void)
{
  memset(g_tx, 0, sizeof(t_tx *) * MAX_FD_NUM);
}
/*---------------------------------------------------------------------------*/

