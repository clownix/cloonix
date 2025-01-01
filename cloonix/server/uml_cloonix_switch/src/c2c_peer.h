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
enum {
  state_idle = 0,
  state_master_up_initialised,
  state_master_try_connect_to_peer,
  state_master_connection_peered,
  state_master_udp_peer_conf_sent,
  state_master_udp_peer_conf_received,
  state_master_up_process_running,

  state_slave_up_initialised,
  state_slave_connection_peered,
  state_slave_udp_peer_conf_sent,
  state_slave_udp_peer_conf_received,
  state_slave_up_process_running,
};


void wrap_proxy_callback_connect(char *name, int pair_llid);
void wrap_disconnect_to_peer(t_ovs_c2c *c2c);
void wrap_try_connect_to_peer(t_ovs_c2c *c2c);
void wrap_send_c2c_peer_create(t_ovs_c2c *c2c, int is_ack);
void wrap_send_c2c_peer_ping(t_ovs_c2c *c2c,  int status);
int wrap_send_c2c_peer_conf(t_ovs_c2c *cur, int is_ack);
/*---------------------------------------------------------------------------*/



