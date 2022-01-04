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
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include "mdl.h"
#include "low_write.h"
#include "wrap.h"
#include "debug.h"
#include "thread_tx.h"
#include "dialog_thread.h"


/*****************************************************************************/
typedef struct t_lw
{
  int fd_dst;
  int type;
  t_outflow outflow;
  struct t_lw_el *first;
  struct t_lw_el *last;
} t_lw;


static t_lw *g_lw[MAX_FD_NUM];

/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int low_write_first_el(t_lw *lw)
{
  int result = -1;
  t_lw_el *el = lw->first;
  ssize_t len;
  if (el)
    {  
    len = el->outflow(lw->fd_dst, el->data+el->offst, el->payload-el->offst);
    if (len < 0)
      {
      if (errno != EINTR && errno != EAGAIN)
        {
        KERR("%d %s", lw->fd_dst, strerror(errno));
        result = -2;
        }
      else
        result = -1;
      }
    else if (len == 0)
      {
      KERR("%d", lw->fd_dst);
      result = -2;
      }
    else
      {
      el->offst += len;
      if (el->offst == el->payload) 
        result = 0;
      else
        DEBUG_EVT("LOW WRITE NOT COMPLETE %d %d %d", lw->fd_dst, len, 
                                                     el->payload-el->offst);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void chain_el(t_lw *lw, t_lw_el *el)
{
  if (lw->first)
    {
    if (!lw->last)
      KOUT(" ");
    lw->last->next = el;
    lw->last = el;
    }
  else
    {
    if (lw->last)
      KOUT(" ");
    lw->first = el;
    lw->last = el;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_first_el(t_lw *lw)
{
  t_lw_el *el = lw->first;
  lw->first = el->next;
  if (lw->last == el)
    lw->last = NULL;
  wrap_free(el->msg_to_free, __LINE__);
  wrap_free(el, __LINE__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_el_chain(t_lw *lw)
{
  while (lw->first)
    {
    free_first_el(lw);
    }
  wrap_free(lw, __LINE__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
t_lw_el *low_write_alloc_el(t_msg *msg, char *data, int fd_dst,
                            int payload, t_outflow outflow)
{
  t_lw_el *el = (t_lw_el *) wrap_malloc(sizeof(t_lw_el));
  memset(el, 0, sizeof(t_lw_el));
  el->fd_dst      = fd_dst;
  el->msg_to_free = msg;
  el->data        = data;
  el->payload     = payload;
  el->offst       = 0;
  el->outflow     = outflow;
  return el;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int low_write_fd(int s)
{
  int result, used;
  t_lw *lw;
  if ((s < 0) || (s >= MAX_FD_NUM))
    KOUT("%d", s);
  lw = g_lw[s];
  if (!lw)
    KOUT(" ");
  if (lw->fd_dst != s)
    KOUT(" ");
  if ((lw->type == fd_type_srv) || (lw->type == fd_type_cli)) 
    KOUT("%d", lw->type);
  while(1)
    {
    DEBUG_IOCTL_TX_QUEUE(s, lw->type, used);
    result = low_write_first_el(lw);
    if (result == 0)
      free_first_el(lw);
    else 
      break;
    }
  if (result == -2)
    result = -1;
  else
    result = 0;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int low_write_not_empty(int s)
{
  int used, result = 0;
  t_lw *lw;
  if ((s < 0) || (s >= MAX_FD_NUM))
    KOUT("%d", s);
  lw = g_lw[s];
  if (!lw)
    KOUT(" ");
  if (lw->fd_dst != s)
    KOUT(" ");
  if ((lw->type == fd_type_srv) || (lw->type == fd_type_cli)) 
    KOUT("%d", lw->type);
  if (lw->first)
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int low_write_raw(int fd_dst, t_msg *msg, int all)
{
  int payload, used_slot_nb, stored_bytes, result = 0;
  char *ptr;
  t_lw *lw;
  t_lw_el *el;
  if ((fd_dst < 0) || (fd_dst >= MAX_FD_NUM))
    KOUT("%d", fd_dst);
  if (!msg)
    KOUT(" ");
  lw = g_lw[fd_dst];
  if (!lw)
    {
    KERR("%d", fd_dst);
    result = -1;
    }
  else
    {
    if (lw->fd_dst != fd_dst)
      KOUT("%d %d", lw->fd_dst, fd_dst);
    if (all)
      {
      payload = msg->len + g_msg_header_len;
      ptr = (char *) msg;
      }
    else
      {
      payload = msg->len;
      ptr = (char *) msg->buf;
      }
    el = low_write_alloc_el(msg, (char *) ptr, fd_dst, payload, lw->outflow);
    if (lw->type == fd_type_srv)
      {
      DEBUG_DUMP_ENQUEUE(fd_dst, msg, all, 1);
      result = thread_tx_add_el(el, payload);
      if (result == 0)
        {
        if (thread_tx_get_levels(fd_dst, &used_slot_nb, &stored_bytes))
          result = -1;
        }
      DEBUG_DUMP_ENQUEUE_LEVELS(fd_dst, used_slot_nb, stored_bytes);
      }
    else
      {
      DEBUG_DUMP_ENQUEUE(fd_dst, msg, all, 0);
      chain_el(lw, el);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void low_write_open(int s, int type, t_outflow outflow)
{
  int pipe_fd[2];
  if ((s < 0) || (s >= MAX_FD_NUM))
    KOUT("%d", s);
  if (g_lw[s])
    KOUT(" ");
  if ((type <= fd_type_min) || (type >= fd_type_max)) 
    KOUT("%d", type);
  if (type == fd_type_cli)
    KOUT("%d", type);
  g_lw[s] = (t_lw *) wrap_malloc(sizeof(t_lw));
  memset(g_lw[s], 0, sizeof(t_lw));
  g_lw[s]->fd_dst = s;
  g_lw[s]->type = type;
  g_lw[s]->outflow = outflow;
  if (type == fd_type_srv)
    thread_tx_open(s);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void low_write_modify(int s, int type)
{
  t_lw *lw;
  if ((s < 0) || (s >= MAX_FD_NUM))
    KOUT("%d", s);
  lw = g_lw[s];
  if (!lw)
    KOUT(" ");
  if (lw->fd_dst != s)
    KOUT(" ");
  if (lw->type != fd_type_srv)
    KOUT("%d", lw->type);
  if (type != fd_type_srv_ass)
    KOUT("%d", lw->type);
  lw->type = type;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void low_write_close(int s)
{
  t_lw *lw;
  if ((s < 0) || (s >= MAX_FD_NUM))
    KOUT("%d", s);
  lw = g_lw[s];
  if (!lw)
    KOUT(" ");
  if (lw->fd_dst != s)
    KOUT(" ");
  if (lw->type == fd_type_cli)
    KOUT("%d", lw->type);
  if ((lw->type == fd_type_srv) || 
      (lw->type == fd_type_srv_ass)) 
    thread_tx_close(s);
  else
    {
    if (low_write_not_empty(s))
      {
      if (low_write_fd(s))
        KERR(" ");
      if (low_write_not_empty(s))
        {
        KERR("CLOSE ON NOT EMPTY WITH LOSS %d", s);
        DEBUG_EVT("CLOSE ON NOT EMPTY WITH LOSS %d", s);
        }
      }
    }
  free_el_chain(lw);
  g_lw[s] = NULL;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int low_write_levels_above_thresholds(int fd_dst)
{
  return thread_tx_levels_above_thresholds(fd_dst);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void low_write_init(void)
{
  int i;
  memset(g_lw, 0, MAX_FD_NUM * sizeof(t_lw *)); 
}
/*--------------------------------------------------------------------------*/

