/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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
#include "io_clownix.h"

int sock_nonblock_client_unix(char *pname);

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

int sock_nonblock_client_unix(char *pname)
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
    nonblock_fd(sock);
    if (connect(sock,(struct sockaddr *) &addr, len))
      close(sock);
    else
      result = sock;
    }
  return result;
}

