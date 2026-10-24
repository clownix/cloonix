/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The agent of a cnt: doorways connects to the socket of mountbear. */
#include <stdio.h>
#include <sys/select.h>

#include "glob_common.h"
#include "sock.h"
#include "commun.h"

int main(int argc, char *argv[])
{
  int fd, fd_listen;
  fd_set infd;
  agent_init();
  fd_listen = util_socket_listen_unix(CRUNSOCK);
  if (fd_listen < 0)
    KOUT("Bad crun socket %s", CRUNSOCK);
  FD_ZERO(&infd);
  FD_SET(fd_listen, &infd);
  if ((select(fd_listen + 1, &infd, NULL, NULL, NULL) <= 0) ||
      (!FD_ISSET(fd_listen, &infd)))
    KOUT("Bad crun socket %s", CRUNSOCK);
  fd = sock_fd_accept(fd_listen);
  if (fd < 0)
    KOUT("Bad crun socket %s", CRUNSOCK);
  agent_run(fd);
  return 0;
}
