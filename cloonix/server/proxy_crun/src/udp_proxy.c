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
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include "io_clownix.h"
#include "proxy_crun.h"
#include "util_sock.h"

static char g_unix_dgram_rx[MAX_PATH_LEN];
static char g_unix_dgram_tx[MAX_PATH_LEN];
static struct sockaddr_in g_loc_addr;
static uint32_t g_distant_ip;
static uint16_t g_distant_udp_port;
static int g_udp_sock, g_unix_sock;
static int g_udp_llid, g_unix_llid;
static int g_udp_forward_ok;


/***************************************************************************/
static int udp_init_pick_a_free_udp_port(uint16_t *udp_port, int *udp_sock)
{
  int i, fd, result = -1, max_try=1000;
  uint16_t port;
  uint32_t len = sizeof(struct sockaddr_in);
  *udp_port = -1;
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0))< 0)
    KOUT("%d", errno);
  memset(&g_loc_addr,  0, len);
  g_loc_addr.sin_family = AF_INET;
  g_loc_addr.sin_addr.s_addr = INADDR_ANY;
  for(i=0; i<max_try; i++)
    {
    port = 51001 + (rand()%4000);
    g_loc_addr.sin_port = htons(port);
    if (bind(fd, (const struct sockaddr *)&g_loc_addr, len) == 0)
      {
      result = 0;
      *udp_port = port;
      *udp_sock = fd;
      break;
      }
    }
  if (result)
    KERR("No port udp found");
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void from_crun_err_cb(int llid, int err, int from)
{   
  KERR("ERROR FROM CRUN %d %d", err, from);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  udp_proxy_end();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void from_udp_err_cb(int llid, int err, int from)
{   
  KERR("ERROR FROM UDP %d %d", err, from);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  udp_proxy_end();
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int from_crun_rx_cb(int llid, int fd)
{
  char buf[MAX_DGRAM_LEN];
  struct sockaddr_in dist_addr;
  struct sockaddr *pdist_addr = (struct sockaddr *) (&dist_addr);
  uint32_t ln = sizeof(struct sockaddr_in);
  int len_tx, len, result = 0;
  len = read(fd, buf, MAX_DGRAM_LEN-1);
  if (len == 0)
    {
    KERR("ERROR");
    udp_proxy_end();
    }
  else if (len < 0)
    {
    if ((errno != EAGAIN) && (errno != EINTR))
      {
      KERR("ERROR");
      udp_proxy_end();
      }
    }
  else
    {
    if ((g_distant_ip) && (g_distant_udp_port))
      {
      memset(pdist_addr, 0, sizeof(struct sockaddr_in));
      dist_addr.sin_family = AF_INET;
      dist_addr.sin_addr.s_addr = htonl(g_distant_ip);
      dist_addr.sin_port = htons(g_distant_udp_port);
      len_tx = sendto(g_udp_sock, buf, len, 0, pdist_addr, ln);
      while ((len_tx == -1) && (errno == EAGAIN))
        {
        usleep(1000);
        len_tx = sendto(g_udp_sock, buf, len, 0, pdist_addr, ln);
        }
      if (len_tx != len)
        {
        KERR("ERROR %d %d %d", len_tx, len, errno);
        udp_proxy_end();
        result = -1;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int from_udp_rx_cb(int llid, int fd)
{
  char buf[MAX_DGRAM_LEN];
  struct sockaddr_un name;
  struct sockaddr *pname = (struct sockaddr *)&name;
  int len_tx, len, len_un = sizeof(struct sockaddr_un);
  int result = 0;
  len = read(fd, buf, MAX_DGRAM_LEN-1);
  if (len == 0)
    {
    KERR("ERROR");
    udp_proxy_end();
    }
  else if (len < 0)
    {
    if ((errno != EAGAIN) && (errno != EINTR))
      {
      KERR("ERROR");
      udp_proxy_end();
      }
    }
  else
    {
    memset(&name, 0, sizeof(struct sockaddr_un));
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, g_unix_dgram_rx, 107);
    len_tx = sendto(g_unix_sock, buf, len, 0, pname, len_un);
    while ((len_tx == -1) && (errno == EAGAIN))
      {
      usleep(1000);
      len_tx = sendto(g_unix_sock, buf, len, 0, pname, len_un);
      }
    if (len_tx != len)
      {
      KERR("ERROR %d %d %d", len_tx, len, errno);
      udp_proxy_end();
      result = -1;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void udp_proxy_end(void)
{
  if (msg_exist_channel(g_udp_llid))
    msg_delete_channel(g_udp_llid);
  if (msg_exist_channel(g_unix_llid))
    msg_delete_channel(g_unix_llid);
  close(g_unix_sock);
  close(g_udp_sock);
  g_udp_llid = 0;
  g_unix_llid = 0;
  g_distant_ip = 0;
  g_distant_udp_port = 0;
  g_udp_sock = -1;
  g_unix_sock = -1;
  unlink(g_unix_dgram_tx);
  memset(g_unix_dgram_rx, 0, MAX_PATH_LEN);
  memset(g_unix_dgram_tx, 0, MAX_PATH_LEN);
  g_udp_forward_ok = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void udp_proxy_dist_udp_ip_port(uint32_t ip, uint16_t udp_port)
{
  g_distant_ip = ip;
  g_distant_udp_port = udp_port;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int udp_proxy_forward_ok(void)
{
  return g_udp_forward_ok;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int udp_proxy_init(char *proxyshare, uint16_t *udp_port)
{
  int sock, result = -1;
  g_distant_ip = 0;
  g_distant_udp_port = 0;
  g_udp_llid = 0;
  g_unix_llid = 0;
  g_udp_sock = -1;
  g_unix_sock = -1;
  g_udp_forward_ok = 0;
  memset(g_unix_dgram_rx, 0, MAX_PATH_LEN);
  memset(g_unix_dgram_tx, 0, MAX_PATH_LEN);
  if (!udp_init_pick_a_free_udp_port(udp_port, &sock))
    {
    printf("\nFREE UDP PORT FOUND %hu\n\n", *udp_port);
    snprintf(g_unix_dgram_tx, MAX_PATH_LEN-1,
             "%s/proxy_dgram_%hu_tx.sock", proxyshare, *udp_port);
    snprintf(g_unix_dgram_rx, MAX_PATH_LEN-1,
             "%s/proxy_dgram_%hu_rx.sock", proxyshare, *udp_port);
    if (strlen(g_unix_dgram_rx) >= 108)
      KOUT("ERROR PATH LEN %lu", strlen(g_unix_dgram_rx));
    if (strlen(g_unix_dgram_tx) >= 108)
      KOUT("ERROR PATH LEN %lu", strlen(g_unix_dgram_tx));
    g_unix_sock = util_socket_unix_dgram(g_unix_dgram_tx);
    if (g_unix_sock != -1)
      {
      g_udp_sock = sock;
      g_udp_llid = msg_watch_fd(g_udp_sock, from_udp_rx_cb,
                                from_udp_err_cb, "udp_inet");
      g_unix_llid = msg_watch_fd(g_unix_sock, from_crun_rx_cb,
                                 from_crun_err_cb, "udp_unix");
      if ((g_udp_llid == 0) || (g_unix_llid == 0))
        KERR("ERROR %d %d", g_udp_llid, g_unix_llid);
      else
        {
        g_udp_forward_ok = 1;
        result = 0;
        }
      }
    }
  return result; 
}
/*--------------------------------------------------------------------------*/
