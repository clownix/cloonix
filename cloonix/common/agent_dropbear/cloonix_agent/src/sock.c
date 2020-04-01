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
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include "commun.h"

/****************************************************************************/
int sock_header_get_size(void)
{
  return 16;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void sock_header_set_info(char *tx,
                          int llid, int len, int type, int val,
                          char **ntx)
{
  tx[0] = ((llid & 0xFF00) >> 8) & 0xFF;
  tx[1] = llid & 0xFF;
  tx[2] = ((len & 0xFF00) >> 8) & 0xFF;
  tx[3] = len & 0xFF;
  tx[4] = ((type & 0xFF00) >> 8) & 0xFF;
  tx[5] = type & 0xFF;
  tx[6] = ((val & 0xFF00) >> 8) & 0xFF;
  tx[7] = val & 0xFF;
  tx[8] = 0xDE;
  tx[9] = 0xAD;
  tx[10] = 0xCA;
  tx[11] = 0xFE;
  tx[12] = 0xDE;
  tx[13] = 0xCA;
  tx[14] = 0xBE;
  tx[15] = 0xAF;
  *ntx = &(tx[16]);
}
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/

/****************************************************************************/
int sock_header_get_info(char *rx,
                          int *llid, int *len, int *type, int *val,
                          char **nrx)
{
  int result = -1;
  *llid = ((rx[0] & 0xFF) << 8) + (rx[1] & 0xFF);
  *len  = ((rx[2] & 0xFF) << 8) + (rx[3] & 0xFF);
  *type = ((rx[4] & 0xFF) << 8) + (rx[5] & 0xFF);
  *val  = ((rx[6] & 0xFF) << 8) + (rx[7] & 0xFF);
  if (((rx[8] & 0xFF) == 0xDE) &&
      ((rx[9] & 0xFF) == 0xAD) &&
      ((rx[10] & 0xFF) == 0xCA) &&
      ((rx[11] & 0xFF) == 0xFE) &&
      ((rx[12] & 0xFF) == 0xDE) &&
      ((rx[13] & 0xFF) == 0xCA) &&
      ((rx[14] & 0xFF) == 0xBE) &&
      ((rx[15] & 0xFF) == 0xAF))
    {
    result = 0;
    }
  else
    {
    *llid = 0;
    *len  = 0;
    *type = 0;
    *val  = 0;
    KERR("%02X %02X %02X %02X %02X %02X %02X %02X",
         (unsigned int) (rx[8] & 0xFF), (unsigned int) (rx[9] & 0xFF),
         (unsigned int) (rx[10] & 0xFF), (unsigned int) (rx[11] & 0xFF),
         (unsigned int) (rx[12] & 0xFF), (unsigned int) (rx[13] & 0xFF),
         (unsigned int) (rx[14] & 0xFF), (unsigned int) (rx[15] & 0xFF));
    }

  *nrx  = &(rx[16]);
  return result;
}
/*--------------------------------------------------------------------------*/




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
static void fd_cloexec(int fd)
{
  int flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    KOUT(" ");
  flags |= FD_CLOEXEC;
  if (fcntl(fd, F_SETFD, flags) == -1)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
/*
static void nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    KOUT(" ");
  flags |= O_NONBLOCK|O_NDELAY;
  if (fcntl(fd, F_SETFL, flags) == -1)
    KOUT(" ");
}
*/
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int sock_client_unix(char *pname)
{
  int len,  sock, result = -1;
  struct sockaddr_un addr;
  if (!test_file_is_socket(pname))
    {
    sock = socket (AF_UNIX, SOCK_STREAM, 0);
    if (sock <= 0)
      KOUT(" ");
    memset (&addr, 0, sizeof (struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, pname);
    len = sizeof (struct sockaddr_un);
//    nonblock(sock);
    fd_cloexec(sock);
    if (connect(sock,(struct sockaddr *) &addr, len))
      close(sock);
    else
      result = sock;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int sock_fd_accept(int fd_listen)
{
  int fd;
  struct sockaddr s_addr;
  socklen_t sz = sizeof(s_addr);
  fd = accept( fd_listen, &s_addr, &sz );
  return fd;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int util_socket_listen_unix(char *pname)
{
  int ret, fd, len;
  struct sockaddr_un serv;
  mode_t old_mask;

  if (strlen(pname) >= 108)
    KOUT("%d", (int)(strlen(pname)));
  old_mask = umask (0000);
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
    if (ret == 0)
      {
      umask (old_mask);
      fd_cloexec(fd);
      }
    else
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

/*****************************************************************************/
int sock_open_virtio_port(char *path)
{
  int fd;
  fd = open(path, O_RDWR);
  return fd;
}
/*--------------------------------------------------------------------------*/

