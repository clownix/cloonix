/*****************************************************************************/
/*    Copyright (C) 2006-2019 cloonix@cloonix.net License AGPL-3             */
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
#define NO_DEFINED_VALUE "NO_DEFINED_VALUE_ITEM"
#define DTACH_SOCK "dtach"
#define DOORS_CTRL_SOCK "doors_ctrl_sock"
#define SPICE_SOCK "spice_sock"
#define CLOONIX_SWITCH "cloonix_switch"
#define XWY_TRAFFIC_SOCK "xwy_traf"
#define XWY_CONTROL_SOCK "xwy_ctrl"
#define MUSWITCH_SOCK_DIR "mu"
#define MUSWITCH_TRAF_DIR "tmu"
#define ENDP_SOCK_DIR "endp"
#define CLI_SOCK_DIR "cli"
#define SNF_PCAP_DIR "snf"


#define MAX_STATS_ITEMS 30

#define MAX_LAN           (0x3FFF + 2)
#define MAX_FD_QTY         5000
#define MAX_BUF_SIZE       2000
#define MAX_ITEM_LEN       200
#define MAX_TYPE_LEN       32
#define MAX_BIG_BUF        20000 
#define MAX_PRINT_LEN      5000
#define HALF_SEC_BEAT      50 
#define MAX_ARGC_ARGV      100
#define MAX_INTF_MNGT      100
#define SPY_TX "spy_tx"
#define SPY_RX "spy_rx"

#define MAX_LIST_COMMANDS_LEN     256
#define MAX_LIST_COMMANDS_QTY     500

#define ROOTFS_STATIC_PREFIX "rootfs_"
#define NVRAM_STATIC_PREFIX "nvram_"

enum {
    type_hop_unused = 0,
    type_hop_mulan,
    type_hop_endp,
    type_hop_snf,
    type_hop_doors,
    type_hop_max
    };


enum {
  vmcmd_min = 0,
  vmcmd_del,
  vmcmd_halt_with_qemu,
  vmcmd_reboot_with_qemu,
  vmcmd_promiscious_flag_set,
  vmcmd_promiscious_flag_unset,
  vmcmd_max,
};


enum
{
  vm_evt_none = 0,
  vm_evt_poll_not_used,
  vm_evt_dtach_launch_ok,
  vm_evt_dtach_launch_ko,
  vm_evt_ping_ok,
  vm_evt_ping_ko,
  vm_evt_cloonix_ga_ping_ok,
  vm_evt_cloonix_ga_ping_ko,
  c2c_evt_connection_ok,
  c2c_evt_connection_ko,
  c2c_evt_mod_master_slave,
  snf_evt_capture_on,
  snf_evt_capture_off,
  snf_evt_recpath_change,
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
  type_llid_trace_mulan,
  type_llid_trace_endp_kvm_eth,
  type_llid_trace_endp_kvm_wlan,
  type_llid_trace_endp_tap,
  type_llid_trace_endp_raw,
  type_llid_trace_endp_wif,
  type_llid_trace_endp_snf,
  type_llid_trace_endp_c2c,
  type_llid_trace_endp_nat,
  type_llid_trace_endp_a2b,
  type_llid_trace_jfs,
  type_llid_trace_unix_qmonitor,
  type_llid_trace_unix_xwy,
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
typedef struct t_blkd_reports
{
  int nb_blkd_reports;
  t_blkd_item *blkd_item;
} t_blkd_reports;
/*---------------------------------------------------------------------------*/
typedef struct t_c2c_req_info
  {
  char cloonix_slave[MAX_NAME_LEN];
  char passwd_slave[MSG_DIGEST_LEN];
  int ip_slave;
  int port_slave;
  } t_c2c_req_info;
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
void doors_io_basic_xml_init(t_llid_tx llid_tx);
int  doors_io_basic_decoder (int llid, int len, char *buf);
/*---------------------------------------------------------------------------*/

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

void send_add_vm(int llid, int tid, t_topo_kvm *kvm);
void recv_add_vm(int llid, int tid, t_topo_kvm *kvm);
void send_sav_vm(int llid, int tid, char *name, int type, char *sav_vm_path);
void recv_sav_vm(int llid, int tid, char *name, int type, char *sav_vm_path);
void send_sav_vm_all(int llid, int tid, int type, char *sav_vm_path);
void recv_sav_vm_all(int llid, int tid, int type, char *sav_vm_path);

void send_add_sat(int llid, int tid, char *name,int type,t_c2c_req_info *c2c);
void recv_add_sat(int llid, int tid, char *name,int type,t_c2c_req_info *c2c);
void send_del_sat(int llid, int tid, char *name);
void recv_del_sat(int llid, int tid, char *name);

void send_add_lan_endp(int llid, int tid, char *name, int num, char *lan);
void recv_add_lan_endp(int llid, int tid, char *name, int num, char *lan);
void send_del_lan_endp(int llid, int tid, char *name, int num, char *lan);
void recv_del_lan_endp(int llid, int tid, char *name, int num, char *lan);

void send_eventfull_sub(int llid, int tid);
void recv_eventfull_sub(int llid, int tid);
void send_eventfull(int llid, int tid, int nb_endp, t_eventfull_endp *endp); 
void recv_eventfull(int llid, int tid, int nb_endp, t_eventfull_endp *endp);

void send_list_pid_req(int llid, int tid);
void recv_list_pid_req(int llid, int tid);

void send_list_pid_resp(int llid, int tid, int qty, t_pid_lst *pid);
void recv_list_pid_resp(int llid, int tid, int qty, t_pid_lst *pid);

void send_list_commands_req(int llid, int tid);
void recv_list_commands_req(int llid, int tid);

void send_list_commands_resp(int llid, int tid, int qty, t_list_commands *list);
void recv_list_commands_resp(int llid, int tid, int qty, t_list_commands *list);

void send_blkd_reports_sub(int llid, int tid, int sub);
void recv_blkd_reports_sub(int llid, int tid, int sub);
void send_blkd_reports(int llid, int tid, t_blkd_reports *blkd);
void recv_blkd_reports(int llid, int tid, t_blkd_reports *blkd);

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
void send_mucli_dialog_req(int llid, int tid, char *name, int num, char *line);
void recv_mucli_dialog_req(int llid, int tid, char *name, int num, char *line);
void send_mucli_dialog_resp(int llid, int tid, char *name, int num, 
                            char *line, int status);
void recv_mucli_dialog_resp(int llid, int tid, char *name, int num,
                            char *line, int status);
/*---------------------------------------------------------------------------*/
char *prop_flags_ascii_get(int prop_flags);
/*---------------------------------------------------------------------------*/
void send_qmp_sub(int llid, int tid, char *name);
void recv_qmp_sub(int llid, int tid, char *name);
void send_qmp_req(int llid, int tid, char *name, char *msg);
void recv_qmp_req(int llid, int tid, char *name, char *msg);
void send_qmp_resp(int llid, int tid, char *name, char *line, int status);
void recv_qmp_resp(int llid, int tid, char *name, char *line, int status);
/*---------------------------------------------------------------------------*/
