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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>


#include "lib_doorways.h"

#define PATH_SOCK "/home/perrier/nemo/cli/111111"
#define INIT_STR "127.0.0.1:43211=nemoclown=nat@user=root=ip=172.17.0.11=port=23456=cloonix_info_endturlututu"

/*****************************************************************************/
static int test_file_is_socket(char *path)
{
  int result = -1;
  struct stat stat_file;
  if (!stat(path, &stat_file))
    {
    if (S_ISSOCK(stat_file.st_mode))
      result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int connect_to_socket(char *path)
{
  int result = -1;
  int len,  sock;
  struct sockaddr_un addr;
  char *pname = path;
  if (test_file_is_socket(pname))
    fprintf(stderr, "err path name not socket %s\n", pname);
  else
    {
    sock = socket (AF_UNIX,SOCK_STREAM,0);
    if (sock <= 0)
      fprintf(stderr, "err socket SOCK_STREAM\n");
    else
      {
      memset (&addr, 0, sizeof (struct sockaddr_un));
      addr.sun_family = AF_UNIX;
      strcpy(addr.sun_path, pname);
      len = sizeof (struct sockaddr_un);
      result = connect(sock,(struct sockaddr *) &addr, len);
      if (result == -1)
        {
        close(sock);
        fprintf(stderr, "NO SERVER LISTENING TO %s\n", pname);
        }
      else
        {
        result = sock;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void lostconn(int signo)
{
  fprintf(stderr, "pipe brocken lost connection doorways\n");
  exit(1);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int main (int argc, char *argv[])
{
  int pid, len, sock;
  char *msg = "coucou";
  char buf[100];

  if ((pid = fork()) == 0)
    doorways_access_init(PATH_SOCK);
  else
    {
(void) signal(SIGPIPE, lostconn);
    sleep(1);
    sock = connect_to_socket(PATH_SOCK);
    if (sock >= 0)
      { 
      len = write(sock, INIT_STR, strlen(INIT_STR)+1); 
      if (len != strlen(INIT_STR)+1)
        {
        fprintf(stderr, "err1\n");
        exit(1);
        } 
      while(1)
        {
        sleep(1);
        len = read(sock, buf, 100);
        if (len <= 0)
          {
          fprintf(stderr, "err read\n");
          exit(1);
          } 
        fprintf(stdout, "RX: %s\n", buf);
        len = write(sock, msg, strlen(msg)+1);
        if (len != strlen(msg)+1)
          {
          fprintf(stderr, "err write\n");
          exit(1);
          } 
        }
      }
    }
  return 0;
}
/*--------------------------------------------------------------------------*/


