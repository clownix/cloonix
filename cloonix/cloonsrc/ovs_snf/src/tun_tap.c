/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/if_tun.h>
#include <linux/if_arp.h>
#include <netinet/in.h>

#include "io_clownix.h"
#include "intf_flags.h"
#include "rpc_clownix.h"
#include "pcap_record.h"
#include "eventfull.h"

#define MAX_BUF_LEN 2000

static char g_tap_name[MAX_NAME_LEN];
static int g_llid;
static uint8_t g_buf[MAX_BUF_LEN];

typedef struct t_mac_src
{
  uint8_t mac[6];
  struct t_mac_src *prev;
  struct t_mac_src *next;
} t_mac_src;

static t_mac_src *g_head_mac_tx;
static t_mac_src *g_head_mac_rx;

static t_mac_src *find_mac(int tx, uint8_t *mac)
{
  t_mac_src *cur;
  if (tx)
    cur = g_head_mac_tx;
  else
    cur = g_head_mac_rx;
  while(cur)
    {
    if (!memcmp(cur->mac, mac, 6))
      break;
    cur = cur->next;
    }
  return cur;
}

static inline uint8_t *get_src_mac(uint8_t *data)
{
  return (&(data[6]));
}

static int rx_tap(int llid, int fd)
{
  int len, is_tx = 0, set_tx = 0;
  uint64_t usec;
  uint8_t *mac = get_src_mac(g_buf);
  t_mac_src *cur = NULL;
  if ((g_head_mac_rx != NULL) && (g_head_mac_tx != NULL))
    KERR("ERROR RX and TX both configured %s", g_tap_name);
  if (g_head_mac_rx != NULL)
    {
    is_tx = 1;
    set_tx = 0;
    cur = g_head_mac_rx; 
    }
  else if (g_head_mac_tx != NULL)
    {
    is_tx = 0;
    set_tx = 1;
    cur = g_head_mac_tx; 
    }
  len = read(fd, g_buf, MAX_BUF_LEN);
  if (len <= 0)
    KERR("ERROR TAP %s", g_tap_name);
  else
    {
    while(cur)
      {
      if (!memcmp(cur->mac, mac, 6))
        {
        is_tx = set_tx;
        break;
        }
      cur = cur->next;
      }
    /* Wall clock: this goes into the pcap record header. */
    usec = cloonix_get_epoch_usec();
    eventfull_hook_spy(is_tx, len);
    pcap_record_rx_packet(usec, len, g_buf);
    }
  return (len > 0) ? len : 0;
}

static void err_tap(int llid, int err, int from)
{
  KOUT("ERROR TAP %d", llid);
}

static int open_tap(char *tap, char *err)
{
  int fd = tap_open(tap, err);
  if (fd >= 0)
    {
    /* Before the interface goes up, so the first solicitation is never sent
       rather than sent and then captured. */
    tap_no_ipv6(tap);
    if (set_intf_flags_iff_promisc(tap))
      KERR("WARNING PROMISC %s", tap);
    }
  return fd;
}

void tun_tap_add_mac(int tx, char *mac_spyed_on)
{
  t_mac_src *cur;
  uint8_t mac[6];
  if (sscanf(mac_spyed_on, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
      &(mac[0]),&(mac[1]),&(mac[2]),&(mac[3]),&(mac[4]),&(mac[5])) != 6)
    KERR("%s", mac_spyed_on);
  else if (find_mac(tx, mac))
    KERR("ERROR EXISTS %s", mac_spyed_on); 
  else
    {
    cur = (t_mac_src *) malloc(sizeof(t_mac_src));
    memset(cur, 0, sizeof(t_mac_src));
    memcpy(cur->mac, mac, 6); 
    if (tx)
      {
      if (g_head_mac_tx)
        g_head_mac_tx->prev = cur;
      cur->next = g_head_mac_tx;
      g_head_mac_tx = cur;
      }
    else
      {
      if (g_head_mac_rx)
        g_head_mac_rx->prev = cur;
      cur->next = g_head_mac_rx;
      g_head_mac_rx = cur;
      }
    }
}

void tun_tap_purge_mac(void)
{
  t_mac_src *next, *cur;
  cur = g_head_mac_tx;
  while(cur)
    {
    next = cur->next;
    free(cur);
    cur = next;
    }
  g_head_mac_tx = NULL;

  cur = g_head_mac_rx;
  while(cur)
    {
    next = cur->next;
    free(cur);
    cur = next;
    }
  g_head_mac_rx = NULL;
}

#define TAP_FREE_TRIES   50
#define TAP_FREE_WAIT_US 100000

int tun_tap_open(char *name)
{
  char err[MAX_PATH_LEN];
  int i, fd, result = -1;
  g_head_mac_tx = NULL;
  g_head_mac_rx = NULL;
  memset(err, 0, MAX_PATH_LEN);
  memset(g_tap_name, 0, MAX_NAME_LEN);
  strncpy(g_tap_name, name, MAX_NAME_LEN-1);
  /* Capture off then on: the tap of the snf before this one lasts as long
     as its tap child does. Before the loop runs, so a wait is harmless. */
  for (i = 0; (i < TAP_FREE_TRIES) && intf_exists(g_tap_name); i++)
    usleep(TAP_FREE_WAIT_US);
  if (i)
    KERR("WARNING tap %s of the snf before this one was still there, "
         "waited %d ms for it%s", g_tap_name, i * (TAP_FREE_WAIT_US / 1000),
         (i == TAP_FREE_TRIES) ? ", it never went" : "");
  fd = open_tap(g_tap_name, err);
  if (fd >= 0)
    {
    if (set_intf_flags_iff_up_and_promisc(g_tap_name))
      {
      KERR("ERROR Not able to set up flag for tap: %s", g_tap_name);
      close(fd);
      }
    else
      {
      g_llid = msg_watch_fd(fd, rx_tap, err_tap, "tap");
      if (g_llid == 0)
        KERR("ERROR Not able to watch tap fd: %s", g_tap_name);
      else
        result = 0;
      }
    }
  else
    KERR("ERROR failure open tap: %s", err); 
  return result;
}

