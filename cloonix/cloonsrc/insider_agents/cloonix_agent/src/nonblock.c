/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <poll.h>
#include "glob_common.h"
#include "commun.h"

#define MAX_FD 1000
/* no queue of the agent goes past this: a message beyond is dropped */
#define NONBLOCK_CAP (64 * 1024 * 1024)
typedef struct t_elem
{
  char *buf;
  int len;
  int len_done;
  struct t_elem *next;
} t_elem;
typedef struct t_fd_tx
{
  t_elem *first;
  t_elem *last;
  int queued;
  unsigned long written;
} t_fd_tx;
typedef struct t_fd_chain 
{
  int fd;
  struct t_fd_chain *prev;
  struct t_fd_chain *next;
} t_fd_chain;
static t_fd_tx *g_fd_tx[MAX_FD];
static t_fd_chain *g_head_fd_chain;

static t_fd_tx *get_fd_tx(int fd)
{
  if ((fd >= MAX_FD) || (fd < 0))
    KOUT("%d", fd);
  return (g_fd_tx[fd]);
}

static t_fd_chain *get_fd_chain(int fd)
{
  t_fd_chain *cur = g_head_fd_chain;
  while(cur)
    {
    if (cur->fd == fd)
      break;
    cur = cur->next;
    }
  return cur;
}

static void nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    KOUT(" ");
  flags |= O_NONBLOCK|O_NDELAY;
  if (fcntl(fd, F_SETFL, flags) == -1)
    KOUT(" ");
}

static int do_write(t_fd_tx *fd_tx, int fd, t_elem *elem)
{
  int len, result = 0;
  char *buf = elem->buf + elem->len_done;
  int len_to_do = elem->len - elem->len_done;
  if (len_to_do <= 0)
    KOUT("%d ", len_to_do);
  len = write(fd, buf, len_to_do);
  if (len > 0)
    fd_tx->written += len;
  if (len != len_to_do)
    {
    if (len > 0)
      elem->len_done += len;
    /* the reader gone: the read of its end closes the channel */
    else if ((errno == EPIPE) || (errno == ECONNRESET))
      result = -2;
    else if ((errno != EAGAIN) && (errno != EINTR))
      KERR("WARNING write on fd %d: %s", fd, strerror(errno));
    if (result == 0)
      result = -1;
    }
  return result;
}

static void do_free_first_elem(t_fd_tx *fd_tx)
{
  t_elem *elem = fd_tx->first;
  fd_tx->first = elem->next;
  if (!fd_tx->first)
    fd_tx->last = NULL;
  fd_tx->queued -= elem->len;
  free(elem->buf);
  free(elem);
}

static void continue_writing(int fd)
{
  int res;
  t_fd_tx *fd_tx = get_fd_tx(fd);
  if (fd_tx == NULL)
    KERR("WARNING fd %d: a write event with no queue", fd);
  else
    {
    if (!fd_tx->first)
      KERR("WARNING fd %d: a write event with nothing queued", fd);
    else
      {
      while (fd_tx->first)
        {
        res = do_write(fd_tx, fd, fd_tx->first);
        if (res == -2)
          {
          while (fd_tx->first)
            do_free_first_elem(fd_tx);
          }
        else if (res)
          break;
        else
          do_free_first_elem(fd_tx);
        }
      }
    }
}

void nonblock_send(int fd, char *buf, int len)
{
  t_fd_tx *fd_tx = get_fd_tx(fd);
  t_elem *elem;
  static unsigned long dropped;
  if (fd_tx == NULL)
    KERR("WARNING %d bytes for fd %d, which has no queue", len, fd);
  else if (fd_tx->queued + len > NONBLOCK_CAP)
    {
    dropped += 1;
    KERR("WARNING fd %d: %d bytes queued, past the cap of %d, %lu "
         "message(s) dropped so far", fd, fd_tx->queued, NONBLOCK_CAP,
         dropped);
    }
  else
    {
    elem = (t_elem *) malloc(sizeof(t_elem)); 
    elem->buf = (char *) malloc(len);
    memcpy(elem->buf, buf, len);
    elem->len = len;
    elem->len_done = 0;
    elem->next = NULL;
    if (fd_tx->last)
      fd_tx->last->next = elem;
    else
      fd_tx->first = elem;
    fd_tx->last = elem;
    fd_tx->queued += len;
    }
}

/* KOUT: what is queued for fd goes out, ms at most */
void nonblock_flush(int fd, int ms)
{
  struct pollfd pfd;
  t_fd_tx *fd_tx = get_fd_tx(fd);
  int i;
  for (i = 0; (i < ms / 10) && fd_tx && fd_tx->first; i++)
    {
    pfd.fd = fd;
    pfd.events = POLLOUT;
    if (poll(&pfd, 1, 10) > 0)
      continue_writing(fd);
    }
}

unsigned long nonblock_written(int fd)
{
  t_fd_tx *fd_tx = get_fd_tx(fd);
  return (fd_tx ? fd_tx->written : 0);
}

int nonblock_queued(int fd)
{
  t_fd_tx *fd_tx = get_fd_tx(fd);
  return (fd_tx ? fd_tx->queued : 0);
}

void nonblock_prepare_fd_set(int fd, fd_set *outfd)
{
  t_fd_tx *fd_tx = get_fd_tx(fd);
  if ((fd_tx) && (fd_tx->first))
    {
    FD_SET(fd, outfd);
    }
}

void nonblock_process_events(fd_set *outfd)
{
  t_fd_chain *cur = g_head_fd_chain;
  while(cur)
    {
    if (FD_ISSET(cur->fd, outfd))
      {
      continue_writing(cur->fd);
      }
    cur = cur->next;
    }
}

void nonblock_del_fd(int fd)
{
  t_fd_tx *fd_tx = get_fd_tx(fd);
  t_fd_chain *fd_chain = get_fd_chain(fd);
  t_elem *cur, *next;
  if (fd_tx == NULL)
    KERR("WARNING fd %d: removed, but it has no queue", fd);
  else if (fd_chain == NULL)
    KERR("WARNING fd %d: removed, but it has no queue", fd);
  else
    {
    cur = fd_tx->first;
    while(cur)
      {
      next = cur->next;
      free(cur->buf);
      free(cur);
      cur = next;
      }
    if (fd_chain->prev)
      fd_chain->prev->next = fd_chain->next;
    if (fd_chain->next)
      fd_chain->next->prev = fd_chain->prev;
    if (fd_chain == g_head_fd_chain)
      g_head_fd_chain = fd_chain->next;
    free(fd_tx);
    free(fd_chain);
    g_fd_tx[fd] = NULL;
    }
}

void nonblock_add_fd(int fd)
{
  t_fd_tx *fd_tx = get_fd_tx(fd);
  t_fd_chain *fd_chain = get_fd_chain(fd);
  if (fd_tx)
    KERR("WARNING fd %d: added twice", fd);
  else if (fd_chain)
    KERR("WARNING fd %d: added twice", fd);
  else
    {
    nonblock(fd);
    g_fd_tx[fd] = (t_fd_tx *) malloc(sizeof(t_fd_tx));
    memset(g_fd_tx[fd], 0, sizeof(t_fd_tx));
    fd_chain = (t_fd_chain *) malloc(sizeof(t_fd_chain));
    memset(fd_chain, 0, sizeof(t_fd_chain));
    fd_chain->fd = fd;
    if (g_head_fd_chain)
      g_head_fd_chain->prev = fd_chain;
    fd_chain->next = g_head_fd_chain;
    g_head_fd_chain = fd_chain;    
    }
}

void nonblock_init(void)
{
  memset(g_fd_tx, 0, MAX_FD * sizeof(t_fd_tx *));
  g_head_fd_chain = NULL;
}
