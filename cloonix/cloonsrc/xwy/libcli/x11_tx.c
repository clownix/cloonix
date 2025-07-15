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
#include <sys/types.h>

#include "mdl.h"
#include "wrap.h"
#include "glob_common.h"


/*****************************************************************************/

typedef struct t_x11_tx_el
{
  int fd;
  char *buf;
  int payload;
  int offset;
  struct t_x11_tx_el *next;
} t_x11_tx_el;
/*--------------------------------------------------------------------------*/

typedef struct t_x11_tx
{
  int fd;
  int nb_els;
  struct t_x11_tx_el *first;
  struct t_x11_tx_el *last;
} t_x11_tx;
/*--------------------------------------------------------------------------*/

static t_x11_tx *g_x11_tx[MAX_FD_NUM];
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_first_el(t_x11_tx *pr)
{
  t_x11_tx_el *el = pr->first;
  pr->first = el->next;
  if (pr->last == el)
    pr->last = NULL;
  wrap_free(el->buf, __LINE__);
  wrap_free(el, __LINE__);
  pr->nb_els -= 1;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int write_first_el(t_x11_tx *pr)
{
  int result = -1;
  t_x11_tx_el *el = pr->first;
  ssize_t len;
  if (el)
    {
    len = write(pr->fd, el->buf + el->offset, el->payload - el->offset);
    if (len < 0)
      {
      if (errno != EINTR && errno != EAGAIN)
        {
        KERR("%d %s", pr->fd, strerror(errno));
        result = -2;
        }
      }
    else if (len == 0)
      {
      KERR("WRITE CLOSED DIALOG %d", pr->fd);
      result = -2;
      }
    else
      {
      el->offset += len;
      if (el->offset == el->payload)
        result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static t_x11_tx_el *alloc_el(int fd, int len, char *buf)
{
  t_x11_tx_el *el = (t_x11_tx_el *) wrap_malloc(sizeof(t_x11_tx_el));
  el->buf = (char *) wrap_malloc(len);
  memcpy(el->buf, buf, len);
  el->fd        = fd;
  el->payload   = len;
  el->offset    = 0;
  el->next      = NULL;
  return el;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void chain_el(t_x11_tx *pr, t_x11_tx_el *el)
{
  if (pr->first)
    {
    if (!pr->last)
      KOUT(" ");
    pr->last->next = el;
    pr->last = el;
    }
  else
    {
    if (pr->last)
      KOUT(" ");
    pr->first = el;
    pr->last = el;
    }
  pr->nb_els += 1;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void tx_add_queue(t_x11_tx *pr, int len, char *buf)
{
  t_x11_tx_el *el;
  el = alloc_el(pr->fd, len, buf); 
  chain_el(pr, el);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int x11_tx_add_queue(int fd, int len, char *buf)
{
  int result = -1;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    KOUT("DIALOG FD OUT OF RANGE %d", fd);
  if (!g_x11_tx[fd])
    KERR("DIALOG FD DOES NOT EXISTS %d", fd);
  else 
    {
    tx_add_queue(g_x11_tx[fd], len, buf);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_tx_queue_non_empty(int fd)
{
  int result = 0;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    KOUT("DIALOG FD OUT OF RANGE %d", fd);
  if (!g_x11_tx[fd])
    KERR("DIALOG FD DOES NOT EXISTS %d", fd);
  else if (g_x11_tx[fd]->first)
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_tx_ready(int fd)
{
  int result, used;
  t_x11_tx *pr;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    KOUT("DIALOG FD OUT OF RANGE %d", fd);
  pr = g_x11_tx[fd];
  if (!pr)
    KOUT("DIALOG FD DOES NOT EXIST %d", fd);
  if (pr->fd != fd)
    KOUT("DIALOG FD INCOHERENT %d %d", pr->fd, fd);
  while(1)
    {
    result = write_first_el(pr);
    if (result != 0)
      break;
    free_first_el(pr);
    }
  if (result == -2)
    result = -1;
  else
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_tx_open(int fd)
{
  int result = -1;
  t_x11_tx *pr;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    KOUT("DIALOG FD OUT OF RANGE %d", fd);
  if (g_x11_tx[fd])
    KERR("DIALOG FD EXISTS %d", fd);
  else
    {
    pr = (t_x11_tx *) wrap_malloc(sizeof(t_x11_tx));
    memset(pr, 0, sizeof(t_x11_tx));
    pr->fd = fd;
    g_x11_tx[fd] = pr;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_tx_close(int fd)
{
  t_x11_tx *pr;
  if ((fd < 0) || (fd >= MAX_FD_NUM))
    KOUT("DIALOG FD OUT OF RANGE %d", fd);
  if (!g_x11_tx[fd])
    KERR("DIALOG FD DOES NOT EXISTS %d", fd);
  else
    {
    pr = g_x11_tx[fd];
    g_x11_tx[fd] = NULL;
    if (pr->fd != fd)
      KOUT("DIALOG FD INCOHERENT %d %d", pr->fd, fd);
//    if (pr->first)
//      KERR("CLOSE PURGE LEFT DATA %d  %d el", fd, pr->nb_els);
    while(pr->first)
      free_first_el(pr);
    wrap_free(pr, __LINE__);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_tx_init(void)
{
  memset(g_x11_tx, 0, MAX_FD_NUM * sizeof(t_x11_tx *));
}
/*--------------------------------------------------------------------------*/

