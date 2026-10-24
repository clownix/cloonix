/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include "glob_common.h"
#include "commun.h"

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

static void fd_cloexec(int fd)
{
  int flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    KOUT(" ");
  flags |= FD_CLOEXEC;
  if (fcntl(fd, F_SETFD, flags) == -1)
    KOUT(" ");
}

int sock_client_unix(char *pname)
{
  int len,  sock, result = -1;
  struct sockaddr_un addr;
  if (!test_file_is_socket(pname))
    {
    sock = socket (AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0)
      KOUT("ERROR socket(AF_UNIX): %s", strerror(errno));
    memset (&addr, 0, sizeof (struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, pname);
    len = sizeof (struct sockaddr_un);
    fd_cloexec(sock);
    if (connect(sock,(struct sockaddr *) &addr, len))
      close(sock);
    else
      result = sock;
    }
  return result;
}

int sock_fd_accept(int fd_listen)
{
  int fd;
  struct sockaddr s_addr;
  socklen_t sz = sizeof(s_addr);
  fd = accept( fd_listen, &s_addr, &sz );
  return fd;
}

int util_socket_listen_unix(char *pname)
{
  int ret, fd, len;
  struct sockaddr_un serv;

  if (strlen(pname) >= 108)
    KOUT("%d", (int)(strlen(pname)));
  unlink (pname);
  fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    KOUT("%d %d", fd, errno);
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

int sock_open_virtio_port(char *path)
{
  int fd;
  fd = open(path, O_RDWR);
  if (fd >= 0)
    {
    if (isatty(fd))
      {
      struct termios tio;
      if (!tcgetattr(fd, &tio))
        {
        cfmakeraw(&tio);
        /* No flow control and no modem lines: this is a virtio channel
           pretending to be a tty, not a serial cable. */
        tio.c_cflag |= CLOCAL | CREAD;
        tio.c_cc[VMIN] = 1;
        tio.c_cc[VTIME] = 0;
        if (tcsetattr(fd, TCSANOW, &tio))
          KERR("ERROR tcsetattr on %s", path);
        }
      else
        KERR("ERROR tcgetattr on %s", path);
      }
    }
  return fd;
}

