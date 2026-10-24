/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The agent of a kvm: the host is at the end of a virtio port. */
#include <stdio.h>
#include <sys/select.h>

#include "glob_common.h"
#include "sock.h"
#include "commun.h"

int main(int argc, char *argv[])
{
  int fd;
  agent_init();
  fd = sock_open_virtio_port(VIRTIOPORT);
  if (fd < 0)
    KOUT("Bad Virtio port %s", VIRTIOPORT);
  agent_run(fd);
  return 0;
}
