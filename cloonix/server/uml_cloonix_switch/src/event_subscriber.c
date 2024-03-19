/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "layout_rpc.h"
#include "cfg_store.h"
#include "commun_daemon.h"
#include "event_subscriber.h"
#include "heartbeat.h"
#include "system_callers.h"
#include "pid_clone.h"
#include "llid_trace.h"
#include "layout_topo.h"
#include "lib_topo.h"


/*****************************************************************************/
static void subscribing_llid_send(int sub_evt, void *info)
{
  int llid, prev_llid, tid;
  if (llid_get_event2llid(sub_evt, 0, &llid, &tid))
    {
    if (!msg_exist_channel(llid))
      KOUT("%d\n", sub_evt);
    do
      {
      switch (sub_evt)
        {
        case sub_evt_print:
          send_evt_print(llid, tid, (char *)info);
        break;
        case sub_evt_sys:
          send_event_sys(llid, tid, (t_sys_info *)info);
        break;
        case sub_evt_topo:
          send_event_topo(llid, tid, (t_topo_info *)info);
        break;
        case topo_small_event:
          send_topo_small_event(llid, tid, ((t_small_evt *)info)->name, 
                                ((t_small_evt *)info)->param1, 
                                ((t_small_evt *)info)->param2,
                                ((t_small_evt *)info)->evt);
        break;
        default:
          KOUT(" ");
        }
      prev_llid = llid;
      } while(llid_get_event2llid(sub_evt, prev_llid, &llid, &tid));
    }
  switch (sub_evt)
    {
    case sub_evt_sys:
      sys_info_free((t_sys_info *)info);
    break;
    case sub_evt_topo:
      topo_free_topo((t_topo_info *)info);
    break;
    }      
}   
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void event_print (const char * format, ...)
{
  va_list arguments;
  char line[MAX_PRINT_LEN];
    
  va_start (arguments, format);
  memset(line, 0, MAX_PRINT_LEN);
  vsnprintf (line, MAX_PRINT_LEN, format, arguments);
  line[MAX_PRINT_LEN-1] = 0;
  if (!i_am_a_clone())           
    {
    subscribing_llid_send(sub_evt_print, line);
    }
  va_end (arguments);
}
/*-----------------------------------------------------------------------*/

/*****************************************************************************/
void event_subscribe(int sub_evt, int llid, int tid)
{
  llid_set_event2llid(llid, sub_evt, tid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void event_unsubscribe(int sub_evt, int llid)
{ 
  llid_unset_event2llid(llid, sub_evt);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void event_subscriber_send(int sub_evt, void *info)
{
  if ((sub_evt <= 0) || (sub_evt >= sub_evt_max))
    KOUT(" ");
  if (!i_am_a_clone())           
    {
    subscribing_llid_send(sub_evt, info);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void event_subscriber_init(void)
{
}
/*---------------------------------------------------------------------------*/

