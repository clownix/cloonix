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
#include <unistd.h>
#include <sys/time.h>

#include "mdl.h"
#include "wrap.h"

static int g_listen_sock_cloon;
static int g_sock_cloon;
static struct timeval g_last_cloonix_tv;

/****************************************************************************/
static void sock_cloonix_action(void)
{
  int len;
  char buf[MAX_TXT_LEN];
  len = wrap_read_cloon(g_sock_cloon, buf, MAX_TXT_LEN);
  if (len == 0)
    {
    XOUT("ERROR socket");
    }
  else if (len < 0)
    {
    if (errno != EINTR && errno != EAGAIN)
      XOUT("ERROR socket");
    }
  else 
    {
    if (!strcmp(buf, CLOONIX_PID_REQ))
      {
      snprintf(buf, MAX_TXT_LEN, CLOONIX_PID_RESP, getpid());
      len = wrap_write_cloon(g_sock_cloon, buf, strlen(buf) + 1);
      if (len != strlen(buf) + 1)
        XOUT("%s %d %d %s", strerror(errno), len, strlen(buf) + 1, buf );
      gettimeofday(&g_last_cloonix_tv, NULL);
      }
    else if (!strcmp(buf, CLOONIX_KIL_REQ))
      {
      exit(0);
      }
    else
      XERR("%d %s", len, buf);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int server_loop_wait_cloon(void)
{
  static int count = 0;
  int ret, result = -1;
  fd_set readfds;
  struct timeval cltimeout;
  cltimeout.tv_sec = 1;
  cltimeout.tv_usec = 0;
  count += 1;
  if (count > 10)
    XOUT("Waiting too long for cloon connect");
  FD_ZERO(&readfds);
  FD_SET(g_listen_sock_cloon, &readfds);
  ret = select(g_listen_sock_cloon + 1, &readfds, NULL, NULL, &cltimeout);
  if (ret < 0)
    {
    if (errno != EINTR && errno != EAGAIN)
      XOUT("%s", strerror(errno));
    }
  else if (ret == 0)
    {
    cltimeout.tv_sec = 1;
    cltimeout.tv_usec = 0;
    }
  else
    {
    if (FD_ISSET(g_listen_sock_cloon, &readfds))
      {
      g_sock_cloon = wrap_accept(g_listen_sock_cloon, fd_type_cloon, 0,
                                    __FUNCTION__);
      wrap_close(g_listen_sock_cloon, __FUNCTION__);
      g_listen_sock_cloon = -1;
      if (g_sock_cloon < 0)
        XOUT("accept4 %s", strerror(errno));
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
  if (g_sock_cloon != -1)
    FD_SET(g_sock_cloon, readfds);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cloonix_fdisset(fd_set *readfds)
{
  if (g_sock_cloon != -1)
    {
    if (FD_ISSET(g_sock_cloon, readfds))
      sock_cloonix_action();
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int cloonix_get_max_fd(int val)
{
  int result = val;
  if (g_sock_cloon > result)
    result = g_sock_cloon;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cloonix_beat(struct timeval *tv)
{
  if (g_sock_cloon != -1)
    {
    if ((tv->tv_sec - g_last_cloonix_tv.tv_sec) > 40)
      XOUT("ERROR TIMEOUT");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void cloonix_init(char *input_argv)
{
  g_sock_cloon = -1;
  if (!strcmp(input_argv, "standalone"))
    g_listen_sock_cloon = -1;
  else
    {
    g_listen_sock_cloon = wrap_socket_listen_unix(input_argv,
                            fd_type_listen_cloon, __FUNCTION__);
    while (1)
      {
      if (!server_loop_wait_cloon())
        break;
      }
    }
}
/*--------------------------------------------------------------------------*/
