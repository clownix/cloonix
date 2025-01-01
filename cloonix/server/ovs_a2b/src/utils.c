/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
#include <netinet/in.h>
#include <sys/un.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "utils.h"

static int g_nb_tot_malloc;

/*****************************************************************************/
void* utils_malloc(int len)
{
  void *result;
  g_nb_tot_malloc += 1;
  if (g_nb_tot_malloc > 100000)
    KERR("WARNING %d", g_nb_tot_malloc);
  result = malloc(len);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void utils_free(void *ptr)
{
  g_nb_tot_malloc -= 1;
  free(ptr);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void utils_init(void)
{
  g_nb_tot_malloc = 0;
}
/*---------------------------------------------------------------------------*/

