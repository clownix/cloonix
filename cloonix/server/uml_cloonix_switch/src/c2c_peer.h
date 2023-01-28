/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
void wrap_disconnect_to_peer(t_ovs_c2c *c2c);
void wrap_try_connect_to_peer(t_ovs_c2c *c2c);
void wrap_send_c2c_peer_create(t_ovs_c2c *c2c, int is_ack);
void wrap_send_c2c_peer_ping(t_ovs_c2c *c2c,  int status);
int wrap_send_c2c_peer_conf(t_ovs_c2c *cur, int is_ack);
/*---------------------------------------------------------------------------*/



