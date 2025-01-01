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

/****************************************************************************/
static void proxy_peer_data_to_crun(char *name, int len, char *buf)
{
  char *ptrs, *ptre;
  if ((!buf) || (strlen(buf) == 0))
   KERR("ERROR %d", len);
  else
    {
    ptrs = strstr(buf, "proxy_is_on_data_start:");
    ptre = strstr(buf, ":proxy_is_on_data_end");
    if ((!ptrs) || (!ptre))
      KERR("ERROR %s", buf);
    else
      {
      ptrs = ptrs + strlen("proxy_is_on_data_start:");
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
    KERR("ERROR %d %d", err, from);
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
  if ((!buf) || (strlen(buf) == 0))
    KERR("ERROR %d", len);
  else
    {
    if (!strcmp(buf, "transcrun_proxy_heartbeat_req"))
      proxy_sig_tx(g_llid_proxy_sig, strlen(beat_resp)+1, beat_resp);
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
void proxycrun_transmit_proxy_is_on_data(char *name, int len, char *buf)
{
  char sbuf[5*MAX_PATH_LEN];
  int maxlen = 5*MAX_PATH_LEN-1;
  memset(sbuf, 0, 5*MAX_PATH_LEN);
  snprintf(sbuf, maxlen, "transcrun_proxy_data_from_crun %s %d "
           "proxy_is_on_data_start:%s:proxy_is_on_data_end",
           name, len, buf); 
  if (!msg_exist_channel(g_llid_proxy_sig))
    KERR("ERROR llid %d", g_llid_proxy_sig);
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
  proxy_sig_tx(g_llid_proxy_sig, strlen(buf)+1, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int proxycrun_transmit_config(char *net, int pmain, int pweb, char *passwd)
{
  char buf[MAX_PATH_LEN];
  int llid, result = 0;
  char tmpnet[MAX_NAME_LEN];
  if (!lib_io_proxy_is_on(tmpnet))
    KERR("ERROR IMPOSSIBLE %s %d %d", net, pmain, pweb);
  else if (strcmp(net, tmpnet))
    KERR("ERROR IMPOSSIBLE %s %d %d %s", net, pmain, pweb, tmpnet);
  else
    {
    llid = proxy_sig_client(g_proxy_sig_unix, proxy_sig_err_cb, proxy_sig_rx_cb);
    if (llid == 0)
      KERR("ERROR %s %d %d", net, pmain, pweb);
    else
      {
      g_llid_proxy_sig = llid;
      llid_trace_alloc(g_llid_proxy_sig,"CLOON",0,0,type_llid_trace_unix_proxy);
      memset(buf, 0, MAX_PATH_LEN); 
      snprintf(buf, MAX_PATH_LEN-1,
               "transcrun_set_config %s %d %d %s",
               net, pmain, pweb, passwd);
      if (!msg_exist_channel(g_llid_proxy_sig))
        KERR("ERROR llid %d", g_llid_proxy_sig);
      proxy_sig_tx(g_llid_proxy_sig, strlen(buf)+1, buf);
      result = 1; 
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void proxycrun_init(void)
{
  g_llid_proxy_sig = 0;
  memset(g_proxy_sig_unix, 0, MAX_PATH_LEN-1);
  snprintf(g_proxy_sig_unix, MAX_PATH_LEN-1,
           "%s/proxy_sig_stream.sock", PROXYSHARE);
}
/*--------------------------------------------------------------------------*/
