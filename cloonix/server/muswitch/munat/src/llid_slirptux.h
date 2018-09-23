/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
void llid_clo_low_output(int mac_len, u8_t *mac_data);
int  llid_clo_data_rx_possible(t_tcp_id *tcpid);
void llid_clo_data_rx(t_tcp_id *tcpid, int len, u8_t *data);
void llid_clo_high_syn_rx(t_tcp_id *tcpid);
void llid_clo_high_synack_rx(t_tcp_id *tcpid);
void llid_clo_high_close_rx(t_tcp_id *tcpid);



void llid_slirptux_tcp_close_llid(int llid);
void llid_slirptux_tcp_rx_from_llid(int llid, int len, char *rx_buf);
int  llid_slirptux_tcp_tx_to_slirptux_possible(int llid);
void llid_slirptux_tcp_rx_from_slirptux(int mac_len, char *mac_data);
void llid_slirptux_tcp_init(void);
/*---------------------------------------------------------------------------*/
