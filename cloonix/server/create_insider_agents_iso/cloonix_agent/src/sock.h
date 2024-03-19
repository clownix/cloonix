/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include "header_sock.h"

int  sock_open_virtio_port(char *path);
int  sock_client_unix(char *pname);
int sock_fd_accept(int fd_listen);
int util_socket_listen_unix(char *pname);


int  sock_header_get_size(void);
void sock_header_set_info(char *tx,
                          int dido_llid, int len, int type, int val,
                          char **ntx);
int sock_header_get_info(char *rx,
                          int *dido_llid, int *len, int *type, int *val,
                          char **nrx);
/*---------------------------------------------------------------------------*/

