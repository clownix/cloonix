/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
int util_read (char *ptr, int max, int fd);
int util_client_socket_unix(char *pname, int *fd);
int util_client_socket_inet(__u32 ip, __u16 port, int *fd);
int util_socket_listen_unix(char *pname);
int util_socket_listen_inet( __u16 port );
void util_fd_accept(int fd_listen, int *fd, const char *fct);
void util_free_fd(int fd);
int util_nonblock_client_socket_unix(char *pname, int *fd);
int util_nonblock_client_socket_inet(__u32 ip, __u16 port);
/*---------------------------------------------------------------------------*/















