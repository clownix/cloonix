/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* cloonix_fanout.c - one capture, many receivers. The dumpcap runs as long
   as the snf and serves the same stream to every subscriber.
   Stdout of the capture loop is replaced by a pipe (dup2 onto fd 1) read in
   a thread; the loop keeps writing plain pcap and never knows.
   Records, not bytes: a late receiver gets the 24 byte file header kept from
   the start, then whole records only.
   A slow receiver loses its own records: bounded queue per receiver, non
   blocking writes, dropped records counted per receiver; nothing ever stalls
   the capture loop. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "cloonix_fanout.h"

#define FANOUT_MAX_RCV      16          /* receivers at once                 */
#define FANOUT_QUEUE_MAX    (4*1024*1024) /* per receiver, bytes            */
#define FANOUT_HDR_LEN      24          /* pcap file header                  */
#define FANOUT_REC_HDR_LEN  16          /* pcap record header                */
#define FANOUT_MAX_CAPLEN   (2*1024*1024) /* refuse an absurd incl_len      */

/* The two fixed names, built from the snf path. Hardcoded on purpose: a path */
/* passed on a command line is a path two programs can spell differently.     */
#define FANOUT_SOCK_SUFFIX  "_pcap_sock"
#define FANOUT_FIFO_SUFFIX  "_pcap_fifo"
#define FANOUT_LOG_SUFFIX   "_pcap_log"

/* One receiver: a tcp client or the duplicate fifo. Growable buffer drained from the front. */
typedef struct t_rcv
{
  int            fd;
  int            is_fifo;
  char          *q;
  int            q_len;
  int            q_off;
  int            q_max;
  unsigned long  sent;
  unsigned long  dropped;
} t_rcv;

/* All output goes to <snf>_pcap_log (no terminal). FLOG nominal, FERR faults. */
#define FANOUT_TRACE 1

#define FERR(fmt, ...) \
  fprintf(stderr, "fanout ERROR %s:%d " fmt "\n", \
          __FUNCTION__, __LINE__, ##__VA_ARGS__)

#if FANOUT_TRACE
#define FLOG(fmt, ...) \
  fprintf(stderr, "fanout INFO  %s:%d " fmt "\n", \
          __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define FLOG(fmt, ...) do { } while (0)
#endif

static t_rcv         g_rcv[FANOUT_MAX_RCV];
static int           g_nb_rcv;
static int           g_listen_fd = -1;
static int           g_pipe_rd   = -1;
static int           g_stdout_fd = -1;
static char          g_fifo_path[512];
static char          g_sock_path[512];
static unsigned char g_file_hdr[FANOUT_HDR_LEN];
static int           g_file_hdr_ok;
static int           g_swapped;
static unsigned long g_records;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

static void set_nonblock(int fd)
{
  int fl = fcntl(fd, F_GETFL, 0);
  if (fl < 0)
    FERR("fcntl F_GETFL on fd %d: %s", fd, strerror(errno));
  else if (fcntl(fd, F_SETFL, fl | O_NONBLOCK) < 0)
    FERR("fcntl F_SETFL on fd %d: %s", fd, strerror(errno));
}

/* Exactly len bytes from the capture pipe; short reads loop, 0 means capture ended. */
static int read_exact(int fd, void *buf, int len)
{
  int done = 0;
  ssize_t n;
  while (done < len)
    {
    n = read(fd, ((char *)buf) + done, len - done);
    if (n == 0)
      return 0;
    if (n < 0)
      {
      if ((errno == EINTR) || (errno == EAGAIN))
        continue;
      FERR("read of %d bytes from the capture pipe: %s", len, strerror(errno));
      return -1;
      }
    done += (int) n;
    }
  return 1;
}

static unsigned int get_u32(const unsigned char *p, int swapped)
{
  unsigned int v = ((unsigned int) p[0]) | (((unsigned int) p[1]) << 8) |
                   (((unsigned int) p[2]) << 16) | (((unsigned int) p[3]) << 24);
  if (swapped)
    v = ((v & 0xff000000) >> 24) | ((v & 0x00ff0000) >> 8) |
        ((v & 0x0000ff00) << 8)  | ((v & 0x000000ff) << 24);
  return v;
}

/* Queue bytes for a receiver; the poll loop writes. Returns 0 when dropped. */
static int rcv_queue(t_rcv *r, const unsigned char *buf, int len)
{
  char *nq;
  if (r->fd < 0)
    return 0;
  /* Compact first: q_off bytes at the front have already gone out. The old
     test compacted only when even the live bytes overflowed, so a queue
     with room could still refuse: a header went, its payload did not, and
     the reader downstream announced a record of four billion bytes. */
  if ((r->q_off > 0) && (r->q_len + len > r->q_max))
    {
    memmove(r->q, r->q + r->q_off, r->q_len - r->q_off);
    r->q_len -= r->q_off;
    r->q_off = 0;
    }
  if (r->q_len + len > r->q_max)
    {
    r->dropped++;
    if (r->dropped == 1)
      FERR("receiver fd %d is behind, its %d byte queue is full, dropping",
           r->fd, r->q_max);
    return 0;
    }
  nq = (char *) realloc(r->q, r->q_len + len);
  if (!nq)
    {
    r->dropped++;
    FERR("out of memory growing the queue of fd %d to %d bytes",
         r->fd, r->q_len + len);
    return 0;
    }
  r->q = nq;
  memcpy(r->q + r->q_len, buf, len);
  r->q_len += len;
  return 1;
}

static void rcv_close(t_rcv *r)
{
  if (r->fd >= 0)
    {
    close(r->fd);
    g_nb_rcv--;
    }
  r->fd = -1;
  free(r->q);
  r->q = NULL;
  r->q_len = 0;
  r->q_off = 0;
}

/* Drain what is queued. EAGAIN simply means "later": the fd stays in the     */
/* poll set with POLLOUT and the rest goes on the next turn.                  */
static void rcv_flush(t_rcv *r)
{
  ssize_t n;
  while (r->q_off < r->q_len)
    {
    n = write(r->fd, r->q + r->q_off, r->q_len - r->q_off);
    if (n > 0)
      {
      r->q_off += (int) n;
      continue;
      }
    if ((n < 0) && ((errno == EAGAIN) || (errno == EINTR)))
      return;
    /* EPIPE on the fifo only means webshark went away; the fifo itself is
     * kept, so the next webshark finds it there and simply reconnects. */
    FLOG("receiver fd %d (%s) went away: %s, %lu sent, %lu dropped",
         r->fd, r->is_fifo ? "fifo" : "socket",
         (n < 0) ? strerror(errno) : "closed", r->sent, r->dropped);
    rcv_close(r);
    return;
    }
  r->q_off = 0;
  r->q_len = 0;
}

static void rcv_add(int fd, int is_fifo)
{
  int i;
  if (g_nb_rcv >= FANOUT_MAX_RCV)
    {
    FERR("refusing fd %d: already serving the %d receivers allowed",
         fd, FANOUT_MAX_RCV);
    close(fd);
    return;
    }
  for (i = 0; i < FANOUT_MAX_RCV; i++)
    {
    if (g_rcv[i].fd < 0)
      break;
    }
  if (i == FANOUT_MAX_RCV)
    {
    FERR("refusing fd %d: no free slot although %d of %d are in use",
         fd, g_nb_rcv, FANOUT_MAX_RCV);
    close(fd);
    return;
    }
  set_nonblock(fd);
  memset(&(g_rcv[i]), 0, sizeof(t_rcv));
  g_rcv[i].fd      = fd;
  g_rcv[i].is_fifo = is_fifo;
  g_rcv[i].q_max   = FANOUT_QUEUE_MAX;
  g_nb_rcv++;
  FLOG("receiver %d added on fd %d (%s), %d now attached",
       i, fd, is_fifo ? "fifo" : "socket", g_nb_rcv);
  /* A receiver that arrives mid-capture needs a file header of its own
   * before any record, or everything downstream reads nonsense. */
  if (g_file_hdr_ok)
    {
    FLOG("giving receiver %d the pcap file header, it joined mid capture", i);
    rcv_queue(&(g_rcv[i]), g_file_hdr, FANOUT_HDR_LEN);
    }
  else
    FLOG("receiver %d waits, no pcap file header seen yet", i);
}

/* The duplicate fifo, opened O_WRONLY|O_NONBLOCK: ENXIO while no webshark
   reads, EPIPE when the reader leaves. Each open is a fresh receiver with its
   own file header (O_RDWR never saw readers come and go). */
static int fifo_open(const char *path)
{
  int fd = open(path, O_WRONLY | O_NONBLOCK);
  if ((fd < 0) && (errno != ENXIO))
    FERR("open %s: %s", path, strerror(errno));
  return fd;
}

static int fifo_attached(void)
{
  int i;
  for (i = 0; i < FANOUT_MAX_RCV; i++)
    {
    if ((g_rcv[i].fd >= 0) && (g_rcv[i].is_fifo))
      return 1;
    }
  return 0;
}

static void fifo_try_attach(void)
{
  int fd;
  if ((!g_fifo_path[0]) || fifo_attached())
    return;
  fd = fifo_open(g_fifo_path);
  if (fd >= 0)
    {
    FLOG("a reader opened the fifo %s", g_fifo_path);
    rcv_add(fd, 1);
    }
}

/* The subscriber socket: unix, never a port. The switch connects and relays
   into the rpc. Path from the command line, carrying name and eth. */
static int listen_unix(const char *path)
{
  int fd;
  struct sockaddr_un addr;
  if (strlen(path) >= sizeof(addr.sun_path))
    {
    FERR("%s is %d chars, too long for a unix socket path", path,
         (int) strlen(path));
    return -1;
    }
  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
  /* A socket left by a previous run binds nothing and refuses everything. */
  unlink(path);
  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)))
    {
    FERR("bind %s: %s", path, strerror(errno));
    close(fd);
    return -1;
    }
  if (listen(fd, 8))
    {
    FERR("listen on %s: %s", path, strerror(errno));
    close(fd);
    return -1;
    }
  set_nonblock(fd);
  FLOG("pcap socket %s listening on fd %d", path, fd);
  return fd;
}

static void fanout_to_all(const unsigned char *buf, int len)
{
  int i;
  for (i = 0; i < FANOUT_MAX_RCV; i++)
    {
    if (g_rcv[i].fd >= 0)
      rcv_queue(&(g_rcv[i]), buf, len);
    }
}

/* Service the descriptors without blocking, then come back for more capture. */
/* Zero timeout: the capture pipe is the thing we must not keep waiting.      */
static void poll_once(int timeout_ms)
{
  struct pollfd pfd[FANOUT_MAX_RCV + 1];
  int idx[FANOUT_MAX_RCV + 1];
  int i, nb = 0, cli;

  fifo_try_attach();
  if (g_listen_fd >= 0)
    {
    pfd[nb].fd = g_listen_fd;
    pfd[nb].events = POLLIN;
    idx[nb] = -1;
    nb++;
    }
  for (i = 0; i < FANOUT_MAX_RCV; i++)
    {
    if ((g_rcv[i].fd >= 0) && (g_rcv[i].q_off < g_rcv[i].q_len))
      {
      pfd[nb].fd = g_rcv[i].fd;
      pfd[nb].events = POLLOUT;
      idx[nb] = i;
      nb++;
      }
    }
  if (nb == 0)
    return;
  if (poll(pfd, nb, timeout_ms) <= 0)
    return;
  for (i = 0; i < nb; i++)
    {
    if (idx[i] == -1)
      {
      if (pfd[i].revents & POLLIN)
        {
        cli = accept(g_listen_fd, NULL, NULL);
        if (cli < 0)
          {
          if ((errno != EAGAIN) && (errno != EINTR))
            FERR("accept on the pcap socket: %s", strerror(errno));
          }
        else
          {
          FLOG("a subscriber connected on the pcap socket");
          rcv_add(cli, 0);
          }
        }
      }
    else if (pfd[i].revents & (POLLOUT | POLLERR | POLLHUP))
      rcv_flush(&(g_rcv[idx[i]]));
    }
}

/* The fan-out thread: read one pcap record, give it to everybody, repeat.    */
static void *fanout_thread(void *arg)
{
  unsigned char rec[FANOUT_REC_HDR_LEN];
  unsigned char *pkt = NULL;
  unsigned int magic, caplen;
  int r;
  (void) arg;

  if (read_exact(g_pipe_rd, g_file_hdr, FANOUT_HDR_LEN) != 1)
    {
    FERR("no pcap file header arrived: the capture never started, or the "
         "dumpcap wrote nothing to its stdout");
    return NULL;
    }
  magic = get_u32(g_file_hdr, 0);
  if ((magic == 0xa1b2c3d4) || (magic == 0xa1b23c4d))
    g_swapped = 0;
  else if ((magic == 0xd4c3b2a1) || (magic == 0x4d3cb2a1))
    g_swapped = 1;
  else
    {
    FERR("0x%08x is not a pcap magic, the stream on fd 1 is not a capture",
         magic);
    return NULL;
    }
  pthread_mutex_lock(&g_mutex);
  g_file_hdr_ok = 1;
  /* Anyone who connected before the first packet is still waiting for it. */
  fanout_to_all(g_file_hdr, FANOUT_HDR_LEN);
  pthread_mutex_unlock(&g_mutex);

  FLOG("pcap file header seen, magic 0x%08x, %s endian, serving now",
       magic, g_swapped ? "opposite" : "same");
  pkt = (unsigned char *) malloc(FANOUT_MAX_CAPLEN);
  if (!pkt)
    {
    FERR("out of memory for the %d byte packet buffer", FANOUT_MAX_CAPLEN);
    return NULL;
    }

  while (1)
    {
    struct pollfd cp = { .fd = g_pipe_rd, .events = POLLIN };
    poll_once(0);
    /* A quiet capture must not stop the queues from draining nor a new
     * reader from being attached: wait for a record, but not for ever. */
    if (poll(&cp, 1, 200) == 0)
      continue;
    r = read_exact(g_pipe_rd, rec, FANOUT_REC_HDR_LEN);
    if (r != 1)
      break;
    caplen = get_u32(rec + 8, g_swapped);
    if (caplen > FANOUT_MAX_CAPLEN)
      {
      FERR("a record announces %u bytes, over the %d limit: the stream has "
           "lost its place and cannot be recovered", caplen, FANOUT_MAX_CAPLEN);
      break;
      }
    if (caplen && (read_exact(g_pipe_rd, pkt, (int) caplen) != 1))
      break;
    pthread_mutex_lock(&g_mutex);
    /* Header and payload are queued together so a full queue drops a whole
     * record rather than half of one. */
    for (r = 0; r < FANOUT_MAX_RCV; r++)
      {
      if (g_rcv[r].fd < 0)
        continue;
      if (g_rcv[r].q_len - g_rcv[r].q_off + FANOUT_REC_HDR_LEN + (int) caplen
          > g_rcv[r].q_max)
        {
        g_rcv[r].dropped++;
        continue;
        }
      if (rcv_queue(&(g_rcv[r]), rec, FANOUT_REC_HDR_LEN))
        {
        if (caplen && (!rcv_queue(&(g_rcv[r]), pkt, (int) caplen)))
          {
          /* Never a header without its payload: take the header back. */
          g_rcv[r].q_len -= FANOUT_REC_HDR_LEN;
          continue;
          }
        g_rcv[r].sent++;
        }
      }
    g_records++;
    if (g_records == 1)
      FLOG("first record forwarded, %s", cloonix_fanout_stats());
    else if ((g_records % 10000) == 0)
      FLOG("still going, %s", cloonix_fanout_stats());
    pthread_mutex_unlock(&g_mutex);
    poll_once(0);
    }
  free(pkt);
  /* Drain what is left before going, so a receiver that was only a little
   * behind still gets the end of the capture. */
  for (r = 0; r < 200; r++)
    poll_once(5);
  FLOG("capture ended, %s", cloonix_fanout_stats());
  return NULL;
}

char *cloonix_fanout_stats(void)
{
  static char txt[256];
  int i, nb = 0;
  unsigned long drop = 0;
  for (i = 0; i < FANOUT_MAX_RCV; i++)
    {
    if (g_rcv[i].fd >= 0)
      nb++;
    drop += g_rcv[i].dropped;
    }
  snprintf(txt, sizeof(txt),
           "%d receivers, %lu records, %lu dropped", nb, g_records, drop);
  return txt;
}

int cloonix_fanout_names(const char *snf_path, char *sock, int sock_len,
                         char *fifo, int fifo_len)
{
  if ((!snf_path) || (!snf_path[0]))
    return -1;
  if (sock)
    snprintf(sock, sock_len, "%s%s", snf_path, FANOUT_SOCK_SUFFIX);
  if (fifo)
    snprintf(fifo, fifo_len, "%s%s", snf_path, FANOUT_FIFO_SUFFIX);
  return 0;
}

/* stderr to a file beside the snf, truncated each start: the switch starts
   this with no terminal, and a silent death left only ECONNREFUSED. */
static void redirect_stderr(const char *snf_path)
{
  char log_path[512];
  int fd;
  snprintf(log_path, sizeof(log_path), "%s%s", snf_path, FANOUT_LOG_SUFFIX);
  fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0)
    return;
  dup2(fd, 2);
  if (fd != 2)
    close(fd);
  setvbuf(stderr, NULL, _IOLBF, 0);
}

/* fd 0, 1, 2 must exist before pipe(): with fd 1 closed, pipe() may return
   it as an end, dup2 is a no-op and close(pfd[1]) kills the capture. */
static void ensure_std_fds(void)
{
  int fd;
  int i;
  for (i = 0; i < 3; i++)
    {
    if (fcntl(i, F_GETFD) != -1)
      continue;
    fd = open("/dev/null", O_RDWR);
    if (fd < 0)
      return;
    if (fd != i)
      {
      dup2(fd, i);
      close(fd);
      }
    }
}

int cloonix_fanout_start(const char *snf_path, int keep_stdout)
{
  char sock_path[512];
  char fifo_path[512];
  int pfd[2], i;

  ensure_std_fds();
  redirect_stderr(snf_path);

  if (cloonix_fanout_names(snf_path, sock_path, sizeof(sock_path),
                                     fifo_path, sizeof(fifo_path)))
    {
    FERR("no snf path given, nothing can be served");
    return -1;
    }
  FLOG("starting for %s (pid %d)", snf_path, (int) getpid());
  pthread_t tid;
  pthread_attr_t attr;

  for (i = 0; i < FANOUT_MAX_RCV; i++)
    g_rcv[i].fd = -1;
  g_nb_rcv = 0;

  /* A receiver going away must not kill the dumpcap. Every write here is
   * checked, so the signal has nothing left to tell us. */
  signal(SIGPIPE, SIG_IGN);

  if (keep_stdout)
    {
    g_stdout_fd = dup(1);
    if (g_stdout_fd >= 0)
      rcv_add(g_stdout_fd, 0);
    }

  if (fifo_path[0])
    {
    snprintf(g_fifo_path, sizeof(g_fifo_path), "%s", fifo_path);
    unlink(g_fifo_path);
    if (mkfifo(g_fifo_path, 0666))
      {
      FERR("mkfifo %s: %s", g_fifo_path, strerror(errno));
      g_fifo_path[0] = 0;
      }
    else
      FLOG("fifo %s made, opened when a reader comes", g_fifo_path);
    }

  if (sock_path[0])
    {
    snprintf(g_sock_path, sizeof(g_sock_path), "%s", sock_path);
    g_listen_fd = listen_unix(g_sock_path);
    if (g_listen_fd < 0)
      FERR("no server on %s, subscribers will get ECONNREFUSED",
           g_sock_path);
    }

  if ((!g_fifo_path[0]) && (g_listen_fd < 0))
    {
    FERR("neither the fifo nor the socket could be made, leaving stdout "
         "alone and serving nobody");
    return -1;
    }

  if (pipe(pfd))
    {
    FERR("pipe: %s", strerror(errno));
    return -1;
    }
  /* From here the capture loop's fd 1 is ours. It still writes plain pcap to
   * what it believes is stdout, and never learns otherwise. */
  if (pfd[1] != 1)
    {
    if (dup2(pfd[1], 1) < 0)
      {
      FERR("dup2 of the pipe onto stdout: %s", strerror(errno));
      close(pfd[0]);
      close(pfd[1]);
      return -1;
      }
    /* Only when it is not fd 1 itself (see the /dev/null note above). */
    close(pfd[1]);
    }
  g_pipe_rd = pfd[0];

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&tid, &attr, fanout_thread, NULL))
    {
    FERR("pthread_create for the fan-out thread failed: %s",
         strerror(errno));
    pthread_attr_destroy(&attr);
    return -1;
    }
  pthread_attr_destroy(&attr);
  FLOG("serving fifo=%s sock=%s, waiting for the capture to start",
       g_fifo_path[0] ? g_fifo_path : "none",
       g_sock_path[0] ? g_sock_path : "none");
  return 0;
}
