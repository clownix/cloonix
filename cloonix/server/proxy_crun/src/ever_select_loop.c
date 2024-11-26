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
#include <linux/tcp.h>

#include "proxy_crun_access.h"


static char g_unix_main[MAX_PATH_LEN];
static char g_unix_web[MAX_PATH_LEN];
static int g_sock_main;
static int g_sock_web;
static int g_msg_rx_done;

/****************************************************************************/
static int get_sigbuf(int *sig_client, char *sigbuf)
{
  int len, result = 0;;
  memset(sigbuf, 0, MAX_SIGBUF_LEN);
  len = read(*sig_client, sigbuf, MAX_SIGBUF_LEN-1);
  if (len == 0)
    {
    result = -1;
    *sig_client = -1;
    if (g_msg_rx_done)
      exit(-1);
    }
  else if (len < 0)
    {
    result = -1;
    if ((errno == EINTR) || (errno == EAGAIN))
      KOUT("ERROR SIG NEG");
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int util_socket_listen_inet(uint16_t port)
{
  struct sockaddr s_addr;
  int fd;
  int optval=1;
  int nodelay = 1;

  memset(&s_addr, 0, sizeof(s_addr));
  s_addr.sa_family = AF_INET;
  ((struct sockaddr_in*)&s_addr)->sin_addr.s_addr = htonl(INADDR_ANY);
  ((struct sockaddr_in*)&s_addr)->sin_port = htons( port );

  fd = socket( s_addr.sa_family, SOCK_STREAM, 0 );
  if ( fd <= 0 )
    KOUT("%d", errno);
  if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval))<0)
    KOUT(" ");
  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*) &nodelay, sizeof(int)))
    KOUT(" ");

  if ( bind(fd, &s_addr,sizeof(struct sockaddr_in) ) < 0 )
    {
    close(fd);
    fd = -1;
    }
  else
    {
    if ( listen(fd, 50) < 0 )
      {
      close(fd);
      fd = -1;
      }
    }
  return(fd);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int open_tcp_sock(uint16_t port)
{
  int flags, sock;
  struct sockaddr_in addr;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1)
    KOUT("socket");
  bzero((char *) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port); 
  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1)
    KOUT("bind");
  if (listen(sock, 50) == -1)
    KOUT("listen");
  flags = fcntl(sock, F_GETFL);
  if (flags < 0)
    KOUT("ERROR %s", strerror(errno));
  if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
    KOUT("ERROR %s", strerror(errno));
  fcntl(sock, F_SETFD, FD_CLOEXEC);
  return sock;
}
/*--------------------------------------------------------------------------*/
    
/****************************************************************************/
static void set_config(char *net, uint16_t pmain, uint16_t pweb)
{ 
  int fd;
  fd = util_socket_listen_inet(pmain);
  if (fd < 0)
    KOUT("PORT:%d is in use", pmain);
  close(fd);
  fd = util_socket_listen_inet(pweb);
  if (fd < 0)
    KOUT("PORT:%d is in use", pweb);
  close(fd);
  snprintf(g_unix_main, MAX_PATH_LEN-1, "%s/proxy_%s_main.sock",
                                        get_proxyshare(), net);
  snprintf(g_unix_web, MAX_PATH_LEN-1,  "%s/proxy_%s_nginx.sock",
                                        get_proxyshare(), net);
  g_sock_main = open_tcp_sock(pmain);
  g_sock_web  = open_tcp_sock(pweb);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void prepare_readfds(fd_set *readfds, int sock_sig, int sig_client, 
                            int *sock_max)
{
  FD_ZERO(readfds);
  if (sig_client == -1)
    {
    FD_SET(sock_sig, readfds);
    *sock_max = sock_sig;
    }
  else
    {
    FD_SET(sig_client, readfds);
    *sock_max = sig_client;
    if (g_sock_main != -1)
      {
      FD_SET(g_sock_main, readfds);
      if (g_sock_main > *sock_max)
        *sock_max = g_sock_main;
      }
    if  (g_sock_web != -1)
      {
      FD_SET(g_sock_web, readfds);
      if (g_sock_web > *sock_max)
        *sock_max = g_sock_web;
      }
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void ever_select_loop(int sock_sig)
{
  int ret, sig_client, sock_max = sock_sig;
  fd_set readfds;
  struct timeval cltimeout;
  char sigbuf[MAX_SIGBUF_LEN];
  char net[MAX_NAME_LEN];
  int16_t pmain, pweb;
  sig_client = -1;
  g_sock_main = -1;
  g_sock_web = -1;
  g_msg_rx_done = 0;

  cltimeout.tv_sec = 1;
  cltimeout.tv_usec = 0;
  while (1)
    {
    prepare_readfds(&readfds, sock_sig, sig_client, &sock_max);
    ret = select(sock_max + 1, &readfds, NULL, NULL, &cltimeout);
    if (ret < 0)
      {
      if (errno != EINTR && errno != EAGAIN)
        KOUT("%s", strerror(errno));
      }
    else if (ret == 0)
      {
      cltimeout.tv_sec = 1;
      cltimeout.tv_usec = 0;
      }
    else if (FD_ISSET(sock_sig, &readfds))
      {
      sig_client = accept(sock_sig, NULL, NULL);
      }
    else if (FD_ISSET(sig_client, &readfds))
      {
      if (!get_sigbuf(&sig_client, sigbuf))
        { 
        g_msg_rx_done = 1;
        if (sscanf(sigbuf, "Config: %s %hd %hd", net, &pmain, &pweb) != 3)
          KOUT("ERROR %s", sigbuf);
        set_config(net, pmain, pweb);
        }
      }
    else if (FD_ISSET(g_sock_main, &readfds))
      {
      accept_incoming_connect(g_sock_main, g_unix_main);
      }
    else if (FD_ISSET(g_sock_web, &readfds))
      {
      accept_incoming_connect(g_sock_web, g_unix_web);
      }
    else
      KOUT("ERROR IMPOSSIBLE");
    }
}
/*--------------------------------------------------------------------------*/
