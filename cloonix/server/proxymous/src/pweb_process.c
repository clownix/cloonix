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

static char g_pweb_unix[MAX_PATH_LEN];

typedef struct t_peer_llid
{
  int llid_tcp_front;
  int llid_unix_back;
  struct t_peer_llid *prev;
  struct t_peer_llid *next;
} t_peer_llid;

static t_peer_llid *g_head_peer_llid;

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
static void free_peer(t_peer_llid *cur)
{
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
static void alloc_peer(int llid_tcp_front, int llid_unix_back)
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
  cur->llid_tcp_front = llid_tcp_front;
  cur->llid_unix_back = llid_unix_back;
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
static void err_cb(int llid, int err, int from)
{     
  peer_broken(llid);
}     
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_cb_pweb (int llid, int len, char *buf)
{
  t_peer_llid *cur;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d", llid);
  cur = find_peer(llid);
  if (!cur)
    KERR("ERROR %d", llid);
  else if (llid == cur->llid_tcp_front)
    proxy_traf_unix_tx(cur->llid_unix_back, len, buf);
  else if (llid == cur->llid_unix_back)
    proxy_traf_tcp_tx(cur->llid_tcp_front, len, buf);
  else
    KOUT("ERROR %d", llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_pweb_tcp_client(int llid_listen, int llid_tcp_front)
{ 
  int llid_unix_back;
  llid_unix_back = proxy_traf_unix_client(g_pweb_unix, err_cb, rx_cb_pweb);
  if (llid_unix_back > 0)
    {
    msg_mngt_set_callbacks (llid_tcp_front, err_cb, rx_cb_pweb);
    alloc_peer(llid_tcp_front, llid_unix_back);
    channel_rx_local_flow_ctrl(llid_tcp_front, 0);
    }
  else
    {
    KERR("ERROR %s", g_pweb_unix);
    if (msg_exist_channel(llid_tcp_front))
      msg_delete_channel(llid_tcp_front);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void pweb_init(char *net, uint16_t pweb)
{ 
  char *pox = get_proxyshare();
  g_head_peer_llid = NULL;
  memset(g_pweb_unix, 0, MAX_PATH_LEN);
  snprintf(g_pweb_unix, MAX_PATH_LEN-1, "%s/proxy_pweb.sock", pox);
  if (!proxy_traf_tcp_server(pweb, connect_from_pweb_tcp_client))
    KERR("ERROR PROXYMOUS cannot listen to pweb port %hu", pweb);
}
/*--------------------------------------------------------------------------*/
