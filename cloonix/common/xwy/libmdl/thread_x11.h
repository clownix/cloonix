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
int thread_x11_open(uint32_t randid, int server_side, int sock_fd_ass,
                    int x11_fd, int srv_idx, int cli_idx, int epfd,
                    int diag_thread_fd, int diag_main_fd,
                    t_msg *first_x11_msg);

void thread_x11_heartbeat(void);
void thread_x11_close(int sock_fd_ass);
void thread_x11_start(int sock_fd_ass);
void thread_x11_init(void);
/*---------------------------------------------------------------------------*/

