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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

#include "glob_common.h"
#include "commun.h"
#include "sock.h"
#include "vwifi.h"


enum   
{      
  state_vwifi_syn_cli=13,
  state_vwifi_syn_spy,
  state_vwifi_syn_ctr,
  state_vwifi_traf_cli,
  state_vwifi_traf_spy,
  state_vwifi_traf_ctr,
};



typedef struct t_vwifi_cid
{
  int vwifi_base;
  int vwifi_cid;
  int fd_listen;
  int fd_traffic;
  int type;
  int state;
  int beat;
  struct t_vwifi_cid *prev;
  struct t_vwifi_cid *next;
} t_vwifi_cid;

static int g_pool_fifo_free_index[VWIFI_CID_MAX];
static int g_pool_read_idx;
static int g_pool_write_idx;

static t_vwifi_cid *g_head_cid;

static int g_offset;

static int g_vwifi_base;


/*****************************************************************************/
static t_vwifi_cid *find_cid(int vwifi_cid)
{
  t_vwifi_cid *cur = g_head_cid;
  while(cur)
    {   
    if (cur->vwifi_cid == vwifi_cid)
      break;
    cur = cur->next;
    }
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_send_to_virtio(int base, int cid, int type, int val,
                                 int payload_len, char *payload)
{
  char buf[MAX_A2D_LEN];
  int headsize = sock_header_get_size();
  char *load = buf + headsize;
  memcpy(load, payload, payload_len);
  send_to_virtio(0, base, cid, type, val, payload_len, buf);
  //KERR("KVM TO DOORWAY CID:%d len:%d", cid, payload_len);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void pool_init(int offset)
{
  int i;
  g_offset = offset;
  for(i = 0; i < VWIFI_CID_MAX; i++)
    g_pool_fifo_free_index[i] = g_offset+i+1;
  g_pool_read_idx = 0;
  g_pool_write_idx =  VWIFI_CID_MAX - 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int pool_alloc(void)
{
  int idx = 0;
  if(g_pool_read_idx != g_pool_write_idx)
    {
    idx = g_pool_fifo_free_index[g_pool_read_idx];
    g_pool_read_idx = (g_pool_read_idx + 1) % VWIFI_CID_MAX;
    if ((idx<1+g_offset) || (idx>VWIFI_CID_MAX+g_offset))
      KOUT("ERROR %d %d", g_offset, idx);
    }
  else
    KERR("ERROR %d %d", g_pool_read_idx, g_pool_write_idx);
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void pool_release(int idx)
{
  if ((idx<1+g_offset) || (idx>VWIFI_CID_MAX+g_offset))
    KOUT("ERROR %d %d", g_offset, idx);
  g_pool_fifo_free_index[g_pool_write_idx] =  idx;
  g_pool_write_idx = (g_pool_write_idx + 1) % VWIFI_CID_MAX;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void clean_on_error_base(t_vwifi_cid *cur)
{
  pool_release(cur->vwifi_cid);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_cid)
    g_head_cid = cur->next;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void ko_to_virtio(int vwifi_base, int vwifi_cid, int type, int val_ko)
{
  int len = strlen(LAVWIFITRAF) + 1;
  char *load = LAVWIFITRAF;
  KERR("SEND VIRTIO KO  CID:%d %d %s", vwifi_cid, val_ko, load);
  local_send_to_virtio(vwifi_base, vwifi_cid, type, val_ko, len, load);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void clean_on_error_traf(t_vwifi_cid *cur)
{
  ko_to_virtio(cur->vwifi_base, cur->vwifi_cid, cur->type,
               header_val_vwifi_trf_ko);
  close(cur->fd_traffic);
  clean_on_error_base(cur);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_heartbeat(int cur_sec)
{
  t_vwifi_cid *next, *cur = g_head_cid;
  int newfd;
  while(cur)
    {
    next = cur->next;
    cur->beat += 1;
    if ((cur->state == state_vwifi_syn_cli) ||
        (cur->state == state_vwifi_syn_spy) ||
        (cur->state == state_vwifi_syn_ctr))
      {
      if (cur->beat > 3)
        {
        KERR("ERROR SYN DELETE %d", cur->vwifi_cid);
        newfd = sock_fd_accept(cur->fd_listen);
        close(newfd);
        clean_on_error_base(cur);
        }
      }
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int vwifi_listen_locked(int fd)
{
  t_vwifi_cid *next, *cur = g_head_cid;
  int result = 0;
  while(cur)
    {
    next = cur->next;
    if ((cur->state == state_vwifi_syn_cli) ||
        (cur->state == state_vwifi_syn_spy) ||
        (cur->state == state_vwifi_syn_ctr))
      {
      if (cur->fd_listen == fd)
        result = 1;
      }
    cur = next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int vwifi_trafic_fd_set(fd_set *infd, int max_fd)
{
  t_vwifi_cid *next, *cur = g_head_cid;
  int result = max_fd;
  while(cur)
    {
    next = cur->next;
    if ((cur->state == state_vwifi_traf_cli) ||
        (cur->state == state_vwifi_traf_spy) ||
        (cur->state == state_vwifi_traf_ctr))
      {
      if (cur->fd_traffic > result)
        result = cur->fd_traffic;
      FD_SET(cur->fd_traffic, infd);
      }
    cur = next;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_trafic_fd_isset(fd_set *infd)
{
  char rx[MAX_A2D_LEN];
  int len, headsize = sock_header_get_size();
  t_vwifi_cid *next, *cur = g_head_cid;
  while(cur)
    {
    next = cur->next;
    if (((cur->state == state_vwifi_traf_cli) ||
        (cur->state == state_vwifi_traf_spy)  ||
        (cur->state == state_vwifi_traf_ctr)) &&
        (FD_ISSET(cur->fd_traffic, infd)))
      {
      len = read(cur->fd_traffic, rx, MAX_A2D_LEN-headsize);
      if (len == 0)
        {
        KERR("ERROR TRAF READ 0 DELETE %d", cur->vwifi_cid);
        clean_on_error_traf(cur);
        }
      else if (len == -1)
        {
        if ((errno != EINTR) && (errno != EAGAIN))
          {
          KERR("ERROR TRAF EAGAIN DELETE %d %d", cur->vwifi_cid, errno);
          clean_on_error_traf(cur);
          }
        }
      else
        {
        if (cur->state == state_vwifi_traf_cli)
          local_send_to_virtio(cur->vwifi_base, cur->vwifi_cid,
                               header_type_vwifi_cli,
                               header_val_vwifi_trf_ok, len, rx);
        else if (cur->state == state_vwifi_traf_spy)
          local_send_to_virtio(cur->vwifi_base, cur->vwifi_cid,
                               header_type_vwifi_spy,
                               header_val_vwifi_trf_ok, len, rx);
        else if (cur->state == state_vwifi_traf_ctr)
          local_send_to_virtio(cur->vwifi_base, cur->vwifi_cid,
                               header_type_vwifi_ctr,
                               header_val_vwifi_trf_ok, len, rx);
        else
          KERR("ERROR %d %d %d %s", cur->state, cur->vwifi_cid, len, rx);
        }
      }
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_syn_ok(int cid, int state)
{
  t_vwifi_cid *cur = find_cid(cid);
  int newfd;

  char tx[2];
  tx[0] = ((cid & 0xFF00) >> 8) & 0xFF;
  tx[1] = cid & 0xFF;

  if (!cur)
    KERR("ERROR %d", cid);
  else
    {
    newfd = sock_fd_accept(cur->fd_listen);
    if (newfd < 0)
      {
      KERR("ERROR SYN BAD ACCEPT DELETE %d", cid);
      clean_on_error_base(cur);
      }
    else if (write(newfd, tx, 2) != 2)
      {
      KERR("ERROR SEND 2 FIRST BYTES %d", cid);
      cur->fd_traffic = newfd;
      clean_on_error_traf(cur);
      }
    else
      {
      KERR("SYN OK TRAF OPEN CID %d", cid);
      cur->fd_traffic = newfd;
      cur->state = state;
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_virtio_syn_ok_cli(int vwifi_cid)
{
  local_syn_ok(vwifi_cid, state_vwifi_traf_cli);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_virtio_syn_ok_spy(int vwifi_cid)
{
  local_syn_ok(vwifi_cid, state_vwifi_traf_spy);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_virtio_syn_ok_ctr(int vwifi_cid)
{
  local_syn_ok(vwifi_cid, state_vwifi_traf_ctr);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void local_virtio_syn_ko(int vwifi_cid)
{
  int newfd;
  t_vwifi_cid *cur = find_cid(vwifi_cid);
  if (!cur)
    KERR("ERROR %d", vwifi_cid);
  else
    {
    if ((cur->state == state_vwifi_traf_cli) ||
        (cur->state == state_vwifi_traf_spy) ||
        (cur->state == state_vwifi_traf_ctr))
      {
      KERR("ERROR TRAF REQUEST CLOSE DELETE %d", vwifi_cid);
      close(cur->fd_traffic);
      }
    else
      {
      KERR("ERROR SYN REQUEST CLOSE DELETE %d", vwifi_cid);
      newfd = sock_fd_accept(cur->fd_listen);
      close(newfd);
      }
    clean_on_error_base(cur);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_virtio_syn_ko_cli(int vwifi_cid)
{
  local_virtio_syn_ko(vwifi_cid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_virtio_syn_ko_spy(int vwifi_cid)
{
  local_virtio_syn_ko(vwifi_cid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_virtio_syn_ko_ctr(int vwifi_cid)
{
  local_virtio_syn_ko(vwifi_cid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_client_syn(int vwifi_base, int fd, int state, int type)
{
  int newfd, cid = 0;
  t_vwifi_cid *cur;
  if (vwifi_listen_locked(fd))
    KERR("WARNING LISTEN LOCKED %d", fd);
  else
    cid = pool_alloc();
  if (cid == 0)
    {
    KERR("ERROR POOL %d", fd);
    newfd = sock_fd_accept(fd);
    close(newfd);
    }
  else
    {
    cur = (t_vwifi_cid *) malloc(sizeof(t_vwifi_cid));
    memset(cur, 0, sizeof(t_vwifi_cid));
    cur->vwifi_base = vwifi_base;
    cur->vwifi_cid = cid;
    cur->fd_listen = fd;
    cur->type = type;
    cur->state = state;
    KERR("ALLOC SYN CID:%d", cid);
    if (g_head_cid)
      g_head_cid->prev = cur;
    cur->next = g_head_cid;
    g_head_cid = cur;
    local_send_to_virtio(vwifi_base, cid,  type, header_val_vwifi_syn_ok,
                         strlen(LAVWIFISYN) + 1, LAVWIFISYN);
    }  
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_client_syn_cli(int vwifi_base, int fd)
{
  local_client_syn(vwifi_base,fd,state_vwifi_syn_cli,header_type_vwifi_cli);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_client_syn_spy(int vwifi_base, int fd)
{
  local_client_syn(vwifi_base,fd,state_vwifi_syn_spy,header_type_vwifi_spy);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_client_syn_ctr(int vwifi_base, int fd)
{
  local_client_syn(vwifi_base,fd,state_vwifi_syn_ctr,header_type_vwifi_ctr);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void local_virtio_traf(int cid, char *rx, int len, int type)
{
  t_vwifi_cid *cur = find_cid(cid);
  int txlen;

  if (!cur)
    {
    KERR("ERROR %d %d", cid,  len);
    ko_to_virtio(g_vwifi_base, cid, type, header_val_vwifi_bad_ko);
    }
  else if (cur->state == state_vwifi_traf_cli)
    {
    txlen = write(cur->fd_traffic, rx, len);
    if (txlen != len)
      {
      KERR("ERROR TRAF WRITE BAD DELETE CID:%d len:%d errno:%d", cid, txlen, errno);
      clean_on_error_traf(cur);
      }
    //KERR("DOORWAY TO KVM CID:%d len:%d", cid, txlen);
    }
  else if (cur->state == state_vwifi_traf_spy)
    {
    txlen = write(cur->fd_traffic, rx, len);
    if (txlen != len)
      {
      KERR("ERROR TRAF WRITE BAD DELETE CID:%d len:%d errno:%d", cid, txlen, errno);
      clean_on_error_traf(cur);
      }
    }
  else if (cur->state == state_vwifi_traf_ctr)
    {
    txlen = write(cur->fd_traffic, rx, len);
    if (txlen != len)
      {
      KERR("ERROR TRAF WRITE BAD DELETE CID:%d len:%d errno:%d", cid, txlen, errno);
      clean_on_error_traf(cur);
      }
    }
  else
    KERR("ERROR %d %d", cid, cur->state);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_virtio_traf_cli(int vwifi_cid, char *rx, int len)
{
  local_virtio_traf(vwifi_cid, rx, len, header_type_vwifi_cli);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_virtio_traf_spy(int vwifi_cid, char *rx, int len)
{
  local_virtio_traf(vwifi_cid, rx, len, header_type_vwifi_spy);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_virtio_traf_ctr(int vwifi_cid, char *rx, int len)
{
  local_virtio_traf(vwifi_cid, rx, len, header_type_vwifi_ctr);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void vwifi_init(int offset)
{
  g_vwifi_base = offset;
  pool_init(offset);
  g_head_cid = NULL;
}
/*---------------------------------------------------------------------------*/



