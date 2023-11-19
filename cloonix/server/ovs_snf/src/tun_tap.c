/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include "rpc_clownix.h"
#include "pcap_record.h"
#include "eventfull.h"

#define MAX_TAP_BUF_LEN 2000

static char g_tap_name[MAX_NAME_LEN];
static int g_llid;
static uint8_t g_buf[MAX_TAP_BUF_LEN];

typedef struct t_mac_src
{
  uint8_t mac[6];
  struct t_mac_src *prev;
  struct t_mac_src *next;
} t_mac_src;

static t_mac_src *g_head_mac_tx;
static t_mac_src *g_head_mac_rx;

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static inline uint8_t *get_src_mac(uint8_t *data)
{
  return (&(data[6]));
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_tap(int llid, int fd)
{
  int len, is_tx, set_tx = 0;
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
  len = read(fd, g_buf, MAX_TAP_BUF_LEN);
  while(cur)
    {
    if (!memcmp(cur->mac, mac, 6))
      {
      is_tx = set_tx;
      break;
      }
    cur = cur->next;
    }
  if (len <= 0)
    KERR("ERROR TAP %s", g_tap_name);
  else
    {
    usec = cloonix_get_usec();
    eventfull_hook_spy(is_tx, len);
    pcap_record_rx_packet(usec, len, g_buf);
    }
  return 0;
}
/*-------------------------------------------------------------------------*/

/****************************************************************************/
static void err_tap(int llid, int err, int from)
{
  KOUT("ERROR TAP %d", llid);
}
/*-------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_intf_flags_iff(char *intf, int *flags)
{
  int result = -1, s, io;
  struct ifreq ifr;
  s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
  if (s > 0)
    {
    memset(&ifr, 0, sizeof(struct ifreq));
    strncpy(ifr.ifr_name, intf, IFNAMSIZ);
    io = ioctl (s, SIOCGIFFLAGS, &ifr);
    if(io == 0)
      {
      *flags = ifr.ifr_flags;
      result = 0;
      }
    close(s);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int set_intf_flags_iff_up_and_promisc(char *intf)
{
  int result = -1, s, io;
  struct ifreq ifr;
  int flags;
  if (!get_intf_flags_iff(intf, &flags))
    {
    s = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (s > 0)
      {
      memset(&ifr, 0, sizeof(struct ifreq));
      strncpy(ifr.ifr_name, intf, IFNAMSIZ);
      ifr.ifr_flags = flags | IFF_UP | IFF_RUNNING | IFF_PROMISC;
      io = ioctl (s, SIOCSIFFLAGS, &ifr);
      if(!io)
        result = 0;
      close(s);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int open_tap(char *tap, char *err)
{
  int result = -1;
  struct ifreq ifr;
  int flags, fd;
  uid_t owner = getuid();
  if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
    sprintf(err, "\nError open \"/dev/net/tun\", %d\n", errno);
  else if (!get_intf_flags_iff(tap, &flags))
    sprintf(err, "\nError system already has %s\n\n", tap);
  else
    {
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, tap, sizeof(ifr.ifr_name) - 1);
    if(ioctl(fd, TUNSETIFF, (void *) &ifr) < 0)
      sprintf(err, "\nError ioctl TUNSETIFF \"%s\", %d", tap,  errno);
    else if(ioctl(fd, TUNSETOWNER, owner) < 0)
      sprintf(err, "\nError ioctl TUNSETOWNER \"%s\", %d", tap,  errno);
    else if(ioctl(fd, TUNSETGROUP, owner) < 0)
      sprintf(err, "\nError ioctl TUNSETGROUP \"%s\", %d", tap,  errno);
    else
      result = fd;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void tun_tap_del_mac(int tx, char *mac_spyed_on)
{
  t_mac_src *cur; 
  uint8_t mac[6];
  if (sscanf(mac_spyed_on, "%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX",
      &(mac[0]),&(mac[1]),&(mac[2]),&(mac[3]),&(mac[4]),&(mac[5])) != 6)
    KERR("%s", mac_spyed_on);
  else
    {
    cur = find_mac(tx, mac);
    if (!cur)
      KERR("ERROR DOES NOT EXIST %s", mac_spyed_on);
    else
      {
      memcpy(cur->mac, mac, 6); 
      if (cur->prev)
        cur->prev->next = cur->next;
      if (cur->next)
        cur->next->prev = cur->prev;
      if (tx)
        {
        if (g_head_mac_tx == cur)
          g_head_mac_tx = cur->next;
        }
      else
        {
        if (g_head_mac_rx == cur)
          g_head_mac_rx = cur->next;
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
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
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int tun_tap_open(char *name)
{
  char err[MAX_PATH_LEN];
  int fd, result = -1;
  g_head_mac_tx = NULL;
  g_head_mac_rx = NULL;
  memset(err, 0, MAX_PATH_LEN);
  memset(g_tap_name, 0, MAX_NAME_LEN);
  strncpy(g_tap_name, name, MAX_NAME_LEN-1);
  fd = open_tap(g_tap_name, err);
  if (fd > 0)
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
/*---------------------------------------------------------------------------*/


