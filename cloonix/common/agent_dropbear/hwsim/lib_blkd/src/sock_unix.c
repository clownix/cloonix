/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <sys/epoll.h>


#include "ioc_blkd.h"


/****************************************************************************/
int sock_unix_read (char *buf, int max, int fd)
{
  int rx_len = -1;
  rx_len = read (fd, buf, max);
  if (rx_len == 0)
    {
    KERR(" ");
    rx_len = -1;
    }
  else if (rx_len < 0)
    {
    if ((errno == EAGAIN) || (errno ==EINTR))
      rx_len = 0;
    }
  return rx_len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int sock_unix_write(char *buf, int len, int fd, int *get_out)
{
  int tx_len, result = -1;
  *get_out = 0;
  if (len <= 0)
    KOUT("%d", len);
  tx_len = write (fd, (unsigned char *) buf, len);
  if (tx_len < 0)
    {
    if ((errno == EAGAIN) || (errno == EINTR))
      {
      *get_out = 1;
      result = 0;
      }
    }
  else
    {
    result = tx_len;
    if (tx_len != len)
      {
      *get_out = 1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int connectable_ok(char *pname)
{
  int len, sock, result = 0;
  struct sockaddr_un addr;
  if (!test_file_is_socket(pname))
    {
    sock = socket (AF_UNIX,SOCK_STREAM,0);
    if (sock <= 0)
      KOUT(" ");
    memset (&addr, 0, sizeof (struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, pname);
    len = sizeof (struct sockaddr_un);
    if (!connect(sock,(struct sockaddr *) &addr, len))
      result = 1;
    close(sock);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

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
int sock_client_unix(char *pname, int *fd)
{
  int len,  sock, result = -1;
  struct sockaddr_un addr;
  if (!test_file_is_socket(pname))
    {
    sock = socket (AF_UNIX,SOCK_STREAM,0);
    if (sock <= 0)
      KOUT(" ");
    memset (&addr, 0, sizeof (struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, pname);
    len = sizeof (struct sockaddr_un);
    result = connect(sock,(struct sockaddr *) &addr, len);
    if (result != 0)
      {
      close(sock);
      printf("NO SERVER LISTENING TO %s\n", pname);
      }
    else
      {
      *fd = sock;
      nonblock(*fd);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int sock_listen_unix(char *pname)
{
  int ret, fd, len;
  struct sockaddr_un serv;

  if (strlen(pname) >= 108)
    KOUT("%d", (int)(strlen(pname)));
  if (!access(pname, F_OK))
    {
    if (connectable_ok(pname))
      {
      printf("Socket: %s in use\n", pname);
      return -1;
      }
    unlink(pname);
    }
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
    printf("bind failure %s %d\n", pname, errno);
    }
  return fd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void sock_unix_fd_accept(int fd_listen, int *fd)
{
  struct sockaddr s_addr;
  socklen_t sz = sizeof(s_addr);
  *fd = accept( fd_listen, &s_addr, &sz );
  if (*fd > 0)
    {
    nonblock(*fd);
    }
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

