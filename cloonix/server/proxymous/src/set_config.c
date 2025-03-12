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

static char g_pdoors_unix[MAX_PATH_LEN];
static char g_pmain_unix[MAX_PATH_LEN];
static char g_passwd[MSG_DIGEST_LEN];
static char g_net[MAX_NAME_LEN];

typedef struct t_chain_connect
{
  int llid_tcp_front;
  struct t_chain_connect *next;
} t_chain_connect;

typedef struct t_peer_llid
{
  int llid_tcp_front;
  int llid_unix_back;
  int llid_dido_back;
  int llid_dido_front;
  struct t_peer_llid *prev;
  struct t_peer_llid *next;
} t_peer_llid;

static t_peer_llid *g_head_peer_llid;
static int g_listen_dido_llid;
static t_peer_llid *g_current_peer_llid;
static t_chain_connect *g_head_chain_connect;


/****************************************************************************/
static t_peer_llid *find_peer(int llid)
{
  t_peer_llid *cur = g_head_peer_llid;
  while(cur)
    {
    if ((cur->llid_tcp_front == llid) || (cur->llid_unix_back == llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
static t_peer_llid *find_peer_with_dido(int llid)
{
  t_peer_llid *cur = g_head_peer_llid;
  while(cur)
    {
    if (cur->llid_dido_back == llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static void doors_end(int dido_llid)
{
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void get_type_and_val(int type, int val, char *atype, char *aval,
                             int len, char *buf)
{
  memset(atype, 0, MAX_NAME_LEN);
  memset(aval, 0, MAX_NAME_LEN);
  switch (val)
    {
    case doors_val_c2c:
      strcpy(aval, "doors_val_c2c");
      break;
    case doors_val_none:
      strcpy(aval, "doors_val_none");
      break;
    case doors_val_xwy:
      strcpy(aval, "doors_val_xwy");
      break;
    case doors_val_sig:
      strcpy(aval, "doors_val_sig");
      break;
    case doors_val_init_link:
      strcpy(aval, "doors_val_init_link");
      break;
    case doors_val_link_ok:
      strcpy(aval, "doors_val_link_ok");
      break;
    case doors_val_link_ko:
      strcpy(aval, "doors_val_link_ko");
      break;
    default:
      if (buf)
        strncpy(aval, buf, MAX_NAME_LEN-1);
      else
        KOUT("ERROR TYPE VAL %d %d %d", type, val, len);
    }
  switch (type)
    {
    case doors_type_switch:
      strcpy(atype, "doors_type_switch");
      break;
    case doors_type_spice:
      strcpy(atype, "doors_type_spice");
      break;
    case doors_type_dbssh:
      strcpy(atype, "doors_type_dbssh");
      break;
    case doors_type_dbssh_x11_ctrl:
      strcpy(atype, "doors_type_dbssh_x11_ctrl");
      break;
    case doors_type_dbssh_x11_traf:
      strcpy(atype, "doors_type_dbssh_x11_traf");
      break;
    case doors_type_openssh:
      strcpy(atype, "doors_type_openssh");
      break;
    case doors_type_xwy_main_traf:
      strcpy(atype, "doors_type_xwy_main_traf");
      break;
    case doors_type_xwy_x11_flow:
      strcpy(atype, "doors_type_xwy_x11_flow");
      break;
    default:
      if (buf)
        KOUT("ERROR TYPE VAL %d %d %d %s", type, val, len, buf);
      KOUT("ERROR TYPE VAL %d %d %d", type, val, len);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void doorways_rx_bufraw(int dido_llid, int tid,
                               int len_bufraw, char *doors_bufraw,
                               int type, int val, int len, char *buf)
{
  t_peer_llid *cur;
  char atype[MAX_NAME_LEN];
  char aval[MAX_NAME_LEN];
  get_type_and_val(type, val, atype, aval, len, buf);
  cur = find_peer_with_dido(dido_llid);
  if (cur == NULL)
    KERR("ERROR doors_llid not found llid: %d %d %s", dido_llid, len, buf);
  else
    {
    switch (type)
      {

      case doors_type_switch:
      case doors_type_xwy_main_traf:
      case doors_type_xwy_x11_flow:
      case doors_type_dbssh:
      case doors_type_dbssh_x11_ctrl:
      case doors_type_dbssh_x11_traf:
      case doors_type_openssh:
      case doors_type_spice:
        proxy_traf_unix_tx(cur->llid_unix_back, len_bufraw, doors_bufraw);
      break;

      default:
        KOUT("IMPOSSIBLE %d", type);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_peer(t_peer_llid *cur)
{
  if (msg_exist_channel(cur->llid_dido_front))
    msg_delete_channel(cur->llid_dido_front);
  if (msg_exist_channel(cur->llid_dido_back))
    msg_delete_channel(cur->llid_dido_back);
  if (msg_exist_channel(cur->llid_tcp_front))
    msg_delete_channel(cur->llid_tcp_front);
  if (msg_exist_channel(cur->llid_unix_back))
    msg_delete_channel(cur->llid_unix_back);
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur == g_head_peer_llid)
    g_head_peer_llid = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_peer(int llid_tcp_front, int llid_unix_back,
                       int llid_dido_front, int llid_dido_back)
{
  t_peer_llid *cur;
  if ((llid_tcp_front <= 0) || (llid_tcp_front >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", llid_tcp_front, llid_unix_back);
  if ((llid_unix_back <= 0) || (llid_unix_back >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", llid_tcp_front, llid_unix_back);
  cur = find_peer(llid_tcp_front);
  if (cur)
    KOUT("ERROR %d %d", cur->llid_tcp_front, cur->llid_unix_back);
  cur = find_peer(llid_unix_back);
  if (cur)
    KOUT("ERROR %d %d", cur->llid_tcp_front, cur->llid_unix_back);
  cur = (t_peer_llid *) malloc(sizeof(t_peer_llid));
  memset(cur, 0, sizeof(t_peer_llid));
  cur->llid_dido_front = llid_dido_front;
  cur->llid_dido_back  = llid_dido_back;
  cur->llid_tcp_front  = llid_tcp_front;
  cur->llid_unix_back  = llid_unix_back;
  if (g_head_peer_llid)
    g_head_peer_llid->prev = cur;
  cur->next = g_head_peer_llid;
  g_head_peer_llid = cur; 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void peer_broken(int llid)
{
  t_peer_llid *cur;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d", llid);
  cur = find_peer(llid);
  if (!cur)
    KERR("ERROR %d", llid);
  else
    free_peer(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void doors_rx (int llid, int len, char *buf)
{
  KERR("doors_rx %s", buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void doors_err (int llid, int err, int from)
{
  KERR("doors_err %d %d", err, from);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_cb(int llid, int err, int from)
{     
  peer_broken(llid);
}     
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_cb(int llid, int len, char *buf)
{
  t_peer_llid *cur;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d", llid);
  cur = find_peer(llid);
  if (!cur)
    KERR("ERROR %d", llid);
  else if (llid == cur->llid_tcp_front)
    {
    if (cur->llid_dido_back == 0)
      KERR("ERROR doors_llid ZERO %s", buf);
    else
      proxy_traf_unix_tx(cur->llid_dido_front, len, buf);
    }
  else if (llid == cur->llid_unix_back)
    proxy_traf_tcp_tx(cur->llid_tcp_front, len, buf);
  else
    KOUT("ERROR %d", llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void doors_llid(int listen_dido_llid,  int dido_llid)
{   
  t_peer_llid *cur = g_current_peer_llid;
  if (!cur)
    {
    KERR("ERROR: %d %d", listen_dido_llid, dido_llid);
    }
  else
    {
    if (listen_dido_llid != g_listen_dido_llid)
      KERR("ERROR: %d %d %d", g_listen_dido_llid, listen_dido_llid, dido_llid);
    else
      { 
      alloc_peer(cur->llid_tcp_front, cur->llid_unix_back,
                 cur->llid_dido_front, dido_llid);
      msg_mngt_set_callbacks (cur->llid_tcp_front, err_cb, rx_cb);
      channel_rx_local_flow_ctrl(cur->llid_tcp_front, 0);
      channel_rx_local_flow_ctrl(cur->llid_unix_back, 0);
      channel_rx_local_flow_ctrl(cur->llid_dido_front, 0);
      channel_rx_local_flow_ctrl(dido_llid, 0);
      }
    free(g_current_peer_llid);
    g_current_peer_llid = NULL;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_pmain_tcp_client(int llid_listen, int llid_tcp_front)
{ 
  t_chain_connect *last, *next, *cur;
  channel_rx_local_flow_ctrl(llid_tcp_front, 1);
  cur = (t_chain_connect *) malloc(sizeof(t_chain_connect));
  memset(cur, 0, sizeof(t_chain_connect));
  cur->llid_tcp_front = llid_tcp_front;
  if (g_head_chain_connect == NULL)
    {
    g_head_chain_connect = cur;
    }
  else
    {
    last = g_head_chain_connect;
    next = last->next;
    while(next) 
      {
      last = next;
      next = last->next;
      }
    last->next = cur;
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
/*
static int gettimeofday_delta(void)
{ 
  static struct timeval last;
  struct timespec ts;
  struct timeval cur;
  int delta = 0;
  if (clock_gettime(CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  cur.tv_sec = ts.tv_sec;
  cur.tv_usec = ts.tv_nsec/1000;
  delta = (cur.tv_sec - last.tv_sec)*10000;
  delta += cur.tv_usec/100;
  delta -= last.tv_usec/100;
  last.tv_sec = cur.tv_sec;
  last.tv_usec = cur.tv_usec;
  return (delta/10);
}
*/
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void set_config_rapid_beat(void)
{
  int llid_dido_front, llid_unix_back;
  t_chain_connect *cur;
  if ((g_current_peer_llid == NULL) && (g_head_chain_connect != NULL))
    {
    cur = g_head_chain_connect;
    g_head_chain_connect = cur->next;
//    KERR("DELTA: %dms", gettimeofday_delta());
    g_current_peer_llid = (t_peer_llid *) malloc(sizeof(t_peer_llid));
    memset(g_current_peer_llid, 0, sizeof(t_peer_llid));
    llid_unix_back  = proxy_traf_unix_client(g_pmain_unix, err_cb, rx_cb);
    if (llid_unix_back == 0)
      {
      KERR("WARNING");
      msg_delete_channel(cur->llid_tcp_front);
      free(g_current_peer_llid);
      g_current_peer_llid = NULL;
      }
    else
      {
      channel_rx_local_flow_ctrl(llid_unix_back, 1);
      llid_dido_front=proxy_traf_unix_client(g_pdoors_unix,doors_err,doors_rx);
      if (llid_dido_front == 0)
        {
        KERR("WARNING");
        msg_delete_channel(cur->llid_tcp_front);
        msg_delete_channel(llid_unix_back);
        free(g_current_peer_llid);
        g_current_peer_llid = NULL;
        }
      else
        {
        channel_rx_local_flow_ctrl(llid_dido_front, 1);
        g_current_peer_llid->llid_dido_front = llid_dido_front;
        g_current_peer_llid->llid_tcp_front  = cur->llid_tcp_front;
        g_current_peer_llid->llid_unix_back  = llid_unix_back;
        }
      }
    free(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int set_config(char *net, uint16_t pmain, uint16_t pweb, char *passwd)
{ 
  int result = -1;
  char *pox = get_proxyshare();
//  gettimeofday_delta();
  set_cloonix_name(net);
  g_head_peer_llid = NULL;
  g_current_peer_llid = NULL;
  g_head_chain_connect = NULL;
  memset(g_pdoors_unix, 0, MAX_PATH_LEN);
  memset(g_pmain_unix, 0, MAX_PATH_LEN);
  memset(g_passwd, 0, MSG_DIGEST_LEN);
  memset(g_net, 0, MAX_NAME_LEN);
  strncpy(g_net, net, MAX_NAME_LEN-1);
  strncpy(g_passwd, passwd, MSG_DIGEST_LEN-1);
  snprintf(g_pdoors_unix, MAX_PATH_LEN-1, "%s/proxy_pdoors.sock", pox);
  snprintf(g_pmain_unix, MAX_PATH_LEN-1, "%s/proxy_pmain.sock", pox);
  if (!proxy_traf_tcp_server(pmain, connect_from_pmain_tcp_client))
    KERR("ERROR PROXYMOUS (FATAL?) cannot listen to pmain port %hu", pmain);
  else
    {
    g_listen_dido_llid = doorways_sock_server_proxy(g_net, g_pdoors_unix,
                         g_passwd, doors_llid, doors_end, doorways_rx_bufraw);
    if (!g_listen_dido_llid)
      KERR("ERROR cannot listen to %s", g_pdoors_unix);
    else
      {
      pweb_init(net, pweb);
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/
