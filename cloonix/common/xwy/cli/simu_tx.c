/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
/*                                                                           */
/*  This stxogram is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This stxogram is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this stxogram.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "mdl.h"
#include "wrap.h"
#include "io_clownix.h"
#include "doorways_sock.h"
#include "epoll_hooks.h"
#include "simu_tx.h"

/*****************************************************************************/
typedef struct t_simu_tx_el
{
  int fd;
  char *buf;
  int payload;
  int offset;
  struct t_simu_tx_el *next;
} t_simu_tx_el;
/*--------------------------------------------------------------------------*/
typedef struct t_simu_tx
{
  int llid;
  int sock_fd;
  struct epoll_event *sock_fd_epev;
  int nb_els;
  struct t_simu_tx_el *first;
  struct t_simu_tx_el *last;
} t_simu_tx;
/*--------------------------------------------------------------------------*/
static t_simu_tx *g_simu_tx[CLOWNIX_MAX_CHANNELS];
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static void free_first_el(t_simu_tx *stx)
{
  t_simu_tx_el *el = stx->first;
  stx->first = el->next;
  if (stx->last == el)
    stx->last = NULL;
  wrap_free(el->buf, __LINE__);
  wrap_free(el, __LINE__);
  stx->nb_els -= 1;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int write_first_el(t_simu_tx *stx)
{
  int result = -1;
  t_simu_tx_el *el = stx->first;
  ssize_t len;
  if (el)
    {
    len = write(el->fd, el->buf+el->offset, el->payload-el->offset);
    if (len < 0)
      {
      if (errno != EINTR && errno != EAGAIN)
        {
        KERR("%d %s", el->fd, strerror(errno));
        result = -2;
        }
      }
    else if (len == 0)
      {
      KERR("WRITE CLOSED DIALOG %d", el->fd);
      result = -2;
      }
    else
      {
      el->offset += len;
      if (el->offset == el->payload)
        result = 0;
      else
        KERR("DIALOG WRITE %d %d %d", el->fd, len, el->payload-el->offset);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_simu_tx_el *alloc_el(int fd, int len, char *buf)
{
  t_simu_tx_el *el = (t_simu_tx_el *) wrap_malloc(sizeof(t_simu_tx_el));
  el->buf = (char*) wrap_malloc(len);
  memcpy(el->buf, buf, len);
  el->fd        = fd;
  el->payload   = len;
  el->offset    = 0;
  el->next      = NULL;
  return el;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void chain_el(t_simu_tx *stx, t_simu_tx_el *el)
{
  if (stx->first)
    {
    if (!stx->last)
      KOUT(" ");
    stx->last->next = el;
    stx->last = el;
    }
  else
    {
    if (stx->last)
      KOUT(" ");
    stx->first = el;
    stx->last = el;
    }
  stx->nb_els += 1;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void chain_buf(t_simu_tx *stx, int len, char *buf)
{
  t_simu_tx_el *el;
  el = alloc_el(stx->sock_fd, len, buf); 
  chain_el(stx, el);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int simu_tx_queue_non_empty(t_simu_tx *stx)
{
  int result = 0;
  if (stx->first)
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int simu_tx_ready(t_simu_tx *stx)
{
  int result;
  while(1)
    {
    result = write_first_el(stx);
    if (result != 0)
      break;
    free_first_el(stx);
    }
  if (result == -2)
    result = -1;
  else
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void simu_tx_fdset(int llid)
{
  t_simu_tx *stx;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("LLID OUT OF RANGE %d", llid);
  if (!g_simu_tx[llid - 0])
    KOUT("LLID NOT OPEN %d", llid);
  stx = g_simu_tx[llid - 0];
  if (simu_tx_queue_non_empty(stx))
    stx->sock_fd_epev->events |= EPOLLOUT;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void simu_tx_fdiset(int llid)
{
  t_simu_tx *stx;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("LLID OUT OF RANGE %d", llid);
  if (!g_simu_tx[llid - 0])
    KOUT("LLID NOT OPEN %d", llid);
  stx = g_simu_tx[llid - 0];
  if (simu_tx_ready(stx))
    KOUT("%d", llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void simu_tx_send(int llid, int len, char *buf)
{
  t_simu_tx *stx;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("LLID OUT OF RANGE %d", llid);
  if (!g_simu_tx[llid - 0])
    KOUT("LLID NOT OPEN %d", llid);
  stx = g_simu_tx[llid - 0];
  chain_buf(stx, len, buf);
  if (simu_tx_ready(stx))
    KOUT("%d", llid);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
int simu_tx_open(int llid, int sock_fd, struct epoll_event *sock_fd_epev)
{
  int result = -1;
  t_simu_tx *stx;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("LLID OUT OF RANGE %d", llid);
  if (g_simu_tx[llid - 0])
    KOUT("LLID EXISTS %d", llid);
  else
    {
    stx = (t_simu_tx *) wrap_malloc(sizeof(t_simu_tx));
    memset(stx, 0, sizeof(t_simu_tx));
    stx->llid = llid;
    stx->sock_fd = sock_fd;
    stx->sock_fd_epev = sock_fd_epev;
    g_simu_tx[llid - 0] = stx;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void simu_tx_close(int llid)
{
  t_simu_tx *stx;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("LLID OUT OF RANGE %d", llid);
  if (!g_simu_tx[llid - 0])
    KOUT("LLID DOES NOT EXISTS %d", llid);
  else
    {
    stx = g_simu_tx[llid - 0];
    g_simu_tx[llid - 0] = NULL;
    if (stx->llid != llid)
      KOUT("LLID INCOHERENT %d %d", stx->llid, llid);
    if (stx->first)
      KERR("CLOSE PURGE DATA %d %d %d el", llid, stx->sock_fd, stx->nb_els);
    while(stx->first)
      free_first_el(stx);
    wrap_free(stx, __LINE__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void simu_tx_init(void)
{
  memset(g_simu_tx, 0, CLOWNIX_MAX_CHANNELS * sizeof(t_simu_tx *));
}
/*--------------------------------------------------------------------------*/

