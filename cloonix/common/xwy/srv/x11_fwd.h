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
void x11_connect_ack(int srv_idx, int cli_idx, char *txt);
int  x11_init_cli_msg(uint32_t randid, int sock_fd, char *magic_cookie);
int  x11_alloc_display(uint32_t randid, int srv_idx);
void x11_free_display(int srv_idx);
void x11_init_display(void);
void x11_fdisset(fd_set *readfds, fd_set *writefds);
void x11_fdset(fd_set *readfds, fd_set *writefds);
int  x11_get_max_fd(int max);
void x11_info_flow(uint32_t randid, int sock_fd, int srv_idx, int cli_idx,
                   char *buf);
/*--------------------------------------------------------------------------*/




