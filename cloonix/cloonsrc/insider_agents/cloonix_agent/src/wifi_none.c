/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* No radio relayed by the agent: a cnt, a freebsd kvm. */
#include <string.h>
#include <sys/select.h>

#include "glob_common.h"
#include "commun.h"
#include "sock.h"

void wifi_rx_host_frame(int len, char *rx)
{
}

void wifi_events(fd_set *infd)
{
}

void wifi_rx_host_ctrl(int val, int len, char *rx)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *txt = "no wifi radio on this guest";
  memcpy(buf + headsize, txt, strlen(txt) + 1);
  send_to_virtio(0, 0, 0, header_type_wifi_ctrl, header_val_wifi_ko,
                 strlen(txt) + 1, buf);
}

int wifi_get_fd(void)
{
  return -1;
}
