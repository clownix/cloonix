/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
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

static int get_new_tid(void)
{
  static int tid = 0;
  tid += 1;
  if (tid == 10000)
    tid = 1;
  return tid;
}

static void peer_doorways_client_tx(int llid, int len, char *buf)
{
  t_ovs_c2c *cur;
  static const int MAGIC_PREFIX_LEN = 8;
  if (len <= MAGIC_PREFIX_LEN)
    {
    KERR("ERROR %d too short", len);
    return;
    }
  buf += MAGIC_PREFIX_LEN;
  len -= MAGIC_PREFIX_LEN;
  cur = ovs_c2c_find_with_pair_llid(llid);
  if (!cur)
    KERR("ERROR: no c2c behind llid %d for %d bytes: %.*s", llid,
         len, len, buf);
  else
    proxycrun_transmit_proxy_data(cur->name, len, buf);
}

void peer_doorways_client_tx_status(char *name, int status)
{
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    KTRC("%s: a tx status (%d) arrived for a c2c that is gone", name,
         status);
  else
    {
    if (status)
      carefull_destroy_c2c(cur);
    }
}

void wrap_proxy_callback_connect(char *name, int pair_llid)
{
  t_ovs_c2c *cur = find_c2c(name);
  if (!cur)
    KERR("ERROR %s %d", name, pair_llid);
  cur->pair_llid = pair_llid;
  if (!cur->topo.local_is_master)
    {
    cur->peer_llid = pair_llid;
    cur->topo.tcp_connection_peered = 1;
    c2c_state_progress_up(cur, state_slave_connection_peered);
    wrap_send_c2c_peer_create(cur, 1);
    }
}

void wrap_try_connect_to_peer(t_ovs_c2c *cur)
{
  if (!cur->topo.local_is_master)
    KERR("ERROR %s", cur->name);
  else if (cur->peer_llid)
    KERR("ERROR %s", cur->name);
  else
    {
    if (cur->peer_listen_llid)
      {
      doorways_sock_client_inet_delete(cur->peer_listen_llid);
      cur->peer_listen_llid = 0;
      }
    if (cur->peer_llid)
      KERR("ERROR PROBLEM %s", cur->name);
    if (msg_exist_channel(cur->peer_llid))
      KERR("ERROR PROBLEM %s", cur->name);
    if (cur->topo.tcp_connection_peered)
      KERR("ERROR PROBLEM %s", cur->name);
    if (cur->topo.udp_connection_peered)
      KERR("ERROR PROBLEM %s", cur->name);
    cur->ref_tid = get_new_tid();
    c2c_state_progress_up(cur, state_master_try_connect_to_peer);
    proxycrun_transmit_dist_tcp_ip_port(cur->name, cur->topo.dist_tcp_ip,
                                        cur->topo.dist_tcp_port,
                                        cur->dist_passwd);
    }
}

static int pair_peer(int local_is_master, int pair_llid, int peer_llid)
{
  int llid = 0;
  if (local_is_master && pair_llid)
    {
    llid = pair_llid;
    }
  else if ((!local_is_master) && peer_llid && (msg_exist_channel(peer_llid)))
    {
    llid = peer_llid;
    }
  return llid;
}

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

void wrap_send_c2c_peer_ping(t_ovs_c2c *cur, int status_peer_ping)
{
  int llid;
  llid = pair_peer(cur->topo.local_is_master, cur->pair_llid, cur->peer_llid);
  if (!llid)
    {
    /* No llid can mean two opposite things, and only one of them is a fault. */
    if ((status_peer_ping == status_peer_ping_zero) ||
        (status_peer_ping == status_peer_ping_error))
      {
      }
    else if (cur->topo.tcp_connection_peered == 0)
      {
      KTRC("%s: no llid to ping the peer on yet, the tcp connection is "
           "still being made", cur->name);
      }
    else
      {
      KERR("WARNING %s: the llid to ping the peer on is gone, destroying "
           "this side", cur->name);
      carefull_destroy_c2c(cur);
      }
    }
  else
    {
    if (cur->topo.local_is_master)
      {
      doors_io_basic_tx_set(peer_doorways_client_tx);
      send_c2c_peer_ping(llid, cur->ref_tid, cur->name, status_peer_ping);
      doors_io_basic_tx_set(string_tx);
      }
    else
      {
      send_c2c_peer_ping(llid, cur->ref_tid, cur->name, status_peer_ping);
      }
    }
}

