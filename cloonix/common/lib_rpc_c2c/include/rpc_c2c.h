/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
void doors_io_c2c_init(t_llid_tx llid_tx);
int doors_io_c2c_decoder (int llid, int len, char *buf);
/*---------------------------------------------------------------------------*/
void send_c2c_peer_create(int llid, int tid, char *name,
                          char *master_cloonix, int master_idx,
                          int ip_slave, int port_slave);

void recv_c2c_peer_create(int llid, int tid, char *name,
                          char *master_cloonix, int master_idx,
                          int ip_slave, int port_slave);
/*---------------------------------------------------------------------------*/
void send_c2c_ack_peer_create(int llid, int tid, char *name, 
                              char *master_cloonix, char *slave_cloonix, 
                              int master_idx, int slave_idx, int status);

void recv_c2c_ack_peer_create(int llid, int tid, char *name, 
                              char *master_cloonix, char *slave_cloonix, 
                              int master_idx, int slave_idx, int status);
/*---------------------------------------------------------------------------*/
void send_c2c_peer_ping(int llid, int tid, char *name);
void recv_c2c_peer_ping(int llid, int tid, char *name);
/*---------------------------------------------------------------------------*/
void send_c2c_peer_delete(int llid, int tid, char *name);
void recv_c2c_peer_delete(int llid, int tid, char *name);
/*---------------------------------------------------------------------------*/
void doors_io_c2c_tx_set(t_llid_tx llid_tx);
/*---------------------------------------------------------------------------*/

