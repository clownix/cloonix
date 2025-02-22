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
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "util_sock.h"
#include "rxtx.h"
#include "utils.h"

/*--------------------------------------------------------------------------*/
typedef struct t_udp_flow
{
  char dgram_rx[MAX_PATH_LEN];
  char dgram_tx[MAX_PATH_LEN];
  uint32_t sip;
  uint32_t dip;
  uint16_t sport;
  uint16_t dport;
  uint8_t smac[32];
  uint8_t dmac[32];
  int llid;
  int fd;
  int inactivity_count;
  int data_len;
  uint8_t *data;
  struct t_udp_flow *prev;
  struct t_udp_flow *next;
} t_udp_flow;
/*--------------------------------------------------------------------------*/

static uint32_t g_our_cisco_ip;
static uint32_t g_our_gw_ip;
static uint32_t g_our_dns_ip;
static uint32_t g_host_dns_ip;
static uint32_t g_host_local_ip;
static uint32_t g_offset;
static uint32_t g_post_chk;
static t_udp_flow *g_head_udp_flow;
/*--------------------------------------------------------------------------*/
char *get_nat_name(void);
char *get_net_name(void);

static t_udp_flow *find_udp_flow_llid(int llid);
/*--------------------------------------------------------------------------*/

static int udp_init_unix(int *proxy_fd, int *proxy_llid,  
                         char *dgram_rx, char *dgram_tx,
                         uint32_t sip, uint32_t dip,
                         uint16_t sport, uint16_t dport,
                         uint32_t dest_ip);

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
static int rx_proxy_cb(int llid, int fd)
{
  socklen_t slen;
  int data_len;
  uint8_t buf[MAX_TAP_BUF_LEN + END_FRAME_ADDED_CHECK_LEN];
  uint8_t *data;
  t_udp_flow *cur;
  int max_ln = MAX_TAP_BUF_LEN - g_offset;
  slen = 0;
  data = &(buf[g_offset]);
  data_len = recvfrom(fd, data, max_ln, 0, NULL, &slen);
  if (data_len <= 0)
    KERR("ERROR %d %d", data_len, errno);
  else
    {
    cur = find_udp_flow_llid(llid);
    if (!cur)
      KERR("ERROR");
    else
      {
      utils_fill_udp_packet(buf, data_len, cur->dmac, cur->smac,
                            cur->dip, cur->sip, cur->dport, cur->sport);
      rxtx_tx_enqueue(g_offset + data_len + g_post_chk, buf);
      }
    }
  return data_len;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_proxy_cb(int llid, int err, int from)
{
  KERR("ERROR UDP %d %d", err, from);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_udp_flow *find_udp_flow(uint32_t sip, uint32_t dip,
                                 uint16_t sport, uint16_t dport)

{
  t_udp_flow *cur = g_head_udp_flow;
  while(cur)
    {
    if ((cur->sip == sip) && (cur->dip == dip) && 
        (cur->sport == sport) && (cur->dport == dport))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_udp_flow *find_udp_flow_llid(int llid)

{
  t_udp_flow *cur = g_head_udp_flow;
  while(cur)
    {
    if ((llid > 0) && (cur->llid == llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static t_udp_flow *alloc_udp_flow(int proxy_fd, int proxy_llid,
                                  char *dgram_rx, char *dgram_tx,
                                  uint32_t sip, uint32_t dip,
                                  uint16_t sport, uint16_t dport,
                                  uint8_t *smac, uint8_t *dmac)

{ 
  t_udp_flow *cur = NULL;
  cur = (t_udp_flow *) utils_malloc(sizeof(t_udp_flow));
  if (cur == NULL)
    KOUT("ERROR OUT OF MEMORY");
  else
    {
    memset(cur, 0, sizeof(t_udp_flow));
    strncpy(cur->dgram_rx, dgram_rx, MAX_PATH_LEN-1);
    strncpy(cur->dgram_tx, dgram_tx, MAX_PATH_LEN-1);
    cur->sip = sip;
    cur->dip = dip;
    cur->sport = sport;
    cur->dport = dport;
    memcpy(cur->smac, smac, 6);
    memcpy(cur->dmac, dmac, 6);
    cur->llid = proxy_llid;
    cur->fd = proxy_fd;
    if (g_head_udp_flow)
      g_head_udp_flow->prev = cur;
    cur->next = g_head_udp_flow;
    g_head_udp_flow = cur;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void free_udp_flow(t_udp_flow *cur)
{
  char prefx[MAX_PATH_LEN];
  char *net = get_net_name();
  memset(prefx, 0, MAX_PATH_LEN);
  snprintf(prefx, MAX_PATH_LEN-1, "%s_%s/dgram", PROXYSHARE_IN, net);
  close(cur->fd);
  if (cur->llid > 0)
    {
    if (msg_exist_channel(cur->llid))
      msg_delete_channel(cur->llid);
    }
  if (strncmp(cur->dgram_rx, prefx, strlen(prefx)))
    KERR("ERROR %s", cur->dgram_rx);
  else
    unlink(cur->dgram_rx);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_udp_flow)
    g_head_udp_flow = cur->next;
  utils_free(cur);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timeout_beat(void *data)
{
  t_udp_flow *next, *cur = g_head_udp_flow;
  while(cur)
    {
    next = cur->next;
    cur->inactivity_count += 1;
    if (cur->inactivity_count > 3)
      {
      free_udp_flow(cur);
      }
    cur = next;
    }
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void transfert_onto_udp(uint8_t *smac,  uint8_t *dmac,
                               uint32_t sip,   uint32_t dip,
                               uint16_t sport, uint16_t dport,
                               uint32_t wanted_dest_ip,
                               int data_len, uint8_t *data)
{
  char dgram_rx[MAX_PATH_LEN];
  char dgram_tx[MAX_PATH_LEN];
  int  proxy_fd, proxy_llid, llid_rxtx;
  t_udp_flow *cur = find_udp_flow(sip, dip, sport, dport);
  if (!cur)
    {
    if (!udp_init_unix(&proxy_fd, &proxy_llid,
                       dgram_rx, dgram_tx, 
                       sip, dip, sport, dport, wanted_dest_ip))
      {
      llid_rxtx = get_llid_rxtx();
      cur = alloc_udp_flow(proxy_fd, proxy_llid, dgram_rx, dgram_tx,
                           sip, dip, sport, dport, smac, dmac);
      if (data_len)
        {
        cur->data_len = data_len;
        cur->data = (uint8_t *) malloc(data_len);
        memcpy(cur->data, data, data_len);
        }
      channel_rx_local_flow_ctrl(llid_rxtx, 1);
      }
    }
  if (!cur)
    {
    KERR("ERROR %X %X %hu %hu", sip, dip, sport, dport);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void udp_input(uint8_t *smac, uint8_t *dmac,
               uint32_t sip,  uint32_t dip,
               uint16_t sport, uint16_t dport,
               int data_len, uint8_t *data)
{
  if ((dip & 0xFFFFFF00) == (g_our_gw_ip & 0xFFFFFF00))
    {
    if (dip == g_our_gw_ip)
      {
      if (g_host_dns_ip == g_our_dns_ip)
        transfert_onto_udp(smac, dmac, sip, dip, sport, dport,
                           g_our_gw_ip, data_len, data);
      else
        transfert_onto_udp(smac, dmac, sip, dip, sport, dport,
                           g_host_local_ip, data_len, data);
      }
    else if (dip == g_our_dns_ip)
      {
      transfert_onto_udp(smac, dmac, sip, dip, sport, dport,
                         g_host_dns_ip, data_len, data);
      }
    else if (dip == g_our_cisco_ip)
      KERR("TOOLOOKINTO UDP FROM CISCO %X %X %d %d", sip, dip, sport, dport);
    else
      KERR("ERROR %X %X %hu %hu", sip, dip, sport, dport);
    }
  else
    {
    transfert_onto_udp(smac, dmac, sip, dip, sport, dport,
                       dip, data_len, data);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int dgram_tx_unix(char *dgram_tx, int len, char *data)
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

/****************************************************************************/
static void flow_to_path(char *dgram_rx, char *dgram_tx,
                         uint32_t sip, uint32_t dip,
                         uint16_t sport, uint16_t dport)
{
  char asc_sip[MAX_NAME_LEN];
  char asc_dip[MAX_NAME_LEN];
  char *net = get_net_name();
  memset(asc_sip, 0, MAX_NAME_LEN);
  memset(asc_dip, 0, MAX_NAME_LEN);
  memset(dgram_rx, 0, MAX_PATH_LEN);
  memset(dgram_tx, 0, MAX_PATH_LEN);
  int_to_ip_string(sip, asc_sip);
  int_to_ip_string(dip, asc_dip);
  snprintf(dgram_rx, MAX_PATH_LEN-1,
           "%s_%s/dgram_%s_%hu_%s_%hu_rx.sock", PROXYSHARE_IN, net,
           asc_sip, sport, asc_dip, dport);
  snprintf(dgram_tx, MAX_PATH_LEN-1,
           "%s_%s/dgram_%s_%hu_%s_%hu_tx.sock", PROXYSHARE_IN, net,
           asc_sip, sport, asc_dip, dport);
  if (strlen(dgram_tx) >= 108)
    KOUT("ERROR PATH LEN %lu %s", strlen(dgram_tx), dgram_tx);
  if (strlen(dgram_rx) >= 108)
    KOUT("ERROR PATH LEN %lu %s", strlen(dgram_rx), dgram_rx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int udp_init_unix(int *proxy_fd, int *proxy_llid,  
                         char *dgram_rx, char *dgram_tx,
                         uint32_t sip, uint32_t dip,
                         uint16_t sport, uint16_t dport,
                         uint32_t dest_ip)
{
  int llid, fd, result = -1;
  char buf[4 * MAX_PATH_LEN];
  int llid_prxy = get_llid_prxy();
  char prefx[MAX_PATH_LEN];
  char *net = get_net_name();
  memset(prefx, 0, MAX_PATH_LEN);
  snprintf(prefx, MAX_PATH_LEN-1, "%s_%s/dgram", PROXYSHARE_IN, net);
  if (!msg_exist_channel(llid_prxy))
    {
    KERR("ERROR");
    }
  else
    {
    *proxy_fd = -1;
    *proxy_llid = 0;
    flow_to_path(dgram_rx, dgram_tx, sip, dip, sport, dport);
    if (strncmp(dgram_rx, prefx, strlen(prefx)))
      KERR("ERROR %s", dgram_rx);
    else if (strncmp(dgram_tx, prefx, strlen(prefx)))
      KERR("ERROR %s", dgram_tx);
    else
      {
      fd = util_socket_unix_dgram(dgram_rx);
      if (fd < 0)
        {
        KERR("ERROR %s", dgram_rx);
        }
      else
        {
        llid = msg_watch_fd(fd, rx_proxy_cb, err_proxy_cb, "udpd2d");
        if (llid == 0)
          {
          KERR("ERROR %s", dgram_rx);
          close(fd);
          }
        else
          {
          *proxy_llid = llid;
          *proxy_fd = fd; 
          memset(buf, 0, 4*MAX_PATH_LEN);
          snprintf(buf, 4*MAX_PATH_LEN-1, "nat_proxy_udp_req %s "
                                          "dgram_rx:%s dgram_tx:%s "
                                          "sip:%X dip:%X dest:%X "
                                          "sport:%hu dport:%hu",
          get_nat_name(),dgram_rx,dgram_tx,sip,dip,dest_ip,sport,dport);
          rpct_send_sigdiag_msg(llid_prxy, 0, buf);
          result = 0;
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void udp_dgram_proxy_resp(int status, char *dgram_rx, char *dgram_tx,
                         uint32_t sip, uint32_t dip, uint32_t dest_ip,
                         uint16_t sport, uint16_t dport)
{ 
  t_udp_flow *cur;
  int llid_rxtx = get_llid_rxtx();
  if (status == -1)
    {
    KERR("ERROR udp_dgram_proxy_resp %s "
         "sip:%X dip:%X dest:%X sport:%hu dport:%hu",
         dgram_rx, sip, dip, dest_ip, sport, dport);
    }
  else
    {
    cur = find_udp_flow(sip, dip, sport, dport);
    if (!cur)
      KERR("ERROR udp_dgram_proxy_resp %s "
           "sip:%X dip:%X dest:%X sport:%hu dport:%hu",
           dgram_rx, sip, dip, dest_ip, sport, dport);
    else
      {
      if (cur->data_len)
        {
        dgram_tx_unix(cur->dgram_tx, cur->data_len, (char *)cur->data);
        cur->data_len = 0;
        free(cur->data);
        cur->data = NULL;
        }
      }
    }
  channel_rx_local_flow_ctrl(llid_rxtx, 0);
}     
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void udp_init(void)
{
  if (ip_string_to_int (&g_host_local_ip, "127.0.0.1"))
    KOUT(" ");
  g_our_gw_ip      = utils_get_gw_ip();
  g_our_dns_ip     = utils_get_dns_ip();
  g_our_cisco_ip   = utils_get_cisco_ip();
  g_host_dns_ip    = get_dns_addr();
  clownix_timeout_add(100, timeout_beat, NULL, NULL, NULL);
  g_head_udp_flow = NULL;
  g_offset = ETHERNET_HEADER_LEN +
             IPV4_HEADER_LEN     +
             UDP_HEADER_LEN; 
  g_post_chk = END_FRAME_ADDED_CHECK_LEN;
}
/*--------------------------------------------------------------------------*/
