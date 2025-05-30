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
int tcp_qstore_qty1(t_flagseq *flseq);
int tcp_qstore_qty2(t_flagseq *flseq);
int tcp_qstore_qty(t_flagseq *flseq);
uint8_t *tcp_qstore_get_backup(t_flagseq *flseq, int rank,
                               int *data_len, uint32_t *local_seq);
void tcp_qstore_flush_backup_seq(t_flagseq *flseq, uint32_t local_seq);
uint8_t *tcp_qstore_dequeue(t_flagseq *flseq, int *data_len, int local_seq);
void tcp_qstore_enqueue(t_flagseq *flagseq, int data_len, uint8_t *data);
int tcp_qstore_flush(t_flagseq *flagseq);
void tcp_qstore_init(t_flagseq *flseq);
/*--------------------------------------------------------------------------*/
