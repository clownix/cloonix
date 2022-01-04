/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>

#define KOUT(format, a...)                          \
 do {                                               \
    syslog(LOG_ERR | LOG_USER, "KILL"               \
    " line:%d   " format "\n\n", __LINE__, ## a);   \
    exit(-1);                                       \
    } while (0)


/****************************************************************************/
static int socket_listen_unix(char *pname)
{
  int ret, fd, len;
  struct sockaddr_un serv;
  unlink (pname);
  fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    KOUT(" ");
  memset (&serv, 0, sizeof (struct sockaddr_un));
  serv.sun_family = AF_UNIX;
  strncpy (serv.sun_path, pname, strlen(pname));
  len = sizeof (serv.sun_family) + strlen (serv.sun_path);
  ret = bind (fd, (struct sockaddr *) &serv, len);
  if (ret != 0)
    KOUT(" ");
  ret = listen (fd, 5);
  if (ret != 0)
    KOUT(" ");
  return fd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void action_read_and_echo(int fd)
{
  char buf[101];
  int tx_len, rx_len = read (fd, buf, 100);
  if (rx_len <= 0)
    KOUT(" ");
  tx_len = write(fd, buf, rx_len);
  if (tx_len != rx_len)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int fd_accept(int fd_listen)
{
  int fd;
  struct sockaddr s_addr;
  socklen_t sz = sizeof(s_addr);
  fd = accept( fd_listen, &s_addr, &sz );
  if (fd < 0)
    KOUT(" ");
  return fd;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{
  fd_set infd;
  int ret, fd_echo, fd_listen = socket_listen_unix("/tmp/cloonix_parrot");
  for (;;)
    {
    FD_ZERO(&infd);
    FD_SET(fd_listen, &infd);
    ret = select(fd_listen + 1, &infd, NULL, NULL, NULL);
    if (ret <= 0)
      KOUT(" ");
    if (FD_ISSET(fd_listen, &infd))
      {
      fd_echo = fd_accept(fd_listen);
      FD_ZERO(&infd);
      FD_SET(fd_echo, &infd);
      ret = select(fd_echo + 1, &infd, NULL, NULL, NULL);
      if (ret <= 0)
        KOUT(" ");
      if (FD_ISSET(fd_echo, &infd))
        {
        action_read_and_echo(fd_echo);
        close(fd_echo);
        }
      else
        KOUT(" ");
      }
    }
  return 0;
}
/*--------------------------------------------------------------------------*/
