#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <signal.h>

#include "commun.h"

/*****************************************************************************/
static void fd_cloexec(int fd)
{
  int flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    KOUT(" ");
  flags |= FD_CLOEXEC;
  if (fcntl(fd, F_SETFD, flags) == -1)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
static void set_sock_rx_buf(int fd)
{
  unsigned int datalen = 10000;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                 (char *)&datalen, sizeof(datalen)) < 0)
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/



/*****************************************************************************/
/*
static void nonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    KOUT(" ");
  flags |= O_NONBLOCK|O_NDELAY;
  if (fcntl(fd, F_SETFL, flags) == -1)
    KOUT(" ");
}
*/
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void nonnonblock(int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  flags &= ~O_NONBLOCK;
  flags &= ~O_NDELAY;
  if (fcntl(fd, F_SETFL, flags) == -1)
    KOUT(" ");
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
static void tempo_connect(int no_use)
{
  KERR("SIGNAL Timeout in connect\n");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int test_file_is_socket(char *path)
{
  int result = -1;
  struct stat stat_file;
  if (!stat(path, &stat_file))
    {
    if (S_ISSOCK(stat_file.st_mode))
      result = 0;
    else
      KERR(" ");
    }
  else
    KERR("%s", path);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_client_socket_unix(char *pname, int *fd)
{
  int len,  sock, result = -1;
  struct sockaddr_un addr;
  struct sigaction act;
  struct sigaction oldact;
  if (!test_file_is_socket(pname))
    {
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

    alarm(30);
    result = connect(sock,(struct sockaddr *) &addr, len);

    sigaction(SIGALRM, &oldact, NULL);
    alarm(0);

    if (result != 0)
      {
      close(sock);
      printf("NO SERVER LISTENING TO %s\n", pname);
      }
    else
      {
      *fd = sock;
      nonnonblock(*fd);
      fd_cloexec(*fd);
      set_sock_rx_buf(*fd);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/


