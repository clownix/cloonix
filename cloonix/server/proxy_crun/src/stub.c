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
#include "io_clownix.h"
#include "rpc_clownix.h"

void recv_c2c_peer_conf(int llid, int tid, char *name,
                        int status, char *dist, char *loc,
                        uint32_t dist_udp_ip,   uint32_t loc_udp_ip,
                        uint16_t dist_udp_port, uint16_t loc_udp_port)
{KERR("WARNING TOLOOKAT");}
void recv_c2c_peer_ping(int llid, int tid, char *name, int status)
{KERR("WARNING TOLOOKAT");}
void recv_c2c_add(int llid, int tid, char *name, uint32_t loc_udp_ip,
                  char *dist, uint32_t dist_ip, uint16_t dist_port,
                  char *dist_passwd, uint32_t dist_udp_ip)
{KERR("WARNING TOLOOKAT");}
void recv_c2c_peer_create(int llid, int tid, char *name,
                          int is_ack, char *dist, char *loc)
{KERR("WARNING TOLOOKAT");}
void recv_c2c_cnf(int llid, int tid, char *name, char *cmd)                                {KERR("WARNING TOLOOKAT");}
void rpct_recv_sigdiag_msg(int llid, int tid, char *line)                                  {KERR("WARNING TOLOOKAT");}
void rpct_recv_poldiag_msg(int llid, int tid, char *line)                                  {KERR("WARNING TOLOOKAT");}
void recv_evt_stats_endp_sub(int llid, int tid, char *name, int num, int sub)              {KERR("WARNING TOLOOKAT");}
void recv_evt_stats_sysinfo_sub(int llid, int tid, char *name, int sub)                    {KERR("WARNING TOLOOKAT");}
void recv_blkd_reports_sub(int llid, int tid, int sub)                                     {KERR("WARNING TOLOOKAT");}
void recv_work_dir_req(int llid, int tid)                                                  {KERR("WARNING TOLOOKAT");}
void recv_topo_small_event_sub(int llid, int tid)                                          {KERR("WARNING TOLOOKAT");}
void recv_topo_small_event_unsub(int llid, int tid)                                        {KERR("WARNING TOLOOKAT");}
void recv_event_topo_sub(int llid, int tid)                                                {KERR("WARNING TOLOOKAT");}
void recv_evt_print_sub(int llid, int tid)                                                 {KERR("WARNING TOLOOKAT");}
void recv_set_all_paths(int llid, int tid,   char *ios)                                    {KERR("WARNING TOLOOKAT");}
void recv_add_vm(int llid, int tid, t_topo_kvm *kvm)                                       {KERR("WARNING TOLOOKAT");}
void recv_add_lan_endp(int llid, int tid, char *name, int num, char *lan)                  {KERR("WARNING TOLOOKAT");}
void recv_del_lan_endp(int llid, int tid, char *name, int num, char *lan)                  {KERR("WARNING TOLOOKAT");}
void recv_list_pid_req(int llid, int tid)                                                  {KERR("WARNING TOLOOKAT");}
void recv_list_commands_req(int llid, int tid, int is_layout)                              {KERR("WARNING TOLOOKAT");}
void recv_kill_uml_clownix(int llid, int tid)                                              {KERR("WARNING TOLOOKAT");}
void recv_event_sys_sub(int llid, int tid)                                                 {KERR("WARNING TOLOOKAT");}
void recv_event_sys_unsub(int llid, int tid)                                               {KERR("WARNING TOLOOKAT");}
void recv_event_topo_unsub(int llid, int tid) {KERR("WARNING TOLOOKAT");}
void recv_evt_print_unsub(int llid, int tid) {KERR("WARNING TOLOOKAT");}
void recv_del_all(int llid, int tid) {KERR("WARNING TOLOOKAT");}
void recv_list_vm_req(int llid, int tid) {KERR("WARNING TOLOOKAT");}
void recv_list_eth_req(int llid, int tid, char *name){KERR("WARNING TOLOOKAT");}
void recv_list_vm_eth_lan_req(int llid, int tid, char *name, int eth){KERR("WARNING TOLOOKAT");}
void recv_interface_list_req(int llid, int tid) {KERR("WARNING TOLOOKAT");}
void recv_vmcmd(int llid, int tid, char *name, int cmd, int param) {KERR("WARNING TOLOOKAT");}
void recv_del_name(int llid, int tid, char *name){KERR("WARNING TOLOOKAT");}
void recv_fix_display(int llid, int tid, char *line){KERR("WARNING TOLOOKAT");}
void recv_eventfull_sub(int llid, int tid) {KERR("WARNING TOLOOKAT");}
void recv_slowperiodic_sub(int llid, int tid){KERR("WARNING TOLOOKAT");}
void recv_event_print_sub(int llid, int tid, int on){KERR("WARNING TOLOOKAT");}
void recv_event_print(int llid, int tid, char *line) {KERR("WARNING TOLOOKAT");}
void recv_get_hop_name_list(int llid, int tid){KERR("WARNING TOLOOKAT");}
void recv_layout_modif(int llid, int tid, int modif_type, char *name, int num, int val1, int val2){KERR("WARNING TOLOOKAT");}
void recv_event_spy_sub(int llid, int tid, char *name, char *intf, char *dir) {KERR("WARNING TOLOOKAT");}
void recv_event_spy_unsub(int llid, int tid, char *name, char *intf, char *dir){KERR("WARNING TOLOOKAT");}
void recv_event_spy(int llid, int tid, char *name, char *intf, char *dir, int secs, int usecs, int len, char *msg){KERR("WARNING TOLOOKAT");}
void recv_sav_vm(int llid, int tid, char *name, char *path){KERR("WARNING TOLOOKAT");}
void rpct_recv_kil_req(int llid, int tid){KERR("WARNING TOLOOKAT");}
void rpct_recv_pid_req(int llid, int tid, char *name, int num){KERR("WARNING TOLOOKAT");}
void rpct_recv_pid_resp(int llid, int tid, char *name, int num, int toppid, int pid){KERR("WARNING TOLOOKAT");}
void rpct_recv_hop_sub(int llid, int tid, int flags_hop) {KERR("WARNING TOLOOKAT");}
void rpct_recv_hop_unsub(int llid, int tid){KERR("WARNING TOLOOKAT");}
void rpct_recv_hop_msg(int llid, int tid, int flags_hop, char *txt){KERR("WARNING TOLOOKAT");}
void recv_hop_evt_doors_sub(int llid, int tid, int flags_hop, int nb, t_hop_list *list){KERR("WARNING TOLOOKAT");}
void recv_hop_evt_doors_unsub(int llid, int tid){KERR("WARNING TOLOOKAT");}
void recv_hop_get_name_list_doors(int llid, int tid){KERR("WARNING TOLOOKAT");}
void recv_snf_add(int llid, int tid, char *name, int num, int val){KERR("WARNING TOLOOKAT");}
void recv_color_item(int llid, int tid, char *name, int color){KERR("WARNING TOLOOKAT");}
void recv_novnc_on_off(int llid, int tid, int color){KERR("WARNING TOLOOKAT");}
void recv_lan_cnf(int llid, int tid, char *name, char *cmd){KERR("WARNING TOLOOKAT");}
void recv_a2b_cnf(int llid, int tid, char *name, char *cmd){KERR("WARNING TOLOOKAT");}
void recv_nat_cnf(int llid, int tid, char *name, char *cmd){KERR("WARNING TOLOOKAT");}
void recv_cnt_add(int llid, int tid, t_topo_cnt *cnt){KERR("WARNING TOLOOKAT");}
void recv_tap_add(int llid, int tid, char *name){KERR("WARNING TOLOOKAT");}
void recv_a2b_add(int llid, int tid, char *name){KERR("WARNING TOLOOKAT");}
void recv_nat_add(int llid, int tid, char *name){KERR("WARNING TOLOOKAT");}
void recv_phy_add(int llid, int tid, char *name, int type){KERR("WARNING TOLOOKAT");}
void recv_sync_wireshark_req(int llid, int tid, char *name, int num, int cmd){KERR("WARNING TOLOOKAT");}
void recv_hop_evt_doors(int llid, int tid, int flags_hop, char *name, char *txt){KERR("WARNING TOLOOKAT");}
void recv_hop_name_list_doors(int llid, int tid, int nb, t_hop_list *list){KERR("WARNING TOLOOKAT");}
void recv_event_topo(int llid, int tid, t_topo_info *topo){KERR("WARNING TOLOOKAT");}
void recv_evt_print(int llid,int tid,  char *info){KERR("WARNING TOLOOKAT");}
void recv_status_ok(int llid, int tid, char *info){KERR("WARNING TOLOOKAT");}
void recv_status_ko(int llid, int tid, char *reason){KERR("WARNING TOLOOKAT");}
void recv_list_pid_resp(int llid, int tid, int qty, t_pid_lst *pid){KERR("WARNING TOLOOKAT");}
void recv_list_commands_resp(int llid, int tid, int qty, t_list_commands *list){KERR("WARNING TOLOOKAT");}
void recv_work_dir_resp(int llid, int tid, t_topo_clc *conf){KERR("WARNING TOLOOKAT");}
void recv_eventfull(int llid, int tid, int nb_endp, t_eventfull_endp *endp){KERR("WARNING TOLOOKAT");}
void recv_slowperiodic_qcow2(int llid, int tid, int nb, t_slowperiodic *spic){KERR("WARNING TOLOOKAT");}
void recv_slowperiodic_img(int llid, int tid, int nb, t_slowperiodic *spic){KERR("WARNING TOLOOKAT");}
void recv_sync_wireshark_resp(int llid,int tid,char *name,int num,int status){KERR("WARNING TOLOOKAT");}
void recv_event_sys(int llid, int tid, t_sys_info *info) {KERR("WARNING TOLOOKAT");}

void recv_evt_stats_endp(int llid, int tid, char *network, char *name, int num,
                         t_stats_counts *stats_counts, int status) {KERR("WARNING TOLOOKAT");}
void recv_evt_stats_sysinfo(int llid, int tid, char *network, char *name,
                            t_stats_sysinfo *stats_sysinfo,
                            char *df, int status) {KERR("WARNING TOLOOKAT");}
void recv_topo_small_event(int llid, int tid, char *name, 
                           char *p1, char *p2, int vm_evt) {KERR("WARNING TOLOOKAT");}
void recv_interface_list_resp(int llid, int tid, int nb,
                              char **intf, char *err) {KERR("WARNING TOLOOKAT");}

