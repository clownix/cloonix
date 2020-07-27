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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/syscall.h>



#include "ioc_blkd.h"
static int g_pid;

/*****************************************************************************/
long long cloonix_get_msec(void)
{
  struct timespec ts;
  long long result;
  if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  result = (long long) (ts.tv_sec);
  result *= 1000;
  result += ((long long) ts.tv_nsec) / 1000000;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
long long cloonix_get_usec(void)
{
  struct timespec ts;
  long long result;
  if (syscall(SYS_clock_gettime, CLOCK_MONOTONIC, &ts))
    KOUT(" ");
  result = (long long) (ts.tv_sec);
  result *= 1000000;
  result += ts.tv_nsec / 1000;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void cloonix_set_pid(int pid)
{
  g_pid = pid;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int cloonix_get_pid(void)
{
  return g_pid;
}
/*---------------------------------------------------------------------------*/


