/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "proxy_crun.h"
#include "util_sock.h"
#include "proxy_tls.h"

static char g_pmain_unix[MAX_PATH_LEN];
static char g_net[MAX_NAME_LEN];

typedef struct t_peer_llid
{
  int llid_tcp_front;
  int llid_unix_back;
  int  sniffed;
  void *tls;
  int drain_beats;
  int paused_back;    /* reads from doorways stopped, the client is behind */
  int paused_front;   /* reads from the client stopped, doorways is behind */
  struct t_peer_llid *prev;
  struct t_peer_llid *next;
} t_peer_llid;

static t_peer_llid *g_head_peer_llid;
static int g_drain_armed;

/* Doorways ends a leg when the guest program is done; the tcp side is kept
   until its queue drains, up to this long. */
#define DRAIN_MAX_BEATS 3000

static t_peer_llid *find_peer(int llid)
{
  t_peer_llid *cur = g_head_peer_llid;
  while(cur)
    {
    if ((cur->llid_tcp_front == llid) || (cur->llid_unix_back == llid))
      break;
    cur = cur->next;
    }
  return cur;
}

static void free_peer(t_peer_llid *cur)
{
  if (cur->tls)
    proxy_tls_free(cur->tls);
  if (msg_exist_channel(cur->llid_tcp_front))
    msg_delete_channel(cur->llid_tcp_front);
  if (msg_exist_channel(cur->llid_unix_back))
    msg_delete_channel(cur->llid_unix_back);
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur == g_head_peer_llid)
    g_head_peer_llid = cur->next;
  free(cur);
}

static t_peer_llid *alloc_peer(int llid_tcp_front, int llid_unix_back)
{
  t_peer_llid *cur;
  if ((llid_tcp_front <= 0) || (llid_tcp_front >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", llid_tcp_front, llid_unix_back);
  if ((llid_unix_back <= 0) || (llid_unix_back >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", llid_tcp_front, llid_unix_back);
  cur = find_peer(llid_tcp_front);
  if (cur)
    KOUT("ERROR %d %d", cur->llid_tcp_front, cur->llid_unix_back);
  cur = find_peer(llid_unix_back);
  if (cur)
    KOUT("ERROR %d %d", cur->llid_tcp_front, cur->llid_unix_back);
  cur = (t_peer_llid *) malloc(sizeof(t_peer_llid));
  memset(cur, 0, sizeof(t_peer_llid));
  cur->llid_tcp_front  = llid_tcp_front;
  cur->llid_unix_back  = llid_unix_back;
  if (g_head_peer_llid)
    g_head_peer_llid->prev = cur;
  cur->next = g_head_peer_llid;
  g_head_peer_llid = cur;
  return cur;
}

static void drain_arm(void);

static void drain_beat(void *data)
{
  t_peer_llid *next, *cur = g_head_peer_llid;
  g_drain_armed = 0;
  while (cur)
    {
    next = cur->next;
    if (cur->drain_beats)
      {
      cur->drain_beats -= 1;
      if (!msg_exist_channel(cur->llid_tcp_front))
        free_peer(cur);
      else if (!msg_mngt_get_tx_queue_len(cur->llid_tcp_front))
        free_peer(cur);
      else if (!cur->drain_beats)
        {
        KERR("ERROR client stopped reading, %d bytes of its last answer "
             "dropped", msg_mngt_get_tx_queue_len(cur->llid_tcp_front));
        free_peer(cur);
        }
      }
    cur = next;
    }
  drain_arm();
}

static void drain_arm(void)
{
  t_peer_llid *cur = g_head_peer_llid;
  if (g_drain_armed)
    return;
  while (cur)
    {
    if (cur->drain_beats)
      break;
    cur = cur->next;
    }
  if (cur)
    {
    g_drain_armed = 1;
    clownix_timeout_add(1, drain_beat, NULL, NULL, NULL);
    }
}

static void peer_broken(int llid)
{
  t_peer_llid *cur;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d", llid);
  cur = find_peer(llid);
  if (!cur)
    KERR("ERROR %d", llid);
  else if ((llid == cur->llid_unix_back) && (!cur->drain_beats) &&
           (msg_exist_channel(cur->llid_tcp_front)) &&
           (msg_mngt_get_tx_queue_len(cur->llid_tcp_front)))
    {
    if (msg_exist_channel(cur->llid_unix_back))
      msg_delete_channel(cur->llid_unix_back);
    cur->llid_unix_back = 0;
    cur->drain_beats = DRAIN_MAX_BEATS;
    drain_arm();
    }
  else
    free_peer(cur);
}

static void err_cb(int llid, int err, int from)
{
  peer_broken(llid);
}

static void tls_out_plain(void *user, int len, char *buf)
{
  t_peer_llid *cur = (t_peer_llid *) user;
  proxy_traf_unix_tx(cur->llid_unix_back, len, buf);
}

static void tls_out_cipher(void *user, int len, char *buf)
{
  t_peer_llid *cur = (t_peer_llid *) user;
  proxy_traf_tcp_tx(cur->llid_tcp_front, len, buf);
}

/* First byte: 0x16 is a tls client hello, 0xCA a clear cloonix frame. */
static int sniff_front(t_peer_llid *cur, int len, char *buf)
{
  cur->sniffed = 1;
  if ((len <= 0) || ((buf[0] & 0xFF) != 0x16))
    return 0;
  cur->tls = proxy_tls_alloc();
  if (!cur->tls)
    {
    KERR("WARNING pmain: tls asked by the client and no certificate could "
         "be found or written here, the client will come back in clear");
    return -1;
    }
  return 0;
}

static void rx_cb(int llid, int len, char *buf)
{
  t_peer_llid *cur;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d", llid);
  cur = find_peer(llid);
  if (!cur)
    KERR("ERROR %d", llid);
  else if (llid == cur->llid_tcp_front)
    {
    if (cur->drain_beats)
      { /* the back is gone, only its last bytes are still going out */ }
    else if ((!cur->sniffed) && (sniff_front(cur, len, buf)))
      peer_broken(llid);
    else if (!cur->tls)
      proxy_traf_unix_tx(cur->llid_unix_back, len, buf);
    else if (proxy_tls_rx_cipher(cur->tls, len, buf,
                                 tls_out_plain, tls_out_cipher, cur))
      peer_broken(llid);
    }
  else if (llid == cur->llid_unix_back)
    {
    if (!cur->tls)
      proxy_traf_tcp_tx(cur->llid_tcp_front, len, buf);
    else if (proxy_tls_tx_plain(cur->tls, len, buf, tls_out_cipher, cur))
      peer_broken(llid);
    }
  else
    KOUT("ERROR %d", llid);
}

static void connect_from_pmain_tcp_client(int llid_listen, int llid_tcp_front)
{
  int llid_unix_back = proxy_traf_unix_client(g_pmain_unix, err_cb, rx_cb);
  if (llid_unix_back == 0)
    {
    KERR("ERROR cannot connect to doorways on %s, dropping the client",
         g_pmain_unix);
    msg_delete_channel(llid_tcp_front);
    }
  else
    {
    alloc_peer(llid_tcp_front, llid_unix_back);
    msg_mngt_set_callbacks(llid_tcp_front, err_cb, rx_cb);
    channel_rx_local_flow_ctrl(llid_tcp_front, 0);
    channel_rx_local_flow_ctrl(llid_unix_back, 0);
    }
}

/* Health line, every six seconds: two descriptors per live peer is normal. */
void set_config_rapid_beat(void)
{
  static int beat_count;
  static int hi_fd = -1, hi_nb = -1, quiet_beats;
  t_peer_llid *walk;
  int nb = 0, nb_fd = 0, new_high;
  DIR *d;
  struct dirent *e;
  beat_count += 1;
  for (walk = g_head_peer_llid; walk; walk = walk->next)
    {
    if (walk->drain_beats)
      continue;
    msg_relay_flow_ctrl(walk->llid_unix_back, walk->llid_tcp_front,
                        &(walk->paused_back));
    msg_relay_flow_ctrl(walk->llid_tcp_front, walk->llid_unix_back,
                        &(walk->paused_front));
    }
  if ((beat_count % 600) != 0)
    return;
  for (walk = g_head_peer_llid; walk; walk = walk->next)
    nb += 1;
  nb += jsweb_relay_nb_peers();
  d = opendir("/proc/self/fd");
  if (d)
    {
    while ((e = readdir(d)) != NULL)
      if (e->d_name[0] != '.')
        nb_fd += 1;
    closedir(d);
    }
  new_high = (nb_fd > hi_fd);
  quiet_beats += 1;
  if ((new_high) || (quiet_beats >= 100))
    {
    KTRC("peers alive=%d (max %d), descriptors held=%d (max %d)%s",
         nb, (nb > hi_nb) ? nb : hi_nb, nb_fd, new_high ? nb_fd : hi_fd,
         new_high ? " NEW HIGH" : "");
    quiet_beats = 0;
    }
  if (new_high)
    hi_fd = nb_fd;
  if (nb > hi_nb)
    hi_nb = nb;
  if (nb_fd > (2 * nb) + 100)
    KERR("WARNING TOO MANY DESCRIPTORS: %d descriptors for %d peers "
         "(doors and web)",
         nb_fd, nb);
}

int set_config(char *net, uint16_t pmain, uint16_t pjsweb, char *passwd)
{
  int result = -1;
  char *pox = get_proxyshare();
  set_cloonix_name(net);
  g_head_peer_llid = NULL;
  memset(g_pmain_unix, 0, MAX_PATH_LEN);
  memset(g_net, 0, MAX_NAME_LEN);
  strncpy(g_net, net, MAX_NAME_LEN-1);
  snprintf(g_pmain_unix, MAX_PATH_LEN-1, "%s/proxy_pmain.sock", pox);
  if (proxy_tls_init())
    KERR("WARNING PROXYMOUS no certificate, the doors port %hu is plain only",
         pmain);
  if (!proxy_traf_tcp_server(pmain, connect_from_pmain_tcp_client))
    KERR("ERROR PROXYMOUS (FATAL?) cannot listen to pmain port %hu", pmain);
  else if (jsweb_relay_init(pjsweb))
    KERR("ERROR PROXYMOUS cannot start jsweb relay port %hu", pjsweb);
  else
    result = 0;
  return result;
}
