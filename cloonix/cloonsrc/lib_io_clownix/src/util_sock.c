/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <asm/types.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <stddef.h>

#include "io_clownix.h"
#include "util_sock.h"
#include "channel.h"

static void nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    KOUT(" ");
  flags |= O_NONBLOCK|O_NDELAY;
  if (fcntl(fd, F_SETFL, flags) == -1)
    KOUT(" ");
}

static void nonnonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  flags &= ~O_NONBLOCK;
  flags &= ~O_NDELAY;
  if (fcntl(fd, F_SETFL, flags) == -1)
    KOUT(" ");
}

void nonblock_fd(int fd)
{
  nonblock(fd);
}

void nodelay_fd(int fd)
{
  int nodelay = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*) &nodelay, sizeof(int));
}

void nonnonblock_fd(int fd)
{
  nonnonblock(fd);
}

void util_free_fd(int fd)
{
  close (fd);
}

/*
*/

int util_read (char *ptr, int max, int fd)
{
  int rx_len;
  rx_len = read (fd, ptr, max);
  if (rx_len < 0)
    {
    if ((errno == EAGAIN) || (errno == EINTR) || (errno == EINPROGRESS))
      rx_len = 0;
    }
  else if (rx_len == 0)
    rx_len = -1;
  return rx_len;
}

static void tempo_connect(int no_use)
{
  KERR("SIGNAL Timeout in connect\n");
}

/* AF_UNIX stream socket and the address of pname, abstract when off is 1
   (leading zero byte). -1 with a KERR on a bad name or no descriptor.      */
static int unix_sock(char *pname, int off, struct sockaddr_un *addr, int *len)
{
  int sock = -1;
  signal(SIGPIPE, SIG_IGN);
  if ((!pname) || (strlen(pname) == 0) ||
      (strlen(pname) + off >= sizeof(addr->sun_path)))
    KERR("ERROR unix socket name empty or too long: %s", pname ? pname : "");
  else if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    KERR("ERROR socket(AF_UNIX) failed for %s: errno=%d (%s). If this is "
         "EMFILE(24) the process has run out of file descriptors",
         pname, errno, strerror(errno));
  else
    {
    memset(addr, 0, sizeof(struct sockaddr_un));
    addr->sun_family = AF_UNIX;
    strcpy(addr->sun_path + off, pname);
    *len = off ? offsetof(struct sockaddr_un, sun_path) + strlen(pname) + 1
               : sizeof(struct sockaddr_un);
    }
  return sock;
}

/* Blocking connect bounded by an alarm; the fd is given nonblocking. */
static int timed_connect(int sock, struct sockaddr_un *addr, int len,
                         int seconds, int *fd)
{
  int result;
  struct sigaction act, oldact;
  memset(&act, 0, sizeof(act));
  sigfillset(&act.sa_mask);
  act.sa_handler = tempo_connect;
  sigaction(SIGALRM, &act, &oldact);
  alarm(seconds);
  result = connect(sock, (struct sockaddr *) addr, len);
  sigaction(SIGALRM, &oldact, NULL);
  alarm(0);
  if (result != 0)
    close(sock);
  else
    {
    *fd = sock;
    nonblock(*fd);
    }
  return result;
}

/* -1 silently when pname is absent, with a KERR on anything else wrong. */
static int is_unix_sock(char *pname)
{
  struct stat stat_file;
  int result = -1;
  if ((!pname) || stat(pname, &stat_file))
    {
    if (pname && (errno != ENOENT))
      KERR("ERROR %s cannot be examined: errno=%d (%s)",
           pname, errno, strerror(errno));
    }
  else if (!S_ISSOCK(stat_file.st_mode))
    KERR("ERROR %s exists but is not a socket", pname);
  else
    result = 0;
  return result;
}

int util_nonblock_client_socket_unix(char *pname, int *fd)
{
  int len, sock, result = -1;
  struct sockaddr_un addr;
  if ((is_unix_sock(pname) == 0) &&
      ((sock = unix_sock(pname, 0, &addr, &len)) >= 0))
    {
    nonblock_fd(sock);
    result = connect(sock,(struct sockaddr *) &addr, len);
    if (result == 0)
      *fd = sock;
    else
      {
      if ((errno != ECONNREFUSED) && (errno != ENOENT))
        KERR("ERROR connect to %s failed, errno=%d (%s)",
             pname, errno, strerror(errno));
      close(sock);
      }
    }
  return result;
}

int util_nonblock_client_socket_inet(uint32_t ip, uint16_t port)
{
  int sock, result;
  struct sockaddr_in adresse;
  char ip_string[MAX_NAME_LEN];
  int_to_ip_string (ip, ip_string);
  signal(SIGPIPE, SIG_IGN);
  sock = socket (AF_INET,SOCK_STREAM,0);
  if (sock < 0)
    KOUT(" ");
  adresse.sin_family = AF_INET;
  adresse.sin_port = htons(port);
  adresse.sin_addr.s_addr = htonl(ip);
  nonblock_fd(sock);
  result = connect(sock,(struct sockaddr *) &adresse, sizeof(adresse));
  if (result != -1)
    {
    KERR("ERROR %X %hu %d %d", ip, port, errno, errno);
    result = -1;
    }
  else
    {
    if (errno == EINPROGRESS)
      {
      result = sock;
      }
    else
      {
      if (errno == ENETUNREACH)
        {
        KERR("ERROR %s %hu Network is unreachable", ip_string, port);
        }
      else
        KERR("ERROR %s %hu %d", ip_string, port, errno);
      if (close(sock))
        KERR("ERROR %s %hu %d", ip_string, port, errno);
      result = -1;
      }
    }
  return result;
}

int util_client_abstract_socket_unix(char *pname, int *fd)
{
  int len, sock, result = -1;
  struct sockaddr_un addr;
  if ((sock = unix_sock(pname, 1, &addr, &len)) >= 0)
    result = timed_connect(sock, &addr, len, 1, fd);
  return result;
}

int util_client_socket_unix(char *pname, int *fd)
{
  int len, sock, result = -1;
  struct sockaddr_un addr;
  if ((is_unix_sock(pname) == 0) &&
      ((sock = unix_sock(pname, 0, &addr, &len)) >= 0))
    result = timed_connect(sock, &addr, len, 5, fd);
  return result;
}

/* File socket first, the abstract one of the same name when it fails. */
int util_proxy_client_socket_unix(char *pname, int *fd)
{
  int len, sock, result = -1;
  struct sockaddr_un addr;
  if ((sock = unix_sock(pname, 0, &addr, &len)) >= 0)
    result = timed_connect(sock, &addr, len, 1, fd);
  if (result)
    result = util_client_abstract_socket_unix(pname, fd);
  return result;
}

int util_client_socket_inet(uint32_t ip, uint16_t port, int *fd)
{
  struct sigaction act;
  struct sigaction oldact;
  int   sock, result;
  struct sockaddr_in adresse;
  *fd = -1;
  signal(SIGPIPE, SIG_IGN);
  sock = socket (AF_INET,SOCK_STREAM,0);
  if (sock < 0)
    {
    KERR("ERROR socket(AF_INET) failed: errno=%d (%s). If this is "
         "EMFILE(24) the process has run out of file descriptors",
         errno, strerror(errno));
    return -1;
    }
  adresse.sin_family = AF_INET;
  adresse.sin_port = htons(port);
  adresse.sin_addr.s_addr = htonl(ip);

  memset(&act, 0, sizeof(act));
  sigfillset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = tempo_connect;
  sigaction(SIGALRM, &act, &oldact);

  alarm(5);
  result = connect(sock,(struct sockaddr *) &adresse, sizeof(adresse));
  
  sigaction(SIGALRM, &oldact, NULL);
  alarm(0);

  if (result == 0)
    {
    *fd = sock;
    nonblock(*fd);
    nodelay_fd(*fd);
    }
  else
    {
    close(sock);
    KERR("ERROR no server listening on port %hu: %s", port, strerror(errno));
    result = -1;
    }
  return result;
}

int util_socket_listen_unix(char *pname)
{
  int ret, fd, len;
  struct sockaddr_un serv;

  signal(SIGPIPE, SIG_IGN);
  if ((!pname) || (strlen(pname) == 0))
    {
    KERR("ERROR");
    return -1;
    }
  if (strlen(pname) >= 108)
    KOUT("%d", (int)(strlen(pname)));
  unlink (pname);
  fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    KOUT("%d %d\n", fd, errno);
  memset (&serv, 0, sizeof (struct sockaddr_un));
  serv.sun_family = AF_UNIX;
  strncpy (serv.sun_path, pname, strlen(pname));
  len = sizeof (serv.sun_family) + strlen (serv.sun_path);
  ret = bind (fd, (struct sockaddr *) &serv, len);
  if (ret == 0)
    {
    ret = listen (fd, 50);
    if (ret != 0)
      {
      close(fd);
      fd = -1;
      KERR("ERROR listen %s: %s", pname, strerror(errno));
      }
    }
  else
    {
    close(fd);
    fd = -1;
    KERR("bind failure %s %d\n", pname, errno);
    }
  return fd;
}
  
int util_socket_unix_dgram(char *pname)
{ 
  int ret, fd, len;
  struct sockaddr_un serv;
  signal(SIGPIPE, SIG_IGN);
  
  if ((!pname) || (strlen(pname) == 0))
    {
    KERR("ERROR");
    return -1;
    }
  if (strlen(pname) >= 108)
    KOUT("%d", (int)(strlen(pname)));
  unlink (pname);
  fd = socket (AF_UNIX, SOCK_DGRAM, 0);
  if (fd < 0)
    KOUT("%d %d\n", fd, errno);
  memset (&serv, 0, sizeof (struct sockaddr_un));
  serv.sun_family = AF_UNIX;
  strncpy (serv.sun_path, pname, strlen(pname));
  len = sizeof (serv.sun_family) + strlen (serv.sun_path);
  ret = bind (fd, (struct sockaddr *) &serv, len);
  if (ret)
    {
    close(fd);
    fd = -1;
    KERR("ERROR bind failure %s %d", pname, errno);
    }
  return fd;
}

int util_socket_listen_inet( uint16_t port )
{
  struct sockaddr s_addr;
  int fd, optval=1, nodelay=1;
  signal(SIGPIPE, SIG_IGN);
  memset(&s_addr, 0, sizeof(s_addr));
  s_addr.sa_family = AF_INET;
  ((struct sockaddr_in*)&s_addr)->sin_addr.s_addr = htonl(INADDR_ANY);
  ((struct sockaddr_in*)&s_addr)->sin_port = htons( port );
  fd = socket( s_addr.sa_family, SOCK_STREAM, 0 );
  if ( fd < 0 )
    KOUT("ERROR socket: %s", strerror(errno));
  if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval))<0)
    KOUT(" ");
  if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*) &nodelay, sizeof(int)))
    KOUT(" ");
  if ( bind(fd, &s_addr,sizeof(struct sockaddr_in) ) < 0 )
    {
    KERR("ERROR bind port %hu: %s", port, strerror(errno));
    close(fd);
    fd = -1;
    }
  else
    {
    if ( listen(fd, 50) < 0 )
      {
      KERR("ERROR listen port %hu: %s", port, strerror(errno));
      close(fd);
      fd = -1;
      }
    }
  return(fd);
}

void util_fd_accept(int fd_listen, int *fd, const char *fct)
{
  struct sockaddr s_addr;
  socklen_t sz = sizeof(s_addr);
  *fd = accept( fd_listen, &s_addr, &sz );
  if (*fd >= 0)
    {
    nonblock(*fd);
    nodelay_fd(*fd);
    }
  else
    KOUT(" ");
}

void lio_clean_all_llid(void)
{
  int llid, nb_chan, i;
  clownix_timeout_del_all();
  nb_chan = get_clownix_channels_nb();
  for (i=0; i<=nb_chan; i++)
    {
    llid = channel_get_llid_with_cidx(i);
    if (llid)
      {
      if (msg_exist_channel(llid))
        msg_delete_channel(llid);
      }
    }
}

char *lio_linear(char *argv[])
{
  int i;
  static char result[3*MAX_PATH_LEN];
  memset(result, 0, 3*MAX_PATH_LEN);
  for (i=0;  (argv[i] != NULL); i++)
    {
    strcat(result, argv[i]);
    if (strlen(result) >= 2*MAX_PATH_LEN)
      {
      KERR("NOT POSSIBLE");
      break;
      }
    strcat(result, " ");
    }
  return result;
}

/* Blocking write of the whole buffer, EINTR retried; -1 if the peer went. */
int util_write_all(int fd, const char *buf, int len)
{
  int done = 0, n;
  while (done < len)
    {
    n = (int) write(fd, buf + done, len - done);
    if (n <= 0)
      {
      if ((n < 0) && (errno == EINTR))
        continue;
      return -1;
      }
    done += n;
    }
  return 0;
}
