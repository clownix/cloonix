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
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <linux/tcp.h>
#include "proxy_crun_access.h"

static char PROXYSHARE[MAX_NAME_LEN];
static char g_unix_sig[MAX_PATH_LEN];

/****************************************************************************/
int util_socket_listen_unix(char *pname)
{
  int ret, fd, len;
  struct sockaddr_un serv;

  if (strlen(pname) >= 108)
    KOUT("%d", (int)(strlen(pname)));
  unlink (pname);
  fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    KOUT("%d %d\n", fd, errno);
  memset (&serv, 0, sizeof (struct sockaddr_un));
  serv.sun_family = AF_UNIX;
  strncpy (serv.sun_path, pname, strlen(pname));
  len = sizeof (serv.sun_family) + strlen (serv.sun_path);
  ret = bind (fd, (struct sockaddr *) &serv, len);
  if (ret != 0)
    KOUT("ERROR bind failure %s %d\n", pname, errno);
  ret = listen (fd, 50);
  if (ret != 0)
    KOUT("ERROR listen failure\n");
  return fd;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int util_socket_listen_inet(uint16_t port)
{
  struct sockaddr s_addr;
  int fd;
  int optval=1;
  int nodelay = 1;

  memset(&s_addr, 0, sizeof(s_addr));
  s_addr.sa_family = AF_INET;
  ((struct sockaddr_in*)&s_addr)->sin_addr.s_addr = htonl(INADDR_ANY);
  ((struct sockaddr_in*)&s_addr)->sin_port = htons( port );

  fd = socket( s_addr.sa_family, SOCK_STREAM, 0 );
  if ( fd <= 0 )
    KOUT("%d", errno);
  if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval))<0)
    KOUT(" ");
  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*) &nodelay, sizeof(int)))
    KOUT(" ");

  if ( bind(fd, &s_addr,sizeof(struct sockaddr_in) ) < 0 )
    {
    close(fd);
    fd = -1;
    }
  else
    {
    if ( listen(fd, 50) < 0 )
      {
      close(fd);
      fd = -1;
      }
    }
  return(fd);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
char *get_proxyshare(void)
{
  return PROXYSHARE;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char **argv)
{
  int sock_sig;
  if (argc != 2)
    KOUT("NB ARGC %d", argc);
  memset(PROXYSHARE, 0, MAX_NAME_LEN);
  memset(g_unix_sig, 0, MAX_PATH_LEN);
  strncpy(PROXYSHARE, argv[1], MAX_NAME_LEN-1);
  snprintf(g_unix_sig, MAX_PATH_LEN-1,  "%s/proxy_sig.sock", PROXYSHARE);
  sock_sig = util_socket_listen_unix(g_unix_sig);
  signal(SIGCHLD,  SIG_IGN);
  daemon(0,0);
  ever_select_loop(sock_sig);
  return 0; 
}
/*--------------------------------------------------------------------------*/
