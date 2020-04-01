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
int pty_fork_free_with_sock_fd(int sock_fd);
void pty_fork_msg_type_data_pty(int sock_fd, t_msg *msg);
void pty_fork_msg_type_win_size(int sock_fd, int len, char *buf);
int pty_fork_xauth_add_magic_cookie(int display_val, char *magic_cookie);
int pty_fork_get_max_fd(int val);
void pty_fork_fdisset(fd_set *readfds, fd_set *writefds);
void pty_fork_fdset(fd_set *readfds, fd_set *writefds);
void pty_fork_bin_bash(int action, uint32_t randid, int sock_fd,
                         char *cmd, int display_val);
void pty_fork_init(void);
/*--------------------------------------------------------------------------*/
