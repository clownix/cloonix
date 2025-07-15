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
void nat_tcp_del(t_ctx_nat *ctx);
void nat_tcp_stop_go(t_ctx_nat *ctx, uint32_t sip, uint32_t dip,
                     uint16_t sport, uint16_t dport, int stop);
int nat_tcp_stream_proxy_req(t_ctx_nat *ctx, char *stream,
                             uint32_t sip, uint32_t dip,
                             uint16_t sport, uint16_t dport);
void nat_tcp_init(void);
/*--------------------------------------------------------------------------*/

