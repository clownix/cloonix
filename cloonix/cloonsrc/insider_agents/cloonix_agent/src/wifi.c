/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* The radios of a kvm: mac80211_hwsim of the guest, wlan0 up to wlan3. The
   agent is the medium of the guest: every frame of a radio goes to the host
   with the index of the radio, doorways relays it to the radios on the same
   air, and their frames come back here with the index of the radio to give
   them to. Raw generic netlink: the agent links no library. */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/time.h>

#include "glob_common.h"
#include "commun.h"
#include "sock.h"

#ifdef __linux__
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/sock_diag.h>

#define VERSION_NR 1
enum {
  HWSIM_CMD_UNSPEC, HWSIM_CMD_REGISTER, HWSIM_CMD_FRAME,
  HWSIM_CMD_TX_INFO_FRAME, HWSIM_CMD_NEW_RADIO,
};
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
#define AIR_SIGNAL (-50)

#define NL_BUF 65536
#define OFF_SAY_MS 5000

typedef struct t_radio
{
  uint8_t mac[6];
  /* the address the kernel names it by, learnt from its first frame */
  uint8_t addr[6];
  /* the addresses on its air: from doorways, which replaces the list at
     each change, plus those heard since; a unicast to one is acked */
  uint8_t peer[KVMWIFI_MAX_PEERS][6];
  int nb_peer;
} t_radio;

static int g_fd = -1;
static int g_fam;
static uint32_t g_seq;
static t_radio g_radio[MAX_VWIF];
static int g_nb;
static uint64_t g_off_said;
static unsigned long g_dropped;
static uint64_t g_drop_said;
static unsigned long g_rx_fails;
static uint32_t g_drops;
static uint64_t g_rx_said;

static uint64_t msec(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return ((uint64_t) tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

/*--------------------------------------------------------------------------*/
static void to_host(int type, int val, int len, char *data)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  if (len > MAX_A2D_LEN - headsize)
    {
    KERR("WARNING wifi: %d bytes do not fit the virtio channel", len);
    return;
    }
  memcpy(buf + headsize, data, len);
  send_to_virtio(0, 0, 0, type, val, len, buf);
}

static void say(int val, char *txt)
{
  to_host(header_type_wifi_ctrl, val, strlen(txt) + 1, txt);
}

/*------------------------------ netlink ----------------------------------*/
static void msg_init(char *m, int type, int flags, int cmd)
{
  struct nlmsghdr *nh = (struct nlmsghdr *) m;
  struct genlmsghdr *gh = (struct genlmsghdr *) NLMSG_DATA(nh);
  memset(m, 0, NLMSG_HDRLEN + GENL_HDRLEN);
  nh->nlmsg_len = NLMSG_HDRLEN + GENL_HDRLEN;
  nh->nlmsg_type = type;
  nh->nlmsg_flags = NLM_F_REQUEST | flags;
  nh->nlmsg_seq = ++g_seq;
  gh->cmd = cmd;
  gh->version = VERSION_NR;
}

static int attr_put(char *m, int max, int type, void *data, int len)
{
  struct nlmsghdr *nh = (struct nlmsghdr *) m;
  struct nlattr *a = (struct nlattr *) (m + NLMSG_ALIGN(nh->nlmsg_len));
  int tot = NLMSG_ALIGN(nh->nlmsg_len) + NLA_ALIGN(NLA_HDRLEN + len);
  if (tot > max)
    return -1;
  a->nla_type = type;
  a->nla_len = NLA_HDRLEN + len;
  if (len)
    memcpy((char *) a + NLA_HDRLEN, data, len);
  nh->nlmsg_len = tot;
  return 0;
}

static int attr_u32(char *m, int max, int type, uint32_t v)
{
  return attr_put(m, max, type, &v, 4);
}

/* the attributes of a genl message, by type */
static void attrs_parse(struct nlmsghdr *nh, struct nlattr **tb, int max)
{
  struct nlattr *a;
  int rem = nh->nlmsg_len - NLMSG_HDRLEN - GENL_HDRLEN;
  memset(tb, 0, (max + 1) * sizeof(struct nlattr *));
  a = (struct nlattr *) ((char *) NLMSG_DATA(nh) + GENL_HDRLEN);
  while ((rem >= (int) NLA_HDRLEN) && (a->nla_len >= NLA_HDRLEN) &&
         (a->nla_len <= rem))
    {
    if ((a->nla_type & NLA_TYPE_MASK) <= max)
      tb[a->nla_type & NLA_TYPE_MASK] = a;
    rem -= NLA_ALIGN(a->nla_len);
    a = (struct nlattr *) ((char *) a + NLA_ALIGN(a->nla_len));
    }
}

#define ADATA(a) ((void *) ((char *) (a) + NLA_HDRLEN))
#define ALEN(a)  ((int) ((a)->nla_len - NLA_HDRLEN))

/* A request and its answer, blocking, 2 s at most: 0 or -errno; the
   family id of a GETFAMILY in fam. The kernel acks a new radio with its
   index. */
static int request(int fd, char *m, int *fam)
{
  static char rx[NL_BUF];
  struct nlmsghdr *req = (struct nlmsghdr *) m, *nh;
  struct nlattr *tb[CTRL_ATTR_MAX + 1];
  struct timeval tv = { 2, 0 };
  int len, err;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)))
    KERR("WARNING wifi: no receive timeout: %s", strerror(errno));
  if (send(fd, m, req->nlmsg_len, 0) < 0)
    return -errno;
  for (;;)
    {
    len = recv(fd, rx, sizeof(rx), 0);
    if (len < 0)
      return (errno == EAGAIN) ? -ETIMEDOUT : -errno;
    for (nh = (struct nlmsghdr *) rx; NLMSG_OK(nh, len);
         nh = NLMSG_NEXT(nh, len))
      {
      if (nh->nlmsg_seq != req->nlmsg_seq)
        continue;
      if (nh->nlmsg_type == NLMSG_ERROR)
        {
        err = ((struct nlmsgerr *) NLMSG_DATA(nh))->error;
        return (err < 0) ? err : 0;
        }
      if ((fam) && (nh->nlmsg_type == GENL_ID_CTRL))
        {
        attrs_parse(nh, tb, CTRL_ATTR_MAX);
        if (tb[CTRL_ATTR_FAMILY_ID])
          *fam = *((uint16_t *) ADATA(tb[CTRL_ATTR_FAMILY_ID]));
        }
      }
    }
}

static int family(int fd)
{
  char m[256];
  int fam = -1;
  msg_init(m, GENL_ID_CTRL, NLM_F_ACK, CTRL_CMD_GETFAMILY);
  attr_put(m, sizeof(m), CTRL_ATTR_FAMILY_NAME, "MAC80211_HWSIM",
           strlen("MAC80211_HWSIM") + 1);
  if (request(fd, m, &fam))
    return -1;
  return fam;
}

/*------------------------------ the medium -------------------------------*/
/* A frame given to the radio came from its air: its sender is a peer
   until doorways sends the next list, before the one it may be late with. */
static void peer_heard(t_radio *r, uint8_t *mac)
{
  int i;
  if (mac[0] & 1)
    return;
  for (i = 0; i < r->nb_peer; i++)
    if (!memcmp(r->peer[i], mac, 6))
      return;
  if (r->nb_peer < KVMWIFI_MAX_PEERS)
    {
    memcpy(r->peer[r->nb_peer], mac, 6);
    r->nb_peer += 1;
    }
}

static int peer_of(t_radio *r, uint8_t *mac)
{
  int i;
  for (i = 0; i < r->nb_peer; i++)
    if (!memcmp(r->peer[i], mac, 6))
      return 1;
  return 0;
}

/* the radio of a transmitter: its mac, or the mac with 0x40 in the first
   byte, per kernel version */
static int radio_of(uint8_t *t)
{
  uint8_t hw[6];
  int i;
  for (i = 0; i < g_nb; i++)
    {
    memcpy(hw, g_radio[i].mac, 6);
    hw[0] |= 0x40;
    if ((!memcmp(t, g_radio[i].mac, 6)) || (!memcmp(t, hw, 6)))
      {
      memcpy(g_radio[i].addr, t, 6);
      return i;
      }
    }
  return -1;
}

static void tx_status(struct nlattr **a, uint32_t flags)
{
  char m[512];
  uint32_t sig = (uint32_t) AIR_SIGNAL;
  msg_init(m, g_fam, 0, HWSIM_CMD_TX_INFO_FRAME);
  if (attr_put(m, sizeof(m), HWSIM_ATTR_ADDR_TRANSMITTER,
               ADATA(a[HWSIM_ATTR_ADDR_TRANSMITTER]), 6) ||
      attr_u32(m, sizeof(m), HWSIM_ATTR_FLAGS, flags) ||
      attr_u32(m, sizeof(m), HWSIM_ATTR_SIGNAL, sig) ||
      attr_put(m, sizeof(m), HWSIM_ATTR_TX_INFO,
               ADATA(a[HWSIM_ATTR_TX_INFO]), ALEN(a[HWSIM_ATTR_TX_INFO])) ||
      attr_put(m, sizeof(m), HWSIM_ATTR_COOKIE,
               ADATA(a[HWSIM_ATTR_COOKIE]), 8))
    KERR("WARNING wifi: a tx status does not build");
  else if (send(g_fd, m, ((struct nlmsghdr *) m)->nlmsg_len, 0) < 0)
    KERR("WARNING wifi: a tx status did not go: %s", strerror(errno));
}

/* one frame of the radio: to the host, then its status to the kernel */
static void radio_frame(struct nlmsghdr *nh)
{
  struct nlattr *a[HWSIM_ATTR_MAX_USED + 1];
  char buf[MAX_A2D_LEN];
  uint8_t *frame;
  uint32_t flags, freq = 0, rate = 0;
  int len, acked, idx;
  attrs_parse(nh, a, HWSIM_ATTR_MAX_USED);
  if ((!a[HWSIM_ATTR_ADDR_TRANSMITTER]) || (!a[HWSIM_ATTR_FRAME]) ||
      (!a[HWSIM_ATTR_FLAGS]) || (!a[HWSIM_ATTR_TX_INFO]) ||
      (!a[HWSIM_ATTR_COOKIE]))
    {
    KERR("WARNING wifi: a frame from the kernel lacks an attribute");
    return;
    }
  frame = ADATA(a[HWSIM_ATTR_FRAME]);
  len = ALEN(a[HWSIM_ATTR_FRAME]);
  flags = *((uint32_t *) ADATA(a[HWSIM_ATTR_FLAGS]));
  if (a[HWSIM_ATTR_FREQ])
    freq = *((uint32_t *) ADATA(a[HWSIM_ATTR_FREQ]));
  if ((ALEN(a[HWSIM_ATTR_TX_INFO]) >= 1) &&
      (((int8_t *) ADATA(a[HWSIM_ATTR_TX_INFO]))[0] > 0))
    rate = ((uint8_t *) ADATA(a[HWSIM_ATTR_TX_INFO]))[0];
  idx = radio_of(ADATA(a[HWSIM_ATTR_ADDR_TRANSMITTER]));
  if (idx < 0)
    {
    KERR("WARNING wifi: a frame from a radio not made by cloonix");
    acked = 0;
    }
  else if ((len < 10) || (len + KVMWIFI_FRAME_HEAD > MAX_A2D_LEN - 100))
    {
    KERR("WARNING wifi: a frame of %d bytes, not sent", len);
    acked = 0;
    }
  /* the host behind: lost as on a busy air, no ack, the sender retries */
  else if (virtio_tx_high())
    {
    g_dropped += 1;
    if (msec() - g_drop_said >= OFF_SAY_MS)
      {
      g_drop_said = msec();
      KERR("WARNING wifi: the virtio port is behind, %lu frames of the "
           "radios dropped so far", g_dropped);
      }
    acked = 0;
    }
  else
    {
    buf[0] = (freq >> 24) & 0xff;
    buf[1] = (freq >> 16) & 0xff;
    buf[2] = (freq >> 8) & 0xff;
    buf[3] = freq & 0xff;
    buf[4] = (rate >> 24) & 0xff;
    buf[5] = (rate >> 16) & 0xff;
    buf[6] = (rate >> 8) & 0xff;
    buf[7] = rate & 0xff;
    buf[8] = buf[9] = buf[10] = 0;
    buf[11] = idx & 0xff;
    memcpy(buf + KVMWIFI_FRAME_HEAD, frame, len);
    to_host(header_type_wifi_frame, header_val_none,
            len + KVMWIFI_FRAME_HEAD, buf);
    acked = (frame[4] & 1) || peer_of(&(g_radio[idx]), frame + 4);
    }
  if (acked && !(flags & HWSIM_TX_CTL_NO_ACK))
    flags |= HWSIM_TX_STAT_ACK;
  tx_status(a, flags);
}

/* A frame of another kvm radio, given to the radio of the guest. */
void wifi_rx_host_frame(int len, char *rx)
{
  static char m[MAX_A2D_LEN + 512];
  uint8_t *p = (uint8_t *) rx, *frame = p + KVMWIFI_FRAME_HEAD;
  uint32_t freq, rate, sig = (uint32_t) AIR_SIGNAL;
  int idx, flen = len - KVMWIFI_FRAME_HEAD;
  uint64_t now;
  if (g_fd < 0)
    {
    now = msec();
    if (now - g_off_said >= OFF_SAY_MS)
      {
      g_off_said = now;
      say(header_val_wifi_off, "no radio");
      }
    return;
    }
  if (flen < 10)
    {
    KERR("WARNING wifi: a frame of %d bytes from the host", len);
    return;
    }
  freq = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
  rate = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
  idx = p[11];
  if (idx >= g_nb)
    {
    KERR("WARNING wifi: a frame for radio %d, %d made", idx, g_nb);
    return;
    }
  if (flen >= 16)
    peer_heard(&(g_radio[idx]), frame + 10);
  msg_init(m, g_fam, 0, HWSIM_CMD_FRAME);
  if (attr_put(m, sizeof(m), HWSIM_ATTR_ADDR_RECEIVER, g_radio[idx].addr, 6) ||
      attr_put(m, sizeof(m), HWSIM_ATTR_FRAME, frame, flen) ||
      attr_u32(m, sizeof(m), HWSIM_ATTR_RX_RATE, rate) ||
      attr_u32(m, sizeof(m), HWSIM_ATTR_SIGNAL, sig) ||
      ((freq) && attr_u32(m, sizeof(m), HWSIM_ATTR_FREQ, freq)))
    KERR("WARNING wifi: a frame of %d bytes does not build", flen);
  else if (send(g_fd, m, ((struct nlmsghdr *) m)->nlmsg_len, 0) < 0)
    KERR("WARNING wifi: a frame to the radio did not go: %s",
         strerror(errno));
}

/* the frames the kernel dropped, not read in time */
static void drops_say(void)
{
  static uint64_t said_ms;
  uint32_t mi[SK_MEMINFO_VARS];
  socklen_t len = sizeof(mi);
  if (msec() - said_ms < OFF_SAY_MS)
    return;
  said_ms = msec();
  if (getsockopt(g_fd, SOL_SOCKET, SO_MEMINFO, mi, &len))
    KERR("WARNING wifi: SO_MEMINFO: %s", strerror(errno));
  else if (mi[SK_MEMINFO_DROPS] != g_drops)
    {
    KERR("WARNING wifi: %u frames of the radios dropped, not read in time, "
         "%u so far", mi[SK_MEMINFO_DROPS] - g_drops, mi[SK_MEMINFO_DROPS]);
    g_drops = mi[SK_MEMINFO_DROPS];
    }
}

void wifi_events(fd_set *infd)
{
  static char rx[NL_BUF];
  struct nlmsghdr *nh;
  struct genlmsghdr *gh;
  int len, err;
  if ((g_fd < 0) || (!FD_ISSET(g_fd, infd)))
    return;
  drops_say();
  len = recv(g_fd, rx, sizeof(rx), MSG_DONTWAIT);
  if (len < 0)
    {
    if ((errno != EAGAIN) && (errno != EINTR))
      {
      g_rx_fails += 1;
      if (msec() - g_rx_said >= OFF_SAY_MS)
        {
        g_rx_said = msec();
        KERR("WARNING wifi: netlink receive: %s, %lu so far",
             strerror(errno), g_rx_fails);
        }
      }
    return;
    }
  for (nh = (struct nlmsghdr *) rx; NLMSG_OK(nh, len);
       nh = NLMSG_NEXT(nh, len))
    {
    if (nh->nlmsg_type == NLMSG_ERROR)
      {
      err = ((struct nlmsgerr *) NLMSG_DATA(nh))->error;
      /* -EINVAL: a delivery the radio drops, another channel, nominal */
      if ((err) && (err != -EINVAL))
        KERR("WARNING wifi: the kernel refused a message: %s",
             strerror(-err));
      }
    else if (nh->nlmsg_type == g_fam)
      {
      gh = (struct genlmsghdr *) NLMSG_DATA(nh);
      if (gh->cmd == HWSIM_CMD_FRAME)
        radio_frame(nh);
      }
    }
}

/*------------------------------ the radio --------------------------------*/
/* systemd-networkd leaves the radios to the user: its 80-wifi-*.network
   would take a wlan and flush the addresses given by hand. Two ways, made
   before the radios: the udev property ID_NET_MANAGED_BY (systemd 255 and
   later), set as each radio appears, whatever networkd has loaded yet; and
   an Unmanaged .network with a reload, for an older networkd. */
#define NETWORKD_RUN "/run/systemd/network"
#define UDEV_RUN "/run/udev/rules.d"
static void udev_hands_off(uint8_t *mac, int nb)
{
  char path[MAX_PATH_LEN];
  FILE *fp;
  int i;
  if (access("/run/udev/control", F_OK))
    return;
  if (mkdir(UDEV_RUN, 0755) && (errno != EEXIST))
    {
    KERR("WARNING wifi: mkdir %s: %s", UDEV_RUN, strerror(errno));
    return;
    }
  snprintf(path, MAX_PATH_LEN-1, "%s/00-cloonix-radio.rules", UDEV_RUN);
  fp = fopen(path, "w");
  if (!fp)
    {
    KERR("WARNING wifi: %s: %s", path, strerror(errno));
    return;
    }
  for (i = 0; i < nb; i++)
    fprintf(fp, "SUBSYSTEM==\"net\", ATTR{address}==\"%02x:%02x:%02x:%02x:"
            "%02x:%02x\", ENV{ID_NET_MANAGED_BY}=\"cloonix\"\n", mac[0],
            mac[1], mac[2], mac[3], mac[4], (mac[5] + i) & 0xff);
  fclose(fp);
  if (system("udevadm control --reload >/dev/null 2>&1"))
    KERR("WARNING wifi: udevadm control --reload failed, networkd may "
         "take the radios");
}

static void networkd_hands_off(uint8_t *mac, int nb)
{
  char path[MAX_PATH_LEN];
  FILE *fp;
  int i;
  if (access("/run/systemd/system", F_OK))
    return;
  udev_hands_off(mac, nb);
  if (mkdir(NETWORKD_RUN, 0755) && (errno != EEXIST))
    {
    KERR("WARNING wifi: mkdir %s: %s", NETWORKD_RUN, strerror(errno));
    return;
    }
  snprintf(path, MAX_PATH_LEN-1, "%s/00-cloonix-radio.network", NETWORKD_RUN);
  fp = fopen(path, "w");
  if (!fp)
    {
    KERR("WARNING wifi: %s: %s", path, strerror(errno));
    return;
    }
  fprintf(fp, "[Match]\nPermanentMACAddress=");
  for (i = 0; i < nb; i++)
    fprintf(fp, "%s%02x:%02x:%02x:%02x:%02x:%02x", i ? " " : "", mac[0],
            mac[1], mac[2], mac[3], mac[4], (mac[5] + i) & 0xff);
  fprintf(fp, "\n\n[Link]\nUnmanaged=yes\n");
  fclose(fp);
  /* networkctl needs dbus, absent from a minimal guest: then a restart,
     which applies the same configuration again */
  if ((!access("/run/systemd/netif", F_OK)) &&
      system("networkctl reload >/dev/null 2>&1") &&
      system("systemctl try-restart systemd-networkd >/dev/null 2>&1"))
    KERR("WARNING wifi: networkd not reloaded, it may take wlan0");
}

/* the radios live as long as this socket: wlan0.. in the order made */
static int radios_create(uint8_t *mac, int nb, char *why, int wlen)
{
  struct sockaddr_nl sa;
  char m[256];
  int i, fd, err, one = 1;
  if (access("/sys/module/mac80211_hwsim", F_OK) &&
      system("modprobe mac80211_hwsim radios=0 >/dev/null 2>&1"))
    {
    snprintf(why, wlen, "modprobe mac80211_hwsim failed in the guest");
    return -1;
    }
  fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_GENERIC);
  memset(&sa, 0, sizeof(sa));
  sa.nl_family = AF_NETLINK;
  if ((fd < 0) || bind(fd, (struct sockaddr *) &sa, sizeof(sa)))
    {
    snprintf(why, wlen, "netlink socket: %s", strerror(errno));
    if (fd >= 0)
      close(fd);
    return -1;
    }
  networkd_hands_off(mac, nb);
  g_fam = family(fd);
  if (g_fam < 0)
    {
    snprintf(why, wlen, "no MAC80211_HWSIM netlink family in the guest");
    close(fd);
    return -1;
    }
  for (i = 0; i < nb; i++)
    {
    memcpy(g_radio[i].mac, mac, 6);
    g_radio[i].mac[5] = (mac[5] + i) & 0xff;
    memcpy(g_radio[i].addr, g_radio[i].mac, 6);
    msg_init(m, g_fam, NLM_F_ACK, HWSIM_CMD_NEW_RADIO);
    attr_put(m, sizeof(m), HWSIM_ATTR_PERM_ADDR, g_radio[i].mac, 6);
    attr_put(m, sizeof(m), HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE, NULL, 0);
    err = request(fd, m, NULL);
    if (err)
      {
      snprintf(why, wlen, "radio %d refused: %s", i, strerror(-err));
      close(fd);
      return -1;
      }
    }
  msg_init(m, g_fam, NLM_F_ACK, HWSIM_CMD_REGISTER);
  err = request(fd, m, NULL);
  if (err)
    {
    snprintf(why, wlen, "medium registration refused: %s", strerror(-err));
    close(fd);
    return -1;
    }
  /* lossy as the air: an overflow drops frames, never congests the socket */
  if (setsockopt(fd, SOL_NETLINK, NETLINK_NO_ENOBUFS, &one, sizeof(one)))
    KERR("ERROR wifi: NETLINK_NO_ENOBUFS: %s", strerror(errno));
  g_drops = 0;
  g_nb = nb;
  g_fd = fd;
  return 0;
}

/* KVMWIFI_ON_REQ: the radios made once, then answered on at each ask */
void wifi_rx_host_ctrl(int val, int len, char *rx)
{
  unsigned int m[6];
  uint8_t mac[6];
  char why[MAX_PATH_LEN];
  int i, nb;
  if (val == header_val_wifi_peers)
    {
    if ((len < 1) || ((len - 1) % 6) || ((len - 1) / 6 > KVMWIFI_MAX_PEERS) ||
        ((uint8_t) rx[0] >= g_nb))
      KERR("WARNING wifi: peers of %d bytes for radio %d, %d made", len,
           len ? (uint8_t) rx[0] : -1, g_nb);
    else
      {
      i = (uint8_t) rx[0];
      g_radio[i].nb_peer = (len - 1) / 6;
      memcpy(g_radio[i].peer, rx + 1, len - 1);
      }
    return;
    }
  if (val != header_val_wifi_on)
    {
    KERR("WARNING wifi: control %d from the host", val);
    return;
    }
  if ((sscanf(rx, KVMWIFI_ON_REQ, &nb, &m[0], &m[1], &m[2], &m[3], &m[4],
              &m[5]) != 7) || (nb < 1) || (nb > MAX_VWIF))
    {
    KERR("WARNING wifi: bad request %s", rx);
    say(header_val_wifi_ko, "bad request");
    return;
    }
  for (i = 0; i < 6; i++)
    mac[i] = (uint8_t) m[i];
  memset(why, 0, MAX_PATH_LEN);
  if ((g_fd < 0) && radios_create(mac, nb, why, MAX_PATH_LEN-1))
    {
    KERR("WARNING wifi: %s", why);
    say(header_val_wifi_ko, why);
    }
  else
    say(header_val_wifi_on, "on");
}

int wifi_get_fd(void)
{
  return g_fd;
}

#else

void wifi_rx_host_frame(int len, char *rx)
{
}

void wifi_events(fd_set *infd)
{
}

void wifi_rx_host_ctrl(int val, int len, char *rx)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *txt = "no wifi radio on this guest";
  memcpy(buf + headsize, txt, strlen(txt) + 1);
  send_to_virtio(0, 0, 0, header_type_wifi_ctrl, header_val_wifi_ko,
                 strlen(txt) + 1, buf);
}

int wifi_get_fd(void)
{
  return -1;
}

#endif
