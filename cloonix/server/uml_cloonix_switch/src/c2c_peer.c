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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"
#include "cfg_store.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "doorways_sock.h"
#include "ovs_c2c.h"
#include "c2c_peer.h"
#include "uml_clownix_switch.h"
#include "proxycrun.h"

void c2c_state_progress_up(t_ovs_c2c *cur, int state);

/****************************************************************************/
static int get_new_tid(void)
{
  static int tid = 0;
  tid += 1;
  if (tid == 10000)
    tid = 1;
  return tid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void peer_err_cb (int llid)
{
  t_ovs_c2c *cur = ovs_c2c_find_with_peer_llid(llid);
  char *locnet = cfg_get_cloonix_name();
  if (!cur)
    KERR("ERROR PEER %s %d", locnet, llid);
  else
    {
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    if (cur->peer_llid != llid)
      KERR("ERROR PEER %s %d %d %s",locnet,cur->peer_llid,llid,cur->name);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void peer_rx_cb(int llid, int tid,
                       int len_bufraw, char *doors_bufraw,
                       int type, int val, int len, char *buf)
{
  char *locnet = cfg_get_cloonix_name();
  if ((type == doors_type_switch) &&
      (val != doors_val_link_ok) &&
      (val != doors_val_link_ko))
    {
    if (doors_io_basic_decoder(llid, len, buf))
      KERR("ERROR DECODER %s %d %s", locnet, len, buf);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void peer_doorways_client_tx(int llid, int len, char *buf)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur;
  if (!get_proxy_is_on())
    {
    doorways_tx_bufraw(llid, 0, doors_type_switch, doors_val_c2c, len, buf);
    }
  else
    {
    cur = ovs_c2c_find_with_pair_llid(llid);
    if (!cur)
      KERR("ERROR %s %d %d %s", locnet, llid, len, buf);
    else
      {
      proxycrun_transmit_proxy_is_on_data(cur->name, len, buf);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void wrap_proxy_callback_connect(char *name, int pair_llid)
{
  char *locnet = cfg_get_cloonix_name();
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    KERR("ERROR %s %s %d", locnet, name, pair_llid);
  cur->pair_llid = pair_llid;
  if (!cur->topo.local_is_master)
    {
    cur->topo.tcp_connection_peered = 1;
    c2c_state_progress_up(cur, state_slave_connection_peered);
    wrap_send_c2c_peer_create(cur, 1);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int callback_connect(int llid, int fd)
{
  char *locnet = cfg_get_cloonix_name();
  int peer_llid;
  t_ovs_c2c *c2c = ovs_c2c_find_with_peer_listen_llid(llid);
  if (!c2c)
    KERR("ERROR %s", locnet);
  else
    {
    if (c2c->peer_llid)
      KERR("ERROR %s %s", locnet, c2c->name);
    else
      {
      peer_llid = doorways_sock_client_inet_end(doors_type_switch,
                                                 llid, fd,
                                                 c2c->dist_passwd,
                                                 peer_err_cb, peer_rx_cb);
      if (peer_llid)
        {
        c2c->peer_llid = peer_llid;
        if (doorways_sig_bufraw(peer_llid, llid, doors_type_switch,
                                doors_val_init_link, "OK"))
          KERR("ERROR %s %s", locnet, c2c->name);
        if (msg_exist_channel(c2c->peer_listen_llid))
          msg_delete_channel(c2c->peer_listen_llid);
        c2c->peer_listen_llid = 0;
        }
      }
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_disconnect_to_peer(t_ovs_c2c *cur)
{
  if (cur->peer_listen_llid)
    {
    close(get_fd_with_llid(cur->peer_listen_llid));
    doorways_sock_client_inet_delete(cur->peer_listen_llid);
    cur->peer_listen_llid = 0;
    }
  if (msg_exist_channel(cur->peer_llid))
    msg_delete_channel(cur->peer_llid);
  cur->peer_llid = 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_try_connect_to_peer(t_ovs_c2c *cur)
{
  int llid;
  char *locnet = cfg_get_cloonix_name();
  if (!cur->topo.local_is_master)
    KERR("ERROR %s %s", locnet, cur->name);
  else if (cur->peer_llid)
    KERR("ERROR %s %s", locnet, cur->name);
  else
    {
    if (cur->peer_listen_llid)
      {
      close(get_fd_with_llid(cur->peer_listen_llid));
      doorways_sock_client_inet_delete(cur->peer_listen_llid);
      cur->peer_listen_llid = 0;
      }
    if (cur->peer_llid)
      KERR("ERROR %s PROBLEM %s", locnet, cur->name);
    if (msg_exist_channel(cur->peer_llid))
      KERR("ERROR %s PROBLEM %s", locnet, cur->name);
    if (cur->topo.tcp_connection_peered)
      KERR("ERROR %s PROBLEM %s", locnet, cur->name);
    if (cur->topo.udp_connection_peered)
      KERR("ERROR %s PROBLEM %s", locnet, cur->name);
    cur->ref_tid = get_new_tid();
    c2c_state_progress_up(cur, state_master_try_connect_to_peer);
    if (get_proxy_is_on())
      {
      proxycrun_transmit_dist_tcp_ip_port(cur->name, cur->topo.dist_tcp_ip,
                                          cur->topo.dist_tcp_port,
                                          cur->dist_passwd);
      }
    else
      {
      llid = doorways_sock_client_inet_start(cur->topo.dist_tcp_ip, 
                                             cur->topo.dist_tcp_port,
                                             callback_connect);
      if (llid == 0)
        KERR("ERROR %s %s", locnet, cur->name);
      cur->peer_listen_llid = llid; 
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int pair_peer(int local_is_master, int pair_llid, int peer_llid)
{
  char *locnet = cfg_get_cloonix_name();
  int llid = 0,  proxy_is_on = get_proxy_is_on();
  if (local_is_master && proxy_is_on && pair_llid)
    llid = pair_llid;
  else if ((!local_is_master) && proxy_is_on &&
           peer_llid && (msg_exist_channel(peer_llid)))
    llid = peer_llid;
  else if ((!proxy_is_on) && peer_llid && (msg_exist_channel(peer_llid)))
    llid = peer_llid;
  else
    KERR("ERROR %s master:%d proxy_is_on:%d  pair_llid:%d peer_llid,:%d %d",
         locnet, local_is_master, proxy_is_on, pair_llid, peer_llid,
         msg_exist_channel(peer_llid));
  return llid;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_send_c2c_peer_create(t_ovs_c2c *cur, int is_ack)
{
  char *locnet = cfg_get_cloonix_name();
  int llid;
  llid = pair_peer(cur->topo.local_is_master, cur->pair_llid, cur->peer_llid);
  if (llid)
    {
    if (cur->topo.local_is_master)
      {
      doors_io_basic_tx_set(peer_doorways_client_tx);
      send_c2c_peer_create(llid, cur->ref_tid, cur->name,
                           is_ack, locnet, cur->topo.dist_cloon);
      doors_io_basic_tx_set(string_tx);
      }
    else
      {
      send_c2c_peer_create(llid, cur->ref_tid, cur->name,
                           is_ack, locnet, cur->topo.dist_cloon);
      }
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int wrap_send_c2c_peer_conf(t_ovs_c2c *cur, int is_ack)
{
  char *locnet = cfg_get_cloonix_name();
  int llid, result = -1;
  llid = pair_peer(cur->topo.local_is_master, cur->pair_llid, cur->peer_llid);
  if (llid)
    {
    if (cur->topo.local_is_master)
      {
      doors_io_basic_tx_set(peer_doorways_client_tx);
      send_c2c_peer_conf(llid, cur->ref_tid, cur->name,
                         is_ack, locnet, cur->topo.dist_cloon,
                         cur->topo.loc_udp_ip, cur->topo.dist_udp_ip,
                         cur->topo.loc_udp_port, cur->topo.dist_udp_port);
      doors_io_basic_tx_set(string_tx);
      c2c_state_progress_up(cur, state_master_udp_peer_conf_sent);
      }
    else
      {
      send_c2c_peer_conf(llid, cur->ref_tid, cur->name,
                         is_ack, locnet, cur->topo.dist_cloon,
                         cur->topo.loc_udp_ip, cur->topo.dist_udp_ip,
                         cur->topo.loc_udp_port, cur->topo.dist_udp_port);
      c2c_state_progress_up(cur, state_slave_udp_peer_conf_sent);
      }
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_send_c2c_peer_ping(t_ovs_c2c *cur, int status)
{
  int llid;
  llid = pair_peer(cur->topo.local_is_master, cur->pair_llid, cur->peer_llid);
  if (llid)
    {
    if (cur->topo.local_is_master)
      {
      doors_io_basic_tx_set(peer_doorways_client_tx);
      send_c2c_peer_ping(llid, cur->ref_tid, cur->name, status);
      doors_io_basic_tx_set(string_tx);
      }
    else
      {
      send_c2c_peer_ping(llid, cur->ref_tid, cur->name, status);
      }
    }
}
/*---------------------------------------------------------------------------*/



