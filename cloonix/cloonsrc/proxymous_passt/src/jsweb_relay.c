/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* jsweb_relay.c - https front <-> unix relay for the web port (pjsweb). */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "proxy_crun.h"
#include "proxy_tls.h"

static char g_pjsweb_unix[MAX_PATH_LEN];
static uint16_t g_pjsweb;
static time_t g_start_time;

#define DRAIN_MAX_BEATS 400

typedef struct t_jsweb_peer
{
  int llid_tcp_front;
  int llid_unix_back;
  void *tls;
  int drain_beats;
  int paused_back;
  int paused_front;
  struct t_jsweb_peer *prev;
  struct t_jsweb_peer *next;
} t_jsweb_peer;

static t_jsweb_peer *g_head_jsweb_peer;
static int g_drain_armed;

static t_jsweb_peer *jsweb_find_peer(int llid)
{
  t_jsweb_peer *cur = g_head_jsweb_peer;
  while(cur)
    {
    if ((cur->llid_tcp_front == llid) || (cur->llid_unix_back == llid))
      break;
    cur = cur->next;
    }
  return cur;
}

int jsweb_relay_nb_peers(void)
{
  int nb = 0;
  t_jsweb_peer *cur;
  for (cur = g_head_jsweb_peer; cur; cur = cur->next)
    nb += 1;
  return nb;
}

static void jsweb_free_peer(t_jsweb_peer *cur)
{
  if (msg_exist_channel(cur->llid_tcp_front))
    msg_delete_channel(cur->llid_tcp_front);
  if (msg_exist_channel(cur->llid_unix_back))
    msg_delete_channel(cur->llid_unix_back);
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur == g_head_jsweb_peer)
    g_head_jsweb_peer = cur->next;
  if (cur->tls)
    proxy_tls_free(cur->tls);
  free(cur);
}

static t_jsweb_peer *jsweb_alloc_peer(int llid_tcp_front, int llid_unix_back)
{
  t_jsweb_peer *cur;
  if ((llid_tcp_front <= 0) || (llid_tcp_front >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", llid_tcp_front, llid_unix_back);
  if ((llid_unix_back <= 0) || (llid_unix_back >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", llid_tcp_front, llid_unix_back);
  cur = (t_jsweb_peer *) malloc(sizeof(t_jsweb_peer));
  memset(cur, 0, sizeof(t_jsweb_peer));
  cur->llid_tcp_front = llid_tcp_front;
  cur->llid_unix_back = llid_unix_back;
  if (g_head_jsweb_peer)
    g_head_jsweb_peer->prev = cur;
  cur->next = g_head_jsweb_peer;
  g_head_jsweb_peer = cur;
  return cur;
}

static void jsweb_drain_arm(void);

static void jsweb_drain_beat(void *data)
{
  t_jsweb_peer *next, *cur = g_head_jsweb_peer;
  g_drain_armed = 0;
  while (cur)
    {
    next = cur->next;
    if (cur->drain_beats)
      {
      cur->drain_beats -= 1;
      if (!msg_exist_channel(cur->llid_tcp_front))
        jsweb_free_peer(cur);
      else if (!msg_mngt_get_tx_queue_len(cur->llid_tcp_front))
        jsweb_free_peer(cur);
      else if (!cur->drain_beats)
        {
        KERR("ERROR jsweb browser stopped reading, %d bytes dropped",
             msg_mngt_get_tx_queue_len(cur->llid_tcp_front));
        jsweb_free_peer(cur);
        }
      }
    cur = next;
    }
  jsweb_drain_arm();
}

static void jsweb_drain_arm(void)
{
  t_jsweb_peer *cur = g_head_jsweb_peer;
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
    clownix_timeout_add(1, jsweb_drain_beat, NULL, NULL, NULL);
    }
}

/* Every beat: a browser that is behind stops the reads from doorways. */
void jsweb_relay_rapid_beat(void)
{
  t_jsweb_peer *walk;
  for (walk = g_head_jsweb_peer; walk; walk = walk->next)
    {
    if (walk->drain_beats)
      continue;
    msg_relay_flow_ctrl(walk->llid_unix_back, walk->llid_tcp_front,
                        &(walk->paused_back));
    msg_relay_flow_ctrl(walk->llid_tcp_front, walk->llid_unix_back,
                        &(walk->paused_front));
    }
}

static void jsweb_peer_broken(int llid)
{
  t_jsweb_peer *cur;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d", llid);
  cur = jsweb_find_peer(llid);
  if (!cur)
    return;
  if ((llid == cur->llid_unix_back) && (!cur->drain_beats) &&
      (msg_exist_channel(cur->llid_tcp_front)) &&
      (msg_mngt_get_tx_queue_len(cur->llid_tcp_front)))
    {
    msg_delete_channel(cur->llid_unix_back);
    cur->llid_unix_back = 0;
    cur->drain_beats = DRAIN_MAX_BEATS;
    jsweb_drain_arm();
    }
  else
    jsweb_free_peer(cur);
}

static void jsweb_err_cb(int llid, int err, int from)
{
  jsweb_peer_broken(llid);
}

static void tls_out_plain(void *user, int len, char *buf)
{
  t_jsweb_peer *cur = (t_jsweb_peer *) user;
  proxy_traf_unix_tx(cur->llid_unix_back, len, buf);
}

static void tls_out_cipher(void *user, int len, char *buf)
{
  t_jsweb_peer *cur = (t_jsweb_peer *) user;
  proxy_traf_tcp_tx(cur->llid_tcp_front, len, buf);
}

static void jsweb_rx_cb(int llid, int len, char *buf)
{
  t_jsweb_peer *cur;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d", llid);
  cur = jsweb_find_peer(llid);
  if (!cur)
    KERR("ERROR %d", llid);
  else if (cur->drain_beats)
    {
    }
  else if (llid == cur->llid_tcp_front)
    {
    if (proxy_tls_rx_cipher(cur->tls, len, buf,
                            tls_out_plain, tls_out_cipher, cur))
      jsweb_peer_broken(llid);
    }
  else if (llid == cur->llid_unix_back)
    {
    if (proxy_tls_tx_plain(cur->tls, len, buf, tls_out_cipher, cur))
      jsweb_peer_broken(llid);
    }
  else
    KOUT("ERROR %d", llid);
}

static void jsweb_connect(int llid_tcp_front)
{
  int llid_unix_back;
  void *tls;
  t_jsweb_peer *cur;
  char *back = g_pjsweb_unix;

  tls = proxy_tls_alloc();
  if (!tls)
    {
    KERR("WARNING jsweb tls alloc failed");
    msg_delete_channel(llid_tcp_front);
    return;
    }
  llid_unix_back = proxy_traf_unix_client(back, jsweb_err_cb, jsweb_rx_cb);
  if (llid_unix_back == 0)
    {
    KERR("WARNING jsweb: a browser came for the canvas and the gui server "
         "is not listening on %s (net starting or stopping, or started "
         "without cloonix_web)", back);
    proxy_tls_free(tls);
    msg_delete_channel(llid_tcp_front);
    }
  else
    {
    msg_mngt_set_callbacks(llid_tcp_front, jsweb_err_cb, jsweb_rx_cb);
    cur = jsweb_alloc_peer(llid_tcp_front, llid_unix_back);
    if (cur)
      cur->tls = tls;
    else
      proxy_tls_free(tls);
    channel_rx_local_flow_ctrl(llid_tcp_front, 0);
    channel_rx_local_flow_ctrl(llid_unix_back, 0);
    }
}

static void connect_from_pjsweb_tcp_client(int llid_listen, int llid_tcp_front)
{
  jsweb_connect(llid_tcp_front);
}

static void jsweb_wait_back(void *data)
{
  static int tries;
  struct stat st;
  tries += 1;
  if (((stat(g_pjsweb_unix, &st)) || (st.st_mtime < g_start_time)) &&
      (tries < 300))
    clownix_timeout_add(10, jsweb_wait_back, NULL, NULL, NULL);
  else
    {
    if (tries >= 300)
      KERR("WARNING %s not bound after %d s, listening to %hu anyway",
           g_pjsweb_unix, tries/10, g_pjsweb);
    if (!proxy_traf_tcp_server(g_pjsweb, connect_from_pjsweb_tcp_client))
      KERR("ERROR PROXYMOUS cannot listen to pjsweb port %hu", g_pjsweb);
    else
      KTRC("PROXYMOUS https on port %hu with %s", g_pjsweb,
           proxy_tls_get_path());
    }
}

int jsweb_relay_init(uint16_t pjsweb)
{
  int result = -1;
  char *pox = get_proxyshare();
  g_head_jsweb_peer = NULL;
  g_pjsweb = pjsweb;
  g_start_time = time(NULL) - 1;
  memset(g_pjsweb_unix, 0, MAX_PATH_LEN);
  snprintf(g_pjsweb_unix, MAX_PATH_LEN-1, "%s/proxy_pjsweb.sock", pox);
  if (pjsweb == 0)
    {
    KTRC("PROXYMOUS no cloonix_web port, no canvas");
    result = 0;
    }
  else if (proxy_tls_init())
    KERR("ERROR PROXYMOUS no usable certificate, no canvas on %hu", pjsweb);
  else
    {
    jsweb_wait_back(NULL);
    result = 0;
    }
  return result;
}
