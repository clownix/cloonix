/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <stdint.h>
#include <getopt.h>
#include <signal.h>
#include <math.h>
#include <errno.h>
#include <limits.h>

#include "ioc.h"
#include "hwsim.h"
#include "iface.h"
#include "main.h"
#include "blkd_addr.h"


#ifndef min
#define min(x,y) ((x) < (y) ? (x) : (y))
#endif


#define HWSIM_TX_CTL_REQ_TX_STATUS	1
#define HWSIM_TX_CTL_NO_ACK		(1 << 1)
#define HWSIM_TX_STAT_ACK		(1 << 2)

#define HWSIM_CMD_REGISTER 1
#define HWSIM_CMD_FRAME 2
#define HWSIM_CMD_TX_INFO_FRAME 3

enum {  HWSIM_ATTR_UNSPEC,
	HWSIM_ATTR_ADDR_RECEIVER,
	HWSIM_ATTR_ADDR_TRANSMITTER,
	HWSIM_ATTR_FRAME,
	HWSIM_ATTR_FLAGS,
	HWSIM_ATTR_RX_RATE,
	HWSIM_ATTR_SIGNAL,
	HWSIM_ATTR_TX_INFO,
	HWSIM_ATTR_COOKIE,
	HWSIM_ATTR_CHANNELS,
	HWSIM_ATTR_RADIO_ID,
	HWSIM_ATTR_REG_HINT_ALPHA2,
	HWSIM_ATTR_REG_CUSTOM_REG,
	HWSIM_ATTR_REG_STRICT_REG,
	HWSIM_ATTR_SUPPORT_P2P_DEVICE,
	HWSIM_ATTR_USE_CHANCTX,
	HWSIM_ATTR_DESTROY_RADIO_ON_CLOSE,
	HWSIM_ATTR_RADIO_NAME,
	HWSIM_ATTR_NO_VIF,
	HWSIM_ATTR_FREQ,
	HWSIM_ATTR_PAD,
	__HWSIM_ATTR_MAX,
};
#define HWSIM_ATTR_MAX (__HWSIM_ATTR_MAX - 1)

#define SNR_DEFAULT 30



#define IEEE80211_TX_MAX_RATES 4
#define IEEE80211_NUM_ACS 4

struct hwsim_tx_rate {
	signed char idx;
	unsigned char count;
};


static struct nl_sock *g_nl_sock;
static int g_nl_family_id;
static struct nl_cb *g_nl_cb;

#define MAX_BEAT_IDX 5
static int g_beat_idx;
static int g_beat_tab[MAX_BEAT_IDX];


/****************************************************************************/
static void nl_send_tx_info_frame(uint8_t *hwaddr, int flags, int sig, 
                                  unsigned int tx_rates_cnt,
                                  struct hwsim_tx_rate *tx_rates,
                                  uint64_t cookie)
{
  struct nl_msg *msg;
  int ret;
  msg = nlmsg_alloc();
  if (!msg)
    KERR(" ");
  else if (genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, g_nl_family_id, 0,
                     NLM_F_REQUEST, HWSIM_CMD_TX_INFO_FRAME, 1) == NULL)
    KERR(" ");
  else if (nla_put(msg, HWSIM_ATTR_ADDR_TRANSMITTER, ETH_ALEN, hwaddr) ||
           nla_put_u32(msg, HWSIM_ATTR_FLAGS, flags) ||
           nla_put_u32(msg, HWSIM_ATTR_SIGNAL, sig) ||
           nla_put(msg, HWSIM_ATTR_TX_INFO,
              tx_rates_cnt * sizeof(struct hwsim_tx_rate),
              tx_rates) ||
           nla_put_u64(msg, HWSIM_ATTR_COOKIE, cookie))
    KERR(" ");
  else
    {
    ret = nl_send_auto(g_nl_sock, msg);
    if (ret < 0)
      KERR(" ");
    }
  nlmsg_free(msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int nl_err_cb(struct sockaddr_nl *nla,struct nlmsgerr *nlerr,void *arg)
{
  struct genlmsghdr *gnlh = nlmsg_data(&nlerr->msg);
  KERR("nl: cmd %d, seq %d: %s\n", gnlh->cmd, nlerr->msg.nlmsg_seq,
                                   strerror(abs(nlerr->error)));
  return NL_SKIP;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void hwsim_heartbeat(void)
{
  g_beat_idx += 1;
  if (g_beat_idx == MAX_BEAT_IDX)
    g_beat_idx = 0;
  g_beat_tab[g_beat_idx] = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void regul_tx_to_virtio_port(int idx, int len, char *payload)
{
  int i, tot = 0;
  for (i=0; i<MAX_BEAT_IDX; i++)
    tot += g_beat_tab[g_beat_idx]; 
  if (tot < 300000)
    {
    g_beat_tab[g_beat_idx] += len;
    blkd_addr_tx_virtio(idx, len, payload);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int nl_receive_packet_from_air(struct nl_msg *msg, void *arg)
{
  struct nlattr *attrs[HWSIM_ATTR_MAX+1];
  struct nlmsghdr *nlh = nlmsg_hdr(msg);
  struct genlmsghdr *gnlh = nlmsg_data(nlh);
  t_header_wlan *header_wlan;
  struct ieee80211_hdr *hdr;
  uint8_t *hwaddr, *src, *dst;
  unsigned int data_len, tx_rates_len, tx_rates_cnt;
  struct hwsim_tx_rate *tx_rates;
  char *data;
  char *payload;
  int sig, flags, payload_len, idx_virtio_port;
  uint64_t cookie;
  if (gnlh->cmd == HWSIM_CMD_FRAME)
    {
    genlmsg_parse(nlh, 0, attrs, HWSIM_ATTR_MAX, NULL);
    if (attrs[HWSIM_ATTR_ADDR_TRANSMITTER])
      {
      data = (char *)nla_data(attrs[HWSIM_ATTR_FRAME]);
      data_len = nla_len(attrs[HWSIM_ATTR_FRAME]);
      if (data_len < 6 + 6 + 4)
        KOUT("%d", data_len);
      hwaddr = (uint8_t *)nla_data(attrs[HWSIM_ATTR_ADDR_TRANSMITTER]);
      hdr = (struct ieee80211_hdr *)data;
      src = hdr->addr2;
      dst = hdr->addr1;
      blkd_addr_hwaddr_rx(hwaddr, src);
      flags = nla_get_u32(attrs[HWSIM_ATTR_FLAGS]);
      flags |= HWSIM_TX_STAT_ACK;
      sig = 0;
      tx_rates_len = nla_len(attrs[HWSIM_ATTR_TX_INFO]);
      tx_rates_cnt = tx_rates_len / sizeof(struct hwsim_tx_rate);
      tx_rates = (struct hwsim_tx_rate *) nla_data(attrs[HWSIM_ATTR_TX_INFO]);
      cookie = nla_get_u64(attrs[HWSIM_ATTR_COOKIE]);
      nl_send_tx_info_frame(hwaddr,flags,sig,tx_rates_cnt,tx_rates,cookie);
      if (!blkd_addr_get_dst(hwaddr, src, dst, &idx_virtio_port))
        {
        header_wlan = malloc(sizeof(t_header_wlan) + data_len);
        memset(header_wlan, 0, sizeof(t_header_wlan));
        memcpy(header_wlan->data, data, data_len);
        header_wlan->wlan_header_type = wlan_header_type_peer_data;
        header_wlan->data_len = data_len;
        memcpy(header_wlan->dst, dst, ETH_ALEN);
        memcpy(header_wlan->src, src, ETH_ALEN);
        payload = (char *) header_wlan;
        payload_len = sizeof(t_header_wlan) + data_len;
        regul_tx_to_virtio_port(idx_virtio_port, payload_len, payload);
        free(header_wlan);
        }
      }
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int send_register_msg(void)
{
  int result = -1;
  struct nl_msg *msg;
  msg = nlmsg_alloc();
  if (!msg)
    KERR(" ");
  else if (genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, g_nl_family_id, 0,
                  NLM_F_REQUEST, HWSIM_CMD_REGISTER, 1) == NULL)
    KERR(" ");
  else
    {
    result = nl_send_auto(g_nl_sock, msg);
    if (result < 0)
      KERR(" ");
    }
  nlmsg_free(msg);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int init_netlink(void)
{
  int ret = -1;
  g_nl_cb = nl_cb_alloc(NL_CB_CUSTOM);
  if (!g_nl_cb)
    KERR(" ");
  else
    {
    g_nl_sock = nl_socket_alloc_cb(g_nl_cb);
    if (!g_nl_sock)
      KERR(" ");
    else
      {
      ret = genl_connect(g_nl_sock);
      if (ret < 0)
        KERR(" ");
      else
        {
        g_nl_family_id = genl_ctrl_resolve(g_nl_sock, "MAC80211_HWSIM");
        if (g_nl_family_id < 0)
          KERR(" ");
        else
          {
          nl_cb_set(g_nl_cb, NL_CB_MSG_IN, NL_CB_CUSTOM, 
                    nl_receive_packet_from_air, NULL);
          nl_cb_err(g_nl_cb, NL_CB_CUSTOM, nl_err_cb, NULL);
          ret = 0;
          }
        }
      }
    }
  return ret;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void hwsim_nl_send_packet_to_air(uint8_t *hwaddr, int len, char *data)
{
  struct nl_msg *msg;
  int ret;
  msg = nlmsg_alloc();
  if (!msg)
    KERR(" ");
  else if (genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, g_nl_family_id, 0,
                             NLM_F_REQUEST, HWSIM_CMD_FRAME, 1) == NULL)
    KERR(" ");
  else if (nla_put(msg, HWSIM_ATTR_ADDR_RECEIVER, ETH_ALEN, hwaddr) ||
           nla_put(msg, HWSIM_ATTR_FRAME, len, data) ||
           nla_put_u32(msg, HWSIM_ATTR_RX_RATE, 11) ||
           nla_put_u32(msg, HWSIM_ATTR_SIGNAL, -50))
    KERR(" ");
  else
    {
    ret = nl_send_auto(g_nl_sock, msg);
    if (ret < 0)
      KERR(" ");
    }
  nlmsg_free(msg);
}
/*-------------------------------------------------------------------------*/

/****************************************************************************/
void hwsim_nl_sock_event(void)
{
  nl_recvmsgs_default(g_nl_sock);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int hwsim_init(void)
{
  int fd = -1;
  if (!init_netlink())
    {
    fd = nl_socket_get_fd(g_nl_sock);
    if (fd >= 0)
      {
      if (send_register_msg() < 0)
        fd = -1;
      }
    }
  return fd;
}
/*--------------------------------------------------------------------------*/
