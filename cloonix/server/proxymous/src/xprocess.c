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

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "proxy_crun.h"
#include "util_sock.h"


typedef struct t_X_llid
{
  int is_ssh;
  int llid_front;
  int llid_back;
  struct t_X_llid *prev;
  struct t_X_llid *next;
} t_X_llid;
/*--------------------------------------------------------------------------*/
static t_X_llid *g_head_X_llid;
static char g_x11_main[MAX_PATH_LEN];
static char g_x11_slave[MAX_PATH_LEN];
static uint16_t g_port_display;
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_X_llid *find_X(int llid)
{
  t_X_llid *cur = g_head_X_llid;
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
static void free_X(t_X_llid *cur)
{
  if (msg_exist_channel(cur->llid_front))
    msg_delete_channel(cur->llid_front);
  if (msg_exist_channel(cur->llid_back))
    msg_delete_channel(cur->llid_back);
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur == g_head_X_llid)
    g_head_X_llid = cur->next;
  free(cur);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_X(int llid_front, int llid_back, int is_ssh)
{
  t_X_llid *cur;
  if ((llid_front <= 0) || (llid_front >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", llid_front, llid_back);
  if ((llid_back <= 0) || (llid_back >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d %d", llid_front, llid_back);
  cur = find_X(llid_front);
  if (cur)
    KOUT("ERROR %d %d", cur->llid_front, cur->llid_back);
  cur = find_X(llid_back);
  if (cur)
    KOUT("ERROR %d %d", cur->llid_front, cur->llid_back);
  cur = (t_X_llid *) malloc(sizeof(t_X_llid));
  memset(cur, 0, sizeof(t_X_llid));
  cur->is_ssh = is_ssh;
  cur->llid_front = llid_front;
  cur->llid_back = llid_back;
  if (g_head_X_llid)
    g_head_X_llid->prev = cur;
  cur->next = g_head_X_llid;
  g_head_X_llid = cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void err_cb (int llid, int err, int from)
{
  t_X_llid *cur = find_X(llid);
  if (!cur)
    KERR("ERROR X %d %d %d", llid, err, from);
  else
   free_X(cur);
} 
/*--------------------------------------------------------------------------*/
  
/****************************************************************************/
static void rx_cb (int llid, int len, char *buf)
{
  t_X_llid *cur;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("ERROR %d", llid);
  cur = find_X(llid);
  if (!cur)
    KERR("ERROR %d", llid);
  else if (llid == cur->llid_front)
    {
    proxy_traf_unix_tx(cur->llid_back, len, buf);
    }
  else if (llid == cur->llid_back)
    {
    if (cur->is_ssh)
      proxy_traf_tcp_tx(cur->llid_front, len, buf);
    else
      proxy_traf_unix_tx(cur->llid_front, len, buf);
    }
  else
    KOUT("ERROR %d", llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_x11_client(int llid, int llid_back)
{ 
  int llid_front;
  uint32_t ip;
  if (g_port_display)
    {
    ip_string_to_int(&ip, "127.0.0.1");
    llid_front = proxy_traf_tcp_client(ip, g_port_display, err_cb, rx_cb);
    }
  else
    {
    llid_front = proxy_traf_unix_client(g_x11_main, err_cb, rx_cb);
    }
  if (llid_front > 0)
    {
    msg_mngt_set_callbacks (llid_back, err_cb, rx_cb);
    if (g_port_display)
      alloc_X(llid_front, llid_back, 1);
    else
      alloc_X(llid_front, llid_back, 0);
    channel_rx_local_flow_ctrl(llid_back, 0);
    } 
  else
    {
    KERR("ERROR x11 TO LOOK INTO");
    if (msg_exist_channel(llid_back))
      msg_delete_channel(llid_back);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void X_init(char *proxyshare)
{
  char *display = getenv("DISPLAY");
  int val;

  g_port_display = 0;
  g_head_X_llid = NULL;
  memset(g_x11_main, 0, MAX_PATH_LEN);
  memset(g_x11_slave, 0, MAX_PATH_LEN);
  if (display)
    {
    if (display[0] == ':')
      {
      if (sscanf(display, ":%d", &val) == 1)
        {
        snprintf(g_x11_slave, MAX_PATH_LEN-1, "%s/X11-unix/X713", proxyshare);
        snprintf(g_x11_main, MAX_PATH_LEN-1, "/tmp/.X11-unix/X%d", val);
        }
      else
        KERR("ERROR :%s:", display);
      }
    else if (sscanf(display, "localhost:%d.0", &val) == 1)
      {
      printf("DISPLAY IS FOR SSH PORT %d", 6000 + val);
      snprintf(g_x11_slave, MAX_PATH_LEN-1, "%s/X11-unix/X713", proxyshare);
      g_port_display = 6000 + val;
      }
    else
      {
      printf("ERROR DISPLAY %s NOT IMPLEMENTED", display);
      KERR("ERROR GETENV DISPLAY %s", display);
      }
    }
  else
    {
    printf("ERROR DISPLAY NO DISPLAY FOUND");
    KERR("ERROR DISPLAY NO DISPLAY FOUND");
    }
  if (g_x11_slave[0] == 0)
    KERR("ERROR DISPLAY RUNNING WITHOUT X11 FORWARDING");
  else if (!proxy_traf_unix_server(g_x11_slave, connect_from_x11_client))
    KOUT("ERROR %s", g_x11_slave);
}
/*--------------------------------------------------------------------------*/
