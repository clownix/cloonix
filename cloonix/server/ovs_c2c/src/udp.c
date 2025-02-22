/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/un.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "util_sock.h"
#include "rxtx.h"
#include "udp.h"

static char g_proxy_unix_dgram_rx[MAX_PATH_LEN];
static char g_proxy_unix_dgram_tx[MAX_PATH_LEN];
static int g_llid_proxymous;
static int g_fd_proxymous;
static uint8_t g_rx[MAX_TAP_BUF_LEN+HEADER_TAP_MSG+END_FRAME_ADDED_CHECK_LEN];
static uint8_t g_tx[MAX_TAP_BUF_LEN+HEADER_TAP_MSG+END_FRAME_ADDED_CHECK_LEN];
static int g_traffic_mngt;
void reply_probe_udp(void);

char *get_net_name(void);
 
/****************************************************************************/
static int transmit_unix_dgram_tx(char *dgram_tx, int len, char *data)
{   
  struct sockaddr_un name;
  struct sockaddr *pname = (struct sockaddr *)&name;
  int len_tx, len_un = sizeof(struct sockaddr_un);
  int sock, result=0;  
  if (len >= MAX_DGRAM_LEN)
    KOUT("ERROR SOCK_DGRAM LEN %d %s", len, dgram_tx);
  memset(&name, 0, sizeof(struct sockaddr_un));
  sock = socket(AF_UNIX, SOCK_DGRAM, 0);
  if (sock < 0)
    KOUT("ERROR SOCK_DGRAM %s", dgram_tx);
  name.sun_family = AF_UNIX;
  strncpy(name.sun_path, dgram_tx, 107);

  len_tx = sendto(sock, data, len, 0, pname, len_un);
  while ((len_tx == -1) && (errno == EAGAIN))
    {
    usleep(1000);
    len_tx = sendto(sock, data, len, 0, pname, len_un);
    }
  if (len_tx != len)
    {
    KERR("ERROR %d %d %d %s", len_tx, len, errno, dgram_tx);
    result = -1;
    }
  close(sock);
  return result;
}     
/*---------------------------------------------------------------------------*/

/***************************************************************************/
int udp_tx_sig_send(int len, uint8_t *tx)
{
  int result = 0;
  if ((len <= 0) || (len > MAX_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR SEND  %d", len);
  fct_seqtap_tx(kind_seqtap_sig_hello, g_tx, 0, len, tx);
  if (g_llid_proxymous)
    {
    if (transmit_unix_dgram_tx(g_proxy_unix_dgram_tx, 
                               len+HEADER_TAP_MSG, (char *)g_tx))
      {
      KERR("ERROR %d", len);
      result = -1;
      } 
    }
  else
    {
    KERR("ERROR %d", len);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/***************************************************************************/
int udp_tx_traf_send(int len, uint8_t *buf)
{
  static uint16_t seqtap=0;
  int result = 0;
  seqtap += 1;
  if ((len <= 0) || (len > MAX_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN))
    KOUT("ERROR SEND  %d", len);
  fct_seqtap_tx(kind_seqtap_data, g_tx, seqtap, len, buf);
  if (g_llid_proxymous)
    {
    if (transmit_unix_dgram_tx(g_proxy_unix_dgram_tx,
                               len+HEADER_TAP_MSG,  (char *) g_tx))
      {
      KERR("ERROR %d", len);
      result = -1;
      } 
    }
  else
    {
    KERR("ERROR %d", len);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_cb(int llid, int err, int from)
{
  KERR("ERROR UDP %d %d", err, from);
  udp_close();
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
void udp_close(void)
{
  if (msg_exist_channel(g_llid_proxymous))
    msg_delete_channel(g_llid_proxymous);
  if (g_fd_proxymous != -1) 
    close(g_fd_proxymous);
  g_llid_proxymous = 0;
  g_fd_proxymous = -1;
  unlink(g_proxy_unix_dgram_rx);
  unlink(g_proxy_unix_dgram_tx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int udp_get_traffic_mngt(void)
{
  return g_traffic_mngt;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void udp_enter_traffic_mngt(void)
{
  g_traffic_mngt = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_dgram_cb(int llid, int fd)
{
  static uint16_t seqtap=0;
  uint16_t seq;
  int len, buf_len, result;
  uint8_t *buf;
  len = read(fd, g_rx, MAX_TAP_BUF_LEN+HEADER_TAP_MSG+END_FRAME_ADDED_CHECK_LEN);
  if (len <= 0)
    KOUT("ERROR READ %d %d", len, errno);
  if (g_traffic_mngt == 0)
    {
    result = fct_seqtap_rx(1, len, fd, g_rx, &seq, &buf_len, &buf);
    if (result != kind_seqtap_sig_hello)
      KOUT("ERROR %d", result);
    reply_probe_udp();
    }
  else
    {
    result = fct_seqtap_rx(1, len, fd, g_rx, &seq, &buf_len, &buf);
    if (result != kind_seqtap_data)
      KOUT("ERROR %d", result);
    if (seq != ((seqtap+1)&0xFFFF)) 
      KERR("WARNING %d %d", seq, seqtap);
    seqtap = seq;
    rxtx_tx_enqueue(buf_len, buf);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int udp_init(uint16_t udp_port)
{
  int fd, result = -1;
  char *net = get_net_name();
  g_llid_proxymous = 0;
  g_fd_proxymous = -1;
  g_traffic_mngt = 0;
  if (udp_port == 0)
    KOUT("ERROR No port udp");
  memset(g_proxy_unix_dgram_rx, 0, MAX_PATH_LEN-1);
  memset(g_proxy_unix_dgram_tx, 0, MAX_PATH_LEN-1);
  snprintf(g_proxy_unix_dgram_tx, MAX_PATH_LEN-1,
           "%s_%s/dgram_c2c_%hu_tx.sock", PROXYSHARE_IN, net, udp_port);
  snprintf(g_proxy_unix_dgram_rx, MAX_PATH_LEN-1,
           "%s_%s/dgram_c2c_%hu_rx.sock", PROXYSHARE_IN, net, udp_port);
  if (strlen(g_proxy_unix_dgram_tx) >= 108)
    KOUT("ERROR PATH LEN %lu", strlen(g_proxy_unix_dgram_tx));
  if (strlen(g_proxy_unix_dgram_rx) >= 108)
    KOUT("ERROR PATH LEN %lu", strlen(g_proxy_unix_dgram_rx));
  fd = util_socket_unix_dgram(g_proxy_unix_dgram_rx);
  if (fd < 0)
    KERR("ERROR %s", g_proxy_unix_dgram_rx);
  else
    {
    g_llid_proxymous = msg_watch_fd(fd, rx_dgram_cb, err_cb, "udpd2d");
    if (g_llid_proxymous == 0)
      {
      KERR("ERROR %s", g_proxy_unix_dgram_rx);
      close(fd);
      }
    else
      {
      g_fd_proxymous = fd;
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/
                                                                                                              
