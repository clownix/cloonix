/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* Medium of every air (cnt and kvm radios): frames to the same air only, each
   air written as radiotap pcap to snf/<air>_0; runs as cloonix-suid-power-air. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/genl.h>
#include <netlink/netlink.h>
#include <net/ethernet.h>
#include <linux/sock_diag.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "crun_utils.h"
#include "utils.h"
#include "vwifi.h"
#include "air_pcap.h"
#include "air_link.h"
#include "agent_wire.h"

char *get_net_name(void);
char *get_root_path(void);
int get_cloonix_rank(void);

#define VERSION_NR 1
enum {
  HWSIM_CMD_UNSPEC, HWSIM_CMD_REGISTER, HWSIM_CMD_FRAME,
  HWSIM_CMD_TX_INFO_FRAME, HWSIM_CMD_NEW_RADIO, HWSIM_CMD_DEL_RADIO,
  HWSIM_CMD_GET_RADIO, HWSIM_CMD_ADD_MAC_ADDR, HWSIM_CMD_DEL_MAC_ADDR,
};
#define MAX_ALIAS 4
enum {
  HWSIM_ATTR_UNSPEC, HWSIM_ATTR_ADDR_RECEIVER, HWSIM_ATTR_ADDR_TRANSMITTER,
  HWSIM_ATTR_FRAME, HWSIM_ATTR_FLAGS, HWSIM_ATTR_RX_RATE, HWSIM_ATTR_SIGNAL,
  HWSIM_ATTR_TX_INFO, HWSIM_ATTR_COOKIE, HWSIM_ATTR_CHANNELS,
  HWSIM_ATTR_RADIO_ID, HWSIM_ATTR_REG_HINT_ALPHA2, HWSIM_ATTR_REG_CUSTOM_REG,
  HWSIM_ATTR_REG_STRICT_REG, HWSIM_ATTR_SUPPORT_P2P_DEVICE,
  HWSIM_ATTR_USE_CHANCTX, HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE,
  HWSIM_ATTR_RADIO_NAME, HWSIM_ATTR_NO_VIF, HWSIM_ATTR_FREQ, HWSIM_ATTR_PAD,
  HWSIM_ATTR_TX_INFO_FLAGS, HWSIM_ATTR_PERM_ADDR, HWSIM_ATTR_MAX_USED,
};
#define HWSIM_TX_CTL_NO_ACK 2
#define HWSIM_TX_STAT_ACK   4

typedef struct t_radio
{
  char name[MAX_NAME_LEN];
  int kvm;
  int idx;
  char phy[MAX_NAME_LEN];
  uint8_t mac[6];
  uint8_t hw[6];
  char air[MAX_NAME_LEN];
  uint8_t alias[MAX_ALIAS][6];
  int nb_alias;
  int said_full;
  /* kvm radio: the peers last told its agent, -1 for none told yet */
  uint8_t sent[KVMWIFI_MAX_PEERS][6];
  int nb_sent;
  struct nl_sock *maker;
  struct t_radio *next;
} t_radio;

typedef struct t_air
{
  t_air_pcap cap;
  struct t_air *next;
} t_air;

/* the airs captured, cnf <air> sniff=on: kept by name, a kvm radio may come
   after; off told by the switch when the air is gone */
#define MAX_SNF_AIRS 32
static char g_snf_air[MAX_SNF_AIRS][MAX_NAME_LEN];

static int snf_air_on(char *name)
{
  int i;
  for (i = 0; i < MAX_SNF_AIRS; i++)
    if (!strcmp(g_snf_air[i], name))
      return 1;
  return 0;
}

static void snf_air_set(char *name, int on)
{
  int i;
  for (i = 0; i < MAX_SNF_AIRS; i++)
    if (!strcmp(g_snf_air[i], name))
      memset(g_snf_air[i], 0, MAX_NAME_LEN);
  if (!on)
    return;
  for (i = 0; (i < MAX_SNF_AIRS) && (g_snf_air[i][0]); i++);
  if (i == MAX_SNF_AIRS)
    KERR("WARNING air %s: %d airs captured already, not this one", name,
         MAX_SNF_AIRS);
  else
    strncpy(g_snf_air[i], name, MAX_NAME_LEN-1);
}

/* Radios deleted lately: the frames they queued before going are dropped
   without a word. */
#define NB_GONE 32
#define GONE_MS 10000
typedef struct t_gone
{
  char name[MAX_NAME_LEN];
  uint8_t mac[6];
  uint8_t hw[6];
  uint64_t at;
} t_gone;
static t_gone g_gone[NB_GONE];
static int g_gone_idx;

static struct nl_sock *g_sock;
static int g_fam;
static int g_llid;
static t_radio *g_radios;
static t_air *g_airs;
static int g_module_inserted;
static char g_why[MAX_NAME_LEN];
static int g_link_llid;
static unsigned long g_link_dropped;
static uint64_t g_link_last_say;

static int module_is_loaded(void)
{
  return (!access("/sys/module/mac80211_hwsim", F_OK));
}

/* the airs */
static t_air *air_find(char *name)
{
  t_air *cur = g_airs;
  while (cur && strcmp(cur->cap.name, name))
    cur = cur->next;
  return cur;
}

static int air_used(char *name)
{
  t_radio *r = g_radios;
  while (r && strcmp(r->air, name))
    r = r->next;
  return (r != NULL);
}

static void air_ensure(char *name)
{
  t_air *cur;
  char dir[MAX_PATH_LEN];
  if ((!name[0]) || air_find(name))
    return;
  cur = (t_air *) malloc(sizeof(t_air));
  if (!cur)
    KOUT("ERROR malloc");
  snprintf(dir, MAX_PATH_LEN-1, "%s/%s", get_root_path(), SNF_DIR);
  air_pcap_open(&(cur->cap), dir, name);
  cur->next = g_airs;
  g_airs = cur;
}

static void air_prune(void)
{
  t_air *cur = g_airs, *prev = NULL, *next;
  while (cur)
    {
    next = cur->next;
    if (!air_used(cur->cap.name))
      {
      air_pcap_close(&(cur->cap));
      if (prev)
        prev->next = next;
      else
        g_airs = next;
      free(cur);
      }
    else
      prev = cur;
    cur = next;
    }
}

/* the medium */
/* The kernel names a radio by its address with or without bit 0x40 of byte
   0, by version: learnt from the first frame. */
static int g_addr_form = -1;

static uint8_t *radio_addr(t_radio *r)
{
  return (g_addr_form == 0) ? r->mac : r->hw;
}

static t_radio *radio_with_addr(uint8_t *a)
{
  t_radio *r;
  for (r = g_radios; r; r = r->next)
    {
    if (r->kvm)
      continue;
    if (!memcmp(r->mac, a, 6) || !memcmp(r->hw, a, 6))
      {
      if (g_addr_form == -1)
        g_addr_form = memcmp(r->mac, a, 6) ? 1 : 0;
      return r;
      }
    }
  return NULL;
}

static void gone_add(t_radio *r)
{
  memset(g_gone[g_gone_idx].name, 0, MAX_NAME_LEN);
  strncpy(g_gone[g_gone_idx].name, r->name, MAX_NAME_LEN-1);
  memcpy(g_gone[g_gone_idx].mac, r->mac, 6);
  memcpy(g_gone[g_gone_idx].hw, r->hw, 6);
  g_gone[g_gone_idx].at = cloonix_get_msec();
  g_gone_idx = (g_gone_idx + 1) % NB_GONE;
}

static int gone_lately(uint8_t *a)
{
  uint64_t now = cloonix_get_msec();
  int i;
  for (i = 0; i < NB_GONE; i++)
    if ((g_gone[i].at) && (now - g_gone[i].at < GONE_MS) &&
        ((!memcmp(g_gone[i].mac, a, 6)) || (!memcmp(g_gone[i].hw, a, 6))))
      return 1;
  return 0;
}

static int gone_name_lately(char *name)
{
  uint64_t now = cloonix_get_msec();
  int i;
  for (i = 0; i < NB_GONE; i++)
    if ((g_gone[i].at) && (now - g_gone[i].at < GONE_MS) &&
        (!strcmp(g_gone[i].name, name)))
      return 1;
  return 0;
}

static t_radio *radio_find(char *name, int idx)
{
  t_radio *r = g_radios;
  while (r && (strcmp(r->name, name) || (r->idx != idx)))
    r = r->next;
  return r;
}

static int radio_has(t_radio *r, uint8_t *a)
{
  int i;
  if ((!memcmp(a, r->mac, 6)) || (!memcmp(a, r->hw, 6)))
    return 1;
  for (i = 0; i < r->nb_alias; i++)
    if (!memcmp(a, r->alias[i], 6))
      return 1;
  return 0;
}

/* the air link */
static int kvm_radio_exists(void)
{
  t_radio *r = g_radios;
  while (r && (!r->kvm))
    r = r->next;
  return (r != NULL);
}

static void link_lost(int llid)
{
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  if (llid != g_link_llid)
    return;
  g_link_llid = 0;
  if (kvm_radio_exists())
    KERR("WARNING air: doorways left the air link, the kvm radios hear "
         "nothing until it is back");
}

/* One message to doorways, dropped rather than waited for. No link: the
   kvm radios are not heard, said by doorways which connects. */
static int link_tx(int type, char *name, uint8_t *pay, int len)
{
  static uint8_t buf[AIR_LINK_HEAD + MAX_A2D_LEN];
  uint64_t now;
  if (!g_link_llid)
    return -1;
  if (len > MAX_A2D_LEN)
    {
    KERR("WARNING air: a message of %d bytes for %s, too long for the air "
         "link", len, name);
    return -1;
    }
  memset(buf, 0, AIR_LINK_HEAD);
  buf[0] = type;
  strncpy((char *) buf + 1, name, MAX_NAME_LEN-1);
  memcpy(buf + AIR_LINK_HEAD, pay, len);
  if (send(get_fd_with_llid(g_link_llid), buf, AIR_LINK_HEAD + len,
           MSG_DONTWAIT | MSG_NOSIGNAL) >= 0)
    return 0;
  if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == ENOBUFS))
    {
    g_link_dropped += 1;
    now = cloonix_get_msec();
    if (now - g_link_last_say >= 10000)
      {
      g_link_last_say = now;
      KERR("WARNING air: doorways is not taking frames, %lu dropped so far",
           g_link_dropped);
      }
    }
  else
    {
    KERR("WARNING air: the air link to doorways: %s", strerror(errno));
    link_lost(g_link_llid);
    }
  return -1;
}

static void put_be32(uint8_t *p, uint32_t v)
{
  p[0] = (v >> 24) & 0xff;
  p[1] = (v >> 16) & 0xff;
  p[2] = (v >> 8) & 0xff;
  p[3] = v & 0xff;
}

static uint32_t get_be32(uint8_t *p)
{
  return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static int kvm_frame_tx(t_radio *to, uint8_t *frame, int len, uint32_t rate,
                        uint32_t freq)
{
  static uint8_t pay[MAX_A2D_LEN];
  if (len > MAX_A2D_LEN - KVMWIFI_FRAME_HEAD)
    {
    KERR("WARNING air %s: a frame of %d bytes too long for %s %d", to->air,
         len, to->name, to->idx);
    return -1;
    }
  put_be32(pay, freq);
  put_be32(pay + 4, rate);
  put_be32(pay + 8, to->idx);
  memcpy(pay + KVMWIFI_FRAME_HEAD, frame, len);
  return link_tx(AIR_LINK_FRAME, to->name, pay, KVMWIFI_FRAME_HEAD + len);
}

/* the addresses of the other radios on the air of r */
static int peers_of(t_radio *r, uint8_t *list)
{
  t_radio *x;
  int i, nb = 0;
  if (!r->air[0])
    return 0;
  for (x = g_radios; x; x = x->next)
    {
    if ((x == r) || strcmp(x->air, r->air))
      continue;
    for (i = -1; i < x->nb_alias; i++)
      {
      if (nb == KVMWIFI_MAX_PEERS)
        {
        KERR("WARNING air %s: more than %d addresses, the acks of the "
             "others fail", r->air, KVMWIFI_MAX_PEERS);
        return nb;
        }
      memcpy(list + 6*nb, (i < 0) ? x->mac : x->alias[i], 6);
      nb += 1;
      }
    }
  return nb;
}

/* The agent of a kvm acks a unicast of a radio when its receiver is a
   peer: told, per kvm radio, whenever its peers change. */
static void peers_update(void)
{
  static uint8_t pay[1 + 6*KVMWIFI_MAX_PEERS];
  t_radio *r;
  int nb;
  if (!g_link_llid)
    return;
  for (r = g_radios; r; r = r->next)
    {
    if (!r->kvm)
      continue;
    nb = peers_of(r, pay + 1);
    if ((nb == r->nb_sent) && (!memcmp(pay + 1, r->sent, 6*nb)))
      continue;
    pay[0] = r->idx & 0xff;
    if (link_tx(AIR_LINK_PEERS, r->name, pay, 1 + 6*nb))
      r->nb_sent = -1;
    else
      {
      memcpy(r->sent, pay + 1, 6*nb);
      r->nb_sent = nb;
      }
    }
}

/* A radio address (cnt vif, scan random, kvm sender): frames to it are
   acked as frames to the radio. */
static void alias_add(t_radio *r, uint8_t *m)
{
  if (radio_has(r, m))
    return;
  if (r->nb_alias == MAX_ALIAS)
    {
    if (!r->said_full)
      KERR("WARNING air: %s %d has %d addresses already, the others are "
           "ignored", r->name, r->idx, MAX_ALIAS);
    r->said_full = 1;
    return;
    }
  memcpy(r->alias[r->nb_alias++], m, 6);
  peers_update();
}

static void radio_alias(struct nlattr **a, int add)
{
  t_radio *r;
  uint8_t *m;
  int i;
  if ((!a[HWSIM_ATTR_ADDR_TRANSMITTER]) || (!a[HWSIM_ATTR_ADDR_RECEIVER]))
    {
    KERR("WARNING air: an address change of a radio lacks an address");
    return;
    }
  r = radio_with_addr(nla_data(a[HWSIM_ATTR_ADDR_TRANSMITTER]));
  m = nla_data(a[HWSIM_ATTR_ADDR_RECEIVER]);
  if (!r)
    {
    uint8_t *t = nla_data(a[HWSIM_ATTR_ADDR_TRANSMITTER]);
    if (!gone_lately(t))
      KERR("WARNING air: an address change of %02x:%02x:%02x:%02x:%02x:%02x, "
           "not a radio of the net", t[0], t[1], t[2], t[3], t[4], t[5]);
    return;
    }
  if (add)
    {
    alias_add(r, m);
    return;
    }
  for (i = 0; i < r->nb_alias; i++)
    if (!memcmp(r->alias[i], m, 6))
      break;
  if (i < r->nb_alias)
    {
    r->nb_alias -= 1;
    memmove(r->alias[i], r->alias[i+1], (r->nb_alias - i) * 6);
    peers_update();
    }
}

static int send_frame(t_radio *to, uint8_t *frame, int len, uint32_t rate,
                      uint32_t freq)
{
  struct nl_msg *m;
  int result = -1;
  if (!g_sock)
    return -1;
  m = nlmsg_alloc();
  if (!m)
    {
    KERR("WARNING air: nlmsg_alloc for a frame of %d bytes", len);
    return -1;
    }
  if (genlmsg_put(m, NL_AUTO_PORT, NL_AUTO_SEQ, g_fam, 0, NLM_F_REQUEST,
                  HWSIM_CMD_FRAME, VERSION_NR) &&
      (!nla_put(m, HWSIM_ATTR_ADDR_RECEIVER, 6, radio_addr(to))) &&
      (!nla_put(m, HWSIM_ATTR_FRAME, len, frame)) &&
      (!nla_put_u32(m, HWSIM_ATTR_RX_RATE, rate)) &&
      (!nla_put_u32(m, HWSIM_ATTR_SIGNAL, (uint32_t) AIR_SIGNAL)) &&
      ((!freq) || (!nla_put_u32(m, HWSIM_ATTR_FREQ, freq))))
    result = (nl_send_auto(g_sock, m) < 0) ? -1 : 0;
  else
    KERR("WARNING air: a frame of %d bytes does not fit a netlink message",
         len);
  nlmsg_free(m);
  return result;
}

static void send_tx_info(struct nlattr **a, uint32_t flags)
{
  struct nl_msg *m;
  if (!g_sock)
    return;
  m = nlmsg_alloc();
  if (!m)
    {
    KERR("WARNING air: nlmsg_alloc for a tx status");
    return;
    }
  if (genlmsg_put(m, NL_AUTO_PORT, NL_AUTO_SEQ, g_fam, 0, NLM_F_REQUEST,
                  HWSIM_CMD_TX_INFO_FRAME, VERSION_NR) &&
      (!nla_put(m, HWSIM_ATTR_ADDR_TRANSMITTER, 6,
                nla_data(a[HWSIM_ATTR_ADDR_TRANSMITTER]))) &&
      (!nla_put_u32(m, HWSIM_ATTR_FLAGS, flags)) &&
      (!nla_put_u32(m, HWSIM_ATTR_SIGNAL, (uint32_t) AIR_SIGNAL)) &&
      (!nla_put(m, HWSIM_ATTR_TX_INFO, nla_len(a[HWSIM_ATTR_TX_INFO]),
                nla_data(a[HWSIM_ATTR_TX_INFO]))) &&
      (!nla_put_u64(m, HWSIM_ATTR_COOKIE,
                    nla_get_u64(a[HWSIM_ATTR_COOKIE]))))
    {
    if (nl_send_auto(g_sock, m) < 0)
      KERR("WARNING air: the tx status of a frame did not go");
    }
  else
    KERR("WARNING air: a tx status does not build");
  nlmsg_free(m);
}

/* A frame of a radio, at least 10 bytes, on an air: to the other radios
   of the air, then to its capture. 1 when a receiver has its address 1. */
static int air_deliver(t_radio *from, uint8_t *frame, int len, uint32_t rate,
                       uint32_t freq)
{
  t_radio *r;
  t_air *air;
  int went, heard = 0;
  for (r = g_radios; r; r = r->next)
    {
    if ((r == from) || strcmp(r->air, from->air))
      continue;
    if (r->kvm)
      went = !kvm_frame_tx(r, frame, len, rate, freq);
    else if (send_frame(r, frame, len, rate, freq))
      {
      went = 0;
      KERR("WARNING air %s: a frame to %s %d did not go", r->air, r->name,
           r->idx);
      }
    else
      went = 1;
    if (went && radio_has(r, frame + 4))
      heard = 1;
    }
  if (snf_air_on(from->air))
    {
    air = air_find(from->air);
    if (air)
      air_pcap_write(&(air->cap), frame, len, freq);
    else
      KERR("WARNING air %s: no capture record for it", from->air);
    }
  return heard;
}

/* One frame a cnt radio sent: to its air, then its status. */
static int medium_rx(struct nl_msg *msg, void *arg)
{
  struct genlmsghdr *gh = nlmsg_data(nlmsg_hdr(msg));
  struct nlattr *a[HWSIM_ATTR_MAX_USED + 1];
  t_radio *from;
  uint8_t *frame;
  uint32_t flags, freq = 0, rate = 0;
  int len, acked = 0;
  if ((gh->cmd == HWSIM_CMD_ADD_MAC_ADDR) ||
      (gh->cmd == HWSIM_CMD_DEL_MAC_ADDR))
    {
    if (genlmsg_parse(nlmsg_hdr(msg), 0, a, HWSIM_ATTR_MAX_USED, NULL))
      KERR("WARNING air: an address change of a radio does not parse");
    else
      radio_alias(a, gh->cmd == HWSIM_CMD_ADD_MAC_ADDR);
    return NL_SKIP;
    }
  if (gh->cmd != HWSIM_CMD_FRAME)
    {
    KERR("WARNING air: hwsim command %d on the medium socket, ignored",
         gh->cmd);
    return NL_SKIP;
    }
  if (genlmsg_parse(nlmsg_hdr(msg), 0, a, HWSIM_ATTR_MAX_USED, NULL) ||
      (!a[HWSIM_ATTR_ADDR_TRANSMITTER]) || (!a[HWSIM_ATTR_FRAME]) ||
      (!a[HWSIM_ATTR_FLAGS]) || (!a[HWSIM_ATTR_TX_INFO]) ||
      (!a[HWSIM_ATTR_COOKIE]))
    {
    KERR("WARNING air: a frame from the kernel lacks an attribute");
    return NL_SKIP;
    }
  frame = nla_data(a[HWSIM_ATTR_FRAME]);
  len = nla_len(a[HWSIM_ATTR_FRAME]);
  flags = nla_get_u32(a[HWSIM_ATTR_FLAGS]);
  if (a[HWSIM_ATTR_FREQ])
    freq = nla_get_u32(a[HWSIM_ATTR_FREQ]);
  if (nla_len(a[HWSIM_ATTR_TX_INFO]) >= 1)
    rate = ((int8_t *) nla_data(a[HWSIM_ATTR_TX_INFO]))[0] < 0 ? 0 :
           ((uint8_t *) nla_data(a[HWSIM_ATTR_TX_INFO]))[0];
  from = radio_with_addr(nla_data(a[HWSIM_ATTR_ADDR_TRANSMITTER]));
  if (!from)
    {
    uint8_t *t = nla_data(a[HWSIM_ATTR_ADDR_TRANSMITTER]);
    if (!gone_lately(t))
      KERR("WARNING air: a frame from %02x:%02x:%02x:%02x:%02x:%02x, not a "
           "radio of the net", t[0], t[1], t[2], t[3], t[4], t[5]);
    }
  else if (len < 10)
    KERR("WARNING air: a frame of %d bytes from %s %d", len, from->name,
         from->idx);
  /* a radio on no air: nobody hears it */
  else if (from->air[0])
    acked = air_deliver(from, frame, len, rate, freq) || (frame[4] & 1);
  if (acked && !(flags & HWSIM_TX_CTL_NO_ACK))
    flags |= HWSIM_TX_STAT_ACK;
  send_tx_info(a, flags);
  return NL_OK;
}

/* -EINVAL: receiver down or on another channel, nominal: skip it. */
static int medium_refused(struct sockaddr_nl *nla, struct nlmsgerr *e,
                          void *arg)
{
  if (e->error != -EINVAL)
    KERR("WARNING air: the kernel refused a message of the medium: %s",
         strerror(-e->error));
  return NL_SKIP;
}

/* ENOBUFS when the radios send faster than this reads: the kernel lost
   frames, one line per ten seconds */
static int medium_fd_rx(int llid, int fd)
{
  static unsigned long fails;
  static uint64_t said;
  int err = nl_recvmsgs_default(g_sock);
  if ((err < 0) && (err != -NLE_AGAIN))
    {
    fails += 1;
    if (cloonix_get_msec() - said >= 10000)
      {
      said = cloonix_get_msec();
      KERR("WARNING air: netlink receive: %s, %lu so far", nl_geterror(err),
           fails);
      }
    }
  return 1;
}

static int medium_open(void);

/* A radio medium loses what it cannot read in time, as the air: the socket
   is NETLINK_NO_ENOBUFS, never congested; its drops said on the beat. */
static uint32_t g_drops_said;

static void medium_drops(void)
{
  uint32_t mi[SK_MEMINFO_VARS];
  socklen_t len = sizeof(mi);
  if (!g_sock)
    return;
  if (getsockopt(nl_socket_get_fd(g_sock), SOL_SOCKET, SO_MEMINFO, mi, &len))
    KERR("WARNING air: SO_MEMINFO of the medium: %s", strerror(errno));
  else if (mi[SK_MEMINFO_DROPS] != g_drops_said)
    {
    KERR("WARNING air: %u frames of the radios dropped, not read in time by "
         "the medium, %u so far", mi[SK_MEMINFO_DROPS] - g_drops_said,
         mi[SK_MEMINFO_DROPS]);
    g_drops_said = mi[SK_MEMINFO_DROPS];
    }
}

static void medium_reopen(void *data)
{
  if (medium_open())
    clownix_timeout_add(100, medium_reopen, NULL, NULL, NULL);
  else
    KERR("WARNING air: the medium of the net is registered again");
}

/* the kernel took the radios back from cloonix: registered again */
static void medium_err(int llid, int err, int from)
{
  KERR("ERROR air: the medium socket closed (err %d from %d), registering "
       "again", err, from);
  g_llid = 0;
  nl_socket_free(g_sock);
  g_sock = NULL;
  clownix_timeout_add(100, medium_reopen, NULL, NULL, NULL);
}

/* One frame a kvm radio sent, relayed by doorways: to its air. Its agent
   acks it by the peers it was told. */
static void kvm_frame_rx(char *name, uint8_t *p, int len)
{
  t_radio *from;
  uint8_t *frame = p + KVMWIFI_FRAME_HEAD;
  int idx, flen = len - KVMWIFI_FRAME_HEAD;
  if (flen < 10)
    {
    KERR("WARNING air: a frame of %d bytes from %s", len, name);
    return;
    }
  idx = (int) get_be32(p + 8);
  from = radio_find(name, idx);
  if ((!from) || (!from->kvm))
    {
    if (!gone_name_lately(name))
      KERR("WARNING air: a frame from %s %d, not a kvm radio of the net",
           name, idx);
    return;
    }
  /* a probe request may come from a random address */
  if ((flen >= 16) && (frame[0] != 0x40) && (!(frame[10] & 1)))
    alias_add(from, frame + 10);
  if (from->air[0])
    air_deliver(from, frame, flen, get_be32(p + 4), get_be32(p));
}

static void link_err(int llid, int err, int from)
{
  link_lost(llid);
}

static int link_rx(int llid, int fd)
{
  static uint8_t buf[AIR_LINK_HEAD + MAX_A2D_LEN + 1];
  char name[MAX_NAME_LEN];
  int i, len;
  for (i = 0; i < 64; i++)
    {
    len = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
    if ((len < 0) && ((errno == EAGAIN) || (errno == EINTR)))
      break;
    if (len <= 0)
      {
      if (len < 0)
        KERR("WARNING air: the air link from doorways: %s", strerror(errno));
      link_lost(llid);
      break;
      }
    if ((len <= AIR_LINK_HEAD) || (len > AIR_LINK_HEAD + MAX_A2D_LEN) ||
        (buf[0] != AIR_LINK_FRAME))
      {
      KERR("WARNING air: a message of %d bytes, type %d, on the air link",
           len, buf[0]);
      continue;
      }
    memcpy(name, buf + 1, MAX_NAME_LEN);
    name[MAX_NAME_LEN-1] = 0;
    kvm_frame_rx(name, buf + AIR_LINK_HEAD, len - AIR_LINK_HEAD);
    }
  return 0;
}

/* doorways connects: every kvm radio gets its peers told again */
static int link_listen_rx(int llid, int fd)
{
  t_radio *r;
  int nfd = accept4(fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (nfd < 0)
    {
    KERR("WARNING air: accept on the air link: %s", strerror(errno));
    return 0;
    }
  if (g_link_llid)
    {
    KERR("WARNING air: doorways connects again, the former air link closed");
    msg_delete_channel(g_link_llid);
    }
  g_link_llid = msg_watch_fd(nfd, link_rx, link_err, "airlink");
  if (g_link_llid <= 0)
    {
    KERR("ERROR air: the air link is not watched, the kvm radios hear "
         "nothing");
    close(nfd);
    g_link_llid = 0;
    return 0;
    }
  for (r = g_radios; r; r = r->next)
    r->nb_sent = -1;
  peers_update();
  return 0;
}

static void link_listen_err(int llid, int err, int from)
{
  KERR("ERROR air: the air link listen socket closed (err %d from %d), the "
       "kvm radios hear nothing now", err, from);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}

static void link_listen(void)
{
  struct sockaddr_un addr;
  int fd;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/%s", get_root_path(),
           AIR_LINK_SOCK);
  unlink(addr.sun_path);
  fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0)
    KOUT("ERROR air: socket for %s: %s", addr.sun_path, strerror(errno));
  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) || listen(fd, 4))
    KOUT("ERROR air: listen on %s: %s", addr.sun_path, strerror(errno));
  if (chown(addr.sun_path, get_caller_uid(), get_caller_gid()))
    KERR("ERROR chown %s %d", addr.sun_path, errno);
  if (chmod(addr.sun_path, S_IRUSR | S_IWUSR))
    KERR("ERROR chmod %s %d", addr.sun_path, errno);
  if (msg_watch_fd(fd, link_listen_rx, link_listen_err, "airl") <= 0)
    KOUT("ERROR air: the air link listen socket is not watched");
}

/* The answer of a request: the kernel answers a new radio with its index,
   any value, so whether it answered is kept apart from what it said. */
typedef struct t_answer
{
  int got;
  int err;
} t_answer;

/* NL_SKIP, not NL_STOP: on NL_STOP libnl turns any non-zero answer, the
   index of a new radio included, into its own error */
static int ack_err(struct sockaddr_nl *nla, struct nlmsgerr *e, void *arg)
{
  ((t_answer *) arg)->got = 1;
  ((t_answer *) arg)->err = e->error;
  return NL_SKIP;
}

static int ack_ok(struct nl_msg *msg, void *arg)
{
  ((t_answer *) arg)->got = 1;
  ((t_answer *) arg)->err = 0;
  return NL_STOP;
}

/* A kernel that does not answer is reported, not waited for forever. */
#define ANSWER_SECS 2
static void answer_timeout(int fd)
{
  struct timeval tv = { ANSWER_SECS, 0 };
  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)))
    KERR("WARNING air: no receive timeout on a netlink socket: %s",
         strerror(errno));
}

/* genl request, blocking for its answer: 0, or -errno. The kernel answers
   a new radio with its index, not 0. */
static int request(struct nl_sock *sock, struct nl_msg *m)
{
  int fd = nl_socket_get_fd(sock), err;
  t_answer res = { 0, 0 };
  struct nl_cb *cb = nl_cb_clone(nl_socket_get_cb(sock));
  if (!cb)
    KOUT("ERROR nl_cb_clone");
  nl_cb_err(cb, NL_CB_CUSTOM, ack_err, &res);
  nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_ok, &res);
  err = nl_send_auto(sock, m);
  nlmsg_free(m);
  if (err >= 0)
    {
    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK))
      KERR("WARNING air: blocking the netlink socket: %s", strerror(errno));
    answer_timeout(fd);
    while (!res.got)
      {
      err = nl_recvmsgs(sock, cb);
      if (err < 0)
        break;
      }
    if ((sock == g_sock) && (nl_socket_set_nonblocking(sock) < 0))
      KERR("WARNING air: the medium socket stays blocking");
    }
  nl_cb_put(cb);
  /* reported by the caller, which knows what the request was */
  if (err == -NLE_AGAIN)
    return -ETIMEDOUT;
  if (err < 0)
    {
    KERR("WARNING air: netlink request: %s", nl_geterror(err));
    return -EIO;
    }
  return (res.err < 0) ? res.err : 0;
}

/* the -errno of a refused request */
static const char *request_error(int err)
{
  static char txt[MAX_NAME_LEN];
  if (err < 0)
    snprintf(txt, MAX_NAME_LEN-1, "%s", strerror(-err));
  return txt;
}

static struct nl_msg *new_request(int cmd)
{
  struct nl_msg *m = nlmsg_alloc();
  if (!m)
    KERR("WARNING air: nlmsg_alloc for hwsim command %d", cmd);
  else if (!genlmsg_put(m, NL_AUTO_PORT, NL_AUTO_SEQ, g_fam, 0,
                        NLM_F_REQUEST | NLM_F_ACK, cmd, VERSION_NR))
    {
    KERR("WARNING air: hwsim command %d does not build", cmd);
    nlmsg_free(m);
    m = NULL;
    }
  return m;
}

/* A genl socket made inside cloonix_<net>: its radios are a netgroup. */
static struct nl_sock *genl_in(char *family, int *fam)
{
  char ns[MAX_PATH_LEN];
  struct nl_sock *sock = NULL;
  int self, net;
  *fam = -1;
  snprintf(ns, MAX_PATH_LEN-1, "%s%s_%s", PATH_NAMESPACE, BASE_NAMESPACE,
           get_net_name());
  self = open("/proc/self/ns/net", O_RDONLY | O_CLOEXEC);
  net = open(ns, O_RDONLY | O_CLOEXEC);
  if ((self < 0) || (net < 0) || setns(net, CLONE_NEWNET))
    {
    KERR("ERROR air: cannot enter %s: %s", ns, strerror(errno));
    strncpy(g_why, "no_net_namespace", MAX_NAME_LEN-1);
    if (self >= 0) close(self);
    if (net >= 0) close(net);
    return NULL;
    }
  sock = nl_socket_alloc();
  if (sock && genl_connect(sock))
    {
    nl_socket_free(sock);
    sock = NULL;
    }
  if (sock)
    *fam = genl_ctrl_resolve(sock, family);
  if (setns(self, CLONE_NEWNET))
    KOUT("ERROR air: cannot come back to the namespace of suid_power");
  close(self);
  close(net);
  if ((sock) && (*fam < 0))
    {
    nl_socket_free(sock);
    sock = NULL;
    }
  if (!sock)
    {
    KERR("ERROR air: no %s netlink family in %s", family, ns);
    return NULL;
    }
  nl_socket_disable_seq_check(sock);
  nl_socket_disable_auto_ack(sock);
  return sock;
}

/* A genl socket made inside cloonix_<net>: its radios are a netgroup. */
static struct nl_sock *genl_in_net(void)
{
  struct nl_sock *sock = genl_in("MAC80211_HWSIM", &g_fam);
  if (!sock)
    strncpy(g_why, "mac80211_hwsim_not_loaded_on_host", MAX_NAME_LEN-1);
  return sock;
}

/* The wiphy index of a phy of cloonix_<net>, asked of nl80211 there: its
   name cannot be looked up, sysfs shows the phys of another namespace. */
#define NL80211_CMD_GET_WIPHY   1
#define NL80211_ATTR_WIPHY      1
#define NL80211_ATTR_WIPHY_NAME 2
#define NL80211_ATTR_SPLIT_WIPHY_DUMP 174
typedef struct { char *phy; int idx; } t_wiphy_find;

static int wiphy_rx(struct nl_msg *msg, void *arg)
{
  t_wiphy_find *w = (t_wiphy_find *) arg;
  struct nlattr *a[NL80211_ATTR_WIPHY_NAME + 1];
  if (genlmsg_parse(nlmsg_hdr(msg), 0, a, NL80211_ATTR_WIPHY_NAME, NULL))
    return NL_SKIP;
  if (a[NL80211_ATTR_WIPHY] && a[NL80211_ATTR_WIPHY_NAME] &&
      (!strcmp(nla_get_string(a[NL80211_ATTR_WIPHY_NAME]), w->phy)))
    w->idx = (int) nla_get_u32(a[NL80211_ATTR_WIPHY]);
  return NL_SKIP;
}

static int wiphy_index(char *phy)
{
  t_wiphy_find w;
  struct nl_msg *m;
  int fam, err;
  struct nl_sock *sock = genl_in("nl80211", &fam);
  w.phy = phy;
  w.idx = -1;
  if (!sock)
    return -1;
  m = nlmsg_alloc();
  /* split: a wiphy of hwsim does not fit one message */
  if (m && genlmsg_put(m, NL_AUTO_PORT, NL_AUTO_SEQ, fam, 0, NLM_F_DUMP,
                       NL80211_CMD_GET_WIPHY, 0) &&
      (!nla_put_flag(m, NL80211_ATTR_SPLIT_WIPHY_DUMP)))
    {
    nl_socket_enable_msg_peek(sock);
    answer_timeout(nl_socket_get_fd(sock));
    nl_socket_modify_cb(sock, NL_CB_VALID, NL_CB_CUSTOM, wiphy_rx, &w);
    err = nl_send_auto(sock, m);
    if (err >= 0)
      err = nl_recvmsgs_default(sock);
    if (err < 0)
      KERR("WARNING air: nl80211 dump for %s: %s", phy, nl_geterror(err));
    }
  else
    KERR("WARNING air: the nl80211 dump request for %s does not build", phy);
  if (m)
    nlmsg_free(m);
  nl_socket_free(sock);
  if (w.idx < 0)
    KERR("ERROR air: no wiphy %s in the namespace of the net", phy);
  return w.idx;
}

/* The medium of the netgroup: every frame of its radios comes here. */
static int medium_open(void)
{
  struct nl_msg *m;
  struct nl_cb *cb;
  int err, fd, one = 1;
  if (g_sock)
    return 0;
  g_sock = genl_in_net();
  if (!g_sock)
    return -1;
  nl_socket_modify_cb(g_sock, NL_CB_VALID, NL_CB_CUSTOM, medium_rx, NULL);
  m = new_request(HWSIM_CMD_REGISTER);
  err = m ? request(g_sock, m) : -ENOMEM;
  if (err < 0)
    {
    KERR("ERROR air: registering as the medium of the net: %s",
         request_error(err));
    strncpy(g_why, "medium_registration_refused", MAX_NAME_LEN-1);
    nl_socket_free(g_sock);
    g_sock = NULL;
    return -1;
    }
  g_drops_said = 0;
  if (setsockopt(nl_socket_get_fd(g_sock), SOL_NETLINK, NETLINK_NO_ENOBUFS,
                 &one, sizeof(one)))
    KERR("ERROR air: NETLINK_NO_ENOBUFS on the medium: %s, an overflow "
         "stops the air until it is read empty", strerror(errno));
  cb = nl_socket_get_cb(g_sock);
  if (!cb)
    KERR("ERROR air: no callbacks on the medium socket");
  else
    {
    nl_cb_err(cb, NL_CB_CUSTOM, medium_refused, NULL);
    nl_cb_put(cb);
    }
  /* the channel closes its own copy, g_sock is freed apart */
  fd = dup(nl_socket_get_fd(g_sock));
  if (fd < 0)
    KERR("ERROR air: dup of the medium socket: %s", strerror(errno));
  else
    {
    g_llid = msg_watch_fd(fd, medium_fd_rx, medium_err, "air");
    if (g_llid <= 0)
      {
      KERR("ERROR air: the medium socket is not watched");
      close(fd);
      }
    }
  return 0;
}

/* the radios */
/* One radio made in cloonix_<net>: its wiphy index, -1 if none. */
static int radio_add(char *cnt, int vm_id, int idx, uint8_t *mac)
{
  struct nl_sock *maker = genl_in_net();
  struct nl_msg *m = new_request(HWSIM_CMD_NEW_RADIO);
  t_radio *r;
  int err, wiphy;
  if (!maker)
    {
    if (m)
      nlmsg_free(m);
    return -1;
    }
  r = (t_radio *) malloc(sizeof(t_radio));
  if ((!r) || (!m))
    KOUT("ERROR malloc");
  memset(r, 0, sizeof(t_radio));
  strncpy(r->name, cnt, MAX_NAME_LEN-1);
  r->idx = idx;
  snprintf(r->phy, MAX_NAME_LEN-1, "cl%d_%d_%d", get_cloonix_rank(), vm_id,
           idx);
  r->maker = maker;
  memcpy(r->mac, mac, 6);
  memcpy(r->hw, mac, 6);
  r->hw[0] |= 0x40;
  strncpy(r->air, CNTWIFI_LAN, MAX_NAME_LEN-1);
  if (nla_put(m, HWSIM_ATTR_PERM_ADDR, ETH_ALEN, mac) ||
      nla_put_string(m, HWSIM_ATTR_RADIO_NAME, r->phy) ||
      nla_put_flag(m, HWSIM_ATTR_NO_VIF) ||
      nla_put_flag(m, HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE))
    {
    KERR("ERROR air: the new radio request for %s does not build", cnt);
    nlmsg_free(m);
    nl_socket_free(maker);
    free(r);
    return -1;
    }
  err = request(maker, m);
  /* the radio may be made and its answer lost: the phys of the net say */
  if (err == -ETIMEDOUT)
    KERR("WARNING air: no answer for the radio %s of %s, looked for among "
         "the phys of the net", r->phy, cnt);
  else if (err < 0)
    {
    KERR("ERROR air: the radio %s of %s was refused: %s", r->phy, cnt,
         request_error(err));
    nl_socket_free(maker);
    free(r);
    return -1;
    }
  /* iw names it phy#<index>: iw phy <name> would ask sysfs */
  wiphy = wiphy_index(r->phy);
  if (wiphy >= 0)
    {
    r->next = g_radios;
    g_radios = r;
    air_ensure(r->air);
    return wiphy;
    }
  KERR("ERROR air: the radio %s of %s has no wiphy", r->phy, cnt);
  /* closed, its socket takes the radio with it */
  nl_socket_free(maker);
  free(r);
  return -1;
}

/* The radios of a cnt or kvm. The socket that made a cnt radio: closed,
   the radio is gone. */
static void radios_delete(char *name)
{
  t_radio *r = g_radios, *prev = NULL, *next;
  while (r)
    {
    next = r->next;
    if (!strcmp(r->name, name))
      {
      gone_add(r);
      if (r->maker)
        nl_socket_free(r->maker);
      if (prev)
        prev->next = next;
      else
        g_radios = next;
      free(r);
      }
    else
      prev = r;
    r = next;
    }
  air_prune();
  peers_update();
}

/* the wiphy of each radio into wiphy[] */
static int radios_create(char *cnt, int vm_id, int nb, uint8_t *mac_vwif,
                         int *wiphy)
{
  uint8_t mac[6];
  int i;
  memset(g_why, 0, MAX_NAME_LEN);
  strncpy(g_why, "no_radio", MAX_NAME_LEN-1);
  if (!g_module_inserted)
    {
    strncpy(g_why, "mac80211_hwsim_not_loaded_on_host", MAX_NAME_LEN-1);
    KERR("ERROR cannot add a vwifi interface: the mac80211_hwsim module is "
         "not loaded, and cloonix does not load it for you. For wifi "
         "emulation: sudo modprobe mac80211_hwsim radios=0");
    return -1;
    }
  if (access(pthexec_iw_bin(), X_OK))
    {
    strncpy(g_why, "no_iw_in_this_cloonfs", MAX_NAME_LEN-1);
    KERR("ERROR cannot add a vwifi interface: %s is not there",
         pthexec_iw_bin());
    return -1;
    }
  if ((nb < 1) || (nb > MAX_VWIF))
    {
    strncpy(g_why, "bad_number_of_radios", MAX_NAME_LEN-1);
    KERR("ERROR %s: %d radios asked, 1 to %d", cnt, nb, MAX_VWIF);
    return -1;
    }
  if (medium_open())
    return -1;
  for (i = 0; i < nb; i++)
    {
    memcpy(mac, mac_vwif, 6);
    if (nb != 1)
      mac[5] = i;
    wiphy[i] = radio_add(cnt, vm_id, i, mac);
    if (wiphy[i] < 0)
      {
      radios_delete(cnt);
      return -1;
      }
    }
  peers_update();
  return 0;
}

/* The air of one radio, "" for none. A kvm radio is told its peers even
   when they are the same: it clears what it heard on its former air. */
static void radio_air_set(t_radio *r, char *air)
{
  memset(r->air, 0, MAX_NAME_LEN);
  strncpy(r->air, air, MAX_NAME_LEN-1);
  r->nb_sent = -1;
  air_ensure(r->air);
  air_prune();
  peers_update();
}

static int cnt_radio_air(char *cnt, int idx, char *air)
{
  t_radio *r = radio_find(cnt, idx);
  if ((!r) || (r->kvm))
    {
    KERR("ERROR air: %s has no radio wlan%d", cnt, idx);
    return -1;
    }
  radio_air_set(r, air);
  return 0;
}

/* A kvm radio, told by the switch at each declaration of its kvm and at
   each air change: made the first time. */
static int kvm_radio(char *name, int idx, uint8_t *mac, char *air)
{
  t_radio *r = radio_find(name, idx);
  if ((idx < 0) || (idx >= MAX_VWIF) || ((r) && (!r->kvm)))
    return -1;
  if (!r)
    {
    r = (t_radio *) malloc(sizeof(t_radio));
    if (!r)
      KOUT("ERROR malloc");
    memset(r, 0, sizeof(t_radio));
    strncpy(r->name, name, MAX_NAME_LEN-1);
    r->kvm = 1;
    r->idx = idx;
    memcpy(r->mac, mac, 6);
    memcpy(r->hw, mac, 6);
    r->next = g_radios;
    g_radios = r;
    }
  radio_air_set(r, air);
  return 0;
}

static int g_fd_sync = -1;
static int g_fd_async = -1;

/* The airs of the radios, told by the switch: cloonsuid_air_cnt and
   cloonsuid_air_kvm, answered with _resp_ok or _resp_ko through suid-power. */
static void air_msg(int llid, int tid, char *line)
{
  char name[MAX_NAME_LEN], air[MAX_NAME_LEN], resp[MAX_PATH_LEN];
  unsigned int m[6];
  uint8_t mac[6];
  int i, idx;
  t_air *cur;
  memset(name, 0, MAX_NAME_LEN);
  memset(air, 0, MAX_NAME_LEN);
  memset(resp, 0, MAX_PATH_LEN);
  if (sscanf(line, "cloonsuid_air_snf air=%63s on=%d", air, &idx) == 2)
    {
    snf_air_set(air, idx);
    /* the next dumpcap gets a fresh pcap header */
    cur = air_find(air);
    if ((cur) && (cur->cap.fifo >= 0))
      {
      close(cur->cap.fifo);
      cur->cap.fifo = -1;
      }
    }
  else if (sscanf(line, "cloonsuid_air_cnt name=%63s idx=%d air=%63s", name, &idx,
             air) == 3)
    {
    if (!strcmp(air, "-"))
      air[0] = 0;
    if (cnt_radio_air(name, idx, air))
      snprintf(resp, MAX_PATH_LEN-1, "cloonsuid_air_cnt_resp_ko name=%s "
               "idx=%d reason=no_such_radio", name, idx);
    else
      snprintf(resp, MAX_PATH_LEN-1, "cloonsuid_air_cnt_resp_ok name=%s "
               "idx=%d air=%s", name, idx, air[0] ? air : "-");
    air_line_send(g_fd_async, "resp %d %d %s", llid, tid, resp);
    }
  else if (sscanf(line, "cloonsuid_air_kvm_del name=%63s", name) == 1)
    radios_delete(name);
  else if (sscanf(line, "cloonsuid_air_kvm name=%63s idx=%d "
                  "mac=%02x:%02x:%02x:%02x:%02x:%02x air=%63s", name, &idx,
                  &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], air) == 9)
    {
    for (i = 0; i < 6; i++)
      mac[i] = m[i] & 0xff;
    if (!strcmp(air, "-"))
      air[0] = 0;
    if (kvm_radio(name, idx, mac, air))
      {
      KERR("ERROR air: %s %d is not a kvm radio", name, idx);
      snprintf(resp, MAX_PATH_LEN-1, "cloonsuid_air_kvm_resp_ko name=%s "
               "idx=%d reason=not_a_kvm_radio", name, idx);
      }
    else
      snprintf(resp, MAX_PATH_LEN-1, "cloonsuid_air_kvm_resp_ok name=%s "
               "idx=%d air=%s", name, idx, air[0] ? air : "-");
    air_line_send(g_fd_async, "resp %d %d %s", llid, tid, resp);
    }
  else
    KERR("ERROR air: %s", line);
}

/* the process */
static void medium_beat(void *data)
{
  peers_update();
  medium_drops();
  if ((!g_module_inserted) && module_is_loaded())
    {
    KTRC("mac80211_hwsim has been loaded, wifi emulation is available "
         "from now on");
    g_module_inserted = 1;
    }
  clownix_timeout_add(600, medium_beat, NULL, NULL, NULL);
}

/* create <cnt> <vm_id> <nb> <mac>: answered ok <wiphy>... or ko <why> */
static void sync_line(char *line)
{
  char cnt[MAX_NAME_LEN], ans[AIR_LINE_BUF];
  unsigned int m[6];
  uint8_t mac[6];
  int i, len, vm_id, nb, wiphy[MAX_VWIF];
  if ((sscanf(line, "create %63s %d %d %02x%02x%02x%02x%02x%02x", cnt,
              &vm_id, &nb, &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 9) ||
      (nb < 1) || (nb > MAX_VWIF))
    {
    KERR("ERROR air: a request not understood: %s", line);
    air_line_send(g_fd_sync, "ko bad_request");
    return;
    }
  for (i = 0; i < 6; i++)
    mac[i] = m[i] & 0xff;
  if (radios_create(cnt, vm_id, nb, mac, wiphy))
    air_line_send(g_fd_sync, "ko %s", g_why);
  else
    {
    len = snprintf(ans, sizeof(ans), "ok");
    for (i = 0; i < nb; i++)
      len += snprintf(ans + len, sizeof(ans) - len, " %d", wiphy[i]);
    air_line_send(g_fd_sync, "%s", ans);
    }
}

/* delete <name>, suidroot, air <llid> <tid> <line> */
static void async_line(char *line)
{
  char name[MAX_NAME_LEN];
  int llid, tid, n;
  if (sscanf(line, "delete %63s", name) == 1)
    radios_delete(name);
  else if (!strcmp(line, "suidroot"))
    g_module_inserted = module_is_loaded();
  else if (sscanf(line, "air %d %d %n", &llid, &tid, &n) == 2)
    air_msg(llid, tid, line + n);
  else
    KERR("ERROR air: a message not understood: %s", line);
}

static void parent_gone(int llid, int err, int from)
{
  exit(0);
}

static int sync_rx(int llid, int fd)
{
  static char buf[AIR_LINE_BUF];
  static int len;
  if (air_lines(fd, buf, &len, sync_line))
    exit(0);
  return 0;
}

static int async_rx(int llid, int fd)
{
  static char buf[AIR_LINE_BUF];
  static int len;
  if (air_lines(fd, buf, &len, async_line))
    exit(0);
  return 0;
}

/* The process of the medium: root, its end when suid-power ends. */
void medium_main(int fd_sync, int fd_async)
{
  if (setgid(0) || setuid(0))
    KOUT("ERROR air: not root: %s", strerror(errno));
  g_fd_sync = fd_sync;
  g_fd_async = fd_async;
  g_module_inserted = module_is_loaded();
  g_radios = NULL;
  g_airs = NULL;
  g_link_llid = 0;
  if ((msg_watch_fd(fd_sync, sync_rx, parent_gone, "airs") <= 0) ||
      (msg_watch_fd(fd_async, async_rx, parent_gone, "aira") <= 0))
    KOUT("ERROR air: the pipes of suid-power are not watched");
  link_listen();
  clownix_timeout_add(600, medium_beat, NULL, NULL, NULL);
  cloonix_set_pid(getpid());
  msg_mngt_loop();
}
