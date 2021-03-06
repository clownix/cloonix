/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
void pcap_file_unlink(void);
void pcap_close_and_reinit(t_all_ctx *all_ctx);
int pcap_file_is_recording(t_all_ctx *all_ctx);
void pcap_file_start_end(t_all_ctx *all_ctx);
void pcap_file_rx_packet(t_all_ctx *all_ctx, long long usec, 
                         int len, char *buf);
void pcap_file_init(t_all_ctx *all_ctx, char *net_name, char *name, char *snf);
/*--------------------------------------------------------------------------*/


