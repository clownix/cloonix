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
#include "doors_rpc.h"
/*--------------------------------------------------------------------------*/

void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line) {KOUT();}
void rpct_recv_cli_resp(void *ptr, int llid, int tid,
                     int cli_llid, int cli_tid, char *line) {KOUT();}

/*****************************************************************************/
void rpct_recv_report(void *ptr, int llid, t_blkd_item *item)
{
  KOUT("%p %d %p", ptr, llid, item);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_recv_c2c_clone_birth_pid(int llid, int tid, char *name, int pid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_recv_c2c_clone_death(int llid, int tid, char *name)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_recv_c2c_resp_idx(int llid, int tid, char *name, int local_idx)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_recv_c2c_resp_conx(int llid, int tid, char *name, int fd, int status)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
void doors_recv_event(int llid, int tid, char *name,  char *line)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_info(int llid, int tid, char *type,  char *info)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void doors_recv_status(int llid, int itid, char *name, char *cmd)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_pid_resp(void *ptr, int llid, int tid, char *name, int num,
                        int toppid, int pid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_msg(void *ptr, int llid, int tid, int flags_hop, char *txt)
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




