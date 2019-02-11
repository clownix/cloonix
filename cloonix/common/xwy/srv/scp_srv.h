/*****************************************************************************/
/*    Copyright (C) 2017-2019 cloonix@cloonix.net License AGPL-3             */
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
int recv_scp_data_end(uint32_t randid, int scp_fd, int sock_fd);
int recv_scp_data(int scp_fd, t_msg *msg);
int recv_scp_open(uint32_t randid, int type, int sock_fd,
                  int *cli_scp_fd, char *buf);
int send_scp_to_cli(uint32_t randid, int scp_fd, int sock_fd);
/*--------------------------------------------------------------------------*/
