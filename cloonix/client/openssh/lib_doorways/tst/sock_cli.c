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
#include <time.h>
#include <stdint.h>
#include "sock_utils.h"

#define MAX_NAME_LEN 100

static int g_second_current;
static int g_second_offset;
static int g_fd;
static struct timeval g_timeout;

static char g_ip[MAX_NAME_LEN];
static int g_port;

/*****************************************************************************/
static int get_sec(void)
{
  struct timespec ts;
  int result = -1;
  if (!clock_gettime(CLOCK_MONOTONIC, &ts))
    result = (unsigned int) (ts.tv_sec - g_second_offset);
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int get_sec_init(void)
{
  struct timespec ts;
  int result = -1;
  if (!clock_gettime(CLOCK_MONOTONIC, &ts))
    {
    g_second_offset = ts.tv_sec;
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void action_read(int fd)
{
  int rx_len;
  char buf[101];
  rx_len = read (fd, buf, 100);
  if (rx_len <= 0)
    {
    printf("Problem read\n");
    exit(1);
    }
  else
    {
    printf("%s\n", buf);
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void action_heartbeat(int cur_sec)
{
  char str[100];
  if (g_fd >= 0)
    {
    sprintf(str, "CliBeat: %d", cur_sec);
    write(g_fd, str, strlen(str));
    }
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static int select_wait_and_switch(void)
{
  fd_set infd;
  int cur_sec, ret, result = -1;
  FD_ZERO(&infd);
  FD_SET(g_fd, &infd);
  ret = select(g_fd + 1, &infd, NULL, NULL, &g_timeout);
  if (ret < 0)
    printf("Error select\n");
  else if (ret == 0) 
    {
    g_timeout.tv_sec = 1;
    g_timeout.tv_usec = 0;
    result = 0;
    }
  else if (ret > 0) 
    {
    if (FD_ISSET(g_fd, &infd))
      {
      action_read(g_fd);
      result = 0;
      }
    else
      printf("Wrong select\n");
    }
  if (result == 0)
    {
    cur_sec = get_sec();
    if (cur_sec != g_second_current)
      {
      g_second_current = cur_sec;
      action_heartbeat(cur_sec);
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int check_args(char *ip, char *port)
{
  int result = -1;
  char *endptr;
  unsigned int p[4];
  g_port = strtoul(port, &endptr, 10);
  if ((endptr != NULL) && (endptr[0] == 0))
    {
    if ((sscanf(ip, "%u.%u.%u.%u", &p[0], &p[1], &p[2], &p[3]) == 4) &&
        (p[0]<=0xFF) && (p[1]<=0xFF) && (p[2]<=0xFF) && (p[3]<=0xFF))
      {
      strncpy(g_ip, ip, MAX_NAME_LEN-1);
      result = 0;
      }
    else
      printf("%s", ip);
    }
  else
    printf("%s %d", port, g_port);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void usage(char *name)
{
  printf("\n%s <ip> <port>\n", name);
  exit(1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char *argv[])
{ 
  if (get_sec_init())
    printf("Problem clock\n");
  else
    {
    if (argc != 3)
      usage(argv[0]);
    if (check_args(argv[1], argv[2]))
      usage(argv[0]);
    g_fd = util_client_socket_inet(argv[1], argv[2]);
    if (g_fd < 0)
      printf("Problem socket\n");
    else
      {
      for (;;)
        if (select_wait_and_switch())
          printf("Problem select\n");
      }
    }
  return 0;
}
/*--------------------------------------------------------------------------*/
