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

typedef struct t_peer_llid
{
  int llid_tcp_front;
  int llid_unix_back;
  int llid_listen_dido;
  int llid_dido_back;
  int llid_dido_front;
  struct t_peer_llid *prev;
  struct t_peer_llid *next;
} t_peer_llid;

static t_peer_llid *g_head_peer_llid;

static int g_listen_dido_llid_active;
static int g_listen_dido_llid_stale;

static void timer_del_listen_dido_llid(void *data);

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
static t_peer_llid *find_peer_with_listen_dido(int llid)
{
  t_peer_llid *cur = g_head_peer_llid;
  while(cur)
    {
    if (cur->llid_listen_dido == llid)
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
static void clean_doors_llid(int listen_llid, int dido_llid)
{
  if (msg_exist_channel(listen_llid))
    msg_delete_channel(listen_llid);
  if (msg_exist_channel(dido_llid))
    msg_delete_channel(dido_llid);
  if (g_listen_dido_llid_active)
    {
    if (msg_exist_channel(g_listen_dido_llid_active))
      msg_delete_channel(g_listen_dido_llid_active);
    }
  g_listen_dido_llid_active = 0;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void doors_llid(int listen_llid,  int dido_llid)
{
  t_peer_llid *cur;
  if ((dido_llid < 1) || (dido_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", dido_llid);
  if (g_listen_dido_llid_active == 0)
    {
    KERR("ERROR: %d %d", listen_llid, dido_llid);
    clean_doors_llid(listen_llid, dido_llid);
    }
  else if (listen_llid != g_listen_dido_llid_active)
    {
    KERR("ERROR: %d %d %d", g_listen_dido_llid_active,
                            listen_llid, dido_llid);
    clean_doors_llid(listen_llid, dido_llid);
    }
  else
    {
    cur = find_peer_with_listen_dido(g_listen_dido_llid_active);
    if (!cur)
      {
      KERR("ERROR: %d %d %d", g_listen_dido_llid_active,
                              listen_llid, dido_llid);
      clean_doors_llid(listen_llid, dido_llid);
      }
    else if (cur->llid_dido_back)
      {
      KERR("ERROR: %d %d %d", g_listen_dido_llid_active,
                              listen_llid, dido_llid);
      clean_doors_llid(listen_llid, dido_llid);
      }
    else
      {
      if (msg_exist_channel(g_listen_dido_llid_active))
        msg_delete_channel(g_listen_dido_llid_active);
      cur->llid_dido_back = dido_llid;
      if (g_listen_dido_llid_stale)
        KERR("ERROR");
      g_listen_dido_llid_stale = g_listen_dido_llid_active;
      g_listen_dido_llid_active = 0;
      channel_rx_local_flow_ctrl(cur->llid_tcp_front, 0);
      }
    }
  clownix_timeout_add(10, timer_del_listen_dido_llid, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void doors_end(int dido_llid)
{
  KERR("doors_end %d", dido_llid);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void doorways_rx_bufraw(int dido_llid, int tid,
                               int len_bufraw, char *doors_bufraw,
                               int type, int val, int len, char *buf)
{
  t_peer_llid *cur;
  cur = find_peer_with_dido(dido_llid);
  if (cur == NULL)
    KERR("ERROR doors_llid not found llid: %d %d %s", dido_llid, len, buf);
  else
    {
    switch (type)
      {
      case doors_type_switch:
        if ((val == doors_val_init_link) && (!strcmp(buf, "OK")))
          {
          proxy_traf_unix_tx(cur->llid_unix_back, len_bufraw, doors_bufraw);
          }
        else if ((val == doors_val_none) || (val == doors_val_c2c))
          proxy_traf_unix_tx(cur->llid_unix_back, len_bufraw, doors_bufraw);
        else
          KERR("WARNING %d %d %s", type, val, buf);
      break;
      case doors_type_xwy_main_traf:
      case doors_type_xwy_x11_flow:
        if ((val == doors_val_init_link) && (!strcmp(buf, "OK")))
          {
          proxy_traf_unix_tx(cur->llid_unix_back, len_bufraw, doors_bufraw);
          }
        else if (val == doors_val_xwy)
          proxy_traf_unix_tx(cur->llid_unix_back, len_bufraw, doors_bufraw);
        else
          KERR("WARNING %d %d %s", type, val, buf);
      break;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_del_listen_dido_llid(void *data)
{ 
  if (g_listen_dido_llid_stale)
    {
    if (msg_exist_channel(g_listen_dido_llid_stale))
      msg_delete_channel(g_listen_dido_llid_stale);
    g_listen_dido_llid_stale = 0;
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
                       int llid_listen_dido, int llid_dido_front)
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
  cur->llid_tcp_front = llid_tcp_front;
  cur->llid_unix_back = llid_unix_back;
  cur->llid_listen_dido = llid_listen_dido;
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
static void connect_from_pmain_tcp_client(int llid_listen, int llid_tcp_front)
{ 
  int llid_dido_front, llid_unix_back;
  if (g_listen_dido_llid_active)
    {
    KERR("WARNING");
    return;
    }
  llid_unix_back  = proxy_traf_unix_client(g_pmain_unix, err_cb, rx_cb);
  g_listen_dido_llid_active = doorways_sock_server_proxy(g_net,
                              g_pdoors_unix, g_passwd, doors_llid,
                              doors_end, doorways_rx_bufraw);
  if (g_listen_dido_llid_active == 0)
    {
    KERR("WARNING");
    if (msg_exist_channel(llid_tcp_front))
      msg_delete_channel(llid_tcp_front);
    }
  else if (llid_unix_back == 0)
    {
    KERR("WARNING");
    if (msg_exist_channel(g_listen_dido_llid_active))
      msg_delete_channel(g_listen_dido_llid_active);
    if (msg_exist_channel(llid_tcp_front))
      msg_delete_channel(llid_tcp_front);
    g_listen_dido_llid_active = 0;
    }
  else
    {
    llid_dido_front = proxy_traf_unix_client(g_pdoors_unix, doors_err, doors_rx);
    if (llid_dido_front == 0)
      {
      KERR("WARNING");
      if (msg_exist_channel(g_listen_dido_llid_active))
        msg_delete_channel(g_listen_dido_llid_active);
      if (msg_exist_channel(llid_tcp_front))
        msg_delete_channel(llid_tcp_front);
      if (msg_exist_channel(llid_unix_back))
        msg_delete_channel(llid_unix_back);
      g_listen_dido_llid_active = 0;
      }
    else
      {
      msg_mngt_set_callbacks (llid_tcp_front, err_cb, rx_cb);
      alloc_peer(llid_tcp_front, llid_unix_back,
                 g_listen_dido_llid_active, llid_dido_front);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int set_config(char *net, uint16_t pmain, uint16_t pweb, char *passwd)
{ 
  int result = -1;
  g_listen_dido_llid_active = 0;
  set_cloonix_name(net);
  g_head_peer_llid = NULL;
  memset(g_pdoors_unix, 0, MAX_PATH_LEN);
  memset(g_pmain_unix, 0, MAX_PATH_LEN);
  memset(g_passwd, 0, MSG_DIGEST_LEN);
  memset(g_net, 0, MAX_NAME_LEN);
  strncpy(g_net, net, MAX_NAME_LEN-1);
  strncpy(g_passwd, passwd, MSG_DIGEST_LEN-1);
  snprintf(g_pdoors_unix, MAX_PATH_LEN-1,
           "%s/proxy_%s_pdoors.sock", get_proxyshare(), net);
  snprintf(g_pmain_unix, MAX_PATH_LEN-1,
           "%s/proxy_%s_pmain.sock", get_proxyshare(), net);
  if (!proxy_traf_tcp_server(pmain, connect_from_pmain_tcp_client))
    KERR("ERROR cannot listen to port %hu", pmain);
  else
    result = 0;
  pweb_init(net, pweb);
  return result;
}
/*--------------------------------------------------------------------------*/
