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
#include "io_clownix.h"
#include "rpc_clownix.h"


void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line) {KOUT();}
void rpct_recv_cli_resp(void *ptr, int llid, int tid,
                     int cli_llid, int cli_tid, char *line) {KOUT();}


void recv_qmp_sub(int llid, int tid, char *name){KOUT(" ");};
void recv_qmp_req(int llid, int tid, char *name, char *msg){KOUT(" ");};


/*****************************************************************************/
void rpct_recv_report(void *ptr, int llid, t_blkd_item *item)
{
  KOUT("%p %d %p", ptr, llid, item);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_mucli_dialog_req(int llid, int tid, char *name, int num, char *line)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/


/****************************************************************************/
void recv_c2c_create(int llid, int tid, char *name, 
                     int ip_slave, int port_slave)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/
/****************************************************************************/
void recv_c2c_peer_create(int llid, int tid, char *name,
                          char *master_cloonix, int master_idx,
                          int ip_slave, int port_slave)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_c2c_peer_ping(int llid, int tid, char *name)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void recv_c2c_ack_peer_create(int llid, int tid, char *name,
                                    char *orig_cloonix_name,
                                    char *peer_cloonix_name,
                                    int orig_doors_idx,
                                    int peer_doors_idx,
                                    int status)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/
/****************************************************************************/
void recv_c2c_delete(int llid, int tid, char *name)
{
  KOUT(" ");
}
/*-------------------------------------------------------------------------*/
/****************************************************************************/
void recv_c2c_peer_delete(int llid, int tid, char *name)
{
  KOUT(" ");
}
/*-------------------------------------------------------------------------*/

/****************************************************************************/
void recv_c2c_add_lan(int llid, int tid, char *name, char *lan)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_c2c_del_lan(int llid, int tid, char *name, char *lan)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_evt_stats_endp_sub(int llid, int tid, char *name, int num, int sub)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_evt_stats_sysinfo_sub(int llid, int tid, char *name, int sub)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_blkd_reports_sub(int llid, int tid, int sub)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_evt_stats_c2c_sub(int llid, int tid, char *name, int sub)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
void recv_work_dir_req(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_topo_small_event_sub(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_topo_small_event_unsub(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_event_topo_sub(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_evt_print_sub(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_set_all_paths(int llid, int tid,   char *ios)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_add_vm(int llid, int tid, t_topo_kvm *kvm)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_add_lan_endp(int llid, int tid, char *name, int num, char *lan)
{
  KOUT(" ");
}
void recv_del_lan_endp(int llid, int tid, char *name, int num, char *lan)
{
  KOUT(" ");
}

/*---------------------------------------------------------------------------*/
void recv_list_pid_req(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_list_commands_req(int llid, int tid, int is_layout)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_kill_uml_clownix(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_event_sys_sub(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_event_sys_unsub(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_event_topo_unsub(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_evt_print_unsub(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_del_all(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_list_vm_req(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_list_eth_req(int llid, int tid, char *name)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_list_vm_eth_lan_req(int llid, int tid, char *name, int eth)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/
void recv_interface_list_req(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_vmcmd(int llid, int tid, char *name, int cmd, int param)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_dpdk_ovs_cnf(int llid, int tid, int lcore, int mem, int cpu)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_add_sat(int llid, int tid, char *name, 
                  int mutype, t_c2c_req_info *c2c_req_info)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_del_sat(int llid, int tid, char *name)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_eventfull_sub(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_slowperiodic_sub(int llid, int tid)
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
void recv_event_print(int llid, int tid, char *line)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void recv_get_hop_name_list(int llid, int tid)
{
  KOUT(" ");
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void recv_layout_modif(int llid, int tid, int modif_type, char *name, int num,
                       int val1, int val2)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

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
void recv_sav_vm(int llid, int tid, char *name, int type, char *path)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

void rpct_recv_kil_req(void *ptr, int llid, int tid){KOUT();}
/*****************************************************************************/
void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num)
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
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_sub(void *ptr, int llid, int tid, int flags_hop)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_hop_unsub(void *ptr, int llid, int tid)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
void rpct_recv_hop_msg(void *ptr, int llid, int tid, int flags_hop, char *txt)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/

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
void recv_hop_get_name_list_doors(int llid, int tid)
{
  KOUT(" ");
}
/*--------------------------------------------------------------------------*/


