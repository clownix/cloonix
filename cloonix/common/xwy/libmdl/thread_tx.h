/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
void thread_tx_start(int dst_fd);
void thread_tx_abort(int dst_fd);
int  thread_tx_levels_above_thresholds(int dst_fd);
int  thread_tx_get_levels(int dst_fd, int *used_slot_nb, int *stored_bytes);
int  thread_tx_add_el(t_lw_el *el, int len);
void thread_tx_close(int dst_fd);
void thread_tx_open(int dst_fd);
void thread_tx_purge_el(int dst_fd);
void thread_tx_init(void);
/*--------------------------------------------------------------------------*/

