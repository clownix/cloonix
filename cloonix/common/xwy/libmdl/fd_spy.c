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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdl.h"
#include "debug.h"
#include "wrap.h"

/*****************************************************************************/
typedef struct t_fd_spy
{
  int fd;
  int type;
  int srv_idx;
  int cli_idx;
} t_fd_spy;

static t_fd_spy *g_fd_spy[MAX_FD_NUM];
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int fd_spy_set_info(int fd, int srv_idx, int cli_idx)
{
  int result = -1;
  t_fd_spy *fd_spy = g_fd_spy[fd];
  if (fd_spy)
    {
    fd_spy->srv_idx = srv_idx;
    fd_spy->cli_idx = cli_idx;
    result = 0;
    }
  return result;

}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int  fd_spy_get_type(int fd, int *srv_idx, int *cli_idx)
{
  int result = -1;
  t_fd_spy *fd_spy = g_fd_spy[fd];
  if (fd_spy)
    {
    *srv_idx = fd_spy->srv_idx;
    *cli_idx = fd_spy->cli_idx;
    result = fd_spy->type;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int fd_spy_add(int fd, int fd_type, int line)
{
  int result = -1;
  t_fd_spy *fd_spy;
  char *type;

  if (fd < 0)
    KOUT("%d wrap line:%d", fd, line);

  if (fd >= MAX_FD_NUM)
    KERR("Too big fd:%d max:%d wrap line:%d", fd, MAX_FD_NUM, line);
  else
    {
    result = 0;
    fd_spy = g_fd_spy[fd];
    if (fd_spy)
      {
      type = debug_get_fd_type_txt(fd_spy->type);
      KERR("FD EXISTS %d %s", fd, type);
      }
    else
      {
      fd_spy = (t_fd_spy *) wrap_malloc(sizeof(t_fd_spy));
      memset(fd_spy, 0, sizeof(t_fd_spy));
      fd_spy->fd = fd;
      fd_spy->type = fd_type;
      g_fd_spy[fd] = fd_spy;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void fd_spy_modify(int fd, int fd_type)
{
  t_fd_spy *fd_spy;

  if ((fd < 0) || (fd >= MAX_FD_NUM))
    KOUT("%d", fd);

  fd_spy = g_fd_spy[fd];
  if (!fd_spy)
    {
    KERR("FD DOES NOT EXIST %d", fd);
    }
  else
    {
    fd_spy->type = fd_type;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void fd_spy_del(int fd)
{
  t_fd_spy *fd_spy;

  if ((fd < 0) || (fd >= MAX_FD_NUM))
    KOUT("%d", fd);

  fd_spy = g_fd_spy[fd];
  if (!fd_spy)
    {
    KERR("FD DOES NOT EXIST %d", fd);
    }
  else
    {
    g_fd_spy[fd] = NULL;
    wrap_free(fd_spy, __LINE__);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void fd_spy_heartbeat(void)
{
#ifdef DEBUG
  t_fd_spy *fd_spy;
  char *type;
  int i;
  debug_evt("FD_SPY BEGIN");
  for (i=0; i<MAX_FD_NUM; i++)
    {
    fd_spy = g_fd_spy[i];
    if (fd_spy)
      {
      type = debug_get_fd_type_txt(fd_spy->type);
      debug_evt("FD_SPY %d %s", fd_spy->fd, type);
      }
    }
  debug_evt("FD_SPY END");
#endif
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void fd_spy_init(void)
{
  memset(g_fd_spy, 0, MAX_FD_NUM * sizeof(t_fd_spy *));
}
/*---------------------------------------------------------------------------*/
