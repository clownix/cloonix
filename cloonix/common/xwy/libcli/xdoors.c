/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
/*--------------------------------------------------------------------------*/
#include "mdl.h"
#include "wrap.h"
#include "cli_lib.h"
#include "debug.h"
#include "io_clownix.h"
#include "doorways_sock.h"
#include "epoll_hooks.h"
#include "xdoors.h"
#include "getsock.h"


typedef struct t_xdoors
{
  int connect_llid;
  int doors_llid;
  int cli_idx;
  int type;
  int tid;
} t_xdoors;
/*--------------------------------------------------------------------------*/

static t_xdoors *g_xdoors[CLOWNIX_MAX_CHANNELS];
static int g_inhib_timeout;
static char g_passwd[MSG_DIGEST_LEN];
static uint32_t g_doors_ip;
static int g_doors_port;
static int g_action;
static char *g_cmd, *g_src, *g_dst;


/****************************************************************************/
static void heartbeat(int delta)
{
  mdl_heartbeat();
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void sock_fd_ass_open(int cli_idx)
{
  xdoors_connect(cli_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void sock_fd_ass_close(int llid)
{
  doorways_sock_client_inet_delete(llid);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void sock_fd_tx(int llid, int tid, int type, int len, char *buf)
{
  doorways_tx(llid, tid, type, doors_val_xwy, len, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void xdoors_ok(int llid, int tid, int type, int cli_idx)
{
  if (cli_idx == 0)
    {
    xcli_init(msg_mngt_get_epfd(), llid, tid, type, sock_fd_tx,
              sock_fd_ass_open, sock_fd_ass_close,
              g_action, g_src, g_dst, g_cmd);
    channel_add_epoll_hooks(xcli_fct_before_epoll, xcli_fct_after_epoll);
    }
  else
    xcli_send_randid_associated_end(llid, tid, type, cli_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void xdoors_rx(int llid, int tid, int type, int cli_idx,
                      int len, char *buf)
{
  if (type == doors_type_xwy_x11_flow)
    xcli_x11_doors_rx(cli_idx, len, buf);
  else if (type == doors_type_xwy_main_traf)
    xcli_traf_doors_rx(llid, len, buf);
  else
    KOUT("%d", type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_start_xdoors(int connect_llid, int cli_idx)
{
  t_xdoors *ds = wrap_malloc(sizeof(t_xdoors));
  if ((connect_llid < 1) || (connect_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", connect_llid);
  memset(ds, 0, sizeof(t_xdoors));
  ds->connect_llid = connect_llid;
  ds->cli_idx = cli_idx;
  if (cli_idx == 0)
    ds->type = doors_type_xwy_main_traf;
  else
    ds->type = doors_type_xwy_x11_flow;
  ds->doors_llid = -1;
  ds->tid = -1;
  g_xdoors[connect_llid] = ds;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_type_start_xdoors(int connect_llid)
{
  t_xdoors *ds;
  if ((connect_llid < 1) || (connect_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", connect_llid);
  ds = g_xdoors[connect_llid];
  if (!ds)
    KOUT("%d", connect_llid);
  return (ds->type);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void alloc_middle_xdoors(int connect_llid, int doors_llid, int type)
{
  t_xdoors *ds, *nds;
  if ((connect_llid < 1) || (connect_llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", connect_llid);
  ds = g_xdoors[connect_llid];
  if (!ds)
    KOUT("%d", connect_llid);
  if (ds->type != type)
    KOUT("%d %d %d", connect_llid, ds->type, type);
  if ((ds->tid != -1) || (ds->doors_llid != -1))
    KOUT("%d", connect_llid);
  ds->doors_llid = doors_llid;
  if (g_xdoors[doors_llid])
    KOUT("%d %d", connect_llid, doors_llid);
  nds = (t_xdoors *) wrap_malloc(sizeof(t_xdoors));
  memcpy(nds, ds, sizeof(t_xdoors));
  g_xdoors[doors_llid] = nds;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_doors_link(int is_first, int llid, int tid, int type)
{
  int result = -1;
  t_xdoors *ds;
  if ((llid < 1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  ds = g_xdoors[llid];
  if (!ds)
    KOUT("%d", llid);
  if (ds->type == -1)
    KOUT("%d %d %d", llid, ds->tid, ds->type);
  if ((is_first) && (ds->tid != -1))
    KOUT("%d %d %d", llid, ds->tid, ds->type);
  if ((ds) && (ds->doors_llid == llid) && (ds->type == type))
    {
    ds->tid = tid;
    result = ds->cli_idx;
    }
  if (result == -1)
    KOUT(" ");
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_doors_cli_idx(int llid)
{
  int result = -1;
  t_xdoors *ds;
  if ((llid < 1) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  ds = g_xdoors[llid];
  if (ds)
    result = ds->cli_idx;
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_doors_cli_idx(int llid)
{
  t_xdoors *ds, *ds_connect, *ds_doors;
  int llid_connect, llid_doors;
  doorways_sock_client_inet_delete(llid);
  ds = g_xdoors[llid];
  llid_connect = ds->connect_llid;
  llid_doors = ds->doors_llid;
  if (llid_doors != llid)
    KOUT("%d %d %d", llid_doors, llid_connect, llid);
  wrap_free(g_xdoors[llid_doors], __LINE__);
  wrap_free(g_xdoors[llid_connect], __LINE__);
  g_xdoors[llid_doors] = NULL;
  g_xdoors[llid_connect] = NULL;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timout_connect(void *data)
{
  if (g_inhib_timeout == 0)
    KOUT("\nTIMEOUT trying to reach\n\n");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void doorways_end(int llid)
{
  int cli_idx = get_doors_cli_idx(llid);
  if (cli_idx == -1)
    KERR("%d", llid);
  else
    {
    xcli_killed_x11(cli_idx);
    free_doors_cli_idx(llid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void doorways_rx(int llid,int tid,int type,int val,int len,char *buf)
{
  int cli_idx;
  if ((type == doors_type_xwy_main_traf) ||
      (type == doors_type_xwy_x11_flow))
    {
    if (val == doors_val_link_ko)
      KOUT("%d %d %s %d", type, val, buf, len);
    else if ((val == doors_val_link_ok) && (tid > 0))
      {
      cli_idx = check_doors_link(1, llid, tid, type);
      g_inhib_timeout = 1;
      xdoors_ok(llid, tid, type, cli_idx); 
      }
    else if (val == doors_val_xwy)
      {
      cli_idx = check_doors_link(0, llid, tid, type);
      xdoors_rx(llid, tid, type, cli_idx, len, buf);
      }
    else
      KOUT("%d", val);
    }
  else
    KOUT("%d", type);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int cb_conn(int llid, int fd)
{
  int doors_llid, type = get_type_start_xdoors(llid);
  doors_llid = doorways_sock_client_inet_end(type, llid, fd, g_passwd,
                                             doorways_end, doorways_rx);
  if (doors_llid <= 0)
    KOUT("%d", doors_llid);
  alloc_middle_xdoors(llid, doors_llid, type);
  doorways_tx(doors_llid, 0, type, doors_val_init_link, 3, "OK");
  return 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void xdoors_connect(int cli_idx)
{
  int llid;
  llid = doorways_sock_client_inet_start(g_doors_ip, g_doors_port, cb_conn);
  if (!llid)
    KOUT(" ");
  alloc_start_xdoors(llid, cli_idx);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void xdoors_connect_init(uint32_t ip, int port, char *passwd,
                         int action, char *cmd, char *src, char *dst)
{
  int llid;
  g_doors_ip  = ip;
  g_doors_port= port;
  g_inhib_timeout = 0;
  g_action = action;
  g_cmd = cmd;
  g_src = src;
  g_dst = dst;
  clownix_timeout_add(100, timout_connect, NULL, NULL, NULL);
  msg_mngt_heartbeat_init(heartbeat);
  memset(g_passwd, 0, MSG_DIGEST_LEN);
  strncpy(g_passwd, passwd, MSG_DIGEST_LEN-1);
  llid = doorways_sock_client_inet_start(ip, port, cb_conn);
  if (!llid)
    KOUT(" ");
  alloc_start_xdoors(llid, 0);
}
/*--------------------------------------------------------------------------*/
