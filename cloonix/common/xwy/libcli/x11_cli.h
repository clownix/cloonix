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
void x11_cli_set_params(int cli_idx, int llid_ass, int tid, int type);
int x11_cli_get_srv_idx(int cli_idx);
void x11_cli_write_to_x11(int cli_idx, int len, char *buf);
void x11_cli_init(t_sock_fd_ass_close sock_fd_ass_close);
int x11_fd_epollin_epollout_action(uint32_t evts, int fd);
void x11_fd_epollin_epollout_setup(void);
void rx_x11_msg_cb(uint32_t randid, int llid, int type,
                   int srv_idx, int cli_idx, t_msg *msg);
/*--------------------------------------------------------------------------*/

