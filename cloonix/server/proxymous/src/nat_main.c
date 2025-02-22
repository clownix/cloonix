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
#include <linux/tcp.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "proxy_crun.h"
#include "util_sock.h"
#include "nat_main.h"
#include "nat_utils.h"
#include "nat_icmp.h"
#include "nat_udp.h"
#include "nat_tcp.h"

static t_ctx_nat *g_head_ctx_nat;


/****************************************************************************/
static t_ctx_nat *ctx_nat_find(char *nat)
{
  t_ctx_nat *cur = g_head_ctx_nat;
  while(cur)
    {
    if (!strcmp(cur->nat, nat))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ctx_nat *ctx_nat_find_with_llid_listen(int llid)
{
  t_ctx_nat *cur = g_head_ctx_nat;
  while(cur)
    {
    if ((llid) && (cur->llid_listen == llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_ctx_nat *ctx_nat_find_with_llid_sig(int llid)
{
  t_ctx_nat *cur = g_head_ctx_nat;
  while(cur)
    {
    if ((llid) && (cur->llid_sig == llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void alloc_ctx_nat(char *nat, char *nat_prxy_path, int llid_listen)
{
  t_ctx_nat *cur = (t_ctx_nat *) malloc(sizeof(t_ctx_nat));
  memset(cur, 0, sizeof(t_ctx_nat));
  cur->llid_listen = llid_listen;
  strncpy(cur->nat, nat, MAX_NAME_LEN-1);
  strncpy(cur->nat_prxy_path, nat_prxy_path, MAX_PATH_LEN-1);
  if (g_head_ctx_nat)
    g_head_ctx_nat->prev = cur;
  cur->next = g_head_ctx_nat;
  g_head_ctx_nat = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_ctx_nat(t_ctx_nat *cur)
{
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (g_head_ctx_nat == cur)
    g_head_ctx_nat = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void delete_nat_ctx(t_ctx_nat *cur)
{
  nat_tcp_del(cur);
  nat_udp_del(cur);
  nat_icmp_del(cur);
  if ((cur->llid_listen) && (msg_exist_channel(cur->llid_listen)))
    {
    msg_delete_channel(cur->llid_listen);
    cur->llid_listen = 0;
    }
  if ((cur->llid_sig) && (msg_exist_channel(cur->llid_sig)))
    {
    msg_delete_channel(cur->llid_sig);
    cur->llid_sig = 0;
    }
  unlink(cur->nat_prxy_path);
  free_ctx_nat(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_nat_prxy_cb (int llid, int err, int from)
{
  t_ctx_nat *cur = ctx_nat_find_with_llid_sig(llid);
  if (!cur)
    KERR("ERROR TRAFFIC CONNECTION %d", llid);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
}
/*--------------------------------------------------------------------------*/
  
/****************************************************************************/
static void rx_nat_prxy_cb (int llid, int len, char *buf)
{ 
  if (rpct_decoder(llid, len, buf))
    KOUT("ERROR DECODER %s", buf);
} 
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_nat_prxy_client(int llid, int llid_new)
{
  t_ctx_nat *cur = ctx_nat_find_with_llid_listen(llid);
  if (!cur)
    KERR("ERROR LISTENING CONNECTION %d %d", llid, llid_new);
  else
    {
    msg_mngt_set_callbacks (llid_new, err_nat_prxy_cb, rx_nat_prxy_cb);
    msg_delete_channel(cur->llid_listen);
    cur->llid_listen = 0;
    cur->llid_sig = llid_new;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int nat_main_request(char *nat, int on)
{
  t_ctx_nat *cur;
  int llid_listen, result = -1;
  char nat_prxy_path[MAX_PATH_LEN];
  if ((!nat) || (strlen(nat) == 0))
    KOUT("ERROR");
  cur = ctx_nat_find(nat);
  if (on)
    {
    if (cur)
      KERR("ERROR %s EXISTS", nat);
    else
      {
      memset(nat_prxy_path, 0, MAX_PATH_LEN);
      snprintf(nat_prxy_path, MAX_PATH_LEN-1,"%s/%s/%s",
               get_proxymous_root_path(), NAT_PROXY_DIR, nat);
      if (!access(nat_prxy_path, F_OK))
        {
        KERR("WARNING %s exists ERASING", nat_prxy_path);
        unlink(nat_prxy_path);
        }
      llid_listen = string_server_unix(nat_prxy_path,
                                       connect_from_nat_prxy_client,
                                       "prxy");
      if (llid_listen == 0)
        KERR("ERROR %s LISTEN", nat);
      else
        {
        alloc_ctx_nat(nat, nat_prxy_path, llid_listen);
        result = 0;
        }
      }
    }
  else
    {
    if (!cur)
      KERR("ERROR %s DOES NOT EXIST", nat);
    else
      {
      delete_nat_ctx(cur);
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_poldiag_msg(int llid, int tid, char *line)
{
  char nat[MAX_NAME_LEN];
  char msg[MAX_PATH_LEN];
  t_ctx_nat *cur = ctx_nat_find_with_llid_sig(llid);
  if (!cur)
    KERR("ERROR TRAFFIC CONNECTION %d %s", llid, line);
  else
    {
    if (sscanf(line,
    "nat_proxy_beat_req %s", nat) == 1)
      {
      if (strcmp(nat, cur->nat))
        KERR("ERROR %s %s", nat, cur->nat);
      else
        {
        memset(msg, 0, MAX_PATH_LEN);
        snprintf(msg, MAX_PATH_LEN-1, "nat_proxy_beat_resp %s", nat);
        rpct_send_poldiag_msg(llid, tid, msg);
        }
      }
    else
      KERR("ERROR %s", line);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_sigdiag_msg(int llid, int tid, char *line)
{
  char dgram_rx[MAX_PATH_LEN];
  char dgram_tx[MAX_PATH_LEN];
  char stream[MAX_PATH_LEN];
  char nat[MAX_NAME_LEN];
  uint32_t sip, dip, ipdst;
  uint16_t sport, dport, ident, seq;
  char buf[2*MAX_PATH_LEN];
  t_ctx_nat *cur = ctx_nat_find_with_llid_sig(llid);
  if (!cur)
    KERR("ERROR TRAFFIC CONNECTION %d %s", llid, line);
  else
    {
    memset(buf, 0, 2*MAX_PATH_LEN);
    if (sscanf(line,
    "nat_proxy_hello_req %s", nat) == 1)
      {
      if (strcmp(nat, cur->nat))
        KERR("ERROR %s %s", nat, cur->nat);
      else
        {
        snprintf(buf, 2*MAX_PATH_LEN-1, "nat_proxy_hello_resp %s", nat);
        rpct_send_sigdiag_msg(llid, tid, buf);
        }
      }
    else if (sscanf(line,
    "nat_proxy_tcp_req %s stream:%s sip:%X dip:%X sport:%hu dport:%hu",
    nat, stream, &sip, &dip, &sport, &dport) == 6)
      {
      if (strcmp(nat, cur->nat))
        KERR("ERROR %s %s", nat, cur->nat);
      else if (nat_tcp_stream_proxy_req(cur, stream, sip, dip, sport, dport))
        {
        snprintf(buf, 2*MAX_PATH_LEN-1,
        "nat_proxy_tcp_req_ko %s stream:%s sip:%X dip:%X sport:%hu dport:%hu",
        nat, stream, sip, dip, sport, dport);
        }
      else
        {
        snprintf(buf, 2*MAX_PATH_LEN-1,
        "nat_proxy_tcp_req_ok %s stream:%s sip:%X dip:%X sport:%hu dport:%hu",
        nat, stream, sip, dip, sport, dport);
        }
      rpct_send_sigdiag_msg(llid, 0, buf);
      }
    else if (sscanf(line,
    "nat_proxy_tcp_stop %s sip:%X dip:%X sport:%hu dport:%hu",
    nat, &sip, &dip, &sport, &dport) == 5)
      {
      if (strcmp(nat, cur->nat))
        KERR("ERROR %s %s", nat, cur->nat);
      else
        nat_tcp_stop_go(cur, sip, dip, sport, dport, 1);
      }
    else if (sscanf(line,
    "nat_proxy_tcp_go %s sip:%X dip:%X sport:%hu dport:%hu",
    nat, &sip, &dip, &sport, &dport) == 5)
      {
      if (strcmp(nat, cur->nat))
        KERR("ERROR %s %s", nat, cur->nat);
      else
        nat_tcp_stop_go(cur, sip, dip, sport, dport, 0);
      }
    else if (sscanf(line,
    "nat_proxy_udp_req %s dgram_rx:%s dgram_tx:%s "
    "sip:%X dip:%X dest:%X sport:%hu dport:%hu",
    nat, dgram_rx, dgram_tx, &sip, &dip, &ipdst, &sport, &dport) == 8)
      {
      if (strcmp(nat, cur->nat))
        KERR("ERROR %s %s", nat, cur->nat);
      else if (nat_udp_dgram_proxy_req(cur, dgram_rx, dgram_tx,
                                  sip, dip, ipdst, sport, dport))
        {
        snprintf(buf, 2*MAX_PATH_LEN-1,
                 "nat_proxy_udp_resp_ko %s dgram_rx:%s dgram_tx:%s "
                 "sip:%X dip:%X dest:%X sport:%hu dport:%hu",
                 nat, dgram_rx, dgram_tx, sip, dip, ipdst, sport, dport);
        }
      else
        {
        snprintf(buf, 2*MAX_PATH_LEN-1,
                 "nat_proxy_udp_resp_ok %s dgram_rx:%s dgram_tx:%s "
                 "sip:%X dip:%X dest:%X sport:%hu dport:%hu",
                 nat, dgram_rx, dgram_tx, sip, dip, ipdst, sport, dport);
        }
      rpct_send_sigdiag_msg(llid, 0, buf);
      }
    else if (sscanf(line,
    "nat_proxy_icmp_req %s ipdst:%X ident:%hu seq:%hu",
    nat, &ipdst, &ident, &seq) == 4)
      {
      if (strcmp(nat, cur->nat))
        KERR("ERROR %s %s", nat, cur->nat);
      else
        nat_icmp_req(cur, ipdst, ident, seq);
      }
    else
      KERR("ERROR %s", line);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_main_kil_req(void)
{
  t_ctx_nat *next, *cur = g_head_ctx_nat;
  while(cur)
    {
    next = cur->next;
    delete_nat_ctx(cur);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_ctx_nat *get_ctx_nat(char *nat)
{
  return ctx_nat_find(nat);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void nat_main_init(void)
{
  g_head_ctx_nat = NULL;
  nat_utils_init();
  nat_udp_init();
  nat_icmp_init();
  nat_tcp_init();
}
/*--------------------------------------------------------------------------*/
