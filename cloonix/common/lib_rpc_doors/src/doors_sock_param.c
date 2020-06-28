/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
#include <sys/types.h>
#include <sys/stat.h>
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doors_rpc.h"

/****************************************************************************/
static void tst_port(char *str_port, int *port)
{
  unsigned long val;
  char *endptr;
  val = strtoul(str_port, &endptr, 10);
  if ((endptr == NULL)||(endptr[0] != 0))
    KOUT("%s", str_port);
  if ((val < (unsigned long) 40000) || (val > 65535))
    KOUT("%s: Pick a port between 40000 and 65535", str_port);
  *port = (int) val;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void get_ip_and_port(char *ip_port, uint32_t *ip, int *port)
{
  char *ptr_ip, *ptr_port;
  ptr_ip = strchr(ip_port, ':');
  if (!ptr_ip)
    KOUT("Problem ip addr: %s", ip_port); 
  ptr_ip++;
  ptr_port = strchr(ptr_ip, ':');
  if (!ptr_port)
    KOUT("Problem ip addr: %s", ip_port); 
  *ptr_port = 0;
  ptr_port++;
  if (ip_string_to_int (ip, ptr_ip))
    KOUT("Problem ip addr: %s", ptr_ip); 
  tst_port(ptr_port, port);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int doors_test_file_is_socket(char *path)
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
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_tux_sock_param(char *ipm, char *path, uint32_t *ip, int *port)
{
  char pm[MAX_PATH_LEN];
  if (strlen(ipm) >= MAX_PATH_LEN) 
    KOUT("\nTux LENGTH Problem\n");
  memset(path, 0, MAX_PATH_LEN);
  memset(pm, 0, MAX_PATH_LEN);
  strncpy(pm, ipm, MAX_PATH_LEN-1);
  if (strchr( pm, ':'))
    get_ip_and_port(pm, ip, port);
  else 
    strncpy(path, pm, MAX_PATH_LEN-1);
}
/*--------------------------------------------------------------------------*/


