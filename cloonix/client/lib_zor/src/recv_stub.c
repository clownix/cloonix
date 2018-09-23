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
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "io_clownix.h"
#include "rpc_clownix.h"

void recv_hop_evt_doors_sub(int llid, int tid, int flags_hop, int nb, t_hop_list *list){KOUT();}
void recv_hop_evt_doors_unsub(int llid, int tid){KOUT();}
void recv_hop_evt_doors(int llid, int tid, int flags_hop, char *name, char *txt){KOUT();}
void recv_hop_get_name_list_doors(int llid, int tid){KOUT();}
void recv_hop_name_list_doors(int llid, int tid, int nb, t_hop_list *list){KOUT();}


void rpct_recv_pid_req(void *ptr, int llid, int tid, char *name, int num){KOUT();}
void rpct_recv_pid_resp(void *ptr, int llid, int tid, char *name,
                        int num, int toppid, int pid){KOUT();}
void rpct_recv_hop_sub(void *ptr, int llid, int tid, int flags_hop){KOUT();}
void rpct_recv_hop_unsub(void *ptr, int llid, int tid){KOUT();}
void rpct_recv_hop_msg(void *ptr, int llid, int tid, int flags_hop, char *txt){KOUT();}

void rpct_recv_app_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_diag_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_evt_msg(void *ptr, int llid, int tid, char *line) {KOUT();}
void rpct_recv_cli_req(void *ptr, int llid, int tid,
                    int cli_llid, int cli_tid, char *line) {KOUT();}
void rpct_recv_cli_resp(void *ptr, int llid, int tid,
                     int cli_llid, int cli_tid, char *line) {KOUT();}

/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void rpct_recv_report(void *ptr, int llid, t_blkd_item *item)
{
  KOUT("%p %d %p", ptr, llid, item);
}
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
void recv_work_dir_req(int llid, int tid){KOUT(" ");}
void recv_status_ok(int llid, int tid, char *txt){KOUT(" ");}
void recv_status_ko(int llid, int tid, char *reason){KOUT(" ");}
void recv_add_vm(int llid, int tid, t_topo_kvm *kvm){KOUT(" ");}
void recv_sav_vm(int llid, int tid, char *name, int type, char *sav_vm_path){KOUT(" ");}
void recv_sav_vm_all(int llid, int tid,  int type, char *sav_vm_path){KOUT(" ");}
void recv_add_sat(int llid, int tid, char *name, int mutype,
                  t_c2c_req_info *c2c_req_info){KOUT(" ");}
void recv_del_sat(int llid, int tid, char *sat){KOUT(" ");}
void recv_add_lan_endp(int llid, int tid, char *name, int num, char *lan){KOUT(" ");}
void recv_del_lan_endp(int llid, int tid, char *name, int num, char *lan){KOUT(" ");}
void recv_event_spy_sub(int llid, int tid, char *name, char *intf, char *dir){KOUT(" ");}
void recv_event_spy_unsub(int llid, int tid, char *name, char *intf, char *dir){KOUT(" ");}
void recv_event_spy(int llid, int tid, char *name, char *intf, char *dir,
                    int secs, int usecs, int len, char *msg){KOUT(" ");}
void recv_eventfull_sub(int llid, int tid){KOUT(" ");}
void recv_eventfull(int llid, int tid, int nb_endp, t_eventfull_endp *endp){KOUT(" ");} 
void recv_list_pid_req(int llid, int tid){KOUT(" ");}
void recv_list_pid_resp(int llid, int tid, int qty, t_pid_lst *pid){KOUT(" ");}
void recv_list_commands_req(int llid, int tid){KOUT(" ");}
void recv_list_commands_resp(int llid, int tid, int qty, t_list_commands *list){KOUT(" ");}
void recv_kill_uml_clownix(int llid, int tid){KOUT(" ");}
void recv_del_all(int llid, int tid){KOUT(" ");}
void recv_event_topo_sub(int llid, int tid){KOUT(" ");}
void recv_event_topo_unsub(int llid, int tid){KOUT(" ");}
void recv_evt_print_sub(int llid, int tid){KOUT(" ");}
void recv_evt_print_unsub(int llid, int tid){KOUT(" ");}
void recv_evt_print(int llid, int tid, char *info){KOUT(" ");}

void recv_evt_stats_endp_sub(int llid, int tid, char *name, int num, int sub){KOUT(" ");}
void recv_evt_stats_sysinfo_sub(int llid, int tid, char *name, int sub){KOUT(" ");}
void recv_blkd_reports_sub(int llid, int tid, int sub){KOUT(" ");}

void recv_event_sys_sub(int llid, int tid){KOUT(" ");}
void recv_event_sys_unsub(int llid, int tid){KOUT(" ");}
void recv_event_sys(int llid, int tid, t_sys_info *sys){KOUT(" ");}
void recv_topo_small_event_sub(int llid, int tid){KOUT(" ");}
void recv_topo_small_event_unsub(int llid, int tid){KOUT(" ");}
void recv_topo_small_event(int llid, int tid, char *name, 
                           char *param1, char *param2, int vm_evt){KOUT(" ");}
void recv_vmcmd(int llid, int tid, char *name, int vmcmd, int param){KOUT(" ");}

void recv_mucli_dialog_req(int llid, int tid,
                           char *name, int eth, char *line){KOUT(" ");}
void recv_mucli_dialog_resp(int llid, int tid,
                            char *name, int eth, char *line, int status){KOUT(" ");}

void recv_qmp_sub(int llid, int tid, char *name){KOUT(" ");};
void recv_qmp_req(int llid, int tid, char *name, char *msg){KOUT(" ");};
void recv_qmp_resp(int llid, int tid, char *name, char *line, int status){KOUT(" ");};
/*---------------------------------------------------------------------------*/





