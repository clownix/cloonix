/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/wait.h>

#include "io_clownix.h"
#include "intf_flags.h"
#include "tun_tap.h"

static char g_netns_namespace[MAX_NAME_LEN];
static char g_tap_name[MAX_NAME_LEN];

/* Parent pipe -> tap. EPIPE or ENODEV on the tap means the tap is gone.    */
static void tx_tap(int fd_tap, int fd_rx_from_parent)
{
  uint8_t *frame;
  int len, tx, gap;
  len = tap_child_from_parent(fd_rx_from_parent, &frame, &gap);
  if (len == 0)
    exit(0);
  /* gap: frames the parent dropped at this pipe, it says so itself */
  tx = write(fd_tap, frame, len);
  if ((tx < 0) && ((errno == EPIPE) || (errno == ENODEV)))
    exit(0);
  if (tx != len)
    KOUT("ERROR WRITE TAP %d %d %d", tx, len, errno);
}

/* Second child: only the parent pipe -> tap direction.                     */
static void forked_tx_to_tap(int fd_rx_from_parent, int fd_tap)
{
  int result;
  fd_set infd;
  while(1)
    {
    FD_ZERO(&infd);
    FD_SET(fd_rx_from_parent, &infd);
    result = select(fd_rx_from_parent + 1, &infd, NULL, NULL, NULL);
    if (result < 0)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);
    if (FD_ISSET(fd_rx_from_parent, &infd))
      tx_tap(fd_tap, fd_rx_from_parent);
    }
}

static void exit_if_child_gone(int pid)
{
  siginfo_t infop;
  memset(&infop, 0, sizeof(siginfo_t));
  if ((waitid(P_PID, pid, &infop, WNOHANG|WEXITED) == 0) &&
      (pid == infop.si_pid))
    exit(0);
}

/* First child: tap -> parent pipe, and every second checks its tx child.   */
static void forked_rx_from_tap(int fd_rx_from_parent, int fd_tx_to_parent)
{
  int pid, fd, result;
  fd_set infd;
  struct timeval timeout;
  fd = tap_enter_and_open(g_netns_namespace, g_tap_name);
  if ((pid = fork()) < 0)
    KOUT("ERROR %s", g_tap_name);
  if (pid == 0)
    forked_tx_to_tap(fd_rx_from_parent, fd);
  while(1)
    {
    FD_ZERO(&infd);
    FD_SET(fd, &infd);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    result = select(fd + 1, &infd, NULL, NULL, &timeout);
    if (result < 0)
      KOUT("ERROR %s %s", g_netns_namespace, g_tap_name);
    if (FD_ISSET(fd, &infd))
      {
      if (tap_child_to_parent(fd, fd_tx_to_parent, g_tap_name) == 0)
        exit(0);
      }
    exit_if_child_gone(pid);
    }
}

int tun_tap_open(char *net_namespace, char *name, int *fd_rx, int *fd_tx)
{
  memset(g_netns_namespace, 0, MAX_NAME_LEN);
  strncpy(g_netns_namespace, net_namespace, MAX_NAME_LEN-1);
  memset(g_tap_name, 0, MAX_NAME_LEN);
  strncpy(g_tap_name, name, MAX_NAME_LEN-1);
  return tap_fork_open(g_netns_namespace, g_tap_name, 0,
                       forked_rx_from_tap, fd_rx, fd_tx);
}
