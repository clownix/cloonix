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
void sched_inc_queue_stored(int id, uint64_t len);
void sched_dec_queue_stored(int id, uint64_t len);
void sched_mngt(int id, uint64_t current_usec, uint64_t delta_usec);
int sched_can_enqueue(int id, uint64_t len, uint8_t *buf);
void sched_cnf(int dir, char *type, int val);
void sched_init(void);
/*--------------------------------------------------------------------------*/

