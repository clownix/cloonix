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
int  scp_get_fd(void);
void scp_close_fd(void);
void scp_send_data(void);
void scp_send_data_end(void);

void rx_scp_msg_cb(void *ptr, int llid, int sock_fd, t_msg *msg);
void rx_scp_err_cb (void *ptr, int llid, int fd, char *err);

void scp_send_put(int llid, int tid, int type,
                  t_sock_fd_tx soc_tx, char *src, char *dst);

void scp_send_get(int llid, int tid, int type,
                  t_sock_fd_tx soc_tx, char *src, char *dst);

/*--------------------------------------------------------------------------*/
