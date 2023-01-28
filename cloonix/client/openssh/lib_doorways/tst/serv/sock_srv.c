/*****************************************************************************/
/*    Copyright (C) 2006-2023 clownix@clownix.net License AGPL-3             */
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
#include <time.h>
#include <stdint.h>

#include "sock_utils.h"

static int g_fd;


/****************************************************************************/
static void action_read(int fd)
{
  char buf[101];
  int rx_len;
  rx_len = read (fd, buf, 100);
  if (rx_len == 0)
    {
    fprintf(stderr, "Problem read 0\n");
    exit(1);
    }
  else if (rx_len < 0)
    {
    if ((errno == EAGAIN) || (errno ==EINTR))
      rx_len = 0;
    else
      {
      fprintf(stderr, "Problem read -1\n");
      exit(1);
      }
    }
  else
    {
    fprintf(stdout, "echoing: %d %s\n", rx_len, buf);
    write(g_fd, buf, rx_len);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void select_wait_and_switch(void)
{
  fd_set infd;
  int ret;
  FD_ZERO(&infd);
  FD_SET(g_fd, &infd);
  ret = select(g_fd + 1, &infd, NULL, NULL, NULL);
  if (ret <= 0)
    {
    fprintf(stderr, "Error select %d\n", ret);
    exit(1);
    }
  else 
    {
    if (FD_ISSET(g_fd, &infd))
      {
      action_read(g_fd);
      }
    else
      {
      fprintf(stderr, "Error select nothing %d\n", ret);
      exit(1);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
/*
static void usage(char *name)
{
  printf("\n%s <port>\n", name);
  exit(1);
}
*/
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{ 
 // if (argc != 2)
 //   usage(argv[0]);
 // g_fd = util_server_socket_inet(argv[1]);
  g_fd = util_server_socket_inet("23456");
  if (g_fd < 0)
    printf("Problem socket\n");
  else
    {
    for (;;)
      select_wait_and_switch();
    }
  return 0;
}
/*--------------------------------------------------------------------------*/
