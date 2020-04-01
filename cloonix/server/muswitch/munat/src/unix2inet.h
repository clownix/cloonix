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
void unix2inet_close_tcpid(t_tcp_id *tcpid);
void unix2inet_arp_resp(char *mac, char *ip);
int unix2inet_ssh_syn_ack_arrival(t_tcp_id *tcp_id);
void free_unix2inet_conpath(t_all_ctx *all_ctx, int llid_con);
void unix2inet_finack_state(t_tcp_id *tcpid, int line);
void unix2inet_init(t_all_ctx *all_ctx);
/*--------------------------------------------------------------------------*/

