/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The nat engine on the parent side of the tun_tap fork+pipe bridge: */
/* pipe fd_rx --[seqtap]--> THIS --[u32 be length + frame]--> passt sock */
/* pipe fd_tx <--[seqtap]-- THIS <--[u32 be length + frame]-- passt sock */
/* The passt socket is a byte stream: passt_to_tap keeps a reassembly */
/* buffer. Writes to passt are non-blocking with a bounded backlog, and */
/* the tap is stopped past a high-water mark, so the control link is always */
/* served and the watchdog never starves. */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "glob_common.h"
#include "passt_shuttle.h"
#include "machine.h"

#define PASST_LEN_PREFIX 4
#define MAX_FRAME (TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN)

/* Set to 1 for per-frame logging (extremely loud), 0 for summary-only. */
#define SHUTTLE_LOUD 0

#define SHUTTLE_INFO 0

/* Backlog flow-control thresholds (bytes queued toward passt). */
#define BACKLOG_HIGH_WATER (4 * 1024 * 1024)   /* stop reading tap above this */
#define BACKLOG_LOW_WATER  (1 * 1024 * 1024)   /* resume reading below this */
#define BACKLOG_HARD_MAX   (64 * 1024 * 1024)  /* absolute cap -> KOUT */

#define TAPTX_HARD_MAX     (8 * 1024 * 1024)

#define TAPTX_HIGH_WATER   (2 * 1024 * 1024)
#define TAPTX_LOW_WATER    (512 * 1024)

static uint8_t g_buf_tx[HEADER_TAP_MSG + MAX_FRAME];

#define ACC_SIZE (PASST_LEN_PREFIX + MAX_FRAME + 65536)
static uint8_t g_acc[ACC_SIZE];
static int     g_acc_len = 0;

static int g_fd_rx_from_tap = -1;
static int g_fd_tx_to_tap   = -1;
static int g_passt_fd       = -1;
static int g_llid_tap   = 0;
static int g_llid_passt = 0;
/* Bumped per connection to a passt: the server takes the nat as up only on a
   connection newer than the request. */
static int g_passt_generation = 0;
static int  g_passt_lost = 0;
static int  g_passt_warned = 0;
/* Guest frames dropped since the last connection to a passt. */
static unsigned long g_passt_dropped = 0;
static int  g_passt_retry = 0;
/* When passt was last seen to go, so that the retry has an end. */
static time_t g_passt_lost_at = 0;
/* A passt is alive once a frame came back from it: the arp reply to the
   probe sent after the announce; nothing to prove while no guest was seen. */
static int g_passt_probe_sent = 0;
static unsigned long long g_passt_frames_at_connect = 0;
#define PASST_GIVE_UP_SECS 15
#define MAX_MACHINES 64
static char g_passt_path[MAX_PATH_LEN];

static int g_tap_read_stopped = 0;

/* Backlog: length-prefixed frames the socket did not take whole. */
typedef struct t_bl_el
{
  int len;
  int off;
  struct t_bl_el *next;
  uint8_t buf[0];
} t_bl_el;

static t_bl_el *g_bl_first = NULL;
static t_bl_el *g_bl_last  = NULL;
static long     g_bl_bytes = 0;       /* total unsent bytes queued */
static long     g_bl_peak  = 0;       /* high-water seen (for diagnostics) */
static unsigned long long g_stat_tap_frames    = 0;
static unsigned long long g_stat_passt_frames  = 0;
static unsigned long long g_stat_queued_frames = 0;
static unsigned long long g_stat_drain_calls   = 0;

static t_bl_el *g_tb_first = NULL;
static t_bl_el *g_tb_last  = NULL;
static long     g_tb_bytes = 0;
static long     g_tb_peak  = 0;
static int      g_passt_read_stopped = 0;
static unsigned long long g_stat_tap_tx_queued = 0;
static unsigned long long g_stat_tap_tx_dropped = 0;

static void backlog_push(uint8_t *buf, int len, int already_off)
{
  t_bl_el *el = (t_bl_el *) malloc(sizeof(t_bl_el) + len);
  if (!el)
    KOUT("ERROR passt backlog malloc %d", len);
  el->len = len;
  el->off = already_off;
  el->next = NULL;
  memcpy(el->buf, buf, len);
  if (g_bl_last)
    g_bl_last->next = el;
  else
    g_bl_first = el;
  g_bl_last = el;
  g_bl_bytes += (len - already_off);
  g_stat_queued_frames += 1;
  if (g_bl_bytes > g_bl_peak)
    g_bl_peak = g_bl_bytes;
  if (SHUTTLE_LOUD)
    KTRC("passt backlog PUSH len=%d off=%d total_q=%ld peak=%ld nframes=%llu",
         len, already_off, g_bl_bytes, g_bl_peak, g_stat_queued_frames);
  if (g_bl_bytes > BACKLOG_HARD_MAX)
    KOUT("ERROR passt backlog HARD MAX exceeded %ld > %d (passt stuck?)",
         g_bl_bytes, BACKLOG_HARD_MAX);
}

static void backlog_pop_front(void)
{
  t_bl_el *el = g_bl_first;
  if (!el)
    KOUT("ERROR passt backlog pop empty");
  g_bl_first = el->next;
  if (!g_bl_first)
    g_bl_last = NULL;
  free(el);
}

static void flow_ctrl_update(void)
{
  static unsigned long nb_stop = 0;
  static uint64_t last_say = 0;
  uint64_t now;
  if ((!g_tap_read_stopped) && (g_bl_bytes >= BACKLOG_HIGH_WATER))
    {
    g_tap_read_stopped = 1;
    if (g_llid_tap)
      channel_rx_local_flow_ctrl(g_llid_tap, 1);
    nb_stop += 1;
    now = cloonix_get_msec();
    /* one line per ten seconds for a repeating condition */
    if (now - last_say >= 10000)
      {
      last_say = now;
      KERR("WARNING passt flow control stopped the tap read %lu time(s) so "
           "far: backlog=%ld >= high=%d", nb_stop, g_bl_bytes,
           BACKLOG_HIGH_WATER);
      }
    }
  else if ((g_tap_read_stopped) && (g_bl_bytes <= BACKLOG_LOW_WATER))
    {
    g_tap_read_stopped = 0;
    if (g_llid_tap)
      channel_rx_local_flow_ctrl(g_llid_tap, 0);
    if (SHUTTLE_INFO)
      KTRC("passt FLOW-CTRL RESUME tap read: backlog=%ld <= low=%d",
           g_bl_bytes, BACKLOG_LOW_WATER);
    }
}

static long backlog_drain(void)
{
  int w;
  long pushed = 0;
  t_bl_el *el;
  g_stat_drain_calls += 1;
  if (g_passt_fd < 0)
    {
    while (g_bl_first)
      backlog_pop_front();
    flow_ctrl_update();
    return 0;
    }
  while (g_bl_first)
    {
    el = g_bl_first;
    w = write(g_passt_fd, el->buf + el->off, el->len - el->off);
    if (w > 0)
      {
      el->off += w;
      g_bl_bytes -= w;
      pushed += w;
      if (el->off == el->len)
        backlog_pop_front();
      }
    else if ((w < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
      {
      if (SHUTTLE_LOUD)
        KTRC("passt drain EAGAIN, %ld bytes still queued", g_bl_bytes);
      break;
      }
    else if ((w < 0) && (errno == EINTR))
      {
      continue;
      }
    else
      {
      KERR("WARNING passt drain write w=%d errno=%d, waiting for passt", w, errno);
      g_passt_lost = 1;
      break;
      }
    }
  if ((pushed > 0) && SHUTTLE_LOUD)
    KTRC("passt drain pushed=%ld remaining_q=%ld", pushed, g_bl_bytes);
  flow_ctrl_update();
  return pushed;
}

/* Non-blocking write of a contiguous buffer to passt. */
static void passt_write_nonblock(uint8_t *buf, int len)
{
  int off = 0, w;
  if ((g_passt_fd < 0) || (g_passt_lost))
    {
    /* Guest frames while passt is away, counted: said only once the absence
       lasts (passt_try_again), a delete or a republish stops it on purpose;
       the length prefix is not a frame. */
    if (len != PASST_LEN_PREFIX)
      {
      g_passt_dropped += 1;
      if ((g_passt_warned) && ((g_passt_dropped % 200) == 0))
        KERR("WARNING passt away, %lu guest frame(s) dropped so far",
             g_passt_dropped);
      }
    return;
    }
  if (g_bl_first)
    {
    backlog_push(buf, len, 0);
    return;
    }
  while (off < len)
    {
    w = write(g_passt_fd, buf + off, len - off);
    if (w > 0)
      off += w;
    else if ((w < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
      {
      /* Couldn't finish now: queue the rest, do NOT block. */
      backlog_push(buf, len, off);
      flow_ctrl_update();
      return;
      }
    else if ((w < 0) && (errno == EINTR))
      continue;
    else
      {
      /* passt gone: its socket error callback follows, and a lasting
         absence is said there */
      if ((errno != EPIPE) && (errno != ECONNRESET))
        KERR("WARNING passt write w=%d errno=%d, waiting for passt", w, errno);
      g_passt_lost = 1;
      return;
      }
    }
}

/* passt learns the guest mac from its frames only: machines seen are
   remembered and announced to each new passt as a gratuitous arp. */
static void learn_from_frame(uint8_t *buf, int len)
{
  uint16_t type;
  uint32_t ip;
  if (len < 34)
    return;
  type = (buf[12] << 8) | buf[13];
  if ((type == 0x0806) && (len >= 42) && (buf[20] == 0) &&
      ((buf[21] == 1) || (buf[21] == 2)))
    {
    memcpy(&ip, buf + 28, 4);
    if (ip)
      machine_learn_ip(buf + 22, ip);
    }
  else if ((type == 0x0800) && ((buf[14] >> 4) == 4))
    {
    memcpy(&ip, buf + 26, 4);
    if (ip)
      machine_learn_ip(buf + 6, ip);
    }
}

static int announce_machines(void)
{
  uint8_t macs[6*MAX_MACHINES], frame[60], prefix[PASST_LEN_PREFIX];
  uint32_t ips[MAX_MACHINES], be_len, gw = inet_addr(PASST_GW);
  int i, nb = machine_known(macs, ips, MAX_MACHINES);
  /* passt keeps one guest mac, the last frame's: the newest known goes last */
  for (i=nb-1; i>=0; i--)
    {
    memset(frame, 0, 60);
    memset(frame, 0xff, 6);
    memcpy(frame + 6, macs + 6*i, 6);
    frame[12] = 0x08; frame[13] = 0x06;
    frame[15] = 1;
    frame[16] = 0x08; frame[17] = 0x00;
    frame[18] = 6; frame[19] = 4;
    frame[21] = 1;
    memcpy(frame + 22, macs + 6*i, 6);
    memcpy(frame + 28, &(ips[i]), 4);
    memcpy(frame + 38, &(ips[i]), 4);
    be_len = htonl(60);
    memcpy(prefix, &be_len, PASST_LEN_PREFIX);
    passt_write_nonblock(prefix, PASST_LEN_PREFIX);
    passt_write_nonblock(frame, 60);
    }
  /* Asked for the gateway on behalf of the newest guest: the reply is the
     first frame of this passt, the proof it is up and knows the guest. */
  if (nb)
    {
    memset(frame, 0, 60);
    memset(frame, 0xff, 6);
    memcpy(frame + 6, macs, 6);
    frame[12] = 0x08; frame[13] = 0x06;
    frame[15] = 1;
    frame[16] = 0x08; frame[17] = 0x00;
    frame[18] = 6; frame[19] = 4;
    frame[21] = 1;
    memcpy(frame + 22, macs, 6);
    memcpy(frame + 28, &(ips[0]), 4);
    memcpy(frame + 38, &gw, 4);
    be_len = htonl(60);
    memcpy(prefix, &be_len, PASST_LEN_PREFIX);
    passt_write_nonblock(prefix, PASST_LEN_PREFIX);
    passt_write_nonblock(frame, 60);
    }
  /* a guest that has sent nothing through the nat yet: nominal */
  else
    KTRC("passt reconnected (gen %d): no machine seen yet, nothing to "
         "announce, alive at once", g_passt_generation);
  return nb;
}

int passt_shuttle_connect(char *socket_path)
{
  int fd;
  struct sockaddr_un addr;
  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    {
    KERR("ERROR passt socket %d", errno);
    return -1;
    }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path)-1);
  snprintf(g_passt_path, MAX_PATH_LEN-1, "%s", socket_path);
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
    close(fd);
    return -1;
    }
  if (SHUTTLE_INFO)
    KTRC("passt shuttle connected fd=%d path=%s", fd, socket_path);
  return fd;
}

void passt_shuttle_tap_to_passt(int tap_fd, int passt_fd)
{
  static uint8_t buf_rx[HEADER_TAP_MSG + MAX_FRAME];
  uint16_t seq;
  int len, buf_len, result;
  uint8_t *buf;
  uint8_t prefix[PASST_LEN_PREFIX];
  uint32_t be_len;
  (void)passt_fd;

  len = read(tap_fd, buf_rx, HEADER_TAP_MSG);
  if (len != HEADER_TAP_MSG)
    KOUT("ERROR READ %d %d", len, errno);
  result = fct_seqtap_rx(tap_fd, buf_rx, &seq, &buf_len, &buf);
  if (result != kind_seqtap_data)
    KOUT("ERROR %d", result);
  /* a gap in seq is what the tap child dropped at its pipe: it says so */
  learn_from_frame(buf, buf_len);

  g_stat_tap_frames += 1;
  if (SHUTTLE_LOUD)
    KTRC("passt TAP->PASST frame#%llu len=%d backlog=%ld",
         g_stat_tap_frames, buf_len, g_bl_bytes);

  /* Emit uint32 BE len then frame, NON-BLOCKING (queues if passt is full). */
  be_len = htonl((uint32_t)buf_len);
  memcpy(prefix, &be_len, PASST_LEN_PREFIX);
  passt_write_nonblock(prefix, PASST_LEN_PREFIX);
  passt_write_nonblock(buf, buf_len);
}

static void passt_read_flow_update(void)
{
  if ((!g_passt_read_stopped) && (g_tb_bytes >= TAPTX_HIGH_WATER))
    {
    g_passt_read_stopped = 1;
    if (g_llid_passt)
      channel_rx_local_flow_ctrl(g_llid_passt, 1);
    }
  else if ((g_passt_read_stopped) && (g_tb_bytes <= TAPTX_LOW_WATER))
    {
    g_passt_read_stopped = 0;
    if (g_llid_passt)
      channel_rx_local_flow_ctrl(g_llid_passt, 0);
    }
}

static void tap_tx_backlog_push(uint8_t *buf, int len, int already_off)
{
  t_bl_el *el = (t_bl_el *) malloc(sizeof(t_bl_el) + len);
  if (!el)
    KOUT("ERROR tap tx backlog malloc %d", len);
  el->len = len;
  el->off = already_off;
  el->next = NULL;
  memcpy(el->buf, buf, len);
  if (g_tb_last)
    g_tb_last->next = el;
  else
    g_tb_first = el;
  g_tb_last = el;
  g_tb_bytes += (len - already_off);
  g_stat_tap_tx_queued += 1;
  if (g_tb_bytes > g_tb_peak)
    g_tb_peak = g_tb_bytes;
  /* Never drop a partially sent head frame: it would corrupt the stream. */
  while ((g_tb_bytes > TAPTX_HARD_MAX) && g_tb_first)
    {
    t_bl_el *victim, *prev;
    if (g_tb_first->off > 0)
      {
      prev = g_tb_first;
      victim = g_tb_first->next;
      if (!victim)
        break;                 /* only a partial head left: cannot drop */
      prev->next = victim->next;
      }
    else
      {
      prev = NULL;
      victim = g_tb_first;
      g_tb_first = victim->next;
      }
    if (g_tb_last == victim)
      g_tb_last = prev;
    g_tb_bytes -= (victim->len - victim->off);
    free(victim);
    g_stat_tap_tx_dropped += 1;
    }
}

/* Push as much of the tap tx backlog as the tap will accept, NON-BLOCKING. */
static void tap_tx_backlog_drain(void)
{
  int w;
  t_bl_el *el;
  while (g_tb_first)
    {
    el = g_tb_first;
    w = write(g_fd_tx_to_tap, el->buf + el->off, el->len - el->off);
    if (w > 0)
      {
      el->off += w;
      g_tb_bytes -= w;
      if (el->off == el->len)
        {
        g_tb_first = el->next;
        if (!g_tb_first)
          g_tb_last = NULL;
        free(el);
        }
      }
    else if ((w < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
      break;
    else if ((w < 0) && (errno == EINTR))
      continue;
    else
      KOUT("ERROR tap tx drain write w=%d errno=%d", w, errno);
    }
  passt_read_flow_update();
}

/* passt sends every frame to the last guest mac it saw: the destination mac
   is restored from the destination ip, learned from the guests' frames. */
static void fix_dst_mac(uint8_t *frame, int frame_len)
{
  uint16_t type;
  uint32_t ip;
  uint8_t mac[6];
  if (frame_len < 42)
    return;
  type = (frame[12] << 8) | frame[13];
  if ((type == 0x0800) && ((frame[14] >> 4) == 4) && !(frame[0] & 1))
    {
    memcpy(&ip, frame + 30, 4);
    if (machine_mac_of_ip(ip, mac) == 0)
      memcpy(frame, mac, 6);
    }
  else if ((type == 0x0806) && (frame[20] == 0) && (frame[21] == 2))
    {
    memcpy(&ip, frame + 38, 4);
    if (machine_mac_of_ip(ip, mac) == 0)
      {
      memcpy(frame, mac, 6);
      memcpy(frame + 32, mac, 6);
      }
    }
}

static void emit_frame_to_tap(uint8_t *frame, int frame_len)
{
  static uint16_t seqtap = 0;
  int off = 0, w, len;
  seqtap += 1;
  if ((frame_len <= 0) || (frame_len > MAX_FRAME))
    {
    KERR("ERROR LEN passt_to_tap %d", frame_len);
    return;
    }
  fix_dst_mac(frame, frame_len);
  fct_seqtap_tx(kind_seqtap_data, g_buf_tx, seqtap, frame_len, frame);
  len = frame_len + HEADER_TAP_MSG;
  if (g_tb_first)
    {
    tap_tx_backlog_push(g_buf_tx, len, 0);
    passt_read_flow_update();
    return;
    }
  while (off < len)
    {
    w = write(g_fd_tx_to_tap, g_buf_tx + off, len - off);
    if (w > 0)
      off += w;
    else if ((w < 0) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
      {
      tap_tx_backlog_push(g_buf_tx, len, off);
      passt_read_flow_update();
      return;
      }
    else if ((w < 0) && (errno == EINTR))
      continue;
    else
      KOUT("ERROR WRITE TAP w=%d frame_len=%d errno=%d", w, frame_len, errno);
    }
}

void passt_shuttle_passt_to_tap(int passt_fd, int tap_fd)
{
  int rd, off, frame_len;
  uint32_t be_len;
  (void)tap_fd;

  rd = read(passt_fd, g_acc + g_acc_len, ACC_SIZE - g_acc_len);
  if (rd == 0)
    {
    g_passt_lost = 1;
    return;
    }
  if (rd < 0)
    {
    if ((errno == EINTR) || (errno == EAGAIN))
      return;
    KERR("WARNING passt read %d, waiting for passt", errno);
    g_passt_lost = 1;
    return;
    }
  g_acc_len += rd;

  off = 0;
  while (g_acc_len - off >= PASST_LEN_PREFIX)
    {
    memcpy(&be_len, g_acc + off, PASST_LEN_PREFIX);
    frame_len = (int)ntohl(be_len);
    if ((frame_len <= 0) || (frame_len > MAX_FRAME))
      {
      KERR("ERROR passt frame_len %d -- unrecoverable, resetting acc", frame_len);
      g_acc_len = 0;
      return;
      }
    if (g_acc_len - off - PASST_LEN_PREFIX < frame_len)
      break;
    emit_frame_to_tap(g_acc + off + PASST_LEN_PREFIX, frame_len);
    g_stat_passt_frames += 1;
    off += PASST_LEN_PREFIX + frame_len;
    }
  if (SHUTTLE_LOUD && (rd > 0))
    KTRC("passt PASST->TAP read=%d frames_total=%llu acc_left=%d",
         rd, g_stat_passt_frames, g_acc_len - off);

  if (off > 0)
    {
    g_acc_len -= off;
    if (g_acc_len > 0)
      memmove(g_acc, g_acc + off, g_acc_len);
    }
}

static int tap_rx_cb(int llid, int fd)
{
  unsigned long long before = g_stat_tap_frames;
  int nb = 0;
  (void)llid;
  if (g_tb_first)
    tap_tx_backlog_drain();
  do
    {
    passt_shuttle_tap_to_passt(fd, g_passt_fd);
    nb += 1;
    } while ((nb < SEQTAP_PIPE_BATCH) && (!g_tap_read_stopped) &&
             fct_seqtap_pipe_more(fd));
  if (g_bl_first)
    backlog_drain();
  /* MUST return >0 when we actually consumed a frame. */
  return (g_stat_tap_frames > before) ? 1 : 0;
}

static int passt_rx_cb(int llid, int fd)
{
  unsigned long long before = g_stat_passt_frames;
  (void)llid;
  if (g_bl_first)
    backlog_drain();
  passt_shuttle_passt_to_tap(fd, g_fd_tx_to_tap);
  if (g_passt_lost)
    return -1;
  if (g_tb_first)
    tap_tx_backlog_drain();
  /* >0 on real progress, as tap_rx_cb. */
  return (g_stat_passt_frames > before) ? 1 : 0;
}

static void shuttle_err_cb(int llid, int err, int from)
{
  /* A stale error (torn-down socket of a replaced passt) is only logged:
     the channel layer deletes the channel itself. KOUT here killed live helpers. */
  if (g_llid_passt == 0)
    {
    KERR("WARNING passt shuttle stale error llid=%d err=%d from=%d, "
         "passt already gone, ignored", llid, err, from);
    return;
    }
  if (llid != g_llid_passt)
    {
    KERR("WARNING passt shuttle error for old llid=%d (current %d) err=%d "
         "from=%d, ignored", llid, g_llid_passt, err, from);
    return;
    }
  /* A republish restarts passt on purpose and it is back within the second:
     the warning waits for an absence that lasts. */
  KTRC("passt socket %s closed, trying it again", g_passt_path);
  g_passt_warned = 0;
  g_llid_passt = 0;
  g_passt_fd = -1;
  g_passt_lost_at = time(NULL);
  g_passt_lost = 0;
  g_passt_retry = 0;
  g_acc_len = 0;
  g_passt_read_stopped = 0;
  backlog_drain();
}

static void passt_try_again(void)
{
  int fd;
  struct sockaddr_un addr;
  g_passt_retry += 1;
  fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    {
    KERR("WARNING passt socket: %s", strerror(errno));
    return;
    }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, g_passt_path, sizeof(addr.sun_path)-1);
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
    close(fd);
    /* proxymous never restarts a dead passt: gone this long it is not coming
       back, so exit and let the server free the nat as a silent helper. */
    if ((g_passt_lost_at) &&
        (time(NULL) - g_passt_lost_at > PASST_GIVE_UP_SECS))
      KOUT("ERROR passt on %s did not come back in %d seconds, this nat is "
           "finished", g_passt_path, PASST_GIVE_UP_SECS);
    if ((g_passt_lost_at) && (!g_passt_warned) &&
        (time(NULL) - g_passt_lost_at > 2))
      {
      g_passt_warned = 1;
      KERR("WARNING passt socket %s closed and not back after 2 seconds, "
           "%d tries, %lu guest frame(s) dropped so far", g_passt_path,
           g_passt_retry, g_passt_dropped);
      }
    return;
    }
  nonblock_fd(fd);
  g_llid_passt = msg_watch_fd(fd, passt_rx_cb, shuttle_err_cb, "passtsk");
  if (g_llid_passt == 0)
    KOUT("ERROR passt shuttle passt watch");
  g_passt_fd = fd;
  g_passt_generation += 1;
  g_passt_frames_at_connect = g_stat_passt_frames;
  g_passt_probe_sent = (announce_machines() > 0);
  if (g_passt_warned)
    KERR("WARNING passt back on %s after %d tries, %lu guest frame(s) "
         "dropped while away", g_passt_path, g_passt_retry, g_passt_dropped);
  else
    KTRC("passt back on %s after %d tries", g_passt_path, g_passt_retry);
  g_passt_lost_at = 0;
  g_passt_warned = 0;
  g_passt_dropped = 0;
}

void passt_shuttle_heartbeat(void)
{
  static int beat = 0;
  beat += 1;
  if ((g_passt_fd < 0) && (g_passt_path[0]) && (g_llid_tap))
    passt_try_again();
  if (g_bl_first)
    backlog_drain();
  if (g_tb_first)
    tap_tx_backlog_drain();
  if (SHUTTLE_INFO && ((beat % 50) == 0))
    KERR("TRACE passt shuttle tap_in=%llu passt_in=%llu queued=%llu "
         "backlog=%ld peak=%ld drains=%llu flowstop=%d "
         "taptx_q=%ld taptx_peak=%ld pasststop=%d taptx_drop=%llu",
         g_stat_tap_frames, g_stat_passt_frames, g_stat_queued_frames,
         g_bl_bytes, g_bl_peak, g_stat_drain_calls, g_tap_read_stopped,
         g_tb_bytes, g_tb_peak, g_passt_read_stopped, g_stat_tap_tx_dropped);
}

void passt_shuttle_init(int tap_fd, int passt_fd)
{
  g_fd_rx_from_tap = tap_fd;
  g_passt_fd       = passt_fd;
  g_acc_len        = 0;
  g_bl_first = NULL;
  g_bl_last  = NULL;
  g_bl_bytes = 0;
  g_tap_read_stopped = 0;
  g_tb_first = NULL;
  g_tb_last  = NULL;
  g_tb_bytes = 0;
  g_passt_read_stopped = 0;
  /* Both the passt socket and the tap tx fd non-blocking: no stalled loop. */
  nonblock_fd(passt_fd);
  if (g_fd_tx_to_tap >= 0)
    nonblock_fd(g_fd_tx_to_tap);
  if (SHUTTLE_INFO)
    KTRC("passt shuttle init tap_fd=%d passt_fd=%d tap_tx=%d (all NONBLOCK)",
         tap_fd, passt_fd, g_fd_tx_to_tap);
  g_llid_tap   = msg_watch_fd(tap_fd,   tap_rx_cb,   shuttle_err_cb, "passttap");
  if (g_llid_tap == 0)
    KOUT("ERROR passt shuttle tap watch");
  g_llid_passt = msg_watch_fd(passt_fd, passt_rx_cb, shuttle_err_cb, "passtsk");
  if (g_llid_passt == 0)
    KOUT("ERROR passt shuttle passt watch");
  g_passt_generation += 1;
  if (SHUTTLE_INFO)
    KTRC("passt shuttle watching llid_tap=%d llid_passt=%d",
         g_llid_tap, g_llid_passt);
}

void passt_shuttle_set_tx(int fd_tx_to_tap)
{
  g_fd_tx_to_tap = fd_tx_to_tap;
  if (g_fd_tx_to_tap >= 0)
    nonblock_fd(g_fd_tx_to_tap);
}

int passt_shuttle_connected(void)
{
  return ((g_passt_fd >= 0) && (g_llid_passt != 0));
}

int passt_shuttle_alive(void)
{
  int alive;
  if (g_passt_fd < 0)
    return 0;
  if (!g_passt_probe_sent)
    return 1;
  alive = (g_stat_passt_frames > g_passt_frames_at_connect);
  return alive;
}

int passt_shuttle_generation(void)
{
  return g_passt_generation;
}
