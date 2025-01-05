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
  int llid_front;
  int llid_back;
  struct t_X_llid *prev;
  struct t_X_llid *next;
} t_X_llid;
/*--------------------------------------------------------------------------*/
static t_X_llid *g_head_X_llid;
static char g_wayland_main[MAX_PATH_LEN];
static char g_wayland_slave[MAX_PATH_LEN];
static char g_x11_main[MAX_PATH_LEN];
static char g_x11_slave[MAX_PATH_LEN];
static uint16_t g_forward_port_display;
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
static void alloc_X(int llid_front, int llid_back)
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
    proxy_traf_unix_tx(cur->llid_back, len, buf);
  else if (llid == cur->llid_back)
    proxy_traf_unix_tx(cur->llid_front, len, buf);
  else
    KOUT("ERROR %d", llid);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
//VIPTODO
//static void connect_from_wayland_client(int llid, int llid_back)
//{ 
//  int llid_front;
//  llid_front = proxy_traf_unix_client(g_wayland_main, err_cb, rx_cb);
//  if (llid_front > 0)
//    {
//    msg_mngt_set_callbacks (llid_back, err_cb, rx_cb);
//    alloc_X(llid_front, llid_back);
//    channel_rx_local_flow_ctrl(llid_back, 0);
//    }
//  else
//    {
//    KERR("ERROR wayland");
//    if (msg_exist_channel(llid_back))
//      msg_delete_channel(llid_back);
//    }
//}
///*--------------------------------------------------------------------------*/

/****************************************************************************/
static void connect_from_x11_client(int llid, int llid_back)
{ 
  int llid_front;
  llid_front = proxy_traf_unix_client(g_x11_main, err_cb, rx_cb);
  if (llid_front > 0)
    {
    msg_mngt_set_callbacks (llid_back, err_cb, rx_cb);
    alloc_X(llid_front, llid_back);
    channel_rx_local_flow_ctrl(llid_back, 0);
    } 
  else
    {
    KERR("ERROR x11");
    if (msg_exist_channel(llid_back))
      msg_delete_channel(llid_back);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void X_init(char *proxyshare)
{
  char *display = getenv("DISPLAY");
  char *wayland_display = getenv("WAYLAND_DISPLAY");
  char *xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  int val;

  g_forward_port_display = 0;
  g_head_X_llid = NULL;
  memset(g_wayland_main, 0, MAX_PATH_LEN);
  memset(g_wayland_slave, 0, MAX_PATH_LEN);
  memset(g_x11_main, 0, MAX_PATH_LEN);
  memset(g_x11_slave, 0, MAX_PATH_LEN);
  if ((wayland_display) && (xdg_runtime_dir))
    {
    KERR("WAYLAND_DISPLAY=%s", wayland_display);
    KERR("XDG_RUNTIME_DIR=%s", xdg_runtime_dir);
    KERR("WAYLAND NOT SUPPORTED, X11 SHOULD WORK ANYHOW");
    //VIPTODO
    //snprintf(g_wayland_slave, MAX_PATH_LEN-1, "%s/wayland-0", proxyshare);
    //snprintf(g_wayland_main, MAX_PATH_LEN-1, "%s/%s",
    //         xdg_runtime_dir, wayland_display);
    }
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
      g_forward_port_display = 6000 + val;
      KERR("ERROR NOT YET IMPLEMENTED :%s:", display);
      }
    else
      KERR("ERROR GETENV DISPLAY %s", display);
    }
  else
    KERR("ERROR DISPLAY");

  if ((g_wayland_slave[0]==0) && (g_x11_slave[0]==0))
    KOUT("ERROR DISPLAY");
  //VIPTODO
  //if (g_wayland_slave[0])
  //  {
  //  if (!proxy_traf_unix_server(g_wayland_slave, connect_from_wayland_client))
  //    KOUT("ERROR %s", g_wayland_slave);
  //  }
  if (g_x11_slave[0])
    {
    if (!proxy_traf_unix_server(g_x11_slave, connect_from_x11_client))
      KOUT("ERROR %s", g_x11_slave);
    }
}
/*--------------------------------------------------------------------------*/
