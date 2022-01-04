/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>


#define KOUT(format, a...)                          \
 do {                                               \
    syslog(LOG_ERR | LOG_USER, "KILL"               \
    " line:%d   " format "\n\n", __LINE__, ## a);   \
    exit(-1);                                       \
    } while (0)


/*****************************************************************************/
static void tempo_connect(int no_use)
{
  printf("SIGNAL Timeout in connect\n");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static int cli_socket_unix(char *pname)
{
  int len,  sock, ret;
  struct sockaddr_un addr;
  struct sigaction act;
  struct sigaction oldact;
  sock = socket (AF_UNIX,SOCK_STREAM,0);
  if (sock <= 0)
    KOUT(" ");
  memset (&addr, 0, sizeof (struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, pname);
  len = sizeof (struct sockaddr_un);
  memset(&act, 0, sizeof(act));
  sigfillset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = tempo_connect;
  sigaction(SIGALRM, &act, &oldact);
  alarm(5);
  ret = connect(sock,(struct sockaddr *) &addr, len);
  sigaction(SIGALRM, &oldact, NULL);
  alarm(0);
  if (ret != 0)
    KOUT("NO SERVER LISTENING TO %s", pname);
  return sock;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void action_read_and_print(int fd)
{
  char buf[101];
  int rx_len = read (fd, buf, 100);
  if (rx_len <= 0)
    KOUT(" ");
  printf("%s\n", buf);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{
  fd_set infd;
  int ret, fd_echo = cli_socket_unix("/tmp/cloonix_parrot");
  int tx_len = write(fd_echo, "cloonix", strlen("cloonix") + 1);
  FD_ZERO(&infd);
  FD_SET(fd_echo, &infd);
  ret = select(fd_echo + 1, &infd, NULL, NULL, NULL);
  if (ret <= 0)
    KOUT(" ");
  if (FD_ISSET(fd_echo, &infd))
    action_read_and_print(fd_echo);
  return 0;
}
/*--------------------------------------------------------------------------*/
