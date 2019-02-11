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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <stdint.h>

#include "sock_utils.h"


/*****************************************************************************/
static int nonblock(int fd)
{
  int result = -1;
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags != -1)
    {
    flags |= O_NONBLOCK|O_NDELAY;
    if (fcntl(fd, F_SETFL, flags) != -1)
      result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void tempo_connect(int no_use)
{
  printf("SIGNAL Timeout in connect\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static uint16_t get_port_from_str(char *str_int)
{
  unsigned long num;
  int result;
  char *endptr;
  num = strtoul(str_int, &endptr, 10);
  if ((endptr == NULL)||(endptr[0] != 0))
    {
    printf("Bad input port %s\n", str_int);
    exit(-1);
    }
  else if ((num == 0) || (num > 0xFFFF))
    {
    printf("Bad input port %s\n", str_int);
    exit(-1);
    }
  else
    result = (int) num;
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int ip_string_to_int (uint32_t *inet_addr, char *ip_string)
{
  int result = -1;
  unsigned int part[4];
  if (strlen(ip_string) != 0)
    {
    if ((sscanf(ip_string,"%u.%u.%u.%u",
                          &part[0], &part[1], &part[2], &part[3]) == 4) &&
        (part[0]<=0xFF) && (part[1]<=0xFF) &&
        (part[2]<=0xFF) && (part[3]<=0xFF))
      {
      result = 0;
      *inet_addr = 0;
      *inet_addr += part[0]<<24;
      *inet_addr += part[1]<<16;
      *inet_addr += part[2]<<8;
      *inet_addr += part[3];
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int socket_listen_inet( uint16_t port )
{
  struct sockaddr s_addr;
  int fd, result = -1;
  int optval=1;

  memset(&s_addr, 0, sizeof(s_addr));
  s_addr.sa_family = AF_INET;
  ((struct sockaddr_in*)&s_addr)->sin_addr.s_addr = htonl(INADDR_ANY);
  ((struct sockaddr_in*)&s_addr)->sin_port = htons( port );

  fd = socket( s_addr.sa_family, SOCK_STREAM, 0 );
  if ( fd <= 0 )
    printf("err socket\n");
  else if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval))<0)
    printf("err setsockopt\n");
  else if ( bind(fd, &s_addr,sizeof(struct sockaddr_in) ) < 0 )
    {
    close(fd);
    printf("err bind\n");
    }
  else if (listen(fd, 1) < 0)
    {
    close(fd);
    printf("err listen\n");
    }
  else
    result = fd;
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int socket_listen_unix(char *pname)
{
  int ret, fd, len, result = -1;
  struct sockaddr_un serv;
  mode_t old_mask;
  if (strlen(pname) >= 108)
    printf("Name too long: %d", (int)(strlen(pname)));
  else
    {
    unlink (pname);
    fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
      printf("Erreur socket AF_UNIX");
    else
      {
      memset (&serv, 0, sizeof (struct sockaddr_un));
      serv.sun_family = AF_UNIX;
      strncpy (serv.sun_path, pname, strlen(pname));
      len = sizeof (serv.sun_family) + strlen (serv.sun_path);
      old_mask = umask (0000);
      ret = bind (fd, (struct sockaddr *) &serv, len);
      umask (old_mask);
      if (ret != 0)
        {
        close(fd);
        printf("bind failure %s %d\n", pname, errno);
        }
      else
        {
        ret = listen (fd, 5);
        if (ret != 0)
          {
          close(fd);
          printf("listen failure\n");
          }
        else
          result = fd;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static int util_fd_accept(int fd_listen)
{
  int fd, result = -1;
  struct sockaddr s_addr;
  socklen_t sz = sizeof(s_addr);
  fd = accept( fd_listen, &s_addr, &sz );
  if (fd < 0)
    printf("err accept");
  else
    {
    if (nonblock(fd))
      printf("err nonblock\n");
    else
      result = fd;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int select_and_accept(int fd_listen)
{
  int ret, result = -1;
  fd_set infd;
    FD_ZERO(&infd);
    FD_SET(fd_listen, &infd);
    ret = select(fd_listen+1, &infd, NULL, NULL, NULL);
    if (ret <= 0)
      printf("err select %d\n", ret);
    else
      {
      if (!(FD_ISSET(fd_listen, &infd)))
        printf("err isset\n");
      else
        result = util_fd_accept(fd_listen);
      }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int test_file_is_socket(char *path)
{
  int result = -1;
  struct stat stat_file;
  if (!stat(path, &stat_file))
    {
    if (S_ISSOCK(stat_file.st_mode))
      result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_server_socket_inet(char *str_port)
{
  int fd_listen , result = -1;
  uint16_t port = get_port_from_str(str_port);
  fd_listen = socket_listen_inet(port);
  if (fd_listen == -1)
    printf("err fd_listen\n");
  else
    result = select_and_accept(fd_listen);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_client_socket_inet(char *ip_string, char *str_port)
{
  struct sigaction act;
  struct sigaction oldact;
  int   sock, ret, result = -1;
  struct sockaddr_in adresse;
  uint32_t ip;
  uint16_t port = get_port_from_str(str_port);
  sock = socket (AF_INET,SOCK_STREAM,0);
  if (ip_string_to_int (&ip, ip_string))
    printf("err ip_string_to_int\n");
  else if (sock < 0)
    printf("err socket SOCK_STREAM\n");
  else
    {
    adresse.sin_family = AF_INET;
    adresse.sin_port = htons(port);
    adresse.sin_addr.s_addr = htonl(ip);
    memset(&act, 0, sizeof(act));
    sigfillset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = tempo_connect;
    sigaction(SIGALRM, &act, &oldact);
    alarm(5);
    ret = connect(sock,(struct sockaddr *) &adresse, sizeof(adresse));
    sigaction(SIGALRM, &oldact, NULL);
    alarm(0);
    if (ret == 0)
      {
      if (nonblock(sock))
        printf("err nonblock\n");
      else
        result = sock;
      }
    else
      {
      close(sock);
      printf("NO SERVER LISTENING TO %d\n", port);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_server_socket_unix(char *path)
{
  int fd_listen , result = -1;
  fd_listen = socket_listen_unix(path);
  if (fd_listen == -1)
    printf("err fd_listen\n");
  else
    result = select_and_accept(fd_listen);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_client_socket_unix(char *pname)
{
  int len,  sock, ret, result = -1;
  struct sockaddr_un addr;
  struct sigaction act;
  struct sigaction oldact;
  if (test_file_is_socket(pname))
    printf("err path name not socket %s\n", pname);
  else
    {
    sock = socket (AF_UNIX,SOCK_STREAM,0);
    if (sock <= 0)
      printf("err socket SOCK_STREAM\n");
    else
      {
      memset (&addr, 0, sizeof (struct sockaddr_un));
      addr.sun_family = AF_UNIX;
      strcpy(addr.sun_path, pname);
      len = sizeof (struct sockaddr_un);
      memset(&act, 0, sizeof(act));
      sigfillset(&act.sa_mask);
      act.sa_flags = 0;
      act.sa_handler = tempo_connect;
      sigaction(SIGALRM, &act, &oldact);
      alarm(5);
      ret = connect(sock,(struct sockaddr *) &addr, len);
      sigaction(SIGALRM, &oldact, NULL);
      alarm(0);
      if (ret != 0)
        {
        close(sock);
        printf("NO SERVER LISTENING TO %s\n", pname);
        }
      else
        {
        result = sock;
        nonblock(sock);
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

