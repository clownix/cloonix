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
#include <asm/types.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <stddef.h>

#include "io_clownix.h"
#include "util_sock.h"
#include "channel.h"


/*****************************************************************************/
static void nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    KOUT(" ");
  flags |= O_NONBLOCK|O_NDELAY;
  if (fcntl(fd, F_SETFL, flags) == -1)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void nonnonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  flags &= ~O_NONBLOCK;
  flags &= ~O_NDELAY;
  if (fcntl(fd, F_SETFL, flags) == -1)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void nonblock_fd(int fd)
{
  nonblock(fd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void nodelay_fd(int fd)
{
  int nodelay = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*) &nodelay, sizeof(int));
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void nonnonblock_fd(int fd)
{
  nonnonblock(fd);
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void util_free_fd(int fd)
{
  close (fd);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*
int util_read_brakes_on(char *ptr, int max, int fd, int llid)
{
  int rx_len;
  rx_len = read (fd, ptr, max);
  if (rx_len < 0)
    {
    if ((errno == EAGAIN) || (errno ==EINTR))
      {
      rx_len = 0;
      channel_rx_slow_down(llid);
      }
    }
  else if (rx_len == 0)
    rx_len = -1;
  return rx_len;
}
*/
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_read (char *ptr, int max, int fd)
{
  int rx_len;
  rx_len = read (fd, ptr, max);
  if (rx_len < 0)
    {
    if ((errno == EAGAIN) || (errno == EINTR) || (errno == EINPROGRESS))
      rx_len = 0;
    }
  else if (rx_len == 0)
    rx_len = -1;
  return rx_len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void tempo_connect(int no_use)
{
  KERR("SIGNAL Timeout in connect\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_nonblock_client_socket_unix(char *pname, int *fd)
{
  int len,  sock, result = -1;
  struct sockaddr_un addr;
  struct stat stat_file;
  signal(SIGPIPE, SIG_IGN);
  if ((!pname) || (strlen(pname) == 0))
    {
    KERR("ERROR");
    return -1;
    }
  if (stat(pname, &stat_file))
    {
    KERR("ERROR %s", pname);
    return -1;
    }
  if (!S_ISSOCK(stat_file.st_mode))
    {
    KERR("ERROR %s", pname);
    return -1;
    }
  sock = socket (AF_UNIX,SOCK_STREAM,0);
  if (sock <= 0)
    KOUT("ERROR");
  memset (&addr, 0, sizeof (struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, pname);
  len = sizeof (struct sockaddr_un);
  nonblock_fd(sock);
  result = connect(sock,(struct sockaddr *) &addr, len);
  if (result != 0)
    {
    close(sock);
    KERR("ERROR %s %d", pname, errno);
    }
  else
   *fd = sock;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_nonblock_client_socket_inet(uint32_t ip, uint16_t port)
{
  int sock, result;
  struct sockaddr_in adresse;
  char ip_string[MAX_NAME_LEN];
  int_to_ip_string (ip, ip_string);
  signal(SIGPIPE, SIG_IGN);
  sock = socket (AF_INET,SOCK_STREAM,0);
  if (sock < 0)
    KOUT(" ");
  adresse.sin_family = AF_INET;
  adresse.sin_port = htons(port);
  adresse.sin_addr.s_addr = htonl(ip);
  nonblock_fd(sock);
  result = connect(sock,(struct sockaddr *) &adresse, sizeof(adresse));
  if (result != -1)
    {
    KERR("ERROR %X %hu %d %d", ip, port, errno, errno);
    result = -1;
    }
  else
    {
    if (errno == EINPROGRESS)
      {
      result = sock;
      }
    else
      {
      if (errno == ENETUNREACH)
        {
        KERR("ERROR %s %hu Network is unreachable", ip_string, port);
        }
      else
        KERR("ERROR %s %hu %d", ip_string, port, errno);
      if (close(sock))
        KERR("ERROR %s %hu %d", ip_string, port, errno);
      result = -1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_client_abstract_socket_unix(char *pname, int *fd)
{
  int len,  sock, result = -1;
  struct sockaddr_un addr;
  struct sigaction act;
  struct sigaction oldact;
  signal(SIGPIPE, SIG_IGN);
  if ((!pname) || (strlen(pname) == 0))
    {
    KERR("ERROR");
    return -1;
    }
  sock = socket (AF_UNIX,SOCK_STREAM,0);
  if (sock <= 0)
    KOUT("ERROR");
  memset (&addr, 0, sizeof (struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  addr.sun_path[0] = 0;
  strcpy(&(addr.sun_path[1]), pname);
  len = strlen(pname) + 1;
  memset(&act, 0, sizeof(act));
  sigfillset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = tempo_connect;
  sigaction(SIGALRM, &act, &oldact);
  alarm(1);
  result = connect(sock,(struct sockaddr *) &addr, offsetof(struct sockaddr_un, sun_path) + len);
  sigaction(SIGALRM, &oldact, NULL);
  alarm(0);
  if (result != 0)
    {
    close(sock);
    KERR("NO SERVER LISTENING TO ABSTRACT %s\n", pname);
    }
  else
    {
    *fd = sock;
    nonblock(*fd);
    KERR("ABSTRACT OPEN OK %s\n", pname);
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int util_client_socket_unix(char *pname, int *fd)
{
  int len,  sock, result = -1;
  struct sockaddr_un addr;
  struct sigaction act;
  struct sigaction oldact;
  struct stat stat_file;
  signal(SIGPIPE, SIG_IGN);
  if ((!pname) || (strlen(pname) == 0))
    {
    KERR("ERROR");
    return -1;
    }
  if (stat(pname, &stat_file))
    {
    KERR("ERROR %s", pname);
    return -1;
    }
  if (!S_ISSOCK(stat_file.st_mode))
    {
    KERR("ERROR %s", pname);
    return -1;
    }
  sock = socket (AF_UNIX,SOCK_STREAM,0);
  if (sock <= 0)
    KOUT(" ");
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
  result = connect(sock,(struct sockaddr *) &addr, len);

  sigaction(SIGALRM, &oldact, NULL);
  alarm(0);

  if (result != 0)
    {
    close(sock);
    printf("NO SERVER LISTENING TO %s\n", pname);
    KERR("ERROR NO SERVER LISTENING TO %s", pname);
    }
  else
    {
    *fd = sock;
    nonblock(*fd);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_proxy_client_socket_unix(char *pname, int *fd)
{
  int len,  sock, result = -1;
  struct sockaddr_un addr;
  struct sigaction act;
  struct sigaction oldact;
  signal(SIGPIPE, SIG_IGN);
  if ((!pname) || (strlen(pname) == 0))
    {
    KERR("ERROR");
    return -1;
    }
  sock = socket (AF_UNIX,SOCK_STREAM,0);
  if (sock <= 0)
    KOUT(" ");
  memset (&addr, 0, sizeof (struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, pname);
  len = sizeof(struct sockaddr_un);
  memset(&act, 0, sizeof(act));
  sigfillset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = tempo_connect;
  sigaction(SIGALRM, &act, &oldact);
  alarm(1);
  result = connect(sock,(struct sockaddr *) &addr, len);
  sigaction(SIGALRM, &oldact, NULL);
  alarm(0);
  if (result != 0)
    {
    close(sock);
    printf("NO SERVER LISTENING TO %s\n", pname);
    KERR("NO SERVER LISTENING TO %s\n", pname);
    }
  else
    {
    *fd = sock;
    nonblock(*fd);
    }
  if (result)
    result = util_client_abstract_socket_unix(pname, fd);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_client_socket_inet(uint32_t ip, uint16_t port, int *fd)
{
  struct sigaction act;
  struct sigaction oldact;
  int   sock, result;
  struct sockaddr_in adresse;
  *fd = -1;
  signal(SIGPIPE, SIG_IGN);
  sock = socket (AF_INET,SOCK_STREAM,0);
  if (sock <= 0)
    KOUT(" ");
  adresse.sin_family = AF_INET;
  adresse.sin_port = htons(port);
  adresse.sin_addr.s_addr = htonl(ip);

  memset(&act, 0, sizeof(act));
  sigfillset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = tempo_connect;
  sigaction(SIGALRM, &act, &oldact);

  alarm(5);
  result = connect(sock,(struct sockaddr *) &adresse, sizeof(adresse));
  
  sigaction(SIGALRM, &oldact, NULL);
  alarm(0);

  if (result == 0)
    {
    *fd = sock;
    nonblock(*fd);
    nodelay_fd(*fd);
    }
  else
    {
    close(sock);
    printf("NO SERVER LISTENING TO %d\n", port);
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int util_socket_listen_unix(char *pname)
{
  int ret, fd, len;
  struct sockaddr_un serv;

  signal(SIGPIPE, SIG_IGN);
  if ((!pname) || (strlen(pname) == 0))
    {
    KERR("ERROR");
    return -1;
    }
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
  if (ret == 0)
    {
    ret = listen (fd, 50);
    if (ret != 0)
      {
      close(fd);
      fd = -1;
      printf("listen failure\n");
      }
    }
  else
    {
    close(fd);
    fd = -1;
    KERR("bind failure %s %d\n", pname, errno);
    }
  return fd;
}
/*--------------------------------------------------------------------------*/
  
/****************************************************************************/
int util_socket_unix_dgram(char *pname)
{ 
  int ret, fd, len;
  struct sockaddr_un serv;
  signal(SIGPIPE, SIG_IGN);
  
  if ((!pname) || (strlen(pname) == 0))
    {
    KERR("ERROR");
    return -1;
    }
  if (strlen(pname) >= 108)
    KOUT("%d", (int)(strlen(pname)));
  unlink (pname);
  fd = socket (AF_UNIX, SOCK_DGRAM, 0);
  if (fd < 0)
    KOUT("%d %d\n", fd, errno);
  memset (&serv, 0, sizeof (struct sockaddr_un));
  serv.sun_family = AF_UNIX;
  strncpy (serv.sun_path, pname, strlen(pname));
  len = sizeof (serv.sun_family) + strlen (serv.sun_path);
  ret = bind (fd, (struct sockaddr *) &serv, len);
  if (ret)
    {
    close(fd);
    fd = -1;
    KERR("ERROR bind failure %s %d", pname, errno);
    }
  return fd;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int util_socket_listen_inet( uint16_t port )
{
  struct sockaddr s_addr;
  int fd, optval=1, nodelay=1;
  signal(SIGPIPE, SIG_IGN);
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

/*****************************************************************************/
void util_fd_accept(int fd_listen, int *fd, const char *fct)
{
  struct sockaddr s_addr;
  socklen_t sz = sizeof(s_addr);
  *fd = accept( fd_listen, &s_addr, &sz );
  if (*fd >= 0)
    {
    nonblock(*fd);
    nodelay_fd(*fd);
    }
  else
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/




