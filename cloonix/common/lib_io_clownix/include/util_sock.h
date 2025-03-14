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
void channel_rx_slow_down(int llid);
int util_read_brakes_on(char *ptr, int max, int fd, int llid);
int util_read (char *ptr, int max, int fd);
int util_proxy_client_socket_unix(char *pname, int *fd);
int util_client_socket_unix(char *pname, int *fd);
int util_client_socket_inet(uint32_t ip, uint16_t port, int *fd);
int util_socket_unix_dgram(char *pname);
int util_socket_listen_unix(char *pname);
int util_socket_listen_inet( uint16_t port );
void util_fd_accept(int fd_listen, int *fd, const char *fct);
void util_free_fd(int fd);
int util_nonblock_client_socket_unix(char *pname, int *fd);
int util_nonblock_client_socket_inet(uint32_t ip, uint16_t port);
/*---------------------------------------------------------------------------*/















