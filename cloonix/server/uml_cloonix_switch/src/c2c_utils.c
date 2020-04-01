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
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "io_clownix.h"
#include "commun_daemon.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "event_subscriber.h"
#include "rpc_c2c.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "doorways_sock.h"
#include "llid_trace.h"
#include "c2c_utils.h"
#include "endp_mngt.h"




static t_sc2c *g_head_c2c;

/****************************************************************************/
void peer_doorways_client_tx(int llid, int len, char *buf)
{
  doorways_tx(llid, 0, doors_type_switch, doors_val_none, len, buf);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_c2c(void *data)
{
  t_timer_c2c *tim = (t_timer_c2c *) data;
  t_sc2c *c2c = c2c_find(tim->name);
  if (!c2c)
    KERR("%s", tim->name);
  else
    {
    c2c->timer_abs_beat = 0;
    c2c->timer_ref = 0;
    if (tim->token_id != c2c->timer_token_id)
      KERR("%s %d %d", tim->name, tim->token_id, c2c->timer_token_id);
    else
      {
      c2c->timer_token_id = 0;
      if(!tim->callback)
        KOUT(" ");
      tim->callback(c2c);
      }
    }
  clownix_free(tim, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_del_timer(t_sc2c *c2c)
{
  t_timer_c2c *tim;
  if (c2c->timer_abs_beat)
    {
    tim = (t_timer_c2c *)clownix_timeout_del(c2c->timer_abs_beat, 
                                             c2c->timer_ref, 
                                             __FILE__, __LINE__);
    if (!tim)
      KOUT(" ");
    if (c2c->timer_token_id != tim->token_id)
      KERR("%d %d", c2c->timer_token_id, tim->token_id); 
    c2c->timer_token_id = 0;
    clownix_free(tim, __FUNCTION__);
    c2c->timer_abs_beat = 0;
    c2c->timer_ref = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_arm_timer(t_sc2c *c2c, int val, t_fct_timeout_cb cb)
{
  static int token_id = 0;
  t_timer_c2c *tim;
  c2c_del_timer(c2c);
  tim = (t_timer_c2c *) clownix_malloc(sizeof(t_timer_c2c), 9);
  memset(tim, 0, sizeof(t_timer_c2c));
  strncpy(tim->name, c2c->name, MAX_NAME_LEN-1);
  tim->callback = cb;
  if (c2c->timer_token_id)
    KERR(" ");
  c2c->timer_token_id = token_id++;
  tim->token_id = c2c->timer_token_id;
  clownix_timeout_add(val, timer_c2c, (void *) tim, 
                      &(c2c->timer_abs_beat), &(c2c->timer_ref));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_globtopo_small_event(char *name, char *src, char *dst, int evt)
{
  t_small_evt vm_evt;
  memset(&vm_evt, 0, sizeof(t_small_evt));
  strncpy(vm_evt.name, name, MAX_NAME_LEN-1);
  strncpy(vm_evt.param1, src, MAX_NAME_LEN-1);
  strncpy(vm_evt.param2, dst, MAX_NAME_LEN-1);
  vm_evt.evt = evt;
  event_subscriber_send(topo_small_event, (void *) &vm_evt);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int c2c_globtopo_add(t_sc2c *c2c)
{
  int type, result = -1;
  if (!endp_mngt_exists(c2c->name, 0, &type))
    KERR("%s", c2c->name);
  else if (type != endp_type_c2c)
    KERR("%s %d", c2c->name, type);
  else
    {
    endp_mngt_c2c_info(c2c->name, 0, c2c->local_is_master,
                       c2c->master_cloonix, c2c->slave_cloonix,
                       c2c->ip_slave, c2c->port_slave, c2c->slave_passwd);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_sc2c *c2c_find(char *name)
{
  t_sc2c *result = NULL;
  t_sc2c *cur = g_head_c2c;
  while(cur)
    {
    if (!strcmp(cur->name, name))
      {
      result = cur;
      break;
      }
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_sc2c *c2c_find_with_llid(int llid)
{
  t_sc2c *result = NULL;
  t_sc2c *cur = g_head_c2c;
  if (llid>0)
    {
    while(cur)
      {
      if (cur->peer_switch_llid == llid)
        {
        result = cur;
        break;
        }
      cur = cur->next;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_sc2c *c2c_find_with_connect_llid(int llid)
{
  t_sc2c *result = NULL;
  t_sc2c *cur = g_head_c2c;
  if (llid>0)
    {
    while(cur)
      {
      if (cur->connect_llid == llid)
        {
        result = cur;
        break;
        }
      cur = cur->next;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
t_sc2c *c2c_alloc(int local_is_master, char *name, 
                  int ip_slave, int port_slave, char *passwd_slave) 
{
  t_sc2c *cur = (t_sc2c *) clownix_malloc(sizeof(t_sc2c), 9);
  memset(cur, 0, sizeof(t_sc2c));
  cur->ip_slave = ip_slave;
  cur->port_slave = port_slave;
  strncpy(cur->name, name, MAX_NAME_LEN-1);
  if (passwd_slave)
    strncpy(cur->slave_passwd, passwd_slave, MSG_DIGEST_LEN-1);
  cur->local_is_master = local_is_master;
  if (g_head_c2c)
    g_head_c2c->prev = cur;
  cur->next = g_head_c2c;
  g_head_c2c = cur;
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void tell_peer_slave_to_free(t_sc2c *c2c)
{
  if (msg_exist_channel(c2c->peer_switch_llid))
    {
    if (c2c->local_is_master)
      {
      doors_io_c2c_tx_set(peer_doorways_client_tx);
      send_c2c_peer_delete(c2c->peer_switch_llid, 0, c2c->name);
      doors_io_c2c_tx_set(string_tx);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void timer_llid_trace_free(void *data)
{
  unsigned long ul_llid = (unsigned long) data;
  int llid = (int) ul_llid;
  doorways_clean_llid(llid);
  llid_trace_free(llid, 0, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_globtopo_free(void *data)
{
  char *name = (char *) data;
  endp_mngt_stop(name, 0);
  clownix_free(name, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void arm_timer_globtopo_free(char *name)
{
  char *nm;
  nm = (char *) clownix_malloc(MAX_NAME_LEN, 9);
  memset(nm, 0, MAX_NAME_LEN);
  strncpy(nm, name, MAX_NAME_LEN-1);
  clownix_timeout_add(10, timer_globtopo_free, (void *) nm, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void c2c_free(t_sc2c *cur)
{
  unsigned long ul_llid;
  c2c_del_timer(cur);
  if (cur->peer_switch_llid)
    {
    tell_peer_slave_to_free(cur);
    ul_llid = (unsigned long) cur->peer_switch_llid;
    clownix_timeout_add(10, timer_llid_trace_free, (void *)ul_llid,NULL,NULL);
    }
  arm_timer_globtopo_free(cur->name);
  doors_send_c2c_req_free(get_doorways_llid(), 0, cur->name);
  if (cur->prev)
    cur->prev->next = cur->next;
  if (cur->next)
    cur->next->prev = cur->prev;
  if (cur == g_head_c2c)
    g_head_c2c = cur->next;
  clownix_free(cur, __FUNCTION__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void c2c_free_ctx(char *name)
{
  t_sc2c *cur = c2c_find(name);
  if (cur)
    c2c_free(cur);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void ping_timer(void *data)
{
  char *name = (char *) data;
  t_sc2c *cur = c2c_find(name);
  if (cur)
    {
    cur->ping_timer_count++;
    if (cur->ping_timer_count > 3)
      {
      c2c_globtopo_small_event(name, cur->master_cloonix, 
                               cur->slave_cloonix, c2c_evt_connection_ko);
      c2c_set_state(cur, state_slave_waiting_reconnect);
      if (cur->local_is_master)
        {
        if (cur->peer_switch_llid)
          {
          llid_trace_free(cur->peer_switch_llid, 0, __FUNCTION__);
          cur->peer_switch_llid = 0;
          cur->master_idx = 0;
          cur->slave_idx = 0;
          cur->ping_timer_count = 0; 
          c2c_arm_timer(cur, 300, time_try_connect_peer);
          }
        else
          KERR(" ");
        }
      else
        {
        if (cur->peer_switch_llid)
          {
          llid_trace_free(cur->peer_switch_llid, 0, __FUNCTION__);
          cur->peer_switch_llid = 0;
          cur->master_idx = 0;
          cur->slave_idx = 0;
          cur->ping_timer_count = 0;
          }
        else
          KERR(" ");
        }
      }
    else
      clownix_timeout_add(100, ping_timer, data, NULL, NULL);
    if (cur->local_is_master)
      doors_io_c2c_tx_set(peer_doorways_client_tx);
    send_c2c_peer_ping(cur->peer_switch_llid, 0, name);
    if (cur->local_is_master)
      doors_io_c2c_tx_set(string_tx);
    }
  else
    clownix_free(name, __FUNCTION__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void c2c_arm_ping_timer(char *name)
{
  char *nm;
  nm = (char *) clownix_malloc(MAX_NAME_LEN, 9);
  memset(nm, 0, MAX_NAME_LEN);
  strncpy(nm, name, MAX_NAME_LEN-1);
  clownix_timeout_add(100, ping_timer, (void *) nm, NULL, NULL);
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
void c2c_set_state(t_sc2c *c2c, int state)
{
  if ((state <= state_min) || (state >= state_max))
    KOUT("%d", state);
  c2c->state = state; 
  if (c2c->state == state_peered)
    endp_mngt_c2c_peered(c2c->name, 0, 1);
  else
    endp_mngt_c2c_peered(c2c->name, 0, 0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_tx_req_idx_to_doors(char *name)
{
  doors_send_c2c_req_idx(get_doorways_llid(), 0, name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_tx_req_conx_to_doors(char *name, int peer_idx, 
                              int peer_ip, int peer_port, char *passwd)
{
  int our_port = cfg_get_server_port();
  int llid = get_doorways_llid();
  if (peer_ip)
    doors_send_c2c_req_conx(llid,0,name,peer_idx,peer_ip,peer_port,passwd);
  else
    doors_send_c2c_req_conx(llid,0,name,peer_idx,peer_ip,our_port,passwd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_abort_error(t_sc2c *c2c, char *fct, int line)
{
  event_print("%s %s %d", c2c->name, fct, line);
  KERR("%s %s %d", c2c->name, fct, line);
  c2c_free_ctx(c2c->name);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_free_all(void)
{
  t_sc2c *next, *cur = g_head_c2c;
  while(cur)
    {
    next = cur->next;
    c2c_free(cur);
    cur = next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void peer_err_cb (int llid)
{
  t_sc2c *c2c = c2c_find_with_llid(llid);
  if (c2c)
    {
    endp_mngt_c2c_disconnect(c2c->name, 0);
    c2c_abort_error(c2c, (char *) __FUNCTION__, __LINE__);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void peer_rx_cb(int llid, int tid, int type, int val, int len, char *buf)
{
  if ((type == doors_type_switch) &&
      (val != doors_val_link_ok) &&
      (val != doors_val_link_ko))
    {
    if (doors_io_c2c_decoder(llid, len, buf))
      KERR("%d %s", len, buf);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void time_resp_idx_from_doors(t_sc2c *c2c)
{
  c2c_abort_error(c2c,(char *) __FUNCTION__, __LINE__);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int callback_connect(void *ptr, int llid, int fd)
{
  int doors_llid;
  char *buf;
  t_sc2c *c2c = c2c_find_with_connect_llid(llid);
  if (c2c)
    {
    c2c->connect_llid = 0;
    if (c2c->peer_switch_llid == 0)
      {
      doors_llid = doorways_sock_client_inet_end(doors_type_switch, 
                                                 llid, fd,
                                                 c2c->slave_passwd,
                                                 peer_err_cb, peer_rx_cb);
      if (doors_llid)
        {
        llid_trace_alloc(doors_llid,c2c->name,0,0,type_llid_trace_endp_c2c);
        c2c->peer_switch_llid = doors_llid;
        c2c_set_state(c2c, state_master_waiting_idx);
        c2c_tx_req_idx_to_doors(c2c->name);
        c2c_arm_timer(c2c, 1500, time_resp_idx_from_doors);
        buf = clownix_malloc(10, 7);
        strcpy(buf, "OK");
        doorways_tx(doors_llid,0,doors_type_switch,doors_val_init_link,3,buf);
        }
      }
    else
      KERR("TWO CONNECTS FOR ONE REQUEST");
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void time_try_connect_peer(t_sc2c *c2c)
{
  if (!c2c->peer_switch_llid)
    {
    if (c2c->connect_llid)
      {
      close(get_fd_with_llid(c2c->connect_llid));
      doorways_sock_client_inet_delete(c2c->connect_llid);
      }
    c2c->peer_switch_llid = 0;
    c2c->connect_llid = doorways_sock_client_inet_start(c2c->ip_slave, 
                                                        c2c->port_slave,
                                                        callback_connect);
    c2c_arm_timer(c2c, 300, time_try_connect_peer);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void c2c_utils_init(void)
{ 
  g_head_c2c = NULL;
}
/*--------------------------------------------------------------------------*/

