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
#define MIN_SIMU_FD 0
#define MAX_SIMU_FD CLOWNIX_MAX_CHANNELS
void simu_tx_send(int llid, int len, char *buf);
void simu_tx_fdset(int llid);
void simu_tx_fdiset(int llid);
int  simu_tx_open(int llid, int sock_fd, struct epoll_event *sock_fd_epev);
void simu_tx_close(int llid);
void simu_tx_init(void);
/*--------------------------------------------------------------------------*/

