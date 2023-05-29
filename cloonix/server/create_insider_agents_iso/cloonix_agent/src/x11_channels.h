/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
int  x11_pool_alloc(void);
void x11_pool_release(int idx);
int  x11_get_biggest_fd(void);
void x11_write(int idx_fd_x11, int len, char *buf);
void x11_process_events(fd_set *infd);
void x11_prepare_fd_set(fd_set *infd, fd_set *outfd);
void x11_rx_ack_open_x11(int dido_llid, char *rx);
void x11_event_listen(int dido_llid, int x11_display_idx, int fd_x11_listen);
void x11_init(void);
/*--------------------------------------------------------------------------*/


