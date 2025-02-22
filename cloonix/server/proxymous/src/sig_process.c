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
#include <time.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "proxy_crun.h"
#include "util_sock.h"
#include "c2c_peer.h"
#include "nat_main.h"

static int g_heartbeat_count;

/***************************************************************************/
void sig_process_heartbeat(void)
{
  char *beat_req = "transcrun_proxy_heartbeat_req";
  int llid_sig = get_llid_sig();
  if (llid_sig)
    {
    if (!msg_exist_channel(llid_sig))
      KOUT("ERROR");
    proxy_sig_tx(llid_sig, strlen(beat_req)+1, beat_req);
    if (g_heartbeat_count == 3)
      KOUT("ERROR");
    g_heartbeat_count += 1;
    }
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
void sig_process_proxy_peer_data_to_crun(char *name, int len, char *buf)
{
  char sbuf[5*MAX_PATH_LEN];
  int max = 5*MAX_PATH_LEN-1;
  int llid_sig = get_llid_sig();
  memset(sbuf, 0, 5*MAX_PATH_LEN);
  snprintf(sbuf, max,
  "transcrun_proxy_peer_data_to_crun %s %d %s%s%s",
  name, len, PROXYMARKUP_START, buf, PROXYMARKUP_END);
  if (!msg_exist_channel(llid_sig))
    KERR("ERROR llid %d", llid_sig);
  proxy_sig_tx(llid_sig, strlen(sbuf)+1, sbuf);
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
void sig_process_tx_proxy_callback_connect(char *name, int pair_llid)
{
  char buf[MAX_PATH_LEN];
  int max = MAX_PATH_LEN-1;
  int llid_sig = get_llid_sig();
  memset(buf, 0, MAX_PATH_LEN);
  snprintf(buf, max,
  "transcrun_proxy_callback_connect %s %d", name, pair_llid);
  if (!msg_exist_channel(llid_sig))
    KERR("ERROR llid %d", llid_sig);
  proxy_sig_tx(llid_sig, strlen(buf)+1, buf);
}
/*--------------------------------------------------------------------------*/

/***************************************************************************/
static void dist_udp_ip_port_ok(char *name, uint32_t ip, uint16_t udp_port)
{   
  char buf[MAX_PATH_LEN];
  int max = MAX_PATH_LEN-1;
  int llid_sig = get_llid_sig();
  memset(buf, 0, MAX_PATH_LEN);
  snprintf(buf, max,
  "transcrun_dist_udp_ip_port_OK %s %x %hu", name, ip, udp_port);
  if (!msg_exist_channel(llid_sig))
    KERR("ERROR llid %d", llid_sig);
  proxy_sig_tx(llid_sig, strlen(buf)+1, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void sig_process_rx(char *proxyshare, char *buf)
{
  char name[MAX_NAME_LEN];
  char passwd[MSG_DIGEST_LEN];
  uint16_t udp_port, tcp_port;
  uint32_t ip;
  int16_t pmain, pweb;
  char *ptrs, *ptre;
  char sbuf[MAX_PATH_LEN];
  int len, max = MAX_PATH_LEN-1;
  int llid_sig = get_llid_sig();

/*--------------------------------------------------------------------------*/
  if (!strcmp(buf, "transcrun_proxy_heartbeat_resp"))
    {
    g_heartbeat_count = 0;
    }
/*--------------------------------------------------------------------------*/
  else if (sscanf(buf,
  "transcrun_set_config %s %hd %hd %s", name, &pmain, &pweb, passwd)==4)
    {
    if (set_config(name, pmain, pweb, passwd))
      KOUT("ERROR %s", buf);
    }
/*--------------------------------------------------------------------------*/
  else if (sscanf(buf,
  "transcrun_req_nat_on %s", name) == 1)
    {
    if (nat_main_request(name, 1))
      KERR("ERROR %s", buf);
    }
/*--------------------------------------------------------------------------*/
  else if (sscanf(buf,
  "transcrun_req_nat_off %s", name) == 1)
    {
    if (nat_main_request(name, 0))
      KERR("ERROR %s", buf);
    }
/*--------------------------------------------------------------------------*/
  else if (sscanf(buf,
  "transcrun_dist_tcp_ip_port %s %x %hu %s",name,&ip,&tcp_port,passwd)==4)
    {
    if (c2c_peer_dist_tcp_ip_port(name, ip, tcp_port, passwd))
      KERR("ERROR %s", buf);
    }
/*--------------------------------------------------------------------------*/
  else if (sscanf(buf,
  "transcrun_proxy_data_from_crun %s %d", name, &len) == 2)
    {
    ptrs = strstr(buf, PROXYMARKUP_START);
    ptre = strstr(buf, PROXYMARKUP_END);
    if ((!ptrs) || (!ptre))
      KERR("ERROR %s", buf);
    else
      {
      ptrs = ptrs + strlen(PROXYMARKUP_START);
      ptre[0] = 0;
      if (strlen(ptrs) +1 != len)
        KERR("ERROR %s %lu %d", buf, strlen(ptrs), len);
      else
        {
        if (c2c_peer_proxy_data_from_crun(name, len, ptrs))
          KERR("ERROR %s", buf);
        }
      }
    }
/*--------------------------------------------------------------------------*/
  else if (sscanf(buf,
  "transcrun_req_udp %s", name) == 1)
    {
    memset(sbuf, 0, MAX_PATH_LEN);
    if (udp_proxy_init(proxyshare, &udp_port))
      snprintf(sbuf,max,"transcrun_req_udp_KO %s", name);
    else
      snprintf(sbuf,max,"transcrun_req_udp_OK %s %hu", name, udp_port);
    if (!msg_exist_channel(llid_sig))
      KERR("ERROR llid %d", llid_sig);
    proxy_sig_tx(llid_sig, strlen(sbuf)+1, sbuf);
    }
  else if (sscanf(buf,
  "transcrun_dist_udp_ip_port %s %x %hu", name, &ip, &udp_port) == 3)
    {
    udp_proxy_dist_udp_ip_port(ip, udp_port);
    dist_udp_ip_port_ok(name, ip, udp_port);
    }
  else
    KOUT("ERROR %s", buf);
}
/*--------------------------------------------------------------------------*/
