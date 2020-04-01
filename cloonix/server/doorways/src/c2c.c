/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <signal.h>
#include <sys/types.h>
#include <errno.h>

#include "io_clownix.h"
#include "util_sock.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"
#include "doorways_sock.h"
#include "dispach.h"
#include "c2c.h"
#include "pid_clone.h"

#define MAX_C2C_IDX 100

int get_doorways_llid(void);

static int pool_fifo_free_index[MAX_C2C_IDX];
static int pool_read_idx;
static int pool_write_idx;


static t_c2c *g_c2c[MAX_C2C_IDX+1];
static t_c2c *g_head_c2c;
static t_c2c *llid2ctx[CLOWNIX_MAX_CHANNELS];

static void clean_connect_timer(t_c2c *c2c);

/****************************************************************************/
typedef struct t_mu_arg
{
  char net_name[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char bin_path[MAX_PATH_LEN];
  char sock[MAX_PATH_LEN];
  char endp_type[MAX_NAME_LEN];
  char addr_port[MAX_NAME_LEN];
} t_mu_arg;
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
t_c2c *c2c_get_llid_ctx(int llid)
{
  t_c2c *result;
  if ((llid<=0) || (llid>=CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  result = llid2ctx[llid];
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void set_llid_ctx(int llid, t_c2c *ctx)
{
  if ((llid<=0) || (llid>=CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  if (llid2ctx[llid])
    KOUT("%d", llid);
  if (!ctx)
    KOUT("%d", llid);
  llid2ctx[llid] = ctx;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void c2c_set_llid_ctx(int llid, t_c2c *c2c)
{
  set_llid_ctx(llid, c2c);
  c2c->tux_llid = llid;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void unset_llid_ctx(int llid, t_c2c *ctx)
{
  if ((llid<=0) || (llid>=CLOWNIX_MAX_CHANNELS))
    KOUT("%d", llid);
  if (!llid2ctx[llid])
    KOUT("%d", llid);
  if (llid2ctx[llid] != ctx)
    KOUT("%d", llid);
  llid2ctx[llid] = NULL;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void pool_init(void)
{
  int i;
  for(i = 0; i < MAX_C2C_IDX; i++)
    pool_fifo_free_index[i] = i+1;
  pool_read_idx = 0;
  pool_write_idx =  MAX_C2C_IDX - 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int pool_alloc(void)
{
  int idx = 0;
  if(pool_read_idx != pool_write_idx)
    {
    idx = pool_fifo_free_index[pool_read_idx];
    pool_read_idx = (pool_read_idx + 1)%MAX_C2C_IDX;
    }
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void pool_release(int idx)
{
  pool_fifo_free_index[pool_write_idx] =  idx;
  pool_write_idx=(pool_write_idx + 1)%MAX_C2C_IDX;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_c2c *find_with_name(char *name)
{
  t_c2c *cur = g_head_c2c;
  while(cur && strcmp(cur->name, name))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_c2c *find_with_idx(int idx)
{
  t_c2c *cur = g_head_c2c;
  while(cur && (cur->local_idx != idx))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static t_c2c *find_with_connect_llid(int connect_llid)
{
  t_c2c *cur = g_head_c2c;
  while(cur && (cur->connect_llid != connect_llid))
    cur = cur->next;
  return cur;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void send_to_switch_resp_idx(char *name, int idx)
{
  int llid = get_doorways_llid();
  doors_send_c2c_resp_idx(llid, 0, name, idx);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void send_to_switch_event_conx(char *name, int fd, int status)
{
  int llid = get_doorways_llid();
  doors_send_c2c_resp_conx(llid, 0, name, fd, status);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int alloc_c2c(char *name)
{
  int idx = pool_alloc();
  t_c2c *c2c;
  if (!idx)
    KERR("%s", name);
  else
    {
    c2c = find_with_idx(idx);
    if (c2c)
      KOUT("%s %d", name, idx);
    c2c = (t_c2c *) clownix_malloc(sizeof(t_c2c), 10);
    memset(c2c, 0, sizeof(t_c2c));
    strncpy(c2c->name, name, MAX_NAME_LEN);
    c2c->local_idx = idx;
    if (g_head_c2c)
      g_head_c2c->prev = c2c;
    c2c->next = g_head_c2c;
    g_head_c2c = c2c;
    }
  return idx;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void free_c2c(char *name)
{
  t_c2c *c2c = find_with_name(name);
  if (c2c)
    {
    clean_connect_timer(c2c);
    if (c2c->dido_llid)
      {
      unset_llid_ctx(c2c->dido_llid, c2c);
      doorways_clean_llid(c2c->dido_llid);
      }
    if (c2c->tux_llid)
      {
      unset_llid_ctx(c2c->tux_llid, c2c);
      if (msg_exist_channel(c2c->tux_llid))
        msg_delete_channel(c2c->tux_llid);
      }
    pool_release(c2c->local_idx);
    if (c2c->prev)
      c2c->prev->next = c2c->next;
    if (c2c->next)
      c2c->next->prev = c2c->prev;
    if (c2c == g_head_c2c)
      g_head_c2c = c2c->next;
    clownix_free(c2c, __FUNCTION__);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_from_switch_req_free(char *name)
{
  free_c2c(name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_abort_error(char *name, char *msg, char *fct, int line)
{
  t_c2c *c2c = find_with_name(name);
  char peer_ip[MAX_NAME_LEN];
  int peer_port;
  if (c2c)
    {
    int_to_ip_string (c2c->peer_ip, peer_ip);
    peer_port = c2c->peer_port;
    }
  else
    {
    strcpy(peer_ip, "0.0.0.0");
    peer_port = 0;
    }
  KERR("%s ip:%s port:%d msg:%s %s %d",name,peer_ip,peer_port,msg,fct,line);
  send_to_switch_event_conx(name, -1, 1);
  free_c2c(name);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void client_err_cb (int dido_llid)
{
  t_c2c *cur = c2c_get_llid_ctx(dido_llid);
  if (!cur)
    {
    doorways_clean_llid(dido_llid);
    }
  else
    c2c_abort_error(cur->name, "Closed connection", 
                    (char *) __FUNCTION__, __LINE__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void client_rx_cb(int dido_llid, int tid, int type, int val,
                         int len, char *buf)
{
  t_c2c *cur = c2c_get_llid_ctx(dido_llid);
  if (!cur)
    doorways_clean_llid(dido_llid);
  else
    {
    if (type == doors_type_c2c_init)
      {
      send_to_switch_event_conx(cur->name, get_fd_with_llid(dido_llid), 0);
      }
    else
      KERR(" ");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_from_switch_req_idx(char *name)
{
  int idx;
  t_c2c *c2c = find_with_name(name);
  if (c2c)
    {
    send_to_switch_resp_idx(name, c2c->local_idx);
    }
  else
    {
    idx = alloc_c2c(name);
    if (!idx)
      send_to_switch_resp_idx(name, 0);
    else
      {
      c2c = find_with_name(name);
      if (!c2c)
        KOUT(" ");
      send_to_switch_resp_idx(name, idx);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void clean_connect_timer(t_c2c *c2c)
{
  if (c2c->connect_abs_beat_timer)
    clownix_timeout_del(c2c->connect_abs_beat_timer, c2c->connect_ref_timer,
                        __FILE__, __LINE__);
  c2c->connect_abs_beat_timer = 0;
  c2c->connect_ref_timer = 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_c2c_connect(void *data)
{
  t_c2c *c2c_bis, *c2c = (t_c2c *) data;
  if (!c2c) 
    KOUT(" ");
  c2c_bis = find_with_name(c2c->name);
  if ((!c2c_bis) || (c2c_bis != c2c))
    KOUT("%p %p", c2c, c2c_bis);
  c2c->connect_abs_beat_timer = 0;
  c2c->connect_ref_timer = 0;
  close(get_fd_with_llid(c2c->connect_llid));
  doorways_sock_client_inet_delete(c2c->connect_llid);
  c2c_abort_error(c2c->name, "Timeout in Connect",
                  (char *) __FUNCTION__, __LINE__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int callback_connect(void *ptr, int llid, int fd)
{
  t_c2c *c2c = find_with_connect_llid(llid);
  int dido_llid;
  char hello[2*MAX_NAME_LEN];
  if (!c2c)
    KERR("%d %d", llid, fd);
  else
    {
    clean_connect_timer(c2c);
    c2c->connect_llid = 0;
    dido_llid = doorways_sock_client_inet_end(doors_type_c2c_init, llid, fd,
                                          c2c->passwd,
                                          client_err_cb, client_rx_cb);
    if (!dido_llid)
      {
      c2c_abort_error(c2c->name, "Connect not possible",
                             (char *) __FUNCTION__, __LINE__);
      }
    else
      {
      set_llid_ctx(dido_llid, c2c);
      c2c->dido_llid = dido_llid;
      snprintf(hello, 2*MAX_NAME_LEN, "hello_%s", c2c->name);
      hello[MAX_NAME_LEN-1] = 0;
      if (doorways_tx(dido_llid, 0, doors_type_c2c_init,
                c2c->peer_idx, strlen(hello)+1 , hello))
        {
        c2c_abort_error(c2c->name, "Transmit not possible",
                             (char *) __FUNCTION__, __LINE__);
        }
      }
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void init_traff_c2c_conx(t_c2c *c2c)
{
  int connect_llid;
  connect_llid = doorways_sock_client_inet_start(c2c->peer_ip, 
                                                 c2c->peer_port,
                                                 callback_connect);
  if (connect_llid)
    {
    c2c->connect_llid = connect_llid;
    clownix_timeout_add(300, timer_c2c_connect, (void *) c2c,
                        &(c2c->connect_abs_beat_timer),
                        &(c2c->connect_ref_timer));
    }
  else
    c2c_abort_error(c2c->name, "Peer not reacheable",
                    (char *) __FUNCTION__, __LINE__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_from_switch_req_conx(char *name, int peer_idx, 
                              int peer_ip, int peer_port, char *passwd)
{
  t_c2c *c2c = find_with_name(name);
  if (c2c)
    {
    c2c->peer_idx = peer_idx;
    c2c->peer_ip = peer_ip;
    c2c->peer_port = peer_port;
    if (passwd)
      strncpy(c2c->passwd, passwd, MSG_DIGEST_LEN-1);
    if (peer_ip)
      {
      init_traff_c2c_conx(c2c);
      }
    }
  else
    {
    send_to_switch_event_conx(name, -1, 1);
    KERR("%s", name);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void c2c_server_dispach_rx_init(int dido_llid, int val, int len, char *buf)
{
  t_c2c *cur = find_with_idx(val);
  char hello[2*MAX_NAME_LEN];
  if (!cur)
    KERR(" ");
  else
    {
    snprintf(hello, 2*MAX_NAME_LEN, "hello_%s", cur->name);
    hello[MAX_NAME_LEN-1] = 0;
    set_llid_ctx(dido_llid, cur);
    cur->dido_llid = dido_llid;
    if (strcmp(buf, hello)) 
      {
      KERR("%d %s %s", len, buf, hello);
      send_to_switch_event_conx(cur->name, -1, 1);
      }
    else
      {
      doorways_tx(dido_llid, 0, doors_type_c2c_init, cur->peer_idx, len, buf);
      send_to_switch_event_conx(cur->name, get_fd_with_llid(dido_llid), 0);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void c2c_server_dispach_rx_traf(int dido_llid, int val, int len, char *buf)
{
  t_c2c *cur = c2c_get_llid_ctx(dido_llid);
  if (!cur)
    KERR(" ");
  else
    {
    if (msg_exist_channel(cur->tux_llid))
      watch_tx(cur->tux_llid, len, buf);
    else
      c2c_abort_error(cur->name, "Traffic connection broken",
                      (char *) __FUNCTION__, __LINE__);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void musat_c2c_death(void *data, int status, char *name)
{
  t_mu_arg *mu = (t_mu_arg *) data;
  int llid = get_doorways_llid();
  (void) status;
  if (strcmp(name, mu->name))
    KERR("%s %s", name, mu->name);
  doors_send_c2c_clone_death(llid, 0, name);
  clownix_free(mu, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int musat_c2c_birth(void *data)
{
  t_mu_arg *mu = (t_mu_arg *) data;
  static char fd[MAX_NAME_LEN];
  char *argv[] = {mu->bin_path, mu->net_name, mu->name, 
                  mu->sock, mu->endp_type, fd, mu->addr_port, NULL};
  int fd_not_closed =  get_fd_not_to_close();
  if (fd_not_closed == -1)
    KERR("%d", fd_not_closed);
  else
    {
    nodelay_fd(fd_not_closed);
    memset(fd, 0, MAX_NAME_LEN);
    snprintf(fd, MAX_NAME_LEN-1, "%d", fd_not_closed);
    execv(mu->bin_path, argv);
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void check_close_fd_used_by_doors_clone(int fd)
{
  int i, llid, nb_chan = get_clownix_channels_nb();
  for (i=0; i<=nb_chan; i++)
    {
    llid = channel_get_llid_with_cidx(i);
    if (llid)
      {
      if (msg_exist_channel(llid))
        {
        if (fd == get_fd_with_llid(llid))
          {
          KERR("%d", fd);
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_c2c_clone_birth(int llid, int tid,  char *net_name, 
                                char *name, int fd, int endp_type,
                                char *bin_path, char *sock)
{
  char tmp_addr_port[2*MAX_NAME_LEN];
  int doors_llid = get_doorways_llid();
  int pid = 0;
  t_mu_arg *mu;
  char peer_ip[MAX_NAME_LEN];
  t_c2c *c2c = find_with_name(name);
  if (!c2c)
    KERR("%s", name);
  else
    {
    mu = (t_mu_arg *) clownix_malloc(sizeof(t_mu_arg), 5);
    memset(mu, 0, sizeof(t_mu_arg));
    strncpy(mu->net_name, net_name, MAX_NAME_LEN-1);
    strncpy(mu->name, name, MAX_NAME_LEN-1);
    strncpy(mu->bin_path, bin_path, MAX_PATH_LEN-1);
    strncpy(mu->sock, sock, MAX_PATH_LEN-1);
    snprintf(mu->endp_type, MAX_NAME_LEN-1, "%d", endp_type);
    if (c2c->peer_ip)
      {
      int_to_ip_string (c2c->peer_ip, peer_ip);
      snprintf(tmp_addr_port, 2*MAX_NAME_LEN, "%s:%d", peer_ip, c2c->peer_port);
      tmp_addr_port[MAX_NAME_LEN-1] = 0;
      strcpy(mu->addr_port, tmp_addr_port);
      }
    else
      {
      snprintf(mu->addr_port, MAX_NAME_LEN-1, "0.0.0.0:%d", c2c->peer_port);
      }
    pid = pid_clone_launch(musat_c2c_birth, musat_c2c_death, NULL,
                           mu, mu, NULL, name, fd, 0);
    c2c_from_switch_req_free(name);
    check_close_fd_used_by_doors_clone(fd);
    doors_send_c2c_clone_birth_pid(doors_llid, 0, name, pid);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_init(void)
{
  pool_init();
  memset(g_c2c, 0, (MAX_C2C_IDX+1) * sizeof(t_c2c *));
  g_head_c2c = NULL;
}
/*--------------------------------------------------------------------------*/

