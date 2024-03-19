/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include <sys/time.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "sock.h"
#include "llid_traffic.h"
#include "llid_x11.h"
#include "llid_backdoor.h"
#include "doorways_sock.h"
#include "util_sock.h"
#include "commun.h"



typedef struct t_sub_dido_x11
{
  int dido_llid;
  int sub_dido_idx;
  int fd_traf_x11;
  int llid_traf_x11;
} t_sub_dido_x11;

typedef struct t_local_dropbear
{
  int dido_llid;
  int local_llid;
  int fd;
  int idx_display_sock_x11;
  int fd_display_sock_x11;
  int llid_display_sock_x11;
  t_sub_dido_x11 sub_dido_x11[MAX_IDX_X11];
  int pool_fifo_free_index[MAX_IDX_X11];
  int pool_read_idx;
  int pool_write_idx;
  struct t_local_dropbear *prev;
  struct t_local_dropbear *next;
} t_local_dropbear;



static int pool_fifo_free_index[MAX_IDX_X11];
static int pool_read_idx;
static int pool_write_idx;



static t_local_dropbear *g_local_dropbear;

static t_local_dropbear *g_idx_display_sock_x11[MAX_IDX_X11];
static t_local_dropbear *g_dido_llid[CLOWNIX_MAX_CHANNELS];
static t_local_dropbear *g_local_llid[CLOWNIX_MAX_CHANNELS];
static t_sub_dido_x11 *g_local_x11_llid[CLOWNIX_MAX_CHANNELS];


int get_doorways_llid(void);
char *get_local_dropbear_sock(void);
void send_resp_ok_to_traf_client(int dido_llid, int idx_display_sock_x11); 
static int listen_x11_evt(void *ptr, int llid, int fd_x11_listen);
static void listen_x11_err(void *ptr, int llid, int err, int from);


/****************************************************************************/
static t_sub_dido_x11 *sub_dido_x11_get(t_local_dropbear *ld, int sub_dido_idx)
{
  t_sub_dido_x11 *result;
  if ((sub_dido_idx < 1) || (sub_dido_idx >= MAX_IDX_X11))
    KOUT("%d", sub_dido_idx);
  result = &(ld->sub_dido_x11[sub_dido_idx]);
  if (result->sub_dido_idx != sub_dido_idx)
    KERR("%d %d", result->sub_dido_idx, sub_dido_idx);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_sub_dido_x11 *find_with_llid_traf_x11(int llid)
{
  t_sub_dido_x11 *cur;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  cur = g_local_x11_llid[llid];
  if (!cur)
    KERR("%d", llid);
  else if (llid != cur->llid_traf_x11)
    {
    KERR("%d %d", llid, cur->llid_traf_x11);
    cur = NULL;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_local_dropbear *find_with_local_llid(int llid, int is_listen_x11)
{
  t_local_dropbear *cur;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  cur = g_local_llid[llid];
  if (!cur)
    KERR("%d", llid);
  else
    {
    if (is_listen_x11)
      {
      if (llid != cur->llid_display_sock_x11)
        {
        KERR("%d %d", llid, cur->llid_display_sock_x11);
        cur = NULL;
        }
      }
    else
      {
      if (llid != cur->local_llid)
        {
        KERR("%d %d", llid, cur->local_llid);
        cur = NULL;
        }
      }
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_local_dropbear *find_with_idx_display_sock_x11(int idx)
{
  t_local_dropbear *cur;
  if ((idx <= 0) || (idx >= MAX_IDX_X11))
    KOUT("%d", idx);
  cur = g_idx_display_sock_x11[idx];
  if (!cur)
    KERR("%d", idx);
  else if (idx != cur->idx_display_sock_x11)
    {
    KERR("%d %d", idx, cur->idx_display_sock_x11);
    cur = NULL;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_local_dropbear *find_with_dido_llid_fast(int llid)
{
  t_local_dropbear *cur;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  cur = g_dido_llid[llid];
  if (!cur)
    KERR("%d", llid);
  else if (llid != cur->dido_llid)
    {
    KERR("%d %d", llid, cur->dido_llid);
    cur = NULL;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_local_dropbear *find_with_dido_llid_by_scan(int llid)
{
  t_local_dropbear *cur = g_local_dropbear;
  if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  while (cur)
    {
    if (llid == cur->dido_llid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*--------------------------------------------------------------------------*/



/*****************************************************************************/
static void pool_init_display_sock_x11(void)
{
  int i;
  for(i = 0; i < MAX_IDX_X11; i++)
    pool_fifo_free_index[i] = i+1;
  pool_read_idx = 0;
  pool_write_idx =  MAX_IDX_X11 - 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int pool_alloc_display_sock_x11(void)
{
  int idx = 0;
  if(pool_read_idx != pool_write_idx)
    {
    idx = pool_fifo_free_index[pool_read_idx];
    pool_read_idx = (pool_read_idx + 1)%MAX_IDX_X11;
    }
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void pool_release_display_sock_x11(int idx)
{
  pool_fifo_free_index[pool_write_idx] =  idx;
  pool_write_idx=(pool_write_idx + 1)%MAX_IDX_X11;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void pool_init_sub_dido_idx(t_local_dropbear *ld)
{
  int i;
  for(i = 0; i < MAX_IDX_X11; i++)
    ld->pool_fifo_free_index[i] = i+1;
  ld->pool_read_idx = 0;
  ld->pool_write_idx =  MAX_IDX_X11 - 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int pool_can_alloc_sub_dido_idx(t_local_dropbear *ld)
{
  int result = 0;
  if(ld->pool_read_idx != ld->pool_write_idx)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int pool_alloc_sub_dido_idx(t_local_dropbear *ld)
{
  int idx = 0;
  if(ld->pool_read_idx != ld->pool_write_idx)
    {
    idx = ld->pool_fifo_free_index[ld->pool_read_idx];
    ld->pool_read_idx = (ld->pool_read_idx + 1)%MAX_IDX_X11;
    }
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void pool_release_sub_dido_idx(t_local_dropbear *ld, int idx)
{
  ld->pool_fifo_free_index[ld->pool_write_idx] =  idx;
  ld->pool_write_idx=(ld->pool_write_idx + 1)%MAX_IDX_X11;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int add_listen_x11(int display_sock_x11)
{
  int fd_listen;
  char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s%d", UNIX_X11_SOCKET_PREFIX,
                                 display_sock_x11 + IDX_X11_DISPLAY_ADD);
  fd_listen = util_socket_listen_unix(path);
  return fd_listen;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void remove_listen_x11(int display_sock_x11, int llid)
{
  char path[MAX_PATH_LEN];
  memset(path, 0, MAX_PATH_LEN);
  snprintf(path, MAX_PATH_LEN-1, "%s%d", UNIX_X11_SOCKET_PREFIX,
                                 display_sock_x11 + IDX_X11_DISPLAY_ADD);
  if (msg_exist_channel(llid))
    msg_delete_channel(llid);
  unlink(path);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int listen_x11_begin(t_local_dropbear *ld)
{
  int result = -1;
  pool_init_sub_dido_idx(ld);
  ld->idx_display_sock_x11 = pool_alloc_display_sock_x11();
  ld->llid_display_sock_x11 = 0;
  ld->fd_display_sock_x11 = add_listen_x11(ld->idx_display_sock_x11);
  if (ld->fd_display_sock_x11 < 0)
    {
    DOUT(FLAG_HOP_DOORS, "BAD LISTEN START");
    KERR(" ");
    pool_release_sub_dido_idx(ld, ld->idx_display_sock_x11);
    ld->idx_display_sock_x11 = 0;
    }
  else
    {
    ld->llid_display_sock_x11 = msg_watch_fd(ld->fd_display_sock_x11,
                                             listen_x11_evt,
                                             listen_x11_err,
                                             "listen_x11");
    if (!ld->llid_display_sock_x11)
      {
      DOUT(FLAG_HOP_DOORS, "BAD LISTEN START");
      KERR(" ");
      }
    else
      {
      DOUT(FLAG_HOP_DOORS, "LISTEN START %d", ld->idx_display_sock_x11);
      if (g_local_llid[ld->llid_display_sock_x11])
        KOUT("%d", ld->llid_display_sock_x11);
      g_local_llid[ld->llid_display_sock_x11] = ld;
      g_idx_display_sock_x11[ld->idx_display_sock_x11] = ld;;
      result = 0;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void traf_x11_end(t_sub_dido_x11 *cur)
{
  char *buf = get_gbuf();
  t_local_dropbear *ld;
  if (cur)
    {
    ld = find_with_dido_llid_by_scan(cur->dido_llid);
    if (!ld)
      KOUT(" ");
    DOUT(FLAG_HOP_DOORS, 
               "TRAF END display_sock_x11:%d sub_dido_idx:%d",
               ld->idx_display_sock_x11, cur->sub_dido_idx);
    memset(buf, 0, MAX_A2D_LEN); 
    snprintf(buf, MAX_A2D_LEN-1, LAX11OPENKO, cur->sub_dido_idx);
    x11_open_close(0, ld->dido_llid, buf);
    if (msg_exist_channel(cur->llid_traf_x11))
      msg_delete_channel(cur->llid_traf_x11);
    g_local_x11_llid[cur->llid_traf_x11] = NULL;
    pool_release_sub_dido_idx(ld, cur->sub_dido_idx);
    memset(cur, 0, sizeof(t_sub_dido_x11));
    }
  else
    KERR(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void listen_x11_end(t_local_dropbear *ld)
{
  int i;
  if (ld->idx_display_sock_x11)
    {
    DOUT(FLAG_HOP_DOORS, "LISTEN END %d", ld->idx_display_sock_x11);
    for (i=1; i<MAX_IDX_X11; i++)
      {
      if (ld->sub_dido_x11[i].sub_dido_idx == i)
        traf_x11_end(&(ld->sub_dido_x11[i]));
      }
    remove_listen_x11(ld->idx_display_sock_x11, ld->llid_display_sock_x11);
    pool_release_display_sock_x11(ld->idx_display_sock_x11);
    if (g_local_llid[ld->llid_display_sock_x11] != ld)
      KERR("%d", ld->llid_display_sock_x11);
    g_local_llid[ld->llid_display_sock_x11] = NULL;
    if (g_idx_display_sock_x11[ld->idx_display_sock_x11] != ld)
      KERR("%d %d", ld->llid_display_sock_x11, ld->idx_display_sock_x11);
    g_idx_display_sock_x11[ld->idx_display_sock_x11] = NULL;
    ld->idx_display_sock_x11 = 0;
    ld->llid_display_sock_x11 = 0;
    ld->fd_display_sock_x11 = 0;
    }
  else
    KERR(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void local_dropbear_receive_x11_from_client(int idx_display_sock_x11,
                                            int sub_dido_idx,
                                            int len, char *buf)
{
  t_local_dropbear *ld = find_with_idx_display_sock_x11(idx_display_sock_x11);
  t_sub_dido_x11 *cur;
  if (ld)
    {
    cur = sub_dido_x11_get(ld, sub_dido_idx);
    if (cur)
      {
      if (cur->sub_dido_idx == sub_dido_idx)
        {
        if (msg_exist_channel(cur->llid_traf_x11))
          watch_tx(cur->llid_traf_x11, len, buf);
        else
          {
          KERR(" ");
          traf_x11_end(cur);
          }
        }
      else
        KERR(" ");
      }
    else
      KERR("%d %d", cur->sub_dido_idx, sub_dido_idx);
    }
  else
    KERR(" ");
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void listen_x11_err(void *ptr, int llid, int err, int from)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int sock_fd_accept(int fd_listen)
{
  int fd;
  struct sockaddr s_addr;
  socklen_t sz = sizeof(s_addr);
  fd = accept( fd_listen, &s_addr, &sz );
  return fd;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void x11_err_cb(void *ptr, int llid, int err, int from)
{
  t_sub_dido_x11 *cur = find_with_llid_traf_x11(llid);
  if (!cur)
    {
    KERR("%d %d", err, from);
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
  else
    {
    traf_x11_end(cur);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int x11_rx_cb(void *ptr, int llid, int fd)
{
  int len, result = 0;
  t_sub_dido_x11 *cur = find_with_llid_traf_x11(llid);
  char *buf = get_gbuf();
  if (!cur)
    {
    KERR(" ");
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    }
  else
    {
    if (cur->fd_traf_x11 != fd)
      KOUT("%d %d", cur->fd_traf_x11, fd);
    len = read (fd, buf, MAX_A2D_LEN);
    if (len == 0)
      {
      KERR(" ");
      traf_x11_end(cur);
      }
    else if (len < 0)
      {
      if ((errno != EAGAIN) && (errno != EINTR))
        {
        KERR(" ");
        traf_x11_end(cur);
        }
      }
    else
      {
      x11_write(cur->dido_llid, cur->sub_dido_idx, len, buf);
      result = len;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void local_dropbear_x11_open_to_agent(int dido_llid, int sub_dido_idx)
{
  t_local_dropbear *ld = find_with_dido_llid_by_scan(dido_llid);
  t_sub_dido_x11 *cur;
  if (!ld)
    KERR(" ");
  else
    {
    cur = sub_dido_x11_get(ld, sub_dido_idx);
    if (!cur)
      KERR(" ");
    else
      {
      if ((cur->sub_dido_idx) && (cur->llid_traf_x11 == 0)) 
        {
        cur->llid_traf_x11 = msg_watch_fd(cur->fd_traf_x11, 
                                          x11_rx_cb, x11_err_cb, "x11local");
        if (!cur->llid_traf_x11)
          {
          KERR(" ");
          traf_x11_end(cur);
          }
        else
          g_local_x11_llid[cur->llid_traf_x11] = cur;
        }
      else
        {
        KERR("%d %d", cur->sub_dido_idx, cur->llid_traf_x11);
        traf_x11_end(cur);
        }
      }
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int listen_x11_evt(void *ptr, int llid, int fd_x11_listen)
{
  int idx, fd;
  char *buf = get_gbuf();
  t_local_dropbear *ld = find_with_local_llid(llid, 1);
  if (ld)
    {
    if (!pool_can_alloc_sub_dido_idx(ld))
      {
      KERR("%d", ld->idx_display_sock_x11);
      }
    else
      {
      fd = sock_fd_accept(fd_x11_listen);
      if (fd <= 0)
        {
        KERR("%d", errno);
        listen_x11_end(ld);
        }
      else
        {
        idx = pool_alloc_sub_dido_idx(ld);
        ld->sub_dido_x11[idx].dido_llid = ld->dido_llid;
        ld->sub_dido_x11[idx].sub_dido_idx = idx;
        ld->sub_dido_x11[idx].fd_traf_x11 = fd; 
        memset(buf, 0, MAX_A2D_LEN);
        snprintf(buf,MAX_A2D_LEN-1,LAX11OPEN,idx,ld->idx_display_sock_x11);
        x11_open_close(0, ld->dido_llid, buf);
        DOUT(FLAG_HOP_DOORS, "TRAF START display_sock_x11:%d sub_dido_idx:%d",
             ld->idx_display_sock_x11);
        }
      }
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int alloc_local_ctx(t_local_dropbear *ld, int dido_llid, 
                           int local_llid, int fd)
{
  int result = 0;
  if (ld->dido_llid != dido_llid)
    KOUT(" ");
  if ((ld->fd) || (ld->local_llid))
    KOUT(" ");
  if (g_dido_llid[dido_llid])
    KOUT(" ");
  if (g_local_llid[local_llid])
    KOUT(" ");
  if ((!fd) || (!local_llid))
    KOUT(" ");
  g_dido_llid[dido_llid] = ld;
  g_local_llid[local_llid] = ld;
  ld->fd = fd;
  ld->local_llid = local_llid;
  result = listen_x11_begin(ld);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void free_local_ctx(t_local_dropbear *ld)
{
  if (!ld->local_llid)
    {
    KERR(" ");
    if (g_dido_llid[ld->dido_llid])
      KOUT(" ");
    }
  else
    {
    if ((!g_dido_llid[ld->dido_llid]) || (!g_local_llid[ld->local_llid]))
      KOUT(" ");
    if (g_dido_llid[ld->dido_llid] != g_local_llid[ld->local_llid])
      KOUT(" ");
    listen_x11_end(ld);
    g_dido_llid[ld->dido_llid] = NULL;
    g_local_llid[ld->local_llid] = NULL;
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void local_err_cb (void *ptr, int local_llid, int err, int from)
{
  t_local_dropbear *ld = find_with_local_llid(local_llid, 0);
  if (!ld)
    {
    KERR(" %d %d", err, from);
    if (msg_exist_channel(local_llid))
      msg_delete_channel(local_llid);
    }
  else
    {
    llid_traf_delete(ld->dido_llid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int local_rx_cb(void *ptr, int local_llid, int fd)
{
  int len, result = 0;
  char *buf = get_gbuf();
  t_local_dropbear *ld = find_with_local_llid(local_llid, 0);
  if (!ld)
    {
    KERR(" ");
    if (msg_exist_channel(local_llid))
      msg_delete_channel(local_llid);
    }
  else
    {
    if (ld->fd != fd)
      KOUT("%d %d", ld->fd, fd);
    len = read (fd, buf, MAX_A2D_LEN);
    if (len == 0)
      {
      KERR(" ");
      llid_traf_delete(ld->dido_llid);
      }
    else if (len < 0)
      {
      if ((errno != EAGAIN) && (errno != EINTR))
        {
        KERR(" ");
        llid_traf_delete(ld->dido_llid);
        }
      }
    else
      {
      send_to_traf_client(ld->dido_llid, doors_val_none, len, buf);
      result = len;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void local_dropbear_receive_from_client(int dido_llid, int len, char *buf)
{
  t_local_dropbear *ld = find_with_dido_llid_fast(dido_llid);
  if (!ld)
    KERR(" ");
  else
    {
    if (msg_exist_channel(ld->local_llid))
      watch_tx(ld->local_llid, len, buf);
    else
      {
      KERR(" ");
      llid_traf_delete(ld->dido_llid);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int local_dropbear_init_dido(int dido_llid)
{
  t_local_dropbear *cur = find_with_dido_llid_by_scan(dido_llid);
  char *bearsock = get_local_dropbear_sock();
  int fd, local_llid;
  if (!cur)
    KERR(" ");
  else
    {
    fd = sock_nonblock_client_unix(bearsock);
    if (fd <= 0)
      KERR("%d %d", fd, errno);
    else
      {
      local_llid = msg_watch_fd(fd, local_rx_cb, local_err_cb, "local");
      if (alloc_local_ctx(cur, dido_llid, local_llid, fd))
        KERR(" ");
      send_resp_ok_to_traf_client(dido_llid, cur->idx_display_sock_x11);
      }
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void local_dropbear_add_llid(int dido_llid)
{
  t_local_dropbear *ld = find_with_dido_llid_by_scan(dido_llid);
  if (ld)
    KERR("%d", dido_llid);
  else
    {
    ld = (t_local_dropbear *) clownix_malloc(sizeof(t_local_dropbear), 9);
    memset(ld, 0, sizeof(t_local_dropbear));
    ld->dido_llid = dido_llid;
    if (g_local_dropbear)
      g_local_dropbear->prev = ld;
    ld->next = g_local_dropbear;
    g_local_dropbear = ld;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void local_dropbear_del_llid(int dido_llid)
{
  t_local_dropbear *ld = find_with_dido_llid_by_scan(dido_llid);
  if (!ld)
    KOUT("%d", dido_llid);
  free_local_ctx(ld);
  if (ld->prev)
    ld->prev->next = ld->next;
  if (ld->next)
    ld->next->prev = ld->prev;
  if (ld == g_local_dropbear)
    g_local_dropbear = ld->next;
  clownix_free(ld, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void local_dropbear_init(void)
{
  pool_init_display_sock_x11();
  g_local_dropbear = NULL;
}
/*--------------------------------------------------------------------------*/
