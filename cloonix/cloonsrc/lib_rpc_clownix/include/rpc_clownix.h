/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */

enum {
    type_hop_unused = 0,
    type_hop_ovs,
    type_hop_ovsdb,
    type_hop_endp,
    type_hop_doors,
    type_hop_suid_power,
    type_hop_gui_server,
    type_hop_nat_main,
    type_hop_proxymous,
    type_hop_c2c,
    type_hop_a2b,
    type_hop_snf,
    type_hop_max
    };

enum {
  vmcmd_min = 0,
  vmcmd_reboot_with_qemu,
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
  type_llid_trace_proxymous,
  type_llid_trace_jfs,
  type_llid_trace_unix_proxy,
  type_llid_trace_unix_qmp,
  type_llid_trace_endp_suid,
  type_llid_max,
};
typedef struct t_pid_lst
{
  char name[MAX_NAME_LEN];
  int pid;
  int unusedalign;
  int cpu_tenth;
  int rss_kb;
} t_pid_lst;
typedef struct t_list_commands
{
  char cmd[MAX_LIST_COMMANDS_LEN];
} t_list_commands;
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
typedef struct t_slowperiodic
{
  char name[MAX_NAME_LEN];
} t_slowperiodic;
/* One image of bulk: kind is cnt or qcow2, owner the user the tree is his. */
typedef struct t_image_item
{
  char kind[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  char owner[MAX_NAME_LEN];
} t_image_item;
void doors_io_basic_json_init(t_llid_tx llid_tx);
int  doors_io_basic_decoder (int llid, int len, char *buf);
typedef void (*t_json_trace)(int llid, int is_tx, char *buf, int len);
void doors_io_basic_json_trace_set(t_json_trace json_trace);

void send_work_dir_req(int llid, int tid);
void recv_work_dir_req(int llid, int tid);

void send_work_dir_resp(int llid, int tid, t_topo_clc *clc);
void recv_work_dir_resp(int llid, int tid, t_topo_clc *clc);

void send_status_ok(int llid, int tid, char *txt);
void recv_status_ok(int llid, int tid, char *txt);
void send_status_ko(int llid, int tid, char *reason);
void recv_status_ko(int llid, int tid, char *reason);

void send_cnt_add(int llid, int tid, t_topo_cnt *cnt);
void recv_cnt_add(int llid, int tid, t_topo_cnt *cnt);
void send_add_vm(int llid, int tid, t_topo_kvm *kvm);
void recv_add_vm(int llid, int tid, t_topo_kvm *kvm);
void send_ovs_cmd_req(int llid, int tid, char *tool, char *args);
void recv_ovs_cmd_req(int llid, int tid, char *tool, char *args);
void send_ovs_cmd_resp(int llid, int tid, char *tool, int status,
                       int qty, t_list_commands *list);
void recv_ovs_cmd_resp(int llid, int tid, char *tool, int status,
                       int qty, t_list_commands *list);

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
void send_slowperiodic_cnt(int llid, int tid, int nb, t_slowperiodic *spic); 
void recv_slowperiodic_cnt(int llid, int tid, int nb, t_slowperiodic *spic);

void send_list_pid_req(int llid, int tid);
void recv_list_pid_req(int llid, int tid);

void send_list_pid_resp(int llid, int tid, int qty, t_pid_lst *pid);
void recv_list_pid_resp(int llid, int tid, int qty, t_pid_lst *pid);

void send_list_commands_req(int llid, int tid, int is_layout);
void recv_list_commands_req(int llid, int tid, int is_layout);

void send_list_commands_resp(int llid, int tid, int qty, t_list_commands *list);
void recv_list_commands_resp(int llid, int tid, int qty, t_list_commands *list);

void send_sync_wireshark_req(int llid, int tid, char *name, int num, int cmd);
void recv_sync_wireshark_req(int llid, int tid, char *name, int num, int cmd);
void send_sync_wireshark_resp(int llid,int tid,char *name,int num,int status);
void recv_sync_wireshark_resp(int llid,int tid,char *name,int num,int status);

void send_kill_uml_clownix(int llid, int tid);
void recv_kill_uml_clownix(int llid, int tid);

void send_del_all(int llid, int tid);
void recv_del_all(int llid, int tid);

void send_event_topo_sub(int llid, int tid);
void recv_event_topo_sub(int llid, int tid);
void send_event_topo(int llid, int tid, t_topo_info *topo);
void recv_event_topo(int llid, int tid, t_topo_info *topo);

enum {
  trace_kind_min = 0,
  trace_kind_qemu,
  trace_kind_json_cli,
  trace_kind_crun,
  trace_kind_nspace,
  trace_kind_max,
};
#define MAX_TRACE_LEN 32768

void send_trace_sub(int llid, int tid, int kind, int sub_on);
void recv_trace_sub(int llid, int tid, int kind, int sub_on);
void send_trace(int llid, int tid, int kind, char *name, char *txt);
void recv_trace(int llid, int tid, int kind, char *name, char *txt);

void send_topo_small_event_sub(int llid, int tid);
void recv_topo_small_event_sub(int llid, int tid);
void send_topo_small_event(int llid, int tid, char *name, 
                           char *param1, char *param2, int vm_evt);
void recv_topo_small_event(int llid, int tid, char *name, 
                           char *param1, char *param2, int vm_evt);

char *llid_trace_lib(int type);

void send_vmcmd(int llid, int tid, char *name, int vmcmd, int param);
void recv_vmcmd(int llid, int tid, char *name, int vmcmd, int param);

char *prop_flags_ascii_get(int prop_flags);
void send_nat_add(int llid, int tid, char *name, char *ports);
void recv_nat_add(int llid, int tid, char *name, char *ports);
void send_phy_add(int llid, int tid, char *name, int type);
void recv_phy_add(int llid, int tid, char *name, int type);
void send_tap_add(int llid, int tid, char *name);
void recv_tap_add(int llid, int tid, char *name);
void send_a2b_add(int llid, int tid, char *name);
void recv_a2b_add(int llid, int tid, char *name);
void send_c2c_cnf(int llid, int tid, char *name, char *cmd);
void recv_c2c_cnf(int llid, int tid, char *name, char *cmd);
void send_nat_cnf(int llid, int tid, char *name, char *cmd);
void send_cnt_cnf(int llid, int tid, char *name, char *cmd);
void send_images_req(int llid, int tid);
void recv_images_req(int llid, int tid);
void send_images_resp(int llid, int tid, char *runner, int qty,
                      t_image_item *items);
void recv_images_resp(int llid, int tid, char *runner, int qty,
                      t_image_item *items);
void recv_cnt_cnf(int llid, int tid, char *name, char *cmd);
void recv_nat_cnf(int llid, int tid, char *name, char *cmd);
void send_a2b_cnf(int llid, int tid, char *name, char *cmd);
void recv_a2b_cnf(int llid, int tid, char *name, char *cmd);
void send_lan_cnf(int llid, int tid, char *name, char *cmd);
void recv_lan_cnf(int llid, int tid, char *name, char *cmd);
void send_c2c_add(int llid, int tid, char *c2c_name, uint32_t local_udp_ip, 
                  char *slave_cloon, uint32_t ip, uint16_t port,
                  char *passwd, uint32_t udp_ip, uint16_t c2c_udp_port_low);

void recv_c2c_add(int llid, int tid, char *c2c_name, uint32_t local_udp_ip, 
                  char *slave_cloon, uint32_t ip, uint16_t port,
                  char *passwd, uint32_t udp_ip, uint16_t c2c_udp_port_low);

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

/* Canvas layout, set from the cli and kept by the server so every browser
   agrees and a reload does not lose it. */
void send_item_pos(int llid, int tid, char *name, int x, int y);
void recv_item_pos(int llid, int tid, char *name, int x, int y);

void send_item_poseth(int llid, int tid, char *name, int num, int deg);
void recv_item_poseth(int llid, int tid, char *name, int num, int deg);

/* A note on an item, shown on the canvas: kept by the switch like the
   layout, replayed to every topology subscriber, "" removes it. */
void send_item_note(int llid, int tid, char *name, char *txt);
void recv_item_note(int llid, int tid, char *name, char *txt);

#define MAX_PCAP_PKT_LEN  65536
#define MAX_PCAP_FILTER_LEN 512
#define PCAP_TYPE_HEADER  0
#define PCAP_TYPE_PACKET  1

void send_pcap_open(int llid, int tid, char *name, int num,
                    char *filter);
void recv_pcap_open(int llid, int tid, char *name, int num,
                    char *filter);

void send_pcap_close(int llid, int tid, char *name, int num);
void recv_pcap_close(int llid, int tid, char *name, int num);

void send_pcap_traffic(int llid, int tid, char *name, int num, int type,
                       uint32_t pkt_nb, uint32_t sec, uint32_t usec,
                       int caplen, int len, uint8_t *data);
void recv_pcap_traffic(int llid, int tid, char *name, int num, int type,
                       uint32_t pkt_nb, uint32_t sec, uint32_t usec,
                       int caplen, int len, uint8_t *data);

