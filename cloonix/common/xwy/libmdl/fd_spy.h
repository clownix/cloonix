/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
int  fd_spy_add(int fd, int fd_type, int line);
void fd_spy_modify(int fd, int fd_type);
void fd_spy_del(int fd);
void fd_spy_set_info(int fd, int srv_idx, int cli_idx);
int  fd_spy_get_type(int fd, int *srv_idx, int *cli_idx);
void fd_spy_heartbeat(void);
void fd_spy_init(void);
/*---------------------------------------------------------------------------*/

