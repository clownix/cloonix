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
#include <unistd.h>
#include <sys/time.h>

#include "mdl.h"
#include "wrap.h"

static int g_listen_sock_cloonix;
static int g_sock_cloonix;
static struct timeval g_last_cloonix_tv;

/****************************************************************************/
static void sock_cloonix_action(void)
{
  int len;
  char buf[MAX_TXT_LEN];
  len = wrap_read_cloonix(g_sock_cloonix, buf, MAX_TXT_LEN);
  if (len == 0)
    {
    KOUT(" ");
    }
  else if (len < 0)
    {
    if (errno != EINTR && errno != EAGAIN)
      KOUT(" ");
    }
  else 
    {
    if (!strcmp(buf, CLOONIX_PID_REQ))
      {
      snprintf(buf, MAX_TXT_LEN, CLOONIX_PID_RESP, getpid());
      len = wrap_write_cloonix(g_sock_cloonix, buf, strlen(buf) + 1);
      if (len != strlen(buf) + 1)
        KOUT("%s %d %d %s", strerror(errno), len, strlen(buf) + 1, buf );
      gettimeofday(&g_last_cloonix_tv, NULL);
      }
    else if (!strcmp(buf, CLOONIX_KIL_REQ))
      {
      exit(0);
      }
    else
      KERR("%d %s", len, buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int server_loop_wait_cloonix(void)
{
  static int count = 0;
  int ret, result = -1;
  fd_set readfds;
  struct timeval cltimeout;
  cltimeout.tv_sec = 1;
  cltimeout.tv_usec = 0;
  count += 1;
  if (count > 10)
    KOUT("Waiting too long for cloonix connect");
  FD_ZERO(&readfds);
  FD_SET(g_listen_sock_cloonix, &readfds);
  ret = select(g_listen_sock_cloonix + 1, &readfds, NULL, NULL, &cltimeout);
  if (ret < 0)
    {
    if (errno != EINTR && errno != EAGAIN)
      KOUT("%s", strerror(errno));
    }
  else if (ret == 0)
    {
    cltimeout.tv_sec = 1;
    cltimeout.tv_usec = 0;
    }
  else
    {
    if (FD_ISSET(g_listen_sock_cloonix, &readfds))
      {
      g_sock_cloonix = wrap_accept(g_listen_sock_cloonix, fd_type_cloonix, 0,
                                    __FUNCTION__);
      wrap_close(g_listen_sock_cloonix, __FUNCTION__);
      g_listen_sock_cloonix = -1;
      if (g_sock_cloonix < 0)
        KOUT("accept4 %s", strerror(errno));
      gettimeofday(&g_last_cloonix_tv, NULL);
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cloonix_fdset(fd_set *readfds)
{
  if (g_sock_cloonix != -1)
    FD_SET(g_sock_cloonix, readfds);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cloonix_fdisset(fd_set *readfds)
{
  if (g_sock_cloonix != -1)
    {
    if (FD_ISSET(g_sock_cloonix, readfds))
      sock_cloonix_action();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int cloonix_get_max_fd(int val)
{
  int result = val;
  if (g_sock_cloonix > result)
    result = g_sock_cloonix;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cloonix_beat(struct timeval *tv)
{
  if (g_sock_cloonix != -1)
    {
    if ((tv->tv_sec - g_last_cloonix_tv.tv_sec) > 5)
      KOUT(" ");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cloonix_init(char *input_argv)
{
  g_sock_cloonix = -1;
  if (!strcmp(input_argv, "standalone"))
    g_listen_sock_cloonix = -1;
  else
    {
    g_listen_sock_cloonix = wrap_socket_listen_unix(input_argv,
                            fd_type_listen_cloonix, __FUNCTION__);
    while (1)
      {
      if (!server_loop_wait_cloonix())
        break;
      }
    }
}
/*--------------------------------------------------------------------------*/
