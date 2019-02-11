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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/time.h>

#include "mdl.h"
#include "low_write.h"
#include "wrap.h"
#include "debug.h"
#include "getsock.h"

static char g_x11_path[MAX_TXT_LEN];
static int  g_x11_port;
static int  g_x11_ok;
static int  g_x11_srv_idx;
static int g_epfd;
static char g_magic_cookie[2*MAGIC_COOKIE_LEN+1];


/*****************************************************************************/
static void init_x11_magic(void)
{
  struct timeval last_tv;
  int i, ln;
  gettimeofday(&last_tv, NULL);
  srand((int) (last_tv.tv_usec & 0xFFFF));
  for (i=0; i < 2*MAGIC_COOKIE_LEN; i++)
    {
    ln = snprintf(&(g_magic_cookie[i]), 2, "%x", (rand() & 0xF));
    if (ln != 1)
      KOUT("%d", ln);
    }
  g_magic_cookie[2*MAGIC_COOKIE_LEN] = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_xauth_magic(char *display, char *err)
{
  int result = -1;
  char cmd[MAX_TXT_LEN];
  char buf[MAX_TXT_LEN];
  char *ptr, *end;
  FILE *fp;
  memset(err, 0, MAX_TXT_LEN);
  memset(cmd, 0, MAX_TXT_LEN);
  memset(buf, 0, MAX_TXT_LEN);
  snprintf(cmd, MAX_TXT_LEN-1, "xauth list %s", display);
  fp = popen(cmd, "r");
  if (fp == NULL)
    snprintf(err, MAX_TXT_LEN-1, "%s, popen", cmd);
  else
    {
    if (!fgets(buf, MAX_TXT_LEN-1, fp))
      snprintf(err, MAX_TXT_LEN-1, "%s, fgets", cmd);
    else
      {
      if (!strlen(buf))
        snprintf(err, MAX_TXT_LEN-1, "%s, strlen", cmd);
      else
        {
        ptr = strstr(buf, "MIT-MAGIC-COOKIE-1");
        if (!ptr)
          snprintf(err, MAX_TXT_LEN-1, "%s, MIT-MAGIC", buf);
        else
          {
          ptr += strlen("MIT-MAGIC-COOKIE-1");
          ptr += strspn(ptr, " \t");
          if (strlen(ptr) < 1)
            snprintf(err, MAX_TXT_LEN-1, "%s, MIT-MAGIC strlen1", buf);
          else
            {
            end = strchr(ptr, '\r');
            if (end)
              *end = 0;
            end = strchr(ptr, '\n');
            if (end)
              *end = 0;
            if (strlen(ptr) < 1)
              snprintf(err, MAX_TXT_LEN-1, "%s, MIT-MAGIC strlen2", buf);
            else
              {
              strncpy(g_magic_cookie, ptr, 2*MAGIC_COOKIE_LEN);
              g_magic_cookie[2*MAGIC_COOKIE_LEN] = 0;
              result = 0;
              }
            }
          }
        }
      }
    if (pclose(fp))
      snprintf(err, MAX_TXT_LEN-1, "%s, pclose", cmd);
    }
  return result;
}


/****************************************************************************/
static int x11_init_unix(char *display, int num)
{
  int val, result = -1;
  char err[MAX_TXT_LEN];
  snprintf(g_x11_path, MAX_TXT_LEN-1, UNIX_X11_SOCKET_PREFIX, num);
  if (access(g_x11_path, F_OK))
    {
    KERR("X11 socket not found: %s", g_x11_path);
    memset(g_x11_path, 0, MAX_TXT_LEN);
    }
  else
    {
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int x11_init_inet(char *display, int num)
{
  unsigned long inet_addr;
  int tmp_fd, result = -1;
  char err[MAX_TXT_LEN];
  g_x11_port = X11_OFFSET_PORT + num;
  mdl_ip_string_to_int(&inet_addr, "127.0.0.1"); 
  tmp_fd = wrap_socket_connect_inet(inet_addr, g_x11_port, 
                                    fd_type_x11_connect_tst, __FUNCTION__);
  if (tmp_fd < 0)
    {
    KERR("X11 port not working: %d", g_x11_port);
    g_x11_port = 0;
    }
  else
    {
    wrap_close(tmp_fd, __FUNCTION__);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_x11_path(void)
{
  return g_x11_path;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_x11_port(void)
{
  return g_x11_port;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_x11_ok(void)
{
  return g_x11_ok;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int get_srv_idx(void)
{
  return g_x11_srv_idx;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
char *get_x11_magic(void)
{
  return g_magic_cookie;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_init_resp(int srv_idx, t_msg *msg)
{
  if (!strcmp(msg->buf, "OK"))
    {
    if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
      KERR("%d", srv_idx);
    else
      {
      g_x11_ok = 1;
      g_x11_srv_idx = srv_idx;
      }
    }
  else
    KERR("%s", msg->buf);
  wrap_free(msg, __LINE__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_init_magic(void)
{
  int val;
  char err[MAX_TXT_LEN];
  char *display = getenv("DISPLAY");
  g_x11_port = 0;
  g_x11_ok = 0;
  memset(g_x11_path, 0, MAX_TXT_LEN);
  if (!display)
    {
    KERR("MAGIC X11 no DISPLAY");
    init_x11_magic();
    }
  else
    {
    if ((sscanf(display, ":%d", &val) == 1) ||
        (sscanf(display, "unix:%d.0", &val) == 1))
      {
      if (x11_init_unix(display, val))
        {
        display = NULL;
        KERR("MAGIC X11 Problem with unix magic");
        }
      }
    else if (sscanf(display, "localhost:%d.0", &val) == 1)
      {
      if (x11_init_inet(display, val))
        {
        display = NULL;
        KERR("MAGIC X11 Problem with inet magic");
        }
      }
    else
      {
      display = NULL;
      KERR("MAGIC X11 Problem with magic");
      }
    if (display == NULL)
      init_x11_magic();
    else if (get_xauth_magic(display, err))
      {
      KERR("MAGIC X11 get_xauth_magic %s", err);
      init_x11_magic();
      }
    }
}
/*--------------------------------------------------------------------------*/

