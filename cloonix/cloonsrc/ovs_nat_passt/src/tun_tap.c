/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/select.h>

#include "io_clownix.h"
#include "intf_flags.h"
#include "tun_tap.h"

/* 1 MB pipes: the passt->tap pipe is the bottleneck. */
#define TAP_PIPE_SIZE (1024 * 1024)

static char g_netns_namespace[MAX_NAME_LEN];
static char g_tap_name[MAX_NAME_LEN];

/* Parent pipe -> tap. Gaps are dropped on purpose upstream: counted only.  */
/* A refused tap write is a dropped frame, the guest may be resetting.      */
static void tx_tap(int fd_tap, int fd_rx_from_parent)
{
  static unsigned long long gap_total = 0;
  static unsigned long long gap_since_log = 0;
  uint8_t *frame;
  int len, tx, gap;
  len = tap_child_from_parent(fd_rx_from_parent, &frame, &gap);
  if (len == 0)
    {
    KTRC("the parent closed the pipe, leaving");
    exit(0);
    }
  if (gap > 0)
    {
    gap_total += (unsigned long long)gap;
    gap_since_log += (unsigned long long)gap;
    if (gap_since_log >= 1000)
      {
      KTRC("tap seq gaps: +%llu (total=%llu) frames dropped upstream",
           gap_since_log, gap_total);
      gap_since_log = 0;
      }
    }
  tx = write(fd_tap, frame, len);
  if (tx != len)
    {
    if ((tx < 0) &&
        ((errno == EAGAIN) || (errno == EWOULDBLOCK) ||
         (errno == EPIPE)  || (errno == ECONNRESET) ||
         (errno == ENXIO)  || (errno == EIO) || (errno == EPERM)))
      KERR("WARNING tap write dropped frame %s len=%d errno=%d (guest reset?)",
           g_tap_name, len, errno);
    else
      KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
    }
}

/* The parent pipe is served before the tap: the passt side is the one that */
/* backs up.                                                                */
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
    if (FD_ISSET(fd_rx_from_parent, &infd))
      tx_tap(fd, fd_rx_from_parent);
    else if (FD_ISSET(fd, &infd))
      {
      if (tap_child_to_parent(fd, fd_tx_to_parent, g_tap_name) == 0)
        KOUT("ERROR READ TAP %s gone", g_tap_name);
      }
    }
}

int tun_tap_open(char *net_namespace, char *name, int *fd_rx, int *fd_tx)
{
  memset(g_netns_namespace, 0, MAX_NAME_LEN);
  strncpy(g_netns_namespace, net_namespace, MAX_NAME_LEN-1);
  memset(g_tap_name, 0, MAX_NAME_LEN);
  strncpy(g_tap_name, name, MAX_NAME_LEN-1);
  return tap_fork_open(g_netns_namespace, g_tap_name, TAP_PIPE_SIZE,
                       forked_process_tap, fd_rx, fd_tx);
}
