/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* kvm radios: asked of their agents, frames relayed to suid_power (medium);
   peers of a radio told again to an agent that comes back. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "air_link.h"
#include "rx_pktbuf.h"
#include "llid_backdoor.h"
#include "doorways.h"
#include "kvmwifi.h"

enum
{
  wifi_none = 0,
  wifi_asked,
  wifi_on,
  wifi_refused,
};

#define ASK_MS 3000
#define LINK_RETRY_MS 1000
#define SAY_MS 10000
/* a kvm just deleted: its frames still on the link are dropped silently */
#define NB_GONE 8
#define GONE_MS 10000

static int g_link_llid;
static uint64_t g_link_try_ms;
static unsigned long g_link_fails;
static uint64_t g_link_say_ms;
static unsigned long g_dropped;
static uint64_t g_drop_say_ms;
static unsigned long g_agent_dropped;
static uint64_t g_agent_say_ms;
static char g_gone[NB_GONE][MAX_NAME_LEN];
static uint64_t g_gone_ms[NB_GONE];
static int g_gone_idx;

static int gone_lately(char *name)
{
  uint64_t now = cloonix_get_msec();
  int i;
  for (i = 0; i < NB_GONE; i++)
    if ((g_gone_ms[i]) && (now - g_gone_ms[i] < GONE_MS) &&
        (!strcmp(g_gone[i], name)))
      return 1;
  return 0;
}

/* the air link speaking of a kvm before its add reached here from the
   switch, on another socket: its peers are kept for the add */
typedef struct t_early
{
  char name[MAX_NAME_LEN];
  uint64_t ms;
  int warned;
  int nb_peers[MAX_VWIF];
  uint8_t peers[MAX_VWIF][KVMWIFI_MAX_PEERS][6];
} t_early;
static t_early g_early[NB_GONE];

static void early_note(char *name, uint8_t kind, uint8_t *pay, int paylen)
{
  uint64_t now = cloonix_get_msec();
  t_early *e = NULL;
  int i, old = 0;
  for (i = 0; i < NB_GONE; i++)
    {
    if ((g_early[i].ms) && (!strcmp(g_early[i].name, name)))
      e = &(g_early[i]);
    if (g_early[i].ms < g_early[old].ms)
      old = i;
    }
  if (!e)
    {
    e = &(g_early[old]);
    memset(e, 0, sizeof(t_early));
    strncpy(e->name, name, MAX_NAME_LEN-1);
    e->ms = now;
    for (i = 0; i < MAX_VWIF; i++)
      e->nb_peers[i] = -1;
    }
  else if ((!e->warned) && (now - e->ms > GONE_MS))
    {
    KERR("WARNING kvmwifi: the air link speaks of %s for %d s, no kvm radio "
         "here", name, (int) ((now - e->ms) / 1000));
    e->warned = 1;
    }
  if ((kind == AIR_LINK_PEERS) && (paylen >= 1) && ((paylen - 1) % 6 == 0) &&
      (pay[0] < MAX_VWIF) && ((paylen - 1) / 6 <= KVMWIFI_MAX_PEERS))
    {
    e->nb_peers[pay[0]] = (paylen - 1) / 6;
    memcpy(e->peers[pay[0]], pay + 1, paylen - 1);
    }
}

/* a kvm added here: the peers the air link gave before */
void kvmwifi_born(t_backdoor_vm *bvm)
{
  int i, j;
  for (i = 0; i < NB_GONE; i++)
    if ((g_early[i].ms) && (!strcmp(g_early[i].name, bvm->name)))
      {
      for (j = 0; j < MAX_VWIF; j++)
        if (g_early[i].nb_peers[j] >= 0)
          {
          bvm->wifi_nb_peers[j] = g_early[i].nb_peers[j];
          memcpy(bvm->wifi_peers[j], g_early[i].peers[j],
                 sizeof(bvm->wifi_peers[j]));
          }
      memset(&(g_early[i]), 0, sizeof(t_early));
      }
}

static int radios_asked(void)
{
  t_backdoor_vm *cur;
  for (cur = llid_backdoor_get_first(); cur; cur = cur->next)
    if (cur->wifi != wifi_none)
      return 1;
  return 0;
}

/* suid_power relays for the kvm radios, sent as the agent gave them */
static void link_rx_msg(uint8_t *buf, int len)
{
  char name[MAX_NAME_LEN];
  char tx[MAX_A2D_LEN];
  t_backdoor_vm *bvm;
  int nb, idx, paylen = len - AIR_LINK_HEAD, headsize = sock_header_get_size();
  uint8_t *pay = buf + AIR_LINK_HEAD;
  memcpy(name, buf + 1, MAX_NAME_LEN);
  name[MAX_NAME_LEN-1] = 0;
  bvm = llid_backdoor_get_bvm(name);
  if (!bvm)
    {
    if (!gone_lately(name))
      early_note(name, buf[0], pay, paylen);
    return;
    }
  if (paylen > MAX_A2D_LEN - headsize)
    {
    KERR("WARNING kvmwifi: %d bytes for %s, too long for its agent", paylen,
         name);
    return;
    }
  /* suid_power may speak before the ask of the switch reaches here: any
     radio then, its peers kept for the ask */
  nb = (bvm->wifi == wifi_none) ? MAX_VWIF : bvm->wifi_nb;
  if ((buf[0] == AIR_LINK_PEERS) && (paylen >= 1) && ((paylen - 1) % 6 == 0) &&
      (pay[0] < nb) && ((paylen - 1) / 6 <= KVMWIFI_MAX_PEERS))
    {
    idx = pay[0];
    bvm->wifi_nb_peers[idx] = (paylen - 1) / 6;
    memcpy(bvm->wifi_peers[idx], pay + 1, paylen - 1);
    if (bvm->wifi == wifi_on)
      {
      memcpy(tx + headsize, pay, paylen);
      llid_backdoor_tx_bvm(bvm, paylen, header_type_wifi_ctrl,
                           header_val_wifi_peers, tx);
      }
    }
  else if ((buf[0] == AIR_LINK_FRAME) && (paylen > KVMWIFI_FRAME_HEAD) &&
           (pay[11] < nb))
    {
    /* an agent not up yet: the frame was in the air before its radio */
    if ((bvm->wifi == wifi_on) && msg_tx_queue_high(bvm->backdoor_llid))
      {
      g_agent_dropped += 1;
      if (cloonix_get_msec() - g_agent_say_ms >= SAY_MS)
        {
        g_agent_say_ms = cloonix_get_msec();
        KERR("WARNING kvmwifi: the agent of %s is behind, %lu frames to "
             "the guests dropped so far", name, g_agent_dropped);
        }
      }
    else if (bvm->wifi == wifi_on)
      {
      memcpy(tx + headsize, pay, paylen);
      llid_backdoor_tx_bvm(bvm, paylen, header_type_wifi_frame,
                           header_val_none, tx);
      }
    }
  else
    KERR("WARNING kvmwifi: a message of %d bytes, type %d, for %s on the "
         "air link", len, buf[0], name);
}

static void link_lost(int llid)
{
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  if (llid != g_link_llid)
    return;
  g_link_llid = 0;
  if (radios_asked())
    KERR("WARNING kvmwifi: suid_power left the air link, the kvm radios "
         "hear nothing until it is back");
}

static void link_err(int llid, int err, int from)
{
  link_lost(llid);
}

static int link_rx(int llid, int fd)
{
  static uint8_t buf[AIR_LINK_HEAD + MAX_A2D_LEN + 1];
  int i, len;
  for (i = 0; i < 64; i++)
    {
    len = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
    if ((len < 0) && ((errno == EAGAIN) || (errno == EINTR)))
      break;
    if (len <= 0)
      {
      if (len < 0)
        KERR("WARNING kvmwifi: the air link from suid_power: %s",
             strerror(errno));
      link_lost(llid);
      break;
      }
    if ((len <= AIR_LINK_HEAD) || (len > AIR_LINK_HEAD + MAX_A2D_LEN))
      KERR("WARNING kvmwifi: a message of %d bytes on the air link", len);
    else
      link_rx_msg(buf, len);
    }
  return 0;
}

/* connected when a kvm has radios, tried again at most once a second */
static void link_ensure(void)
{
  struct sockaddr_un addr;
  uint64_t now = cloonix_get_msec();
  int fd;
  if (g_link_llid || (now - g_link_try_ms < LINK_RETRY_MS))
    return;
  g_link_try_ms = now;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/%s", get_root_work(),
           AIR_LINK_SOCK);
  fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if ((fd >= 0) && (!connect(fd, (struct sockaddr *) &addr, sizeof(addr))))
    {
    g_link_llid = msg_watch_fd(fd, link_rx, link_err, "airlink");
    if (g_link_llid > 0)
      {
      if (g_link_fails)
        KERR("WARNING kvmwifi: the air link is up after %lu failures",
             g_link_fails);
      g_link_fails = 0;
      return;
      }
    g_link_llid = 0;
    }
  g_link_fails += 1;
  if (now - g_link_say_ms >= SAY_MS)
    {
    g_link_say_ms = now;
    KERR("WARNING kvmwifi: no air link on %s: %s, %lu failures",
         addr.sun_path, strerror(errno), g_link_fails);
    }
  if (fd >= 0)
    close(fd);
}

static void link_tx_frame(t_backdoor_vm *bvm, int len, char *payload)
{
  static uint8_t buf[AIR_LINK_HEAD + MAX_A2D_LEN];
  uint64_t now;
  link_ensure();
  if (!g_link_llid)
    return;
  memset(buf, 0, AIR_LINK_HEAD);
  buf[0] = AIR_LINK_FRAME;
  strncpy((char *) buf + 1, bvm->name, MAX_NAME_LEN-1);
  memcpy(buf + AIR_LINK_HEAD, payload, len);
  if (send(get_fd_with_llid(g_link_llid), buf, AIR_LINK_HEAD + len,
           MSG_DONTWAIT | MSG_NOSIGNAL) >= 0)
    return;
  if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == ENOBUFS))
    {
    g_dropped += 1;
    now = cloonix_get_msec();
    if (now - g_drop_say_ms >= SAY_MS)
      {
      g_drop_say_ms = now;
      KERR("WARNING kvmwifi: suid_power is not taking frames, %lu dropped "
           "so far", g_dropped);
      }
    }
  else
    {
    KERR("WARNING kvmwifi: the air link to suid_power: %s", strerror(errno));
    link_lost(g_link_llid);
    }
}

/* each radio its peers as last told, none before suid_power tells */
static void peers_tell(t_backdoor_vm *bvm)
{
  char buf[MAX_A2D_LEN];
  int i, nb, headsize = sock_header_get_size();
  for (i = 0; i < bvm->wifi_nb; i++)
    {
    if ((nb = bvm->wifi_nb_peers[i]) < 0)
      continue;
    buf[headsize] = i & 0xff;
    memcpy(buf + headsize + 1, bvm->wifi_peers[i], 6*nb);
    llid_backdoor_tx_bvm(bvm, 1 + 6*nb, header_type_wifi_ctrl,
                         header_val_wifi_peers, buf);
    }
}

static void send_ask(t_backdoor_vm *bvm)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  bvm->wifi_ask_ms = cloonix_get_msec();
  memcpy(buf + headsize, bvm->wifi_req, strlen(bvm->wifi_req) + 1);
  llid_backdoor_tx_bvm(bvm, strlen(bvm->wifi_req) + 1,
                       header_type_wifi_ctrl, header_val_wifi_on, buf);
}

/* KVMWIFI_ON_REQ of the switch: the radios of a kvm */
void kvmwifi_ask(char *name, char *req)
{
  t_backdoor_vm *bvm = llid_backdoor_get_bvm(name);
  unsigned int m[6];
  int nb;
  if (!bvm)
    {
    KERR("WARNING kvmwifi: %s is not a guest of doorways", name);
    return;
    }
  if ((sscanf(req, KVMWIFI_ON_REQ, &nb, &m[0], &m[1], &m[2], &m[3], &m[4],
              &m[5]) != 7) || (nb < 1) || (nb > MAX_VWIF))
    {
    KERR("WARNING kvmwifi: %s: bad request %s", name, req);
    return;
    }
  memset(bvm->wifi_req, 0, MAX_NAME_LEN);
  strncpy(bvm->wifi_req, req, MAX_NAME_LEN-1);
  bvm->wifi_nb = nb;
  bvm->wifi = wifi_asked;
  bvm->wifi_ask_ms = 0;
  link_ensure();
}

/* on each ping answered: asked again until the agent says */
void kvmwifi_agent_alive(t_backdoor_vm *bvm)
{
  if (bvm->wifi != wifi_none)
    link_ensure();
  if ((bvm->wifi == wifi_asked) &&
      (cloonix_get_msec() - bvm->wifi_ask_ms >= ASK_MS))
    send_ask(bvm);
}

void kvmwifi_agent_down(t_backdoor_vm *bvm)
{
  if (bvm->wifi == wifi_on)
    bvm->wifi = wifi_asked;
}

void kvmwifi_rx_ctrl(t_backdoor_vm *bvm, int val, int len, char *payload)
{
  if (bvm->wifi == wifi_none)
    KERR("WARNING kvmwifi: %s speaks of a radio nothing asked for: %d %s",
         bvm->name, val, payload);
  else if (val == header_val_wifi_on)
    {
    if (bvm->wifi != wifi_on)
      {
      bvm->wifi = wifi_on;
      peers_tell(bvm);
      }
    }
  else if (val == header_val_wifi_off)
    {
    /* a restarted agent: its radios went with the old one */
    if (bvm->wifi == wifi_on)
      {
      bvm->wifi = wifi_asked;
      send_ask(bvm);
      }
    }
  else if (val == header_val_wifi_ko)
    {
    KERR("WARNING kvmwifi: %s has no radio: %s", bvm->name, payload);
    bvm->wifi = wifi_refused;
    }
  else
    KERR("WARNING kvmwifi: %s sent the control %d", bvm->name, val);
}

/* a frame of one radio, to suid_power */
void kvmwifi_rx_frame(t_backdoor_vm *bvm, int len, char *payload)
{
  uint8_t *p = (uint8_t *) payload;
  if ((bvm->wifi != wifi_on) || (len < KVMWIFI_FRAME_HEAD + 10) ||
      (len > MAX_A2D_LEN) || (p[11] >= bvm->wifi_nb))
    KERR("WARNING kvmwifi: a frame of %d bytes from %s, radio state %d",
         len, bvm->name, bvm->wifi);
  else
    link_tx_frame(bvm, len, payload);
}

/* a kvm deleted, or dropped and added again by a qmp reconnect: a radio
   told peers before its ask counts as one */
void kvmwifi_gone(t_backdoor_vm *bvm)
{
  int i, told = (bvm->wifi != wifi_none);
  for (i = 0; i < MAX_VWIF; i++)
    if (bvm->wifi_nb_peers[i] >= 0)
      told = 1;
  if (told)
    {
    memset(g_gone[g_gone_idx], 0, MAX_NAME_LEN);
    strncpy(g_gone[g_gone_idx], bvm->name, MAX_NAME_LEN-1);
    g_gone_ms[g_gone_idx] = cloonix_get_msec();
    g_gone_idx = (g_gone_idx + 1) % NB_GONE;
    }
  bvm->wifi = wifi_none;
}
