/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* SIOCGIFFLAGS and SIOCSIFFLAGS on a named interface, shared by all. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#include "glob_common.h"
#include <fcntl.h>
#include <errno.h>
#include <linux/if_tun.h>
#include "intf_flags.h"
#include "io_clownix.h"

int get_intf_flags_iff(char *intf, int *flags)
{
  int result = -1, s, io;
  struct ifreq ifr;
  s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s < 0)
    KERR("ERROR socket for %s: %s", intf, strerror(errno));
  else
    {
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, intf, IFNAMSIZ-1);
    io = ioctl (s, SIOCGIFFLAGS, &ifr);
    if (io == 0)
      {
      *flags = ifr.ifr_flags;
      result = 0;
      }
    else
      KERR("ERROR SIOCGIFFLAGS %s: %s", intf, strerror(errno));
    close(s);
    }
  return result;
}

int intf_exists(char *intf)
{
  int result = 0, s;
  struct ifreq ifr;
  s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s < 0)
    KERR("ERROR socket for %s: %s", intf, strerror(errno));
  else
    {
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, intf, IFNAMSIZ-1);
    if (ioctl(s, SIOCGIFFLAGS, &ifr) == 0)
      result = 1;
    close(s);
    }
  return result;
}

/* The three setters below differ only in the flags they ask for. */
static int put_intf_flags_iff(char *intf, int flags)
{
  int result = -1, s, io;
  struct ifreq ifr;
  s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s < 0)
    KERR("ERROR socket for %s: %s", intf, strerror(errno));
  else
    {
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, intf, IFNAMSIZ-1);
    ifr.ifr_flags = flags;
    io = ioctl (s, SIOCSIFFLAGS, &ifr);
    if (io == 0)
      result = 0;
    else
      KERR("ERROR SIOCSIFFLAGS %s: %s", intf, strerror(errno));
    close(s);
    }
  return result;
}

int set_intf_flags_iff_promisc(char *intf)
{
  int result = -1, flags;
  if (!get_intf_flags_iff(intf, &flags))
    result = put_intf_flags_iff(intf, flags | IFF_PROMISC);
  return result;
}

int set_intf_flags_iff_up_and_promisc(char *intf)
{
  int result = -1, flags;
  if (!get_intf_flags_iff(intf, &flags))
    result = put_intf_flags_iff(intf,
                                flags | IFF_UP | IFF_RUNNING | IFF_PROMISC);
  return result;
}

int set_intf_flags_iff_up_down(char *intf, int up)
{
  int result = -1, flags;
  if (!get_intf_flags_iff(intf, &flags))
    {
    if (up)
      flags |= IFF_UP;
    else
      flags &= ~IFF_UP;
    result = put_intf_flags_iff(intf, flags);
    }
  return result;
}

/* Open a tap of that name owned by the setuid caller, -1 with err set. */
int tap_open(char *tap, char *err)
{
  int fd;
  struct ifreq ifr;
  uid_t owner = cloonix_caller_uid();
  if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
    {
    sprintf(err, "\nError open \"/dev/net/tun\", %d\n", errno);
    return -1;
    }
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  strncpy(ifr.ifr_name, tap, sizeof(ifr.ifr_name) - 1);
  if (intf_exists(tap))
    sprintf(err, "\nError system already has %s\n\n", tap);
  else if (ioctl(fd, TUNSETIFF, (void *) &ifr) < 0)
    sprintf(err, "\nError ioctl TUNSETIFF \"%s\", %d", tap,  errno);
  else if (ioctl(fd, TUNSETOWNER, owner) < 0)
    sprintf(err, "\nError ioctl TUNSETOWNER \"%s\", %d", tap,  errno);
  else if (ioctl(fd, TUNSETGROUP, owner) < 0)
    sprintf(err, "\nError ioctl TUNSETGROUP \"%s\", %d", tap,  errno);
  else
    return fd;
  close(fd);
  return -1;
}

void tap_no_ipv6(char *tap)
{
  char path[MAX_PATH_LEN];
  int fd;
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1,
           "/proc/sys/net/ipv6/conf/%s/disable_ipv6", tap);
  fd = open(path, O_WRONLY);
  if (fd >= 0)
    {
    if (write(fd, "1", 1) != 1)
      KERR("WARNING ipv6 stays on %s: %s", tap, strerror(errno));
    close(fd);
    }
}

int tap_enter_and_open(char *netns, char *tap)
{
  int fd;
  char err[MAX_PATH_LEN];
  fd = open(netns, O_RDONLY|O_CLOEXEC);
  if (fd < 0)
    KOUT("ERROR %s %s", netns, tap);
  if (setns(fd, CLONE_NEWNET) != 0)
    KOUT("ERROR %s %s", netns, tap);
  close(fd);
  fd = tap_open(tap, err);
  if (fd < 0)
    KOUT("ERROR %s %s %s", netns, tap, err);
  tap_no_ipv6(tap);
  if (set_intf_flags_iff_up_and_promisc(tap))
    KOUT("ERROR %s %s", netns, tap);
  return fd;
}

int tap_fork_open(char *netns, char *tap, int pipe_size,
                  void (*child_fn)(int fd_rx_from_parent, int fd_tx_to_parent),
                  int *fd_rx, int *fd_tx)
{
  int pid;
  int fd1[2];
  int fd2[2];
  if (pipe(fd1) == -1)
    KOUT("ERROR %s %s", netns, tap);
  if (pipe(fd2) == -1)
    KOUT("ERROR %s %s", netns, tap);
  if (pipe_size)
    {
    if (fcntl(fd2[0], F_SETPIPE_SZ, pipe_size) < 0)
      KERR("WARNING could not enlarge fd2 pipe to %d errno=%d",
           pipe_size, errno);
    if (fcntl(fd1[0], F_SETPIPE_SZ, pipe_size) < 0)
      KERR("WARNING could not enlarge fd1 pipe to %d errno=%d",
           pipe_size, errno);
    }
  if ((pid = fork()) < 0)
    KOUT("ERROR %s %s", netns, tap);
  /* Non blocking both ways (blocked writers deadlocked under a flood): a
       frame is dropped instead, atomically (< PIPE_BUF). */
  if (pid == 0)
    {
    close(fd1[0]);
    close(fd2[1]);
    fcntl(fd1[1], F_SETFL, fcntl(fd1[1], F_GETFL) | O_NONBLOCK);
    child_fn(fd2[0], fd1[1]);
    KOUT("ERROR %s %s", netns, tap);
    }
  close(fd1[1]);
  close(fd2[0]);
  fcntl(fd2[1], F_SETFL, fcntl(fd2[1], F_GETFL) | O_NONBLOCK);
  *fd_rx = fd1[0];
  *fd_tx = fd2[1];
  return pid;
}

/* Child side of tap_fork_open: one seqtap frame each way, the tap is never
   written here. Return frame length, 0 peer gone, KOUT on a corrupt pipe. */
static uint8_t g_child_rx[HEADER_TAP_MSG+TRAF_TAP_BUF_LEN+END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_child_tx[HEADER_TAP_MSG+TRAF_TAP_BUF_LEN+END_FRAME_ADDED_CHECK_LEN];

int tap_child_from_parent(int fd_rx_from_parent, uint8_t **frame, int *gap)
{
  static uint16_t seqtap = 0;
  uint16_t seq;
  int len, buf_len, result;
  *frame = NULL;
  *gap = 0;
  len = read(fd_rx_from_parent, g_child_rx, HEADER_TAP_MSG);
  if (len == 0)
    return 0;
  if (len != HEADER_TAP_MSG)
    KOUT("ERROR READ %d %d", len, errno);
  result = fct_seqtap_rx(fd_rx_from_parent, g_child_rx, &seq, &buf_len, frame);
  if (result != kind_seqtap_data)
    KOUT("ERROR %d", result);
  if (seq != ((seqtap+1)&0xFFFF))
    *gap = (int)((uint16_t)(seq - seqtap)) - 1;
  seqtap = seq;
  return buf_len;
}

int tap_child_to_parent(int fd_tap, int fd_tx_to_parent, char *tap)
{
  static uint16_t seqtap = 0;
  int len, tx;
  len = read(fd_tap, g_child_tx + HEADER_TAP_MSG,
             TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN);
  if ((len == 0) || ((len < 0) && (errno == ENODEV)))
    return 0;
  if ((len < 0) || (len > TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR READ TAP %s %d %d", tap, len, errno);
  seqtap += 1;
  fct_seqtap_tx(kind_seqtap_data, g_child_tx, seqtap, len, NULL);
  tx = write(fd_tx_to_parent, g_child_tx, len + HEADER_TAP_MSG);
  if ((tx < 0) && (errno == EPIPE))
    return 0;
  if ((tx < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
    {
    static unsigned long dropped, last;
    unsigned long now = (unsigned long) time(NULL);
    dropped += 1;
    if (now - last >= 10)
      {
      last = now;
      KERR("WARNING %s: the parent is not taking frames, %lu dropped so "
           "far at its pipe", tap, dropped);
      }
    return len;
    }
  if (tx != len + HEADER_TAP_MSG)
    KOUT("ERROR WRITE %s %d %d %d", tap, tx, len, errno);
  return len;
}
