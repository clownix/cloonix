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
t_clo *get_head_clo(void);
int clo_mngt_low_input(t_clo *clo, t_low *low, int *inserted);
void clo_mngt_high_input(t_clo *clo, int hlen, u8_t *hdata);
t_clo *clo_mngt_create_tcp(t_tcp_id *tcpid);
int clo_mngt_delete_tcp(t_clo *clo);
void clo_mngt_set_state(t_clo *clo, int state);
int clo_mngt_get_state(t_clo *clo);
u32_t get_g_50ms_count(void);
void clo_mngt_get_ackno_seqno_wnd(t_clo *clo, u32_t *ackno, u32_t *seqno, 
                                  u16_t *rwnd, u16_t *twnd);
int  clo_mngt_adjust_loc_wnd(t_clo *clo, u16_t loc_wnd);
void clo_mngt_adjust_send_next(t_clo *clo, u32_t seqno, int len);
void clo_mngt_adjust_recv_next(t_clo *clo, u32_t ackno, int len);
int  clo_mngt_get_new_rxdata(t_clo *clo, int *len, u8_t **data);
int  clo_mngt_authorised_to_send_nexttx(t_clo *clo);
int clo_mngt_get_txdata(t_clo *clo, t_hdata *hd, u32_t *seqno, 
                         int *len, u8_t **data);
void clo_mngt_timer(void);
void clo_mngt_init(void);
/*--------------------------------------------------------------------------*/

