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
#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mount.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "nat_main.h"
#include "nat_udp.h"
#include "util_sock.h" 
#include "nat_utils.h"
#include "proxy_crun.h"

char *get_net_name(void);
static t_llid_udp *g_head_llid_udp;

static uint32_t g_our_cisco_ip;
static uint32_t g_our_gw_ip;
static uint32_t g_our_dns_ip;
static uint32_t g_host_dns_ip;
static uint32_t g_host_local_ip;
static uint32_t g_offset;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_llid_udp *find_llidrx(int llid)
  
{   
  t_llid_udp *cur = g_head_llid_udp;
  while(cur)
    {
    if (!cur->item)
      KOUT("ERROR");
    if ((llid > 0) && (cur->item->llidrx == llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_llid_udp *find_llidtx(int llid)
  
{
  t_llid_udp *cur = g_head_llid_udp;
  while(cur)
    {
    if (!cur->item)
      KOUT("ERROR");
    if ((llid > 0) && (cur->item->llidtx == llid))
      break;
    cur = cur->next;
    }
  return cur;
} 
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_llid_udp *find_llid_dgram_rx(char *dgram_rx)
{   
  t_llid_udp *cur = g_head_llid_udp;
  while(cur)
    {
    if (!cur->item)
      KOUT("ERROR");
    if ((dgram_rx) && (strlen(dgram_rx)) &&
        (!strcmp(dgram_rx, cur->item->dgram_rx)))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void udp_alloc(t_ctx_nat *ctx,
                      int fdrx, int llidrx, int fdtx, int llidtx,
                      char *dgram_rx, char *dgram_tx,
                      uint32_t sip, uint32_t dip, uint32_t dest_ip,
                      uint16_t sport, uint16_t dport)
{
  t_llid_udp *cur_llid = (t_llid_udp *) malloc(sizeof(t_llid_udp));
  t_udp_flow *cur = (t_udp_flow *) malloc(sizeof(t_udp_flow));
  memset(cur_llid, 0, sizeof(t_llid_udp));
  memset(cur, 0, sizeof(t_udp_flow));
  cur->ctx = ctx;
  cur->sip = sip;
  cur->dip = dip;
  cur->dest_ip = dest_ip;
  cur->sport = sport;
  cur->dport = dport;
  strncpy(cur->dgram_rx, dgram_rx, MAX_PATH_LEN-1);
  cur->llidrx = llidrx;
  cur->fdrx = fdrx;
  strncpy(cur->dgram_tx, dgram_tx, MAX_PATH_LEN-1);
  cur->llidtx = llidtx;
  cur->fdtx = fdtx;
  if (ctx->head_udp)
    ctx->head_udp->prev = cur;
  cur->next = ctx->head_udp;
  ctx->head_udp = cur;
  cur_llid->item = cur;
  if (g_head_llid_udp)
    g_head_llid_udp->prev = cur_llid;
  cur_llid->next = g_head_llid_udp;
  g_head_llid_udp = cur_llid;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void udp_free(t_ctx_nat *ctx, t_llid_udp *cur_llid)
{
  t_udp_flow *cur;
  char prefx[MAX_PATH_LEN];
  char *proxydir = get_proxyshare();

  memset(prefx, 0, MAX_PATH_LEN);
  snprintf(prefx, MAX_PATH_LEN-1, "%s/dgram", proxydir);
  if ((!ctx) || (!cur_llid))
    KOUT("ERROR %p %p", ctx, cur_llid);
  cur = cur_llid->item;
  if (!cur)
    KOUT("ERROR");
  close(cur->fdrx);
  close(cur->fdtx);

  if ((cur->llidrx) && (msg_exist_channel(cur->llidrx)))
    msg_delete_channel(cur->llidrx);
  if ((cur->llidtx) && (msg_exist_channel(cur->llidtx)))
    msg_delete_channel(cur->llidtx);

  if (strncmp(cur->dgram_tx, prefx, strlen(prefx)))
    KERR("ERROR %s", cur->dgram_tx);
  else
    unlink(cur->dgram_tx);

  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == ctx->head_udp)
    ctx->head_udp = cur->next;

  if (cur_llid->prev)
    cur_llid->prev->next = cur_llid->next;
  if (cur_llid->next)
    cur_llid->next->prev = cur_llid->prev;
  if (cur_llid == g_head_llid_udp)
    g_head_llid_udp = cur_llid->next;

  free(cur);
  free(cur_llid);
} 
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static uint32_t get_dns_addr(void)
{
  uint32_t dns_addr = 0;
  char dns_ip[MAX_NAME_LEN];
  char buff[512];
  char buff2[256];
  char *ptr_start, *ptr_end;
  FILE *f;
  f = fopen("/etc/resolv.conf", "r");
  if (f)
    {
    while (fgets(buff, 512, f) != NULL)
      {
      if (sscanf(buff, "nameserver%*[ \t]%256s", buff2) == 1)
        {
        ptr_start = buff2 + strspn(buff2, " \r\n\t");
        ptr_end = ptr_start + strcspn(ptr_start, " \r\n\t");
        if (ptr_end)
          *ptr_end = 0;
        strcpy(dns_ip, ptr_start);
        if (ip_string_to_int (&dns_addr, dns_ip))
          continue;
        else
          break;
        }
      }
    }
  if (!dns_addr)
    {
    dns_addr = g_host_local_ip;
    }
  return dns_addr;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int dgram_tx_unix(char *dgram_rx, int len, char *data)
{
  struct sockaddr_un name;
  struct sockaddr *pname = (struct sockaddr *)&name;
  int len_tx, len_un = sizeof(struct sockaddr_un);
  int sock, result=0;
  t_llid_udp *cur_llid = find_llid_dgram_rx(dgram_rx);
  t_udp_flow *cur;
  if (!cur_llid)
    KERR("ERROR %s", dgram_rx);
  else
    {
    cur = cur_llid->item;
    if (!cur)
      KOUT("ERROR %s", dgram_rx);
    if (len >= MAX_DGRAM_LEN)
      KOUT("ERROR SOCK_DGRAM LEN %d %s", len, dgram_rx);
    memset(&name, 0, sizeof(struct sockaddr_un));
    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0)
      KOUT("ERROR SOCK_DGRAM %s", dgram_rx);
    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, dgram_rx, 107);
    len_tx = sendto(sock, data, len, 0, pname, len_un);
    while ((len_tx == -1) && (errno == EAGAIN))
      {
      usleep(1000);
      len_tx = sendto(sock, data, len, 0, pname, len_un);
      }
    if (len_tx != len)
      {
      KERR("ERROR %d %d %d %s", len_tx, len, errno, dgram_rx);
      result = -1;
      }
    close(sock);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_proxytx_cb(int llid, int fd)
{
  socklen_t slen;
  int data_len, tx;
  uint8_t buf[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
  uint8_t *data;
  t_llid_udp *cur_llid;
  t_udp_flow *cur;
  struct sockaddr_in addr;
  int ln=sizeof (struct sockaddr_in);
  int max_len = HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN;
  slen = 0;
  data = &(buf[g_offset]);
  data_len = recvfrom(fd, data, max_len - g_offset, 0, NULL, &slen);
  if (data_len <= 0)
    KERR("ERROR %d %d", data_len, errno);
  else
    {
    cur_llid = find_llidtx(llid);
    if (!cur_llid)
      KERR("ERROR");
    else
      {
      cur = cur_llid->item;
      if (!cur)
        KOUT("ERROR");
      memset(&addr, 0, sizeof(struct sockaddr_in));
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl(cur->dest_ip);
      addr.sin_port = htons(cur->dport);
      tx = sendto(cur->fdrx,data,data_len,0,(struct sockaddr *)&addr,ln);
      if ((tx == -1) && (errno ==  ENETUNREACH))
        {
        }
      else if (tx != data_len)
        KERR("ERROR %d %d %d %hhu.%hhu.%hhu.%hhu %hu", tx, errno, data_len,
             (cur->dest_ip>>24), (cur->dest_ip>>16),
             (cur->dest_ip>>8), (cur->dest_ip), cur->dport);
      }
    }
  return data_len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_proxyrx_cb(int llid, int fd)
{
  socklen_t slen;
  int data_len;
  uint8_t buf[HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
  uint8_t *data;
  t_llid_udp *cur_llid;
  t_udp_flow *cur;
  int max_len = HEADER_TAP_MSG + TRAF_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN;
  slen = 0;
  data = &(buf[g_offset]);
  data_len = recvfrom(fd, data, max_len - g_offset, 0, NULL, &slen);
  if (data_len <= 0)
    KERR("ERROR %d %d", data_len, errno);
  else
    {
    cur_llid = find_llidrx(llid);
    if (!cur_llid)
      KERR("ERROR");
    else
      {
      cur = cur_llid->item;
      if (!cur)
        KOUT("ERROR");
      if (dgram_tx_unix(cur->dgram_rx, data_len, (char *)data))
        KERR("ERROR");
      }
    }
  return data_len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_proxytx_cb(int llid, int err, int from)
{
  KERR("ERROR UDP %d %d", err, from);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_proxyrx_cb(int llid, int err, int from)
{
  KERR("ERROR UDP %d %d", err, from);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int open_inet_udp_sock(int *ret_fd)
{       
  int result = 0;
  struct sockaddr_in addr;
  int fd = socket(AF_INET,SOCK_DGRAM,0);
  if(fd >= 0)
    {
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr))<0)
      close(fd);
    else
      {
      if ((fd < 0) || (fd >= MAX_SELECT_CHANNELS-1))
        KOUT("%d", fd);
      result = msg_watch_fd(fd, rx_proxyrx_cb, err_proxyrx_cb, "udp");
      if (result <= 0)
        KOUT(" ");
      *ret_fd = fd;
      }
    }
  return result;
} 
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int open_unix_dgram(char *dgram_tx, int *fdtx)
{
  int fd, llid, llidtx = 0;
  fd = util_socket_unix_dgram(dgram_tx);
  if (fd < 0)
    {
    KERR("ERROR %s", dgram_tx);
    }
  else
    {
    llid = msg_watch_fd(fd, rx_proxytx_cb, err_proxytx_cb, "udpd2d");
    if (llid == 0)
      {
      KERR("ERROR %s", dgram_tx);
      close(fd);
      }
    else
      {
      *fdtx = fd;
      llidtx = llid;
      }
    }
  return llidtx;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_beat(void *data)
{
  t_llid_udp *next_llid, *cur_llid = g_head_llid_udp;
  t_udp_flow *cur;
  while (cur_llid)
    {
    next_llid = cur_llid->next;
    cur = cur_llid->item;
    if (!cur)
      KERR("ERROR");
    else
      {
      cur->inactivity_count += 1;
      if (cur->inactivity_count > 5)
        udp_free(cur->ctx, cur_llid);
      }
    cur_llid = next_llid;
    }
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int nat_udp_dgram_proxy_req(t_ctx_nat *ctx, char *dgram_rx, char *dgram_tx,
                            uint32_t sip, uint32_t dip, uint32_t dest_ip,
                            uint16_t sport, uint16_t dport)
{
  int llidrx, llidtx, fdrx, fdtx, result = 0;
  char prefx[MAX_PATH_LEN];
  char *proxydir = get_proxyshare();

  memset(prefx, 0, MAX_PATH_LEN);
  snprintf(prefx, MAX_PATH_LEN-1, "%s/dgram", proxydir);
  if (!ctx)
    KOUT("ERROR");
  if (strncmp(dgram_rx, prefx, strlen(prefx)))
    KERR("ERROR %s", dgram_rx);
  else if (strncmp(dgram_tx, prefx, strlen(prefx)))
    KERR("ERROR %s", dgram_tx);
  else
    {
    llidrx = open_inet_udp_sock(&fdrx);
    llidtx = open_unix_dgram(dgram_tx, &fdtx);
    if ((llidrx == 0) || (llidtx == 0))
      {
      KERR("ERROR %s sip:%X dip:%X dest:%X sport:%hu dport:%hu",
           dgram_rx, sip, dip, dest_ip, sport, dport);
      KERR("ERROR llidrx:%d llidtx:%d", llidrx, llidtx);
      result = -1;
      }
    else
      udp_alloc(ctx, fdrx, llidrx, fdtx, llidtx, dgram_rx, dgram_tx,
                sip, dip, dest_ip, sport, dport);
    }
  return result;
}     
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_udp_del(t_ctx_nat *ctx)
{
  t_llid_udp *next_llid, *cur_llid = g_head_llid_udp;
  t_udp_flow *cur;  
  while (cur_llid)
    {
    next_llid = cur_llid->next;
    cur = cur_llid->item;
    if (!cur)
      KERR("ERROR");
    else
      udp_free(ctx, cur_llid);
    cur_llid = next_llid;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_udp_init(void)
{     
  if (ip_string_to_int (&g_host_local_ip, "127.0.0.1"))
    KOUT(" ");
  g_our_gw_ip      = utils_get_gw_ip();
  g_our_dns_ip     = utils_get_dns_ip();
  g_our_cisco_ip   = utils_get_cisco_ip();
  g_host_dns_ip    = get_dns_addr();
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
  g_head_llid_udp = NULL;
  g_offset = ETHERNET_HEADER_LEN +
             IPV4_HEADER_LEN     + 
             UDP_HEADER_LEN;
}
/*--------------------------------------------------------------------------*/


