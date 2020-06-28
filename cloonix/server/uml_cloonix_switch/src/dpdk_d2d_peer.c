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
#include <errno.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "cfg_store.h"
#include "doorways_mngt.h"
#include "doors_rpc.h"
#include "doorways_sock.h"
#include "dpdk_d2d.h"
#include "dpdk_d2d_peer.h"

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
  t_d2d_cnx *cur = dpdk_d2d_find_with_peer_llid(llid);
  char *locnet = cfg_get_cloonix_name();
  if (!cur)
    KERR("%s", locnet);
  else
    {
    if (msg_exist_channel(llid))
      msg_delete_channel(llid);
    if (cur->peer_llid != llid)
      KERR("%s %d %d %s", locnet, cur->peer_llid, llid, cur->name);
    else
      {
      cur->peer_bad_status_received = 1;
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void peer_rx_cb(int llid, int tid, int type, int val, int len, char *buf)
{
  char *locnet = cfg_get_cloonix_name();
  if ((type == doors_type_switch) &&
      (val != doors_val_link_ok) &&
      (val != doors_val_link_ko))
    {
    if (doors_io_basic_decoder(llid, len, buf))
      KERR("%s %d %s", locnet, len, buf);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void peer_doorways_client_tx(int llid, int len, char *buf)
{
  doorways_tx(llid, 0, doors_type_switch, doors_val_none, len, buf);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int callback_connect(void *ptr, int llid, int fd)
{
  int doors_llid;
  char *buf;
  t_d2d_cnx *d2d = dpdk_d2d_find_with_peer_llid_connect(llid);
  char *locnet = cfg_get_cloonix_name();
  if (!d2d)
    KERR("%s", locnet);
  else
    {
    d2d->peer_llid_connect = 0;
    if (d2d->peer_llid)
      KERR("%s %s", locnet, d2d->name);
    else
      {
      doors_llid = doorways_sock_client_inet_end(doors_type_switch,
                                                 llid, fd,
                                                 d2d->dist_passwd,
                                                 peer_err_cb, peer_rx_cb);
      if (doors_llid)
        {
        d2d->peer_llid = doors_llid;
        buf = clownix_malloc(10, 7);
        strcpy(buf, "OK");
        doorways_tx(doors_llid,0,doors_type_switch,doors_val_init_link,3,buf);
        }
      }
    }
  return 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_disconnect_to_peer(t_d2d_cnx *cur)
{
  if (cur->peer_llid_connect)
    {
    close(get_fd_with_llid(cur->peer_llid_connect));
    doorways_sock_client_inet_delete(cur->peer_llid_connect);
    cur->peer_llid_connect = 0;
    }
  if (msg_exist_channel(cur->peer_llid))
    msg_delete_channel(cur->peer_llid);
  cur->peer_llid = 0;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_try_connect_to_peer(t_d2d_cnx *cur)
{
  int llid;
  char *locnet = cfg_get_cloonix_name();
  if (!cur->local_is_master)
    KERR("%s %s", locnet, cur->name);
  else if (cur->peer_llid)
    KERR("%s %s", locnet, cur->name);
  else
    {
    if (cur->peer_llid_connect)
      {
      close(get_fd_with_llid(cur->peer_llid_connect));
      doorways_sock_client_inet_delete(cur->peer_llid_connect);
      cur->peer_llid_connect = 0;
      }
    if (cur->peer_llid)
      KERR("%s PROBLEM %s", locnet, cur->name);
    if (msg_exist_channel(cur->peer_llid))
      KERR("%s PROBLEM %s", locnet, cur->name);
    if (cur->tcp_connection_peered)
      KERR("%s PROBLEM %s", locnet, cur->name);
    if (cur->udp_connection_peered)
      KERR("%s PROBLEM %s", locnet, cur->name);
    if (cur->ovs_lan_attach_ready)
      KERR("%s PROBLEM %s", locnet, cur->name);
    cur->ref_tid = get_new_tid();
    llid = doorways_sock_client_inet_start(cur->dist_tcp_ip, 
                                           cur->dist_tcp_port,
                                           callback_connect);
    if (llid == 0)
      KERR("%s %s", locnet, cur->name);
    cur->peer_llid_connect = llid; 
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void wrap_send_d2d_peer_create(t_d2d_cnx *cur, int is_ack)
{
  char *locnet = cfg_get_cloonix_name();
  if (msg_exist_channel(cur->peer_llid))
    {
    locnet = cfg_get_cloonix_name();
    if (cur->local_is_master)
      {
      doors_io_basic_tx_set(peer_doorways_client_tx);
      send_d2d_peer_create(cur->peer_llid, cur->ref_tid, cur->name,
                           is_ack, locnet, cur->dist_cloonix);
      doors_io_basic_tx_set(string_tx);
      }
    else
      {
      send_d2d_peer_create(cur->peer_llid, cur->ref_tid, cur->name,
                           is_ack, locnet, cur->dist_cloonix);
      }
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_send_d2d_peer_conf(t_d2d_cnx *cur, int is_ack)
{
  char *locnet = cfg_get_cloonix_name();
  if (msg_exist_channel(cur->peer_llid))
    {
    locnet = cfg_get_cloonix_name();  
    if (cur->local_is_master)
      {
      doors_io_basic_tx_set(peer_doorways_client_tx);
      send_d2d_peer_conf(cur->peer_llid, cur->ref_tid, cur->name,
                         is_ack, locnet, cur->dist_cloonix,
                         cur->loc_udp_ip, cur->dist_udp_ip,
                         cur->loc_udp_port, cur->dist_udp_port);
      doors_io_basic_tx_set(string_tx);
      }
    else
      {
      send_d2d_peer_conf(cur->peer_llid, cur->ref_tid, cur->name,
                         is_ack, locnet, cur->dist_cloonix,
                         cur->loc_udp_ip, cur->dist_udp_ip,
                         cur->loc_udp_port, cur->dist_udp_port);
      }
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_send_d2d_peer_ping(t_d2d_cnx *cur, int status)
{
  if (msg_exist_channel(cur->peer_llid))
    {
    if (cur->local_is_master)
      {
      doors_io_basic_tx_set(peer_doorways_client_tx);
      send_d2d_peer_ping(cur->peer_llid,cur->ref_tid,cur->name,status);
      doors_io_basic_tx_set(string_tx);
      }
    else
      {
      send_d2d_peer_ping(cur->peer_llid,cur->ref_tid,cur->name,status);
      }
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void wrap_send_d2d_peer_mac(t_d2d_cnx *cur, int nb_mac, t_d2d_mac *tabmac)
{
  if (msg_exist_channel(cur->peer_llid))
    {
    if (cur->local_is_master)
      {
      doors_io_basic_tx_set(peer_doorways_client_tx);
      send_d2d_peer_mac(cur->peer_llid,cur->ref_tid,cur->name,nb_mac,tabmac);
      doors_io_basic_tx_set(string_tx);
      }
    else
      {
      send_d2d_peer_mac(cur->peer_llid,cur->ref_tid,cur->name,nb_mac,tabmac);
      }
    }
}
/*---------------------------------------------------------------------------*/



