/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
/* One capture byte stream turned into pcap_traffic messages. The dumpcap  */
/* beside every snf serves <root>/snf/<n>_<num>_pcap_sock, one connection   */
/* per subscriber. The record header is parsed so the message carries the   */
/* timestamp and lengths as fields; a read ending mid record is kept.       */
/* Past msg_tx_queue_high on the doors connection, packets are dropped for  */
/* that subscriber and counted.                                             */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "utils_cmd_line_maker.h"
#include "util_sock.h"
#include "pcap_relay.h"
#include "llid_trace.h"
#include "pcap_filter.h"
#include "cnt.h"
#include "ovs_snf.h"
#include "ovs_tap.h"
#include "ovs_nat_main.h"
#include "ovs_a2b.h"
#include "ovs_c2c.h"
#include "ovs_phy.h"
#include "wifi_air.h"

#define PCAP_HDR_LEN      24
#define PCAP_REC_HDR_LEN  16
#define PCAP_ACC_MAX      (2 * MAX_PCAP_PKT_LEN)
#define PCAP_READ_MAX     (64 * 1024)

typedef struct t_pcap_sub
{
  char name[MAX_NAME_LEN];
  int  num;
  int  cli_llid;
  int  cli_tid;
  int  sock_llid;
  int  hdr_sent;
  int  swapped;
  unsigned int pkt_nb;
  unsigned int dropped;
  unsigned int filtered;
  t_pcap_filter *filter;
  int  acc_len;
  char *acc;
  struct t_pcap_sub *prev;
  struct t_pcap_sub *next;
} t_pcap_sub;

static t_pcap_sub *g_head_sub;
static char g_read_buf[PCAP_READ_MAX];

static t_pcap_sub *find_by_sock(int sock_llid)
{
  t_pcap_sub *cur = g_head_sub;
  while (cur)
    {
    if (cur->sock_llid == sock_llid)
      return cur;
    cur = cur->next;
    }
  return NULL;
}

static t_pcap_sub *find_sub(int cli_llid, char *name, int num)
{
  t_pcap_sub *cur = g_head_sub;
  while (cur)
    {
    if ((cur->cli_llid == cli_llid) && (cur->num == num) &&
        (!strcmp(cur->name, name)))
      return cur;
    cur = cur->next;
    }
  return NULL;
}

static void sub_free(t_pcap_sub *cur)
{
  if (cur->sock_llid)
    {
    llid_trace_free_if_exists(cur->sock_llid);
    if (msg_exist_channel(cur->sock_llid))
      msg_delete_channel(cur->sock_llid);
    cur->sock_llid = 0;
    }
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_sub)
    g_head_sub = cur->next;
  PLOG("%s %d: subscription ended, %u relayed, %u dropped",
       cur->name, cur->num, cur->pkt_nb, cur->dropped);
  if (cur->dropped)
    KERR("WARNING pcap %s %d ended having dropped %u packets, the client "
         "could not keep up", cur->name, cur->num, cur->dropped);
  pcap_filter_free(cur->filter);
  free(cur->acc);
  free(cur);
}

static char *path_no_suffix(char *name, int num)
{
  static char base[MAX_PATH_LEN];
  snprintf(base, sizeof(base), "%s/%s_%d",
           utils_get_snf_pcap_dir(), name, num);
  return base;
}

static void sub_sock_path(char *path, int len, char *name, int num)
{
  snprintf(path, len, "%s/%s_%d_pcap_sock",
           utils_get_snf_pcap_dir(), name, num);
}

static unsigned int get_u32(char *p, int swapped)
{
  unsigned char *u = (unsigned char *) p;
  unsigned int v = ((unsigned int) u[0]) | (((unsigned int) u[1]) << 8) |
                   (((unsigned int) u[2]) << 16) | (((unsigned int) u[3]) << 24);
  if (swapped)
    v = ((v & 0xff000000) >> 24) | ((v & 0x00ff0000) >> 8) |
        ((v & 0x0000ff00) << 8)  | ((v & 0x000000ff) << 24);
  return v;
}

/* Consume whole units from the front of the accumulator: 0 carry on, -1 stream unusable. */
static int consume(t_pcap_sub *cur)
{
  unsigned int magic, sec, usec, caplen, orig;
  int used;

  if (!cur->hdr_sent)
    {
    if (cur->acc_len < PCAP_HDR_LEN)
      return 0;
    magic = get_u32(cur->acc, 0);
    if ((magic == 0xa1b2c3d4) || (magic == 0xa1b23c4d))
      cur->swapped = 0;
    else if ((magic == 0xd4c3b2a1) || (magic == 0x4d3cb2a1))
      cur->swapped = 1;
    else
      {
      KERR("ERROR pcap %s %d: 0x%08x is not a pcap file header",
           cur->name, cur->num, magic);
      return -1;
      }
    /* the filter reads ethernet only: on another link type, all passes */
    if ((cur->filter) && (get_u32(cur->acc + 20, cur->swapped) != 1))
      {
      KERR("WARNING pcap %s %d: link type %u is not ethernet, the filter is "
           "not applied", cur->name, cur->num,
           get_u32(cur->acc + 20, cur->swapped));
      pcap_filter_free(cur->filter);
      cur->filter = NULL;
      }
    /* Link type, snaplen and byte order: nothing decodes without it. */
    send_pcap_traffic(cur->cli_llid, cur->cli_tid, cur->name, cur->num,
                      PCAP_TYPE_HEADER, 0, 0, 0,
                      PCAP_HDR_LEN, PCAP_HDR_LEN, (uint8_t *) cur->acc);
    cur->hdr_sent = 1;
    PLOG("%s %d: pcap file header relayed to the client, %s endian",
         cur->name, cur->num, cur->swapped ? "opposite" : "same");
    memmove(cur->acc, cur->acc + PCAP_HDR_LEN, cur->acc_len - PCAP_HDR_LEN);
    cur->acc_len -= PCAP_HDR_LEN;
    }

  while (cur->acc_len >= PCAP_REC_HDR_LEN)
    {
    sec    = get_u32(cur->acc,      cur->swapped);
    usec   = get_u32(cur->acc + 4,  cur->swapped);
    caplen = get_u32(cur->acc + 8,  cur->swapped);
    orig   = get_u32(cur->acc + 12, cur->swapped);
    if (caplen > MAX_PCAP_PKT_LEN)
      {
      KERR("ERROR pcap %s %d: a record announces %u bytes, the limit is %d",
           cur->name, cur->num, caplen, MAX_PCAP_PKT_LEN);
      return -1;
      }
    used = PCAP_REC_HDR_LEN + (int) caplen;
    if (cur->acc_len < used)
      break;
    if (cur->filter &&
        !pcap_filter_match(cur->filter,
                           (uint8_t *) (cur->acc + PCAP_REC_HDR_LEN),
                           (int) caplen))
      {
      cur->filtered++;
      }
    /* Numbered before the drop decision: a packet dropped here is a gap in
       the numbers the client sees, which is the one sign it gets. */
    else if ((++cur->pkt_nb, msg_tx_queue_high(cur->cli_llid)))
      {
      cur->dropped++;
      if (cur->dropped == 1)
        KERR("WARNING pcap %s %d: the client is behind, dropping packets",
             cur->name, cur->num);
      }
    else
      {
      if (cur->pkt_nb == 1)
        PLOG("%s %d: first packet relayed, the chain works end to end",
             cur->name, cur->num);
      else if ((cur->pkt_nb % 10000) == 0)
        PLOG("%s %d: %u packets relayed, %u dropped",
             cur->name, cur->num, cur->pkt_nb, cur->dropped);
      send_pcap_traffic(cur->cli_llid, cur->cli_tid, cur->name, cur->num,
                        PCAP_TYPE_PACKET, cur->pkt_nb, sec, usec,
                        (int) caplen, (int) orig,
                        (uint8_t *) (cur->acc + PCAP_REC_HDR_LEN));
      }
    memmove(cur->acc, cur->acc + used, cur->acc_len - used);
    cur->acc_len -= used;
    }
  return 0;
}

static void pcap_err_cb(int llid, int err, int from)
{
  t_pcap_sub *cur = find_by_sock(llid);
  (void) err;
  (void) from;
  if (cur)
    {
    KTRC("pcap %s %d: the dumpcap connection ended", cur->name, cur->num);
    send_status_ko(cur->cli_llid, cur->cli_tid, "pcap stream ended");
    sub_free(cur);
    }
}

static int pcap_rx_cb(int llid, int fd)
{
  t_pcap_sub *cur = find_by_sock(llid);
  int len, room;

  if (!cur)
    {
    KERR("ERROR pcap data on llid %d with no subscription behind it, "
         "closing: a subscription was freed without its channel", llid);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    return 0;
    }
  if (!msg_exist_channel(cur->cli_llid))
    {
    PLOG("%s %d: the client is gone, unsubscribing", cur->name, cur->num);
    sub_free(cur);
    return 0;
    }
  room = PCAP_ACC_MAX - cur->acc_len;
  if (room > PCAP_READ_MAX)
    room = PCAP_READ_MAX;
  if (room <= 0)
    {
    KERR("ERROR pcap %s %d: nothing consumed a whole record, giving up",
         cur->name, cur->num);
    send_status_ko(cur->cli_llid, cur->cli_tid, "pcap stream out of step");
    sub_free(cur);
    return 0;
    }
  len = util_read(g_read_buf, room, fd);
  if (len <= 0)
    {
    KTRC("pcap %s %d: end of the dumpcap stream", cur->name, cur->num);
    send_status_ko(cur->cli_llid, cur->cli_tid, "pcap stream ended");
    sub_free(cur);
    return 0;
    }
  memcpy(cur->acc + cur->acc_len, g_read_buf, len);
  cur->acc_len += len;
  if (consume(cur))
    {
    send_status_ko(cur->cli_llid, cur->cli_tid, "pcap stream not usable");
    sub_free(cur);
    }
  return len;
}

/* Why name num has no capture socket, as the client is to read it. */
static void why_no_capture(char *name, int num, char *why)
{
  t_vm *vm = cfg_get_vm(name);
  t_cnt *cnt = find_cnt(name);
  t_eth_table *eth = NULL;
  char (*airs)[MAX_NAME_LEN] = NULL;
  int nb_eth = 0, nb_vwif = 0;
  memset(why, 0, MAX_PATH_LEN);
  if (vm)
    {
    eth = vm->kvm.eth_table;
    nb_eth = vm->kvm.nb_tot_eth;
    nb_vwif = vm->kvm.nb_tot_nb_vwif;
    airs = vm->kvm.vwif_air;
    }
  else if (cnt)
    {
    eth = cnt->cnt.eth_table;
    nb_eth = cnt->cnt.nb_tot_eth;
    nb_vwif = cnt->cnt.nb_tot_nb_vwif;
    airs = cnt->cnt.vwif_air;
    }
  if (wifi_air_in_use(name) && !wifi_air_snf_on(name))
    snprintf(why, MAX_PATH_LEN-1, "the air %s is not captured: cnf %s "
             "sniff=on captures it", name, name);
  else if (wifi_air_in_use(name))
    snprintf(why, MAX_PATH_LEN-1, "the capture of the air %s is not up "
             "yet, try again in a moment", name);
  else if (eth && (num >= nb_eth) && (num < nb_eth + nb_vwif) &&
           (airs[num - nb_eth][0]))
    snprintf(why, MAX_PATH_LEN-1, "%s wlan%d is captured with its air: "
             "sniff %s", name, num - nb_eth, airs[num - nb_eth]);
  else if (eth && (num >= nb_eth) && (num < nb_eth + nb_vwif))
    snprintf(why, MAX_PATH_LEN-1, "%s wlan%d is on no air: nothing to "
             "capture", name, num - nb_eth);
  else if (eth && ((num < 0) || (num >= nb_eth)) && (nb_eth == 0))
    snprintf(why, MAX_PATH_LEN-1, "%s has no eth%d, it has no eth",
             name, num);
  else if (eth && ((num < 0) || (num >= nb_eth)))
    snprintf(why, MAX_PATH_LEN-1, "%s has no eth%d, its eths are eth0 "
             "to eth%d", name, num, nb_eth - 1);
  else if (eth && (eth[num].endp_type != endp_type_eths))
    snprintf(why, MAX_PATH_LEN-1, "eth%d of %s is not sniffed: cnf %s "
             "eth%d sniff=on sniffs it", num, name, name, num);
  else if (ovs_snf_state(name, num) == 1)
    snprintf(why, MAX_PATH_LEN-1, "the capture of %s%s is starting or "
             "stopping, try again in a moment", name,
             wifi_air_iface(name, num));
  else if (eth || ovs_tap_exists(name) || ovs_nat_main_exists(name) ||
           ovs_a2b_exists(name) || ovs_c2c_exists(name) ||
           ovs_phy_exists_vhost(name))
    snprintf(why, MAX_PATH_LEN-1, "%s%s is not sniffed: cnf %s%s sniff=on "
             "sniffs it", name, wifi_air_iface(name, num), name,
             wifi_air_iface(name, num));
  else
    snprintf(why, MAX_PATH_LEN-1, "no item and no air called %s", name);
}

void pcap_relay_open(int llid, int tid, char *name, int num, char *filter)
{
  char path[MAX_PATH_LEN];
  char ferr[MAX_PATH_LEN];
  char kerr[MAX_PATH_LEN];
  t_pcap_filter *flt = NULL;
  t_pcap_sub *cur;
  int fd, sock_llid;

  if (find_sub(llid, name, num))
    {
    KERR("ERROR pcap_open %s %d: already subscribed on this client", name, num);
    send_status_ko(llid, tid, "already subscribed");
    return;
    }
  if (wifi_air_in_use(name) && !wifi_air_snf_on(name))
    {
    why_no_capture(name, num, kerr);
    KERR("WARNING pcap_open %s refused: %s", name, kerr);
    send_status_ko(llid, tid, kerr);
    return;
    }
  if (filter && filter[0])
    {
    flt = pcap_filter_compile(filter, ferr, sizeof(ferr));
    if (!flt)
      {
      KERR("ERROR pcap_open %s %d: bad filter '%s': %s",
           name, num, filter, ferr);
      snprintf(kerr, sizeof(kerr), "bad filter: %s", ferr);
      send_status_ko(llid, tid, kerr);
      return;
      }
    }
  sub_sock_path(path, sizeof(path), name, num);
  PLOG("%s %d: connecting to %s", name, num, path);
  if (util_nonblock_client_socket_unix(path, &fd) || (fd <= 0))
    {
    /* Two different failures, told apart here. */
    if (errno == ECONNREFUSED)
      {
      KERR("ERROR pcap_open %s %d: %s exists but nothing listens, the "
           "dumpcap of this snf has died -- see %s%s",
           name, num, path, path_no_suffix(name, num), "_pcap_log");
      send_status_ko(llid, tid,
                     "the capture process for that interface has died");
      }
    else
      {
      why_no_capture(name, num, kerr);
      KERR("ERROR pcap_open %s %d refused: %s", name, num, kerr);
      send_status_ko(llid, tid, kerr);
      }
    pcap_filter_free(flt);
    return;
    }
  PLOG("%s %d: connected on fd %d, watching it", name, num, fd);
  sock_llid = msg_watch_fd(fd, pcap_rx_cb, pcap_err_cb, "pcap");
  if (sock_llid == 0)
    {
    KERR("ERROR pcap_open %s %d: cannot watch the socket", name, num);
    pcap_filter_free(flt);
    close(fd);
    send_status_ko(llid, tid, "cannot watch the pcap socket");
    return;
    }
  llid_trace_alloc(sock_llid, "pcap", 0, 0, type_llid_trace_endp_ovsdb);
  cur = (t_pcap_sub *) malloc(sizeof(t_pcap_sub));
  if (!cur)
    {
    KERR("ERROR pcap_open %s %d: no memory for the subscription", name, num);
    pcap_filter_free(flt);
    msg_delete_channel(sock_llid);
    send_status_ko(llid, tid, "no memory");
    return;
    }
  memset(cur, 0, sizeof(t_pcap_sub));
  cur->acc = (char *) malloc(PCAP_ACC_MAX);
  if (!cur->acc)
    {
    KERR("ERROR pcap_open %s %d: no memory for the %d byte accumulator",
         name, num, PCAP_ACC_MAX);
    pcap_filter_free(flt);
    msg_delete_channel(sock_llid);
    free(cur);
    send_status_ko(llid, tid, "no memory");
    return;
    }
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  cur->num       = num;
  cur->cli_llid  = llid;
  cur->cli_tid   = tid;
  cur->sock_llid = sock_llid;
  cur->filter    = flt;
  if (g_head_sub)
    g_head_sub->prev = cur;
  cur->next = g_head_sub;
  g_head_sub = cur;
  if (flt)
    PLOG("open %s %d: subscribed, relaying only what matches '%s'",
         name, num, pcap_filter_text(flt));
  else
    PLOG("open %s %d: subscribed, relaying %s", name, num, path);
  send_status_ok(llid, tid, "OK");
}

void pcap_relay_close(int llid, int tid, char *name, int num)
{
  t_pcap_sub *cur = find_sub(llid, name, num);
  if (!cur)
    {
    KERR("ERROR pcap_close %s %d: no such subscription", name, num);
    send_status_ko(llid, tid, "not subscribed");
    return;
    }
  PLOG("close %s %d: %u relayed, %u filtered out, %u dropped",
       name, num, cur->pkt_nb, cur->filtered, cur->dropped);
  sub_free(cur);
  send_status_ok(llid, tid, "OK");
}

void pcap_relay_snf_gone(char *name, int num)
{
  t_pcap_sub *cur = g_head_sub, *next;
  while (cur)
    {
    next = cur->next;
    if ((cur->num == num) && (!strcmp(cur->name, name)))
      {
      KTRC("pcap %s %d: the snf is going, ending the subscription",
           name, num);
      if (msg_exist_channel(cur->cli_llid))
        send_status_ko(cur->cli_llid, cur->cli_tid, "the snf was stopped");
      sub_free(cur);
      }
    cur = next;
    }
}

/* A gone client on a silent interface is only found here.                  */
void pcap_relay_beat(void)
{
  t_pcap_sub *cur = g_head_sub, *next;
  while (cur)
    {
    next = cur->next;
    if (!msg_exist_channel(cur->cli_llid))
      {
      PLOG("%s %d: the client left, unsubscribing", cur->name, cur->num);
      sub_free(cur);
      }
    cur = next;
    }
}

void pcap_relay_init(void)
{
  g_head_sub = NULL;
}
