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
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/un.h>



#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "cfg_store.h"
#include "proxycrun.h"
#include "llid_trace.h"
#include "util_sock.h"
#include "commun_daemon.h"
#include "ovs_c2c.h"
#include "c2c_peer.h"


static int g_llid_proxy_sig;
static char g_proxy_sig_unix[MAX_PATH_LEN];
static char g_passwd[MSG_DIGEST_LEN];
static int g_port_web;
static int g_connect_proxy_ok;

int get_glob_req_self_destruction(void);


/****************************************************************************/
static void proxy_peer_data_to_crun(char *name, int len, char *buf)
{
  char *ptrs, *ptre;
  if ((!buf) || (strlen(buf) == 0))
    KERR("ERROR %d", len);
  else
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
        if (doors_io_basic_decoder(0, len, ptrs))
          KERR("ERROR %s %d %s", name, len, ptrs);
        }
      }
    }

}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void proxy_sig_err_cb (int llid, int err, int from)
{
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  if (!get_glob_req_self_destruction())
    {
    KERR("ERROR SIGCRUN DISCONNECT TO PROXYMOUS %d %d %d", llid, err, from);
    recv_kill_uml_clownix(0, 0);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void proxy_sig_rx_cb(int llid, int len, char *buf)
{
  char name[MAX_NAME_LEN];
  uint16_t udp_port, tcp_port;
  uint32_t ip;
  int inlen, pair_llid;
  char *beat_resp = "transcrun_proxy_heartbeat_resp";
  if (get_glob_req_self_destruction())
    return;
  if ((!buf) || (strlen(buf) == 0))
    KERR("ERROR %d", len);
  else if (!msg_exist_channel(g_llid_proxy_sig))
    KERR("ERROR llid %d", g_llid_proxy_sig);
  else if (llid != g_llid_proxy_sig)
    KERR("ERROR llid %d %d", g_llid_proxy_sig, llid);
  else
    {
    if (!strcmp(buf, "transcrun_proxy_heartbeat_req"))
      {
      g_connect_proxy_ok = 1;
      proxy_sig_tx(g_llid_proxy_sig, strlen(beat_resp)+1, beat_resp);
      }
    else if (sscanf(buf,
    "transcrun_req_udp_KO %s", name) == 1)
      KERR("ERROR %s", buf);
    else if (sscanf(buf,
    "transcrun_req_udp_OK %s %hu", name, &udp_port) == 2)
      ovs_c2c_transmit_get_free_udp_port(name, udp_port);
    else if (sscanf(buf,
    "transcrun_dist_udp_ip_port_OK %s %x %hu", name, &ip, &udp_port) == 3)
      ovs_c2c_proxy_dist_udp_ip_port_OK(name, ip, udp_port);
    else if (sscanf(buf,
    "transcrun_dist_tcp_ip_port_OK %s %x %hu", name, &ip, &tcp_port) == 3)
      ovs_c2c_proxy_dist_tcp_ip_port_OK(name, ip, tcp_port);
    else if (sscanf(buf,
    "transcrun_proxy_callback_connect %s %d", name, &pair_llid) == 2)
      wrap_proxy_callback_connect(name, pair_llid);
    else if (sscanf(buf,
    "transcrun_proxy_peer_data_to_crun %s %d", name, &inlen) == 2)
      proxy_peer_data_to_crun(name, inlen, buf);
    else
      KERR("ERROR %s", buf);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void proxycrun_transmit_proxy_data(char *name, int len, char *buf)
{
  char sbuf[5*MAX_PATH_LEN];
  int maxlen = 5*MAX_PATH_LEN-1;
  memset(sbuf, 0, 5*MAX_PATH_LEN);
  snprintf(sbuf, maxlen, "transcrun_proxy_data_from_crun %s %d %s%s%s",
           name, len, PROXYMARKUP_START, buf, PROXYMARKUP_END); 
  if (!msg_exist_channel(g_llid_proxy_sig))
    KERR("ERROR llid %d", g_llid_proxy_sig);
  else
    proxy_sig_tx(g_llid_proxy_sig, strlen(sbuf)+1, sbuf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void proxycrun_transmit_dist_tcp_ip_port(char *name, uint32_t ip,
                                         uint16_t port, char *passwd)
{
  char buf[MAX_PATH_LEN];
  memset(buf, 0, MAX_PATH_LEN);
  snprintf(buf, MAX_PATH_LEN-1,
  "transcrun_dist_tcp_ip_port %s %x %hu %s", name, ip, port, passwd);
  if (!msg_exist_channel(g_llid_proxy_sig))
    KERR("ERROR llid %d", g_llid_proxy_sig);
  else
    proxy_sig_tx(g_llid_proxy_sig, strlen(buf)+1, buf);
} 
/*--------------------------------------------------------------------------*/
  
/****************************************************************************/
void proxycrun_transmit_dist_udp_ip_port(char *name,
                                         uint32_t ip, uint16_t port)
{
  char buf[MAX_PATH_LEN];
  memset(buf, 0, MAX_PATH_LEN); 
  snprintf(buf, MAX_PATH_LEN-1,
  "transcrun_dist_udp_ip_port %s %x %hu", name, ip, port);
  if (!msg_exist_channel(g_llid_proxy_sig))
    KERR("ERROR llid %d", g_llid_proxy_sig);
  else
    proxy_sig_tx(g_llid_proxy_sig, strlen(buf)+1, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void proxycrun_transmit_req_udp(char *name)
{
  char buf[MAX_PATH_LEN];
  memset(buf, 0, MAX_PATH_LEN); 
  snprintf(buf, MAX_PATH_LEN-1, "transcrun_req_udp %s", name);
  if (!msg_exist_channel(g_llid_proxy_sig))
    KERR("ERROR llid %d", g_llid_proxy_sig);
  else
    proxy_sig_tx(g_llid_proxy_sig, strlen(buf)+1, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void proxycrun_transmit_req_nat(char *name, int on)
{
  char buf[MAX_PATH_LEN];
  memset(buf, 0, MAX_PATH_LEN);
  if (on)
    snprintf(buf, MAX_PATH_LEN-1, "transcrun_req_nat_on %s", name);
  else
    snprintf(buf, MAX_PATH_LEN-1, "transcrun_req_nat_off %s", name);
  if (!msg_exist_channel(g_llid_proxy_sig))
    KERR("ERROR llid %d", g_llid_proxy_sig);
  else
    proxy_sig_tx(g_llid_proxy_sig, strlen(buf)+1, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int proxycrun_transmit_config(int pweb, char *passwd)
{
  char buf[MAX_PATH_LEN];
  int llid, result = -1;
  char *net = cfg_get_cloonix_name();
  int pmain = cfg_get_server_port();
  llid = proxy_sig_client(g_proxy_sig_unix, proxy_sig_err_cb, proxy_sig_rx_cb);
  if (llid)
    {
    g_llid_proxy_sig = llid;
    llid_trace_alloc(g_llid_proxy_sig,"CLOON",0,0,type_llid_trace_unix_proxy);
    memset(buf, 0, MAX_PATH_LEN); 
    snprintf(buf, MAX_PATH_LEN-1, "transcrun_set_config %s %d %d %s",
             net, pmain, pweb, passwd);
    proxy_sig_tx(g_llid_proxy_sig, strlen(buf)+1, buf);
    result = 0; 
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_proxycrun(void *data)
{
  static int count = 0;
  if (proxycrun_transmit_config(g_port_web, g_passwd))
    {
    clownix_timeout_add(50, timer_proxycrun, NULL, NULL, NULL);
    count += 1;
    if (count > 20)
      {
      KERR("ERROR IMPOSSIBLE SIGCRUN CONNECTION TO PROXYMOUS");
      if (!get_glob_req_self_destruction())
        recv_kill_uml_clownix(0, 0);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int proxycrun_connect_proxy_ok(void)
{
  return g_connect_proxy_ok;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_kill_proxycrun(void *data)
{
  if ((g_llid_proxy_sig) && (msg_exist_channel(g_llid_proxy_sig)))
    {
    msg_delete_channel(g_llid_proxy_sig);
    g_llid_proxy_sig = 0;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void proxycrun_kill(void)
{
  if ((g_llid_proxy_sig) && (msg_exist_channel(g_llid_proxy_sig)))
    {
    channel_rx_local_flow_ctrl(g_llid_proxy_sig, 1);
    channel_tx_local_flow_ctrl(g_llid_proxy_sig, 1);
    clownix_timeout_add(50, timer_kill_proxycrun, NULL, NULL, NULL);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void proxycrun_init(int pweb, char *passwd)
{
  char *net = cfg_get_cloonix_name();
  g_connect_proxy_ok = 0;
  g_llid_proxy_sig = 0;
  memset(g_proxy_sig_unix, 0, MAX_PATH_LEN-1);
  snprintf(g_proxy_sig_unix, MAX_PATH_LEN-1,
           "%s_%s/proxymous_sig_stream.sock", PROXYSHARE_IN, net);
  memset(g_passwd, 0, MSG_DIGEST_LEN);
  strncpy(g_passwd, passwd, MSG_DIGEST_LEN-1);
  g_port_web = pweb;
  clownix_timeout_add(10, timer_proxycrun, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
