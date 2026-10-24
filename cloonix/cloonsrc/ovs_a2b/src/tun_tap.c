/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/select.h>

#include "io_clownix.h"
#include "intf_flags.h"
#include "tun_tap.h"

static char g_netns_namespace[MAX_NAME_LEN];
static char g_tap_name[MAX_NAME_LEN];
/* the tap and its item, for the KERR of the child */
static char g_tap_label[MAX_PATH_LEN];
char *get_a2b_name(void);

/* Parent pipe -> tap. A short write is fatal: every frame must go through. */
static void tx_tap(int fd_tap, int fd_rx_from_parent)
{
  uint8_t *frame;
  int len, tx, gap;
  len = tap_child_from_parent(fd_rx_from_parent, &frame, &gap);
  if (len == 0)
    exit(0);
  /* gap: frames the parent dropped at this pipe, it says so itself */
  tx = write(fd_tap, frame, len);
  if (tx != len)
    KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
}

static void forked_process_tap(int fd_rx_from_parent, int fd_tx_to_parent)
{
  int fd, fd_max, result;
  fd_set infd;
  fd = tap_enter_and_open(g_netns_namespace, g_tap_name);
  fd_max = fd;
  if (fd_rx_from_parent > fd_max)
    fd_max = fd_rx_from_parent;
  while(1)
    {
    FD_ZERO(&infd);
    FD_SET(fd, &infd);
    FD_SET(fd_rx_from_parent, &infd);
    result = select(fd_max + 1, &infd, NULL, NULL, NULL);
    if (result < 0)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);
    if (FD_ISSET(fd, &infd))
      {
      if (tap_child_to_parent(fd, fd_tx_to_parent, g_tap_label) == 0)
        exit(0);
      }
    if (FD_ISSET(fd_rx_from_parent, &infd))
      tx_tap(fd, fd_rx_from_parent);
    }
}

int tun_tap_open(char *net_namespace, char *name, int *fd_rx, int *fd_tx)
{
  memset(g_netns_namespace, 0, MAX_NAME_LEN);
  strncpy(g_netns_namespace, net_namespace, MAX_NAME_LEN-1);
  memset(g_tap_name, 0, MAX_NAME_LEN);
  strncpy(g_tap_name, name, MAX_NAME_LEN-1);
  memset(g_tap_label, 0, MAX_PATH_LEN);
  snprintf(g_tap_label, MAX_PATH_LEN-1, "a2b %s tap %s", get_a2b_name(), name);
  return tap_fork_open(g_netns_namespace, g_tap_name, 0,
                       forked_process_tap, fd_rx, fd_tx);
}
