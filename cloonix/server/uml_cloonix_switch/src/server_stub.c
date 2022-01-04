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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "commun_daemon.h"



/*****************************************************************************/
void recv_qmp_resp(int llid, int tid, char *name, char *line, int status)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_recv_command(int llid, int tid, char *name, char *cmd)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_recv_info(int llid, int tid, char *type, char *info)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_recv_status(int llid, int tid, char *name, char *status)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_recv_add_vm(int llid,int tid,char *name, char *way2agent)
{
  KERR("%s %s", name, way2agent);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void doors_recv_del_vm(int llid, int tid, char *name)
{
  KERR("%s", name);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
void recv_hop_evt_doors(int llid, int tid, int flags_hop,
                        char *name, char *txt)
{
 KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_hop_sub(int llid, int tid, int flags_hop)
{
 KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void rpct_recv_hop_unsub(int llid, int tid)
{
 KOUT(" ");
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_work_dir_resp(int llid, int tid, t_topo_clc *conf)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_topo(int llid, int tid, t_topo_info *topo)
{
  KOUT("%d %p", llid, topo);
}
/*---------------------------------------------------------------------------*/
void recv_evt_print(int llid,int tid,  char *info)
{
  KOUT("%d %s", llid, info);
}
/*---------------------------------------------------------------------------*/
void recv_status_ok(int llid, int tid, char *info)
{
  KOUT("%d %s", llid, info);
}
/*---------------------------------------------------------------------------*/
void recv_status_ko(int llid, int tid, char *reason)
{
  KOUT("%d %s", llid, reason);
}
/*---------------------------------------------------------------------------*/
void recv_list_pid_resp(int llid, int tid, int qty, t_pid_lst *pid)
{
  KOUT("%d %d %p", llid, qty, pid);
}
/*---------------------------------------------------------------------------*/
void recv_list_commands_resp(int llid, int tid, int qty, t_list_commands *list)
{
  KOUT("%d %d %s", llid, qty, list[0].cmd);
}
/*---------------------------------------------------------------------------*/
void recv_event_sys(int llid, int tid, t_sys_info *info)
{
  KOUT("%d %p", llid, info);
}
/*---------------------------------------------------------------------------*/
void recv_topo_small_event(int llid, int tid, char *name, 
                           char *p1, char *p2, int vm_evt)
{
  KOUT("%d %s %s %s %d", llid, name, p1, p2, vm_evt);
}
/*---------------------------------------------------------------------------*/
void recv_interface_list_resp(int llid, int tid, int nb, 
                              char **intf, char *err)
{
  KOUT("%d %d %p %s", llid, nb, intf, err);
}
/*---------------------------------------------------------------------------*/
/*****************************************************************************/
void recv_event_spy_sub(int llid, int tid, char *name, char *intf, char *dir)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_spy_unsub(int llid, int tid, char *name, char *intf, char *dir)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_spy(int llid, int tid, char *name, char *intf, char *dir,
                    int secs, int usecs, int len, char *msg)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_eventfull(int llid, int tid, int nb_endp, t_eventfull_endp *endp)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_slowperiodic_qcow2(int llid, int tid, int nb, t_slowperiodic *spic)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_slowperiodic_img(int llid, int tid, int nb, t_slowperiodic *spic)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_event_print_sub(int llid, int tid, int on)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_kil_req(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
/*****************************************************************************/
void rpct_recv_pid_req(int llid, int tid, char *name, int num)
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
void recv_evt_stats_endp(int llid, int tid, char *network, char *name, int num,
                         t_stats_counts *stats_counts, int status)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_evt_stats_sysinfo(int llid, int tid, char *network, char *name,
                            t_stats_sysinfo *stats_sysinfo, 
                            char *df, int status)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/



