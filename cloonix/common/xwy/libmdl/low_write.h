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

typedef struct t_lw_el
{
  t_msg *msg_to_free;
  int fd_dst;
  char *data;
  int payload;
  int offst;
  t_outflow outflow;
  struct t_lw_el *next;
} t_lw_el;


int  low_write_not_empty(int s);
int  low_write_fd(int s);
void low_write_open(int s, int type, t_outflow outflow);
void low_write_modify(int s, int type);
void low_write_close(int s);
void low_write_init(void);
t_lw_el *low_write_alloc_el(t_msg *msg, char *data, int fd_dst,
                            int payload, t_outflow outflow);
int low_write_raw(int fd_dst, t_msg *msg, int all);
int low_write_levels_above_thresholds(int fd_dst);
