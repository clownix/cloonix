/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include "io_clownix.h"
#include "rpc_clownix.h"
#include "doorways_sock.h"
#include "client_clownix.h"

    

#define RESP len+=sprintf

static char *get_local_big_buf(void)
{
  static char big_buf[10000];
  return big_buf;
}


/*****************************************************************************/
char *to_ascii_sys(t_sys_info *sys)
{
  int i, len = 0;
  char *resp=get_local_big_buf();
  if (!sys)
    KOUT(" ");
  RESP(resp+len,"---------------------------------------------------\n");
  RESP(resp+len," Select loops per sec:           %d \n", sys->selects);
  RESP(resp+len," Nb mallocs unfreed:             %lu \n", sys->mallocs[0]);
  RESP(resp+len,"\t\t");
  for (i=1;i<MAX_MALLOC_TYPES; i++)
    RESP(resp+len,"%lu, ", sys->mallocs[i]);
  RESP(resp+len,"\n");
  RESP(resp+len," Nb open fds:                    %lu \n", sys->fds_used[0]);
  RESP(resp+len,"\t\t");
  for (i=1;i<type_llid_max; i++)
    RESP(resp+len,"%lu, ", sys->fds_used[i]);
  RESP(resp+len,"\n");
  RESP(resp+len," Nb Max Channels:                %d \n", sys->max_channels);
  RESP(resp+len," Sum bytes rx by daemon last sec:%d \n", sys->cur_channels_recv);
  RESP(resp+len," Sum bytes tx by daemon last sec:%d \n", sys->cur_channels_send);
  RESP(resp+len," Nb connected clients:           %d \n", sys->clients);
  RESP(resp+len," Max inter-beat time last 5 sec: %d \n", sys->max_time);
  RESP(resp+len," Avg inter-beat time last 5 sec: %d \n", sys->avg_time);
  RESP(resp+len," Inter-beats above 50 ms:        %d \n", sys->above50ms);
  RESP(resp+len," Inter-beats above 20 ms:        %d \n", sys->above20ms);
  RESP(resp+len," Inter-beats above 15 ms:        %d \n", sys->above15ms);
  RESP(resp+len,"---------------------------------------------------\n");
  for (i=0; i<sys->nb_queue_tx; i++)
    {
    RESP(resp+len," Qtx%d %s fd:%d wakin:%d wakout:%d wakerr:%d "
                  "peak: %d in:%d out:%d\n", 
         sys->queue_tx[i].llid, llid_trace_lib(sys->queue_tx[i].type), 
         sys->queue_tx[i].fd,  
         sys->queue_tx[i].waked_count_in, sys->queue_tx[i].waked_count_out, 
         sys->queue_tx[i].waked_count_err, 
         sys->queue_tx[i].peak_size,
         sys->queue_tx[i].in_bytes, sys->queue_tx[i].out_bytes);
    }
  return resp;
}
/*--------------------------------------------------------------------------*/




