/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
/****************************************************************************/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "commun.h"

#define MAX_FD 1000
/*--------------------------------------------------------------------------*/
typedef struct t_elem
{
  char *buf;
  int len;
  int len_done;
  struct t_elem *next;
} t_elem;
/*--------------------------------------------------------------------------*/
typedef struct t_fd_tx
{
  t_elem *first;
} t_fd_tx;
/*--------------------------------------------------------------------------*/
typedef struct t_fd_chain 
{
  int fd;
  struct t_fd_chain *prev;
  struct t_fd_chain *next;
} t_fd_chain;
/*--------------------------------------------------------------------------*/
static t_fd_tx *g_fd_tx[MAX_FD];
/*--------------------------------------------------------------------------*/
static t_fd_chain *g_head_fd_chain;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_fd_tx *get_fd_tx(int fd)
{
  if ((fd >= MAX_FD) || (fd < 0))
    KOUT("%d", fd);
  return (g_fd_tx[fd]);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_fd_chain *get_fd_chain(int fd)
{
  t_fd_chain *cur = g_head_fd_chain;
  while(cur)
    {
    if (cur->fd == fd)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    KOUT(" ");
  flags |= O_NONBLOCK|O_NDELAY;
  if (fcntl(fd, F_SETFL, flags) == -1)
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int do_write(int fd, t_elem *elem)
{
  int len, result = 0;
  char *buf = elem->buf + elem->len_done;
  int len_to_do = elem->len - elem->len_done;
  if (len_to_do < 0)
    KOUT(" ");
  len = write(fd, buf, len_to_do);
  if (len != len_to_do)
    {
    if (len > 0)
      elem->len_done += len;
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void do_free_first_elem(t_fd_tx *fd_tx)
{
  t_elem *elem = fd_tx->first;
  fd_tx->first = elem->next;
  free(elem->buf);
  free(elem);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void continue_writing(int fd)
{
  t_fd_tx *fd_tx = get_fd_tx(fd);
  if (fd_tx == NULL)
    KERR("%d", fd);
  else
    {
    if (!fd_tx->first)
      KERR("%d", fd);
    else
      {
      while (fd_tx->first)
        {
        if (do_write(fd, fd_tx->first))
          break;
        do_free_first_elem(fd_tx);
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nonblock_send(int fd, char *buf, int len)
{
  t_fd_tx *fd_tx = get_fd_tx(fd);
  t_elem *elem, *cur, *prev;
  if (fd_tx == NULL)
    KERR("%d %d", fd, len);
  else
    {
    elem = (t_elem *) malloc(sizeof(t_elem)); 
    elem->buf = (char *) malloc(len);
    memcpy(elem->buf, buf, len);
    elem->len = len;
    elem->len_done = 0;
    elem->next = NULL;
    if (fd_tx->first == NULL)
      fd_tx->first = elem;
    else
      {
      cur = fd_tx->first;
      while (cur)
        {
        prev = cur;
        cur = cur->next;
        }
      prev->next = elem;
      } 
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nonblock_prepare_fd_set(int fd, fd_set *outfd)
{
  t_fd_tx *fd_tx = get_fd_tx(fd);
  if ((fd_tx) && (fd_tx->first))
    {
    FD_SET(fd, outfd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nonblock_process_events(fd_set *outfd)
{
  t_fd_chain *cur = g_head_fd_chain;
  while(cur)
    {
    if (FD_ISSET(cur->fd, outfd))
      {
      continue_writing(cur->fd);
      }
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nonblock_del_fd(int fd)
{
  t_fd_tx *fd_tx = get_fd_tx(fd);
  t_fd_chain *fd_chain = get_fd_chain(fd);
  t_elem *cur, *next;
  if (fd_tx == NULL)
    KERR("%d", fd);
  else if (fd_chain == NULL)
    KERR("%d", fd);
  else
    {
    cur = fd_tx->first;
    while(cur)
      {
      next = cur->next;
      free(cur);
      cur = next;
      }
    if (fd_chain->prev)
      fd_chain->prev->next = fd_chain->next;
    if (fd_chain->next)
      fd_chain->next->prev = fd_chain->prev;
    if (fd_chain == g_head_fd_chain)
      g_head_fd_chain = fd_chain->next;
    free(fd_tx);
    free(fd_chain);
    g_fd_tx[fd] = NULL;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nonblock_add_fd(int fd)
{
  t_fd_tx *fd_tx = get_fd_tx(fd);
  t_fd_chain *fd_chain = get_fd_chain(fd);
  if (fd_tx)
    KERR("%d", fd);
  else if (fd_chain)
    KERR("%d", fd);
  else
    {
    nonblock(fd);
    g_fd_tx[fd] = (t_fd_tx *) malloc(sizeof(t_fd_tx));
    memset(g_fd_tx[fd], 0, sizeof(t_fd_tx));
    fd_chain = (t_fd_chain *) malloc(sizeof(t_fd_chain));
    memset(fd_chain, 0, sizeof(t_fd_chain));
    fd_chain->fd = fd;
    if (g_head_fd_chain)
      g_head_fd_chain->prev = fd_chain;
    fd_chain->next = g_head_fd_chain;
    g_head_fd_chain = fd_chain;    
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nonblock_init(void)
{
  memset(g_fd_tx, 0, MAX_FD * sizeof(t_fd_tx *));
  g_head_fd_chain = NULL;
}
/*--------------------------------------------------------------------------*/
