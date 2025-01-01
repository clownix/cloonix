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
#include <time.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "proxy_crun.h"
#include "util_sock.h"

static char g_tmux_crun[MAX_PATH_LEN];
static char g_tmux_host[MAX_PATH_LEN];

typedef struct t_peer_llid
{
  int llid_front;
  int llid_back;
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
    if ((cur->llid_front == llid) || (cur->llid_back == llid))
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/
 
/****************************************************************************/
static void free_peer(t_peer_llid *cur)
{
  if (msg_exist_channel(cur->llid_front))
    msg_delete_channel(cur->llid_front);
  if (msg_exist_channel(cur->llid_back))
    msg_delete_channel(cur->llid_back);
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
static void alloc_peer(int llid_front, int llid_back)
{
  t_peer_llid *cur;
  if ((llid_front <= 0) || (llid_front >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", llid_front, llid_back);
  if ((llid_back <= 0) || (llid_back >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", llid_front, llid_back);
  cur = find_peer(llid_front);
  if (cur)
    KOUT("ERROR %d %d", cur->llid_front, cur->llid_back);
  cur = find_peer(llid_back);
  if (cur)
    KOUT("ERROR %d %d", cur->llid_front, cur->llid_back);
  cur = (t_peer_llid *) malloc(sizeof(t_peer_llid));
  memset(cur, 0, sizeof(t_peer_llid));
  cur->llid_front = llid_front;
  cur->llid_back = llid_back;
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
  else if (llid == cur->llid_front)
    proxy_traf_unix_tx(cur->llid_back, len, buf);
  else if (llid == cur->llid_back)
    proxy_traf_unix_tx(cur->llid_front, len, buf);
  else
    KOUT("ERROR %d", llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_tmux_client(int llid_listen, int llid_front)
{ 
  int llid_back;
  llid_back = proxy_traf_unix_client(g_tmux_crun, err_cb, rx_cb_pweb);
  if (llid_back > 0)
    {
    msg_mngt_set_callbacks (llid_front, err_cb, rx_cb_pweb);
    alloc_peer(llid_front, llid_back);
    channel_rx_local_flow_ctrl(llid_front, 0);
    }
  else
    {
    KERR("ERROR %s", g_tmux_crun);
    if (msg_exist_channel(llid_front))
      msg_delete_channel(llid_front);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_retry_init(void *data)
{
  int init_done = 0;
  struct stat stat_file;
  if (!stat(g_tmux_crun, &stat_file))
    {
    if (S_ISSOCK(stat_file.st_mode))
      {
      init_done = 1;
      if (!proxy_traf_unix_server(g_tmux_host, connect_from_tmux_client))
        KOUT("ERROR %s", g_tmux_host);
KERR("OOOOOOOOOOOOOOOOO %s %s", g_tmux_crun, g_tmux_host);
      }
    }
  if (init_done == 0)
    clownix_timeout_add(100, timer_retry_init, NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void tmux_init(void)
{ 
  g_head_peer_llid = NULL;
  memset(g_tmux_crun, 0, MAX_PATH_LEN);
  memset(g_tmux_host, 0, MAX_PATH_LEN);
  snprintf(g_tmux_host, MAX_PATH_LEN-1, "%s/tmux_front", get_proxyshare());
  snprintf(g_tmux_crun, MAX_PATH_LEN-1, "%s/tmux_back", get_proxyshare());
  unlink(g_tmux_host);
  clownix_timeout_add(100, timer_retry_init, NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/
     

