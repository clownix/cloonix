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
#include <sys/types.h>
#include <sys/time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_resp(int llid, int tid, char *name, int num,
                        int toppid, int pid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_msg(int llid, int tid, int flags_hop, char *txt)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_get_name_list_doors(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_name_list_doors(int llid, int tid, int nb, t_hop_list *list)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_evt_doors_sub(int llid, int tid, int flags_hop, 
                            int nb, t_hop_list *list)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_evt_doors_unsub(int llid, int tid)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_hop_evt_doors(int llid, int tid, int flags_hop,
                        char *name, char *txt)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_sub(int llid, int tid, int flags_hop)
{
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_unsub(int llid, int tid)
{
}
/*---------------------------------------------------------------------------*/




