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


void rpct_recv_sigdiag_msg(int llid, int tid, char *line) {KOUT();}
void rpct_recv_poldiag_msg(int llid, int tid, char *line) {KOUT();}
void recv_evt_stats_endp_sub(int llid, int tid, char *name, int num, int sub)
{
  KOUT(" ");
}
void recv_evt_stats_sysinfo_sub(int llid, int tid, char *name, int sub)
{
  KOUT(" ");
}
void recv_blkd_reports_sub(int llid, int tid, int sub)
{
  KOUT(" ");
}
void recv_work_dir_req(int llid, int tid)
{
  KOUT(" ");
}
void recv_topo_small_event_sub(int llid, int tid)
{
  KOUT(" ");
}
void recv_topo_small_event_unsub(int llid, int tid)
{
  KOUT(" ");
}
void recv_event_topo_sub(int llid, int tid)
{
  KOUT(" ");
}
void recv_evt_print_sub(int llid, int tid)
{
  KOUT(" ");
}
void recv_set_all_paths(int llid, int tid,   char *ios)
{
  KOUT(" ");
}
void recv_add_vm(int llid, int tid, t_topo_kvm *kvm)
{
  KOUT(" ");
}
void recv_add_lan_endp(int llid, int tid, char *name, int num, char *lan)
{
  KOUT(" ");
}
void recv_del_lan_endp(int llid, int tid, char *name, int num, char *lan)
{
  KOUT(" ");
}
void recv_list_pid_req(int llid, int tid)
{
  KOUT(" ");
}
void recv_list_commands_req(int llid, int tid, int is_layout)
{
  KOUT(" ");
}
void recv_kill_uml_clownix(int llid, int tid)
{
  KOUT(" ");
}
void recv_event_sys_sub(int llid, int tid)
{
  KOUT(" ");
}
void recv_event_sys_unsub(int llid, int tid)
{
  KOUT(" ");
}
void recv_event_topo_unsub(int llid, int tid)
{
  KOUT(" ");
}
void recv_evt_print_unsub(int llid, int tid)
{
  KOUT(" ");
}
void recv_del_all(int llid, int tid)
{
  KOUT(" ");
}
void recv_list_vm_req(int llid, int tid)
{
  KOUT(" ");
}
void recv_list_eth_req(int llid, int tid, char *name)
{
  KOUT(" ");
}
void recv_list_vm_eth_lan_req(int llid, int tid, char *name, int eth)
{
  KOUT(" ");
}
void recv_interface_list_req(int llid, int tid)
{
  KOUT(" ");
}
void recv_vmcmd(int llid, int tid, char *name, int cmd, int param)
{
  KOUT(" ");
}
void recv_del_name(int llid, int tid, char *name)
{
  KOUT(" ");
}
void recv_fix_display(int llid, int tid, char *line)
{
  KOUT(" ");
}
void recv_eventfull_sub(int llid, int tid)
{
  KOUT(" ");
}
void recv_slowperiodic_sub(int llid, int tid)
{
  KOUT(" ");
}
void recv_event_print_sub(int llid, int tid, int on)
{
  KOUT(" ");
}
void recv_event_print(int llid, int tid, char *line)
{
  KOUT(" ");
}
void recv_get_hop_name_list(int llid, int tid)
{
  KOUT(" ");
}
void recv_layout_modif(int llid, int tid, int modif_type, char *name, int num,
                       int val1, int val2)
{
  KOUT(" ");
}
void recv_event_spy_sub(int llid, int tid, char *name, char *intf, char *dir)
{
  KOUT(" ");
}
void recv_event_spy_unsub(int llid, int tid, char *name, char *intf, char *dir)
{
  KOUT(" ");
}
void recv_event_spy(int llid, int tid, char *name, char *intf, char *dir,
                    int secs, int usecs, int len, char *msg)
{
  KOUT(" ");
}
void recv_sav_vm(int llid, int tid, char *name, char *path)
{
  KOUT(" ");
}
void rpct_recv_kil_req(int llid, int tid){KOUT();}
void rpct_recv_pid_req(int llid, int tid, char *name, int num)
{
  KOUT(" ");
}
void rpct_recv_pid_resp(int llid, int tid, char *name, int num,
                        int toppid, int pid)
{
  KOUT(" ");
}
void rpct_recv_hop_sub(int llid, int tid, int flags_hop)
{
  KOUT(" ");
}
void rpct_recv_hop_unsub(int llid, int tid)
{
  KOUT(" ");
}
void rpct_recv_hop_msg(int llid, int tid, int flags_hop, char *txt)
{
  KOUT(" ");
}
void recv_hop_evt_doors_sub(int llid, int tid, int flags_hop, 
                            int nb, t_hop_list *list)
{
  KOUT(" ");
}
void recv_hop_evt_doors_unsub(int llid, int tid)
{
  KOUT(" ");
}
void recv_hop_get_name_list_doors(int llid, int tid)
{
  KOUT(" ");
}
void recv_c2c_add(int llid, int tid, char *name, uint32_t local_udp_ip,
                  char *net, uint32_t ip, uint16_t port,
                  char *passwd, uint32_t udp_ip, uint16_t c2c_udp_port_low)
{
  KOUT(" ");
}
void recv_snf_add(int llid, int tid, char *name, int num, int val)
{
  KOUT(" ");
}
void recv_c2c_peer_create(int llid, int tid, char *c2c_name, int is_ack,
                          char *distant_cloon, char *local_cloon)
{KOUT(" ");}

void recv_c2c_peer_conf(int llid, int tid, char *c2c_name, int is_ack,
                        char *distant_cloon,     char *local_cloon,
                        uint32_t distant_udp_ip,   uint32_t local_udp_ip,
                        uint16_t distant_udp_port, uint16_t local_udp_port)
{KOUT(" ");}

void recv_c2c_peer_ping(int llid, int tid, char *c2c_name, int status)
{KOUT(" ");}
void recv_color_item(int llid, int tid, char *name, int color)
{KOUT(" ");}
void recv_novnc_on_off(int llid, int tid, int color)
{KOUT(" ");}
void recv_lan_cnf(int llid, int tid, char *name, char *cmd)
{KOUT(" ");}
void recv_a2b_cnf(int llid, int tid, char *name, char *cmd)
{KOUT(" ");}
void recv_c2c_cnf(int llid, int tid, char *name, char *cmd)
{KOUT(" ");}
void recv_nat_cnf(int llid, int tid, char *name, char *cmd)
{KOUT(" ");}

void recv_cnt_add(int llid, int tid, t_topo_cnt *cnt)
{KOUT(" ");}
void recv_tap_add(int llid, int tid, char *name)
{KOUT(" ");}
void recv_a2b_add(int llid, int tid, char *name)
{KOUT(" ");}
void recv_nat_add(int llid, int tid, char *name)
{KOUT(" ");}
void recv_phy_add(int llid, int tid, char *name, int type)
{KOUT(" ");}

void recv_sync_wireshark_req(int llid, int tid, char *name, int num, int cmd)
{KOUT(" ");}

void recv_hop_evt_doors(int llid, int tid, int flags_hop,
                        char *name, char *txt) 
{
 KOUT(" ");
}

void recv_hop_name_list_doors(int llid, int tid, int nb, t_hop_list *list)
{                  
  KOUT(" ");
}
void recv_evt_stats_endp(int llid, int tid, char *network, char *name, int num,
                         t_stats_counts *stats_counts, int status)
{ 
  KOUT(" ");
}
void recv_evt_stats_sysinfo(int llid, int tid, char *network, char *name,
                            t_stats_sysinfo *stats_sysinfo,
                            char *df, int status)
{
  KOUT(" ");
}
void recv_event_sys(int llid, int tid, t_sys_info *info)
{ 
  KOUT("%d %p", llid, info);
}
void recv_topo_small_event(int llid, int tid, char *name, 
                           char *p1, char *p2, int vm_evt)
{
  KOUT("%d %s %s %s %d", llid, name, p1, p2, vm_evt);
}
void recv_interface_list_resp(int llid, int tid, int nb,
                              char **intf, char *err)
{
  KOUT("%d %d %p %s", llid, nb, intf, err);
}

void recv_event_topo(int llid, int tid, t_topo_info *topo)
{
  KOUT("%d %p", llid, topo);
}
void recv_evt_print(int llid,int tid,  char *info)
{
  KOUT("%d %s", llid, info);
}
void recv_status_ok(int llid, int tid, char *info)
{
  KOUT("%d %s", llid, info);
}
void recv_status_ko(int llid, int tid, char *reason)
{ 
  KOUT("%d %s", llid, reason);
}
void recv_list_pid_resp(int llid, int tid, int qty, t_pid_lst *pid)
{
  KOUT("%d %d %p", llid, qty, pid);
} 
void recv_list_commands_resp(int llid, int tid, int qty, t_list_commands *list)
{
  KOUT("%d %d %s", llid, qty, list[0].cmd);
}
void recv_work_dir_resp(int llid, int tid, t_topo_clc *conf)
{
  KOUT(" ");
}
void recv_eventfull(int llid, int tid, int nb_endp, t_eventfull_endp *endp)
{
  KOUT(" ");
}
void recv_slowperiodic_qcow2(int llid, int tid, int nb, t_slowperiodic *spic)
{ 
  KOUT(" ");
}
void recv_slowperiodic_img(int llid, int tid, int nb, t_slowperiodic *spic)
{ 
  KOUT(" ");
}

void recv_sync_wireshark_resp(int llid,int tid,char *name,int num,int status)
{KOUT(" ");}



