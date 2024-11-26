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
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include "proxy_crun_access.h"

/****************************************************************************/
static void com(int src, int dst)
{
    char buf[1024 * 4];
    int r, i, j;

    r = read(src, buf, 1024 * 4);

    while (r > 0) {
        i = 0;

        while (i < r) {
            j = write(dst, buf + i, r - i);

            if (j == -1) {
                KOUT("write");
            }

            i += j;
        }

        r = read(src, buf, 1024 * 4);
    }

    if (r == -1) {
        KOUT("read");
    }

    shutdown(src, SHUT_RD);
    shutdown(dst, SHUT_WR);
    close(src);
    close(dst);
    exit(0);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int open_tmp_cloonix_unix_sock(char *unix_path)
{
  int len,  sock;
  struct sockaddr_un addr;
  sock = socket (AF_UNIX,SOCK_STREAM,0);
  if (sock <= 0)
    KOUT("ERROR SOCKET CREATE");
  memset (&addr, 0, sizeof (struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, unix_path);
  len = sizeof (struct sockaddr_un);
  if (connect(sock,(struct sockaddr *) &addr, len))
    KOUT("ERROR CONNECT %s %d", unix_path, errno);
  return sock;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void forward_traffic(int client_socket, char *unix_path)
{
    int forward_socket;
    pid_t down_pid;
    forward_socket = open_tmp_cloonix_unix_sock(unix_path);
    down_pid = fork();
    if (down_pid == -1) {
        KOUT("fork");
    }
    if (down_pid == 0) {
        com(forward_socket, client_socket);
    } else {
        com(client_socket, forward_socket);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void accept_incoming_connect(int sock, char *unix_path)
{
    int client_socket;
    pid_t up_pid;

    client_socket = accept(sock, NULL, NULL);

    if (client_socket == -1) {
        KOUT("accept");
    }
    up_pid = fork();
    if (up_pid == -1)
        KOUT("fork");

    if (up_pid == 0) {
        forward_traffic(client_socket, unix_path);
        exit(1);
    }

    close(client_socket);
}
/*--------------------------------------------------------------------------*/
