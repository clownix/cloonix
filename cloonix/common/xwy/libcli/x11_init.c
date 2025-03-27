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
#include "glob_common.h"
#include "util_sock.h"

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
  char buf[MAX_TXT_LEN];
  char *ptr, *end;
  FILE *fp;
  char *argv[5];
  char *acmd;

  memset(argv, 0, 5*sizeof(char *));
  memset(err, 0, MAX_TXT_LEN);
  memset(buf, 0, MAX_TXT_LEN);
  argv[0] = XAUTH_BIN;
  argv[1] = "list";
  argv[2] = display;
  argv[3] = NULL;
  acmd = mdl_argv_linear(argv);
  fp = mdl_argv_popen(argv);
  if (fp == NULL)
    {
    snprintf(err, MAX_TXT_LEN-1, "%s, popen", acmd);
    KERR("ERROR %s", acmd);
    }
  else
    {
    if (!fgets(buf, MAX_TXT_LEN-1, fp))
      {
      snprintf(err, MAX_TXT_LEN-1, "%s, fgets", acmd);
      }
    else
      {
      if (!strlen(buf))
        {
        snprintf(err, MAX_TXT_LEN-1, "%s, strlen", acmd);
        KERR("ERROR %s", acmd);
        }
      else
        {
        ptr = strstr(buf, "MIT-MAGIC-COOKIE-1");
        if (!ptr)
          {
          snprintf(err, MAX_TXT_LEN-1, "%s, MIT-MAGIC", buf);
          KERR("ERROR %s  %s", acmd, buf);
          }
        else
          {
          ptr += strlen("MIT-MAGIC-COOKIE-1");
          ptr += strspn(ptr, " \t");
          if (strlen(ptr) < 1)
            {
            snprintf(err, MAX_TXT_LEN-1, "%s, MIT-MAGIC strlen1", buf);
            KERR("ERROR %s  %s", acmd, buf);
            }
          else
            {
            end = strchr(ptr, '\r');
            if (end)
              *end = 0;
            end = strchr(ptr, '\n');
            if (end)
              *end = 0;
            if (strlen(ptr) < 1)
              {
              snprintf(err, MAX_TXT_LEN-1, "%s, MIT-MAGIC strlen2", buf);
              KERR("ERROR %s  %s", acmd, buf);
              }
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
      {
      snprintf(err, MAX_TXT_LEN-1, "%s, pclose", acmd);
      KERR("ERROR %s", acmd);
      }
    }
  return result;
}


/****************************************************************************/
static int x11_init_unix(char *display, int num)
{
  int fd, val, result = -1;
  char err[MAX_TXT_LEN];
  snprintf(g_x11_path, MAX_TXT_LEN-1, UNIX_X11_SOCKET_PREFIX, num);
  if (wrap_util_proxy_client_socket_unix(g_x11_path, &fd))
    {
    KERR("X11 SOCKET NOT FOUND: %s", g_x11_path);
    memset(g_x11_path, 0, MAX_TXT_LEN);
    }
  else
    {
    close(fd);
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
  g_x11_ok = 0;
  g_x11_srv_idx = 0;
  if (!strcmp(msg->buf, "OK"))
    {
    if ((srv_idx < SRV_IDX_MIN) || (srv_idx > SRV_IDX_MAX))
      {
      KOUT("%d", srv_idx);
      }
    else
      {
      g_x11_ok = 1;
      g_x11_srv_idx = srv_idx;
      }
    }
  else
    {
    KOUT("%s", msg->buf);
    }
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
    KERR("ERROR MAGIC X11 no DISPLAY");
    init_x11_magic();
    }
  else
    {
    if ((sscanf(display, ":%d", &val) == 1) ||
        (sscanf(display, "unix:%d.0", &val) == 1))
      {
      if (x11_init_unix(display, val))
        {
        KERR("ERROR MAGIC X11 %s", display);
        display = NULL;
        }
      }
    else if (sscanf(display, "localhost:%d.0", &val) == 1)
      {
      if (x11_init_inet(display, val))
        {
        KERR("ERROR MAGIC X11 %s", display);
        display = NULL;
        }
      }
    else if (sscanf(display, "127.0.0.1:%d.0", &val) == 1)
      {
      if (x11_init_inet(display, val))
        {
        KERR("ERROR MAGIC X11 %s", display);
        display = NULL;
        }
      }
    else
      {
      KERR("ERROR MAGIC X11 %s", display);
      display = NULL;
      }
    if ((display == NULL) || (get_xauth_magic(display, err)))
      init_x11_magic();
    }
}
/*--------------------------------------------------------------------------*/

