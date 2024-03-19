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

enum {
    type_hop_unused = 0,
    type_hop_ovs,
    type_hop_ovsdb,
    type_hop_endp,
    type_hop_doors,
    type_hop_suid_power,
    type_hop_nat,
    type_hop_c2c,
    type_hop_a2b,
    type_hop_snf,
    type_hop_max
    };


enum {
  vmcmd_min = 0,
  vmcmd_reboot_with_qemu,
  vmcmd_promiscious_flag_set,
  vmcmd_promiscious_flag_unset,
  vmcmd_max,
};


enum
{
  vm_evt_none = 0,
  vm_evt_poll_not_used,
  vm_evt_ping_ok,
  vm_evt_ping_ko,
  vm_evt_cloonix_ga_ping_ok,
  vm_evt_cloonix_ga_ping_ko,
  vm_evt_qmp_conn_ok,
  vm_evt_qmp_conn_ko,
  vm_evt_max,
};


enum
{
  type_llid_trace_all = 0,
  type_llid_trace_client_unix,
  type_llid_trace_listen_client_unix,
  type_llid_trace_clone,
  type_llid_trace_listen_clone,
  type_llid_trace_doorways,
  type_llid_trace_endp_kvm,
  type_llid_trace_endp_tap,
  type_llid_trace_endp_phy,
  type_llid_trace_endp_nat,
  type_llid_trace_endp_ovs,
  type_llid_trace_endp_ovsdb,
  type_llid_trace_jfs,
  type_llid_trace_unix_xwy,
  type_llid_trace_unix_qmp,
  type_llid_trace_endp_suid,
  type_llid_max,
};
/*---------------------------------------------------------------------------*/
typedef struct t_hop_list
{
  int type_hop;
  char name[MAX_NAME_LEN];
  int  num;
} t_hop_list;
/*---------------------------------------------------------------------------*/
typedef struct t_queue_tx
{
  int peak_size;
  int size;
  int llid;
  int fd;
  int waked_count_in;
  int waked_count_out;
  int waked_count_err;
  int out_bytes;
  int in_bytes;
  int id;
  int type;
  int unusedalign;
  char name[MAX_PATH_LEN];
} t_queue_tx;
/*---------------------------------------------------------------------------*/
typedef struct t_sys_info
{
  int selects;
  unsigned long mallocs[MAX_MALLOC_TYPES];
  unsigned long fds_used[type_llid_max];
  int cur_channels;
  int max_channels;
  int cur_channels_recv;
  int cur_channels_send;
  int clients; 
  int max_time;
  int avg_time;
  int above50ms;
  int above20ms;
  int above15ms;
  int nb_queue_tx;
  t_queue_tx *queue_tx;
} t_sys_info;
/*---------------------------------------------------------------------------*/
typedef struct t_stats_count_item
{
  int time_ms;
  int ptx;
  int btx;
  int prx;
  int brx;
} t_stats_count_item;

/*---------------------------------------------------------------------------*/
typedef struct t_stats_counts
{
  int nb_items;
  t_stats_count_item item[MAX_STATS_ITEMS];
} t_stats_counts;
/*---------------------------------------------------------------------------*/
typedef struct t_stats_sysinfo
{
  int time_ms;
  unsigned long uptime;
  unsigned long load1;
  unsigned long load5;
  unsigned long load15;
  unsigned long totalram;
  unsigned long freeram;
  unsigned long cachedram;
  unsigned long sharedram;
  unsigned long bufferram;
  unsigned long totalswap;
  unsigned long freeswap;
  unsigned long procs;
  unsigned long totalhigh;
  unsigned long freehigh;
  unsigned long mem_unit;
  unsigned long process_utime;
  unsigned long process_stime;
  unsigned long process_cutime;
  unsigned long process_cstime;
  unsigned long process_rss;
} t_stats_sysinfo;
/*---------------------------------------------------------------------------*/
typedef struct t_pid_lst
{
  char name[MAX_NAME_LEN];
  int pid;
  int unusedalign;
} t_pid_lst;
/*---------------------------------------------------------------------------*/
typedef struct t_list_commands
{
  char cmd[MAX_LIST_COMMANDS_LEN];
} t_list_commands;
/*---------------------------------------------------------------------------*/
typedef struct t_eventfull_endp
{
  char name[MAX_NAME_LEN];
  int  num;
  int  type;
  int  ram;
  int  cpu;
  int  ok;
  int  ptx;
  int  prx;
  int  btx;
  int  brx;
  int  ms;
} t_eventfull_endp;
/*---------------------------------------------------------------------------*/
typedef struct t_slowperiodic
{
  char name[MAX_NAME_LEN];
} t_slowperiodic;
/*---------------------------------------------------------------------------*/
void doors_io_basic_xml_init(t_llid_tx llid_tx);
int  doors_io_basic_decoder (int llid, int len, char *buf);
/*---------------------------------------------------------------------------*/
void send_hop_evt_doors_sub(int llid, int tid, int flags_hop,
                            int nb, t_hop_list *list);
void recv_hop_evt_doors_sub(int llid, int tid, int flags_hop,
                            int nb, t_hop_list *list);
/*---------------------------------------------------------------------------*/
void send_hop_evt_doors_unsub(int llid, int tid);
void recv_hop_evt_doors_unsub(int llid, int tid);
/*---------------------------------------------------------------------------*/
void send_hop_evt_doors(int llid, int tid, int flags_hop,
                        char *name, char *txt);
void recv_hop_evt_doors(int llid, int tid, int flags_hop,
                        char *name, char *txt);
/*---------------------------------------------------------------------------*/
void send_hop_get_name_list_doors(int llid, int tid);
void recv_hop_get_name_list_doors(int llid, int tid);
/*---------------------------------------------------------------------------*/
void send_hop_name_list_doors(int llid, int tid, int nb, t_hop_list *list);
void recv_hop_name_list_doors(int llid, int tid, int nb, t_hop_list *list);
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
void send_work_dir_req(int llid, int tid);
void recv_work_dir_req(int llid, int tid);

void send_work_dir_resp(int llid, int tid, t_topo_clc *clc);
void recv_work_dir_resp(int llid, int tid, t_topo_clc *clc);

void send_status_ok(int llid, int tid, char *txt);
void recv_status_ok(int llid, int tid, char *txt);
void send_status_ko(int llid, int tid, char *reason);
void recv_status_ko(int llid, int tid, char *reason);

/*---------------------------------------------------------------------------*/
void send_cnt_add(int llid, int tid, t_topo_cnt *cnt);
void recv_cnt_add(int llid, int tid, t_topo_cnt *cnt);
/*---------------------------------------------------------------------------*/
void send_add_vm(int llid, int tid, t_topo_kvm *kvm);
void recv_add_vm(int llid, int tid, t_topo_kvm *kvm);
void send_sav_vm(int llid, int tid, char *name, char *sav_vm_path);
void recv_sav_vm(int llid, int tid, char *name, char *sav_vm_path);

void send_del_name(int llid, int tid, char *name);
void recv_del_name(int llid, int tid, char *name);

void send_add_lan_endp(int llid, int tid, char *name, int num, char *lan);
void recv_add_lan_endp(int llid, int tid, char *name, int num, char *lan);
void send_del_lan_endp(int llid, int tid, char *name, int num, char *lan);
void recv_del_lan_endp(int llid, int tid, char *name, int num, char *lan);

void send_eventfull_sub(int llid, int tid);
void recv_eventfull_sub(int llid, int tid);
void send_eventfull(int llid, int tid, int nb_endp, t_eventfull_endp *endp); 
void recv_eventfull(int llid, int tid, int nb_endp, t_eventfull_endp *endp);

void send_slowperiodic_sub(int llid, int tid);
void recv_slowperiodic_sub(int llid, int tid);
void send_slowperiodic_qcow2(int llid, int tid, int nb, t_slowperiodic *spic); 
void recv_slowperiodic_qcow2(int llid, int tid, int nb, t_slowperiodic *spic);
void send_slowperiodic_img(int llid, int tid, int nb, t_slowperiodic *spic); 
void recv_slowperiodic_img(int llid, int tid, int nb, t_slowperiodic *spic);

void send_list_pid_req(int llid, int tid);
void recv_list_pid_req(int llid, int tid);

void send_list_pid_resp(int llid, int tid, int qty, t_pid_lst *pid);
void recv_list_pid_resp(int llid, int tid, int qty, t_pid_lst *pid);

void send_list_commands_req(int llid, int tid, int is_layout);
void recv_list_commands_req(int llid, int tid, int is_layout);

void send_list_commands_resp(int llid, int tid, int qty, t_list_commands *list);
void recv_list_commands_resp(int llid, int tid, int qty, t_list_commands *list);

void send_kill_uml_clownix(int llid, int tid);
void recv_kill_uml_clownix(int llid, int tid);

void send_del_all(int llid, int tid);
void recv_del_all(int llid, int tid);

void send_event_topo_sub(int llid, int tid);
void recv_event_topo_sub(int llid, int tid);
void send_event_topo_unsub(int llid, int tid);
void recv_event_topo_unsub(int llid, int tid);
void send_event_topo(int llid, int tid, t_topo_info *topo);
void recv_event_topo(int llid, int tid, t_topo_info *topo);

void send_evt_print_sub(int llid, int tid);
void recv_evt_print_sub(int llid, int tid);
void send_evt_print_unsub(int llid, int tid);
void recv_evt_print_unsub(int llid, int tid);
void send_evt_print(int llid, int tid, char *info);
void recv_evt_print(int llid, int tid, char *info);

void send_evt_stats_endp_sub(int llid, int tid, char *name, int num, int sub);
void recv_evt_stats_endp_sub(int llid, int tid, char *name, int num, int sub);
void send_evt_stats_endp(int llid, int tid, char *network, char *name, int num,
                         t_stats_counts *stats_counts, int status);
void recv_evt_stats_endp(int llid, int tid, char *network, char *name, int num,
                         t_stats_counts *stats_counts, int status);

void send_evt_stats_sysinfo_sub(int llid, int tid, char *name, int sub);
void recv_evt_stats_sysinfo_sub(int llid, int tid, char *name, int sub);
void send_evt_stats_sysinfo(int llid, int tid, char *network, char *name,
                            t_stats_sysinfo *stats_sysinfo, 
                            char *df, int status);
void recv_evt_stats_sysinfo(int llid, int tid, char *network, char *name,
                            t_stats_sysinfo *stats_sysinfo, 
                            char *df, int status);

void send_event_sys_sub(int llid, int tid);
void recv_event_sys_sub(int llid, int tid);
void send_event_sys_unsub(int llid, int tid);
void recv_event_sys_unsub(int llid, int tid);
void send_event_sys(int llid, int tid, t_sys_info *sys);
void recv_event_sys(int llid, int tid, t_sys_info *sys);

void send_topo_small_event_sub(int llid, int tid);
void recv_topo_small_event_sub(int llid, int tid);
void send_topo_small_event_unsub(int llid, int tid);
void recv_topo_small_event_unsub(int llid, int tid);
void send_topo_small_event(int llid, int tid, char *name, 
                           char *param1, char *param2, int vm_evt);
void recv_topo_small_event(int llid, int tid, char *name, 
                           char *param1, char *param2, int vm_evt);

void sys_info_free(t_sys_info *sys);
char *llid_trace_lib(int type);

void send_vmcmd(int llid, int tid, char *name, int vmcmd, int param);
void recv_vmcmd(int llid, int tid, char *name, int vmcmd, int param);

/*---------------------------------------------------------------------------*/
char *prop_flags_ascii_get(int prop_flags);
/*---------------------------------------------------------------------------*/
void send_nat_add(int llid, int tid, char *name);
void recv_nat_add(int llid, int tid, char *name);
void send_phy_add(int llid, int tid, char *name, int type);
void recv_phy_add(int llid, int tid, char *name, int type);
void send_tap_add(int llid, int tid, char *name);
void recv_tap_add(int llid, int tid, char *name);
void send_a2b_add(int llid, int tid, char *name);
void recv_a2b_add(int llid, int tid, char *name);
void send_c2c_cnf(int llid, int tid, char *name, char *cmd);
void recv_c2c_cnf(int llid, int tid, char *name, char *cmd);
void send_nat_cnf(int llid, int tid, char *name, char *cmd);
void recv_nat_cnf(int llid, int tid, char *name, char *cmd);
void send_a2b_cnf(int llid, int tid, char *name, char *cmd);
void recv_a2b_cnf(int llid, int tid, char *name, char *cmd);
void send_lan_cnf(int llid, int tid, char *name, char *cmd);
void recv_lan_cnf(int llid, int tid, char *name, char *cmd);
/*---------------------------------------------------------------------------*/
void send_c2c_add(int llid, int tid, char *c2c_name, uint32_t local_udp_ip, 
                  char *slave_cloon, uint32_t ip, uint16_t port,
                  char *passwd, uint32_t udp_ip);

void recv_c2c_add(int llid, int tid, char *c2c_name, uint32_t local_udp_ip, 
                  char *slave_cloon, uint32_t ip, uint16_t port,
                  char *passwd, uint32_t udp_ip);

void send_snf_add(int llid, int tid, char *name, int num, int val);

void recv_snf_add(int llid, int tid, char *name, int num, int val);

void send_c2c_peer_create(int llid, int tid, char *c2c_name, int is_ack,
                          char *local_cloon, char *distant_cloon);

void recv_c2c_peer_create(int llid, int tid, char *c2c_name, int is_ack,
                          char *distant_cloon, char *local_cloon);

void send_c2c_peer_conf(int llid, int tid, char *c2c_name, int is_ack,
                        char *local_cloon,     char *distant_cloon,
                        uint32_t local_udp_ip,   uint32_t distant_udp_ip,
                        uint16_t local_udp_port, uint16_t distant_udp_port);

void recv_c2c_peer_conf(int llid, int tid, char *c2c_name, int is_ack,
                        char *distant_cloon,     char *local_cloon,
                        uint32_t distant_udp_ip,   uint32_t local_udp_ip,
                        uint16_t distant_udp_port, uint16_t local_udp_port);

void send_c2c_peer_ping(int llid, int tid, char *c2c_name, int status);

void recv_c2c_peer_ping(int llid, int tid, char *c2c_name, int status);
void doors_io_basic_tx_set(t_llid_tx llid_tx);

void send_color_item(int llid, int tid, char *name, int color);
void recv_color_item(int llid, int tid, char *name, int color);

/*---------------------------------------------------------------------------*/
