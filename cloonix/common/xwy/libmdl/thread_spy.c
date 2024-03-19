/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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

#include "mdl.h"
#include "debug.h"
#include "wrap.h"

/*****************************************************************************/
typedef struct t_thread_spy
{
  int fd;
  int fd_bis;
  int epfd;
  int type;
} t_thread_spy;

static t_thread_spy *g_thread_spy_tx[MAX_FD_NUM];
static t_thread_spy *g_thread_spy_x11[MAX_FD_NUM];
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void thread_spy_add(int fd, int fd_bis, int epfd, int fd_type)
{
  char *type;
  t_thread_spy *spy = (t_thread_spy *)wrap_malloc(sizeof(t_thread_spy));
  memset(spy, 0, sizeof(t_thread_spy));
  spy->fd = fd;
  spy->fd_bis = fd_bis;
  spy->epfd = epfd;
  spy->type = fd_type;
  if (fd_type == thread_type_tx)
    {
    if (g_thread_spy_tx[fd])
      {
      type = debug_get_thread_type_txt(fd_type);
      XERR("THREAD EXISTS %d %s", fd, type);
      }
    else
      g_thread_spy_tx[fd] = spy;
    }
  else if (fd_type == thread_type_x11)
    {
    if (g_thread_spy_x11[fd])
      {
      type = debug_get_thread_type_txt(fd_type);
      XERR("THREAD EXISTS %d %s", fd, type);
      }
    else
      g_thread_spy_x11[fd] = spy;
    }
  else
    {
    type = debug_get_thread_type_txt(fd_type);
    XERR("Wrong type %d %s", fd, type);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void thread_spy_del(int fd, int fd_type)
{
  t_thread_spy *spy;
  if (fd_type == thread_type_tx)
    {
    spy = g_thread_spy_tx[fd];;
    if (!spy)
      XERR("THREAD DOES NOT EXIST %d", fd);
    else
      {
      g_thread_spy_tx[fd] = NULL;
      wrap_free(spy, __LINE__);
      }
    }
  else if (fd_type == thread_type_x11)
    {
    spy = g_thread_spy_x11[fd];;
    if (!spy)
      XERR("THREAD DOES NOT EXIST %d", fd);
    else
      {
      g_thread_spy_x11[fd] = NULL;
      wrap_free(spy, __LINE__);
      }
    }
  else
    XERR("Wrong type %d %s", fd, fd_type);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void thread_spy_heartbeat(void)
{
#ifdef DEBUG
  t_thread_spy *spy;
  char *type;
  int i;
  debug_evt("THREAD_SPY BEGIN -------------------");
  for (i=0; i<MAX_FD_NUM; i++)
    {
    spy = g_thread_spy_tx[i];
    if (spy)
      {
      type = debug_get_thread_type_txt(spy->type);
      debug_evt("THREAD_SPY sock: %d  type: %s", spy->fd, type);
      }
    spy = g_thread_spy_x11[i];
    if (spy)
      {
      type = debug_get_thread_type_txt(spy->type);
      debug_evt("THREAD_SPY sock_fd: %d  x11_fd: %d  epfd: %d  type: %s",
                 spy->fd, spy->fd_bis, spy->epfd, type);
      }
    }
  debug_evt("THREAD_SPY END ---------------------");
#endif
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void thread_spy_init(void)
{
  memset(g_thread_spy_tx, 0, MAX_FD_NUM * sizeof(t_thread_spy *));
  memset(g_thread_spy_x11, 0, MAX_FD_NUM * sizeof(t_thread_spy *));
}
/*---------------------------------------------------------------------------*/
