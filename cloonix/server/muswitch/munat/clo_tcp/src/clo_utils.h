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
char *util_state2ascii(int state);
int util_low_tcpid_comp(t_low *low, t_tcp_id *tcpid);
void util_low2tcpid(t_tcp_id *tcpid, t_low *low);
void util_tcpid2low(t_low *low, t_tcp_id *tcpid);
t_ldata *util_insert_ldata(t_ldata **head, t_low *low);
void util_extract_ldata(t_ldata **head, t_ldata *ldata);

int util_get_space_left_hdata(t_hdata *head, t_hdata **elected);
void util_space_left_fill_hdata(t_hdata *elected, int len, u8_t *data);
t_hdata *util_insert_hdata(t_hdata **head, int max_len, int hlen, u8_t *hdata);
void util_extract_hdata(t_hdata **head, t_hdata *hdata);
t_clo *util_insert_clo(t_clo **head, t_tcp_id *tcpid);
int util_extract_clo(t_clo **head, t_clo *clo);

void util_send_reset(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd, 
                     const char *fct, int ln);
void util_send_syn(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd);
void util_send_ack(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd);
void util_send_synack(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd);
void util_send_finack(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd);
void util_send_fin(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd);
void util_send_data(t_tcp_id *tcpid, u32_t ackno, u32_t seqno, u16_t wnd,
                    int hlen, u8_t *hdata);
void util_silent_purge_hdata_and_ldata(t_clo *clo);
/*--------------------------------------------------------------------------*/

