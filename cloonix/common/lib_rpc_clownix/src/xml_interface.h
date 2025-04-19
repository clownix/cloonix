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
#define MAX_CLOWNIX_BOUND_LEN      64
#define MIN_CLOWNIX_BOUND_LEN      2

/*---------------------------------------------------------------------------*/
#define NAT_ADD          "<nat_add>"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "</nat_add>"
/*---------------------------------------------------------------------------*/
#define PHY_ADD          "<phy_add>"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <type> %d </type>\n"\
                         "</phy_add>"
/*---------------------------------------------------------------------------*/
#define TAP_ADD          "<tap_add>"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "</tap_add>"
/*---------------------------------------------------------------------------*/
#define A2B_ADD          "<a2b_add>"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "</a2b_add>"

#define A2B_CNF          "<a2b_cnf>"\
                         "  <tid>  %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <cmd>%s</cmd>\n"\
                         "</a2b_cnf>"

#define A2B_CNF_BIS      "<a2b_cnf>"\
                         "  <tid>  %d </tid>\n"\
                         "  <name> %s </name>\n"

#define C2C_CNF          "<c2c_cnf>"\
                         "  <tid>  %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <cmd>  %s </cmd>\n"\
                         "</c2c_cnf>"

#define NAT_CNF          "<nat_cnf>"\
                         "  <tid>  %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <cmd>  %s </cmd>\n"\
                         "</nat_cnf>"

#define LAN_CNF          "<lan_cnf>"\
                         "  <tid>  %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <cmd>  %s </cmd>\n"\
                         "</lan_cnf>"

/*---------------------------------------------------------------------------*/
#define C2C_MAC_O        "<c2c_mac>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <nb_mac> %d </nb_mac>\n"

#define C2C_MAC_I        "  <mac> %s </mac>\n"

#define C2C_MAC_C        "</c2c_mac>\n"
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
#define SNF_ADD          "<snf_add>"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <num> %d </num>\n"\
                         "  <val> %d </val>\n"\
                         "</snf_add>"
/*---------------------------------------------------------------------------*/
#define C2C_ADD          "<c2c_add>"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <local_udp_ip> %x </local_udp_ip>\n"\
                         "  <slave_net> %s </slave_net>\n"\
                         "  <ip> %x </ip>\n"\
                         "  <port> %hu </port>\n"\
                         "  <passwd> %s </passwd>\n"\
                         "  <udp_ip> %x </udp_ip>\n"\
                         "  <c2c_udp_port_low> %hu </c2c_udp_port_low>\n"\
                         "</c2c_add>"

#define C2C_CREATE       "<c2c_create>"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <is_ack> %d </is_ack>\n"\
                         "  <mnet> %s </mnet>\n"\
                         "  <snet> %s </snet>\n"\
                         "</c2c_create>"

#define C2C_CONF         "<c2c_conf>"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <is_ack> %d </is_ack>\n"\
                         "  <mnet> %s </mnet>\n"\
                         "  <snet> %s </snet>\n"\
                         "  <mip> %x </mip>\n"\
                         "  <sip> %x </sip>\n"\
                         "  <mport> %hu </mport>\n"\
                         "  <sport> %hu </sport>\n"\
                         "</c2c_conf>"

#define C2C_PING         "<c2c_ping>"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <status> %d </status>\n"\
                         "</c2c_ping>"

#define COLOR_ITEM       "<color_item>"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <color> %d </color>\n"\
                         "</color_item>"

#define NOVNC_ON_OFF  "<novnc_on_off>"\
                         "  <tid> %d </tid>\n"\
                         "  <on> %d </on>\n"\
                         "</novnc_on_off>"


/*---------------------------------------------------------------------------*/
#define HOP_GET_LIST_NAME    "<hop_get_list_name>\n"\
                             "  <tid> %d </tid>\n"\
                             "</hop_get_list_name>"
/*---------------------------------------------------------------------------*/
#define HOP_LIST_NAME_O  "<hop_list_name>\n"\
                         "<tid> %d </tid>\n"\
                         "<nb_items> %d </nb_items>\n"

#define HOP_LIST_NAME_I  "<hop_type_item> %d </hop_type_item>\n"\
                         "<hop_name_item> %s </hop_name_item>\n"\
                         "<hop_eth_item> %d </hop_eth_item>\n"

#define HOP_LIST_NAME_C \
                          "</hop_list_name>"
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
#define HOP_EVT_DOORS_O "<hop_evt_doors>\n"\
                        "  <tid> %d </tid>\n"\
                        "  <flags_hop> %d </flags_hop>\n"\
                        "  <name> %s </name>\n"\

#define HOP_EVT_DOORS_C "</hop_evt_doors>"
/*---------------------------------------------------------------------------*/
#define HOP_EVT_DOORS_SUB_O  "<hop_evt_doors_sub>\n"\
                             "  <tid> %d </tid>\n"\
                             "  <flags_hop> %d </flags_hop>\n"\
                             "  <nb_items> %d </nb_items>\n"

#define HOP_EVT_DOORS_SUB_C \
                          "</hop_evt_doors_sub>"
/*---------------------------------------------------------------------------*/
#define HOP_EVT_DOORS_UNSUB "<hop_evt_doors_unsub>\n"\
                            "  <tid> %d </tid>\n"\
                            "</hop_evt_doors_unsub>"
/*---------------------------------------------------------------------------*/



/*---------------------------------------------------------------------------*/
#define STATUS_OK        "<ok>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <txt> %s </txt>\n"\
                         "</ok>"
#define STATUS_KO        "<ko>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <txt> %s </txt>\n"\
                         "</ko>"
/*---------------------------------------------------------------------------*/
#define FIX_DISPLAY      "<fix_display>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <display> %s </display>\n"\
                         "  <txt> %s </txt>\n"\
                         "</fix_display>"
/*---------------------------------------------------------------------------*/
#define WORK_DIR_REQ     "<work_dir_req>\n"\
                         "  <tid> %d </tid>\n"\
                         "</work_dir_req>"

#define WORK_DIR_RESP    "<work_dir_resp>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <version> %s </version>\n"\
                         "  <network_name> %s </network_name>\n"\
                         "  <username> %s </username>\n"\
                         "  <server_port> %d </server_port>\n"\
                         "  <bin_dir> %s </bin_dir>\n"\
                         "  <flags> %d </flags>\n"\
                         "</work_dir_resp>"
/*---------------------------------------------------------------------------*/
#define EVENTFULL_SUB    "<eventfull_sub>\n"\
                         "  <tid> %d </tid>\n"\
                         "</eventfull_sub>"
/*---------------------------------------------------------------------------*/
#define EVENTFULL_O      "<eventfull>\n"\
                         "  <tid> %d </tid>\n"\
                         "  nb_endp:%d \n"

#define EVENTFULL_C      "</eventfull>"
/*---------------------------------------------------------------------------*/
#define EVENTFULL_ENDP   "<eventfull_endp>\n"\
                         "name:%s num:%d type:%d \n"\
                         "ram:%d cpu:%d ok:%d \n"\
                         "ptx:%d prx:%d btx:%d brx:%d ms:%d \n"\
                         "</eventfull_endp>"
/*---------------------------------------------------------------------------*/
#define SLOWPERIODIC_SUB   "<slowperiodic_sub>\n"\
                           "  <tid> %d </tid>\n"\
                           "</slowperiodic_sub>"
/*---------------------------------------------------------------------------*/
#define SLOWPERIODIC_QCOW2_O  "<slowperiodic_qcow2>\n"\
                              "  <tid> %d </tid>\n"\
                              "  nb:%d \n"

#define SLOWPERIODIC_QCOW2_C  "</slowperiodic_qcow2>"
/*---------------------------------------------------------------------------*/
#define SLOWPERIODIC_IMG_O    "<slowperiodic_img>\n"\
                              "  <tid> %d </tid>\n"\
                              "  nb:%d \n"

#define SLOWPERIODIC_IMG_C    "</slowperiodic_img>"
/*---------------------------------------------------------------------------*/
#define SLOWPERIODIC_CVM_O    "<slowperiodic_cvm>\n"\
                              "  <tid> %d </tid>\n"\
                              "  nb:%d \n"

#define SLOWPERIODIC_CVM_C    "</slowperiodic_cvm>"
/*---------------------------------------------------------------------------*/
#define SLOWPERIODIC_SPIC  "<slowperiodic_spic>\n"\
                           "  name:%s \n"\
                           "</slowperiodic_spic>"
/*---------------------------------------------------------------------------*/
#define ADD_KVM_O        "<add_kvm>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <vm_id> %d </vm_id>\n"\
                         "  <color> %d </color>\n"\
                         "  <vm_config_flags> %d </vm_config_flags>\n"\
                         "  <vm_config_param> %d </vm_config_param>\n"\
                         "  <mem> %d </mem>\n"\
                         "  <cpu> %d </cpu>\n"\
                         "  <nb_tot_eth> %d </nb_tot_eth>"

#define VM_ETH_TABLE     "<eth_table>\n"\
                         "  <endp_type> %d </endp_type>\n"\
                         "  <randmac> %d </randmac>\n"\
                         "  <ifname> %s </ifname>\n"\
                         "  <mac> %02X %02X %02X %02X %02X %02X </mac>\n"\
                         "</eth_table>"

#define ADD_KVM_C        "  <linux_kernel> %s </linux_kernel>\n"\
                         "  <rootfs_input> %s </rootfs_input>\n"\
                         "  <install_cdrom> %s </install_cdrom>\n"\
                         "  <added_cdrom> %s </added_cdrom>\n"\
                         "  <added_disk> %s </added_disk>\n"\
                         "</add_kvm>"
/*---------------------------------------------------------------------------*/
#define ADD_CNT_O         "<add_cnt>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <brandtype> %s </brandtype>\n"\
                         "  <name> %s </name>\n"\
                         "  <is_persistent> %d </is_persistent>\n"\
                         "  <vm_id> %d </vm_id>\n"\
                         "  <color> %d </color>\n"\
                         "  <ping_ok> %d </ping_ok>\n"\
                         "  <nb_tot_eth> %d </nb_tot_eth>"

#define ADD_CNT_C        "<startup_env_keyid>%s</startup_env_keyid>\n"\
                         "<vmount_keyid>%s</vmount_keyid>\n"\
                         "<image> %s </image>\n"\
                         "</add_cnt>"
/*---------------------------------------------------------------------------*/


#define SAV_VM           "<sav_vm>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <sav_rootfs_path> %s </sav_rootfs_path>\n"\
                         "</sav_vm>"

#define DEL_SAT          "<del_name>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "</del_name>"

#define ADD_LAN_ENDP     "<add_lan_endp>\n"\
                         "  <tid> %d </tid>\n"\
                         "  name:%s num:%d lan:%s \n"\
                         "</add_lan_endp>"

#define DEL_LAN_ENDP     "<del_lan_endp>\n"\
                         "  <tid> %d </tid>\n"\
                         "  name:%s num:%d lan:%s \n"\
                         "</del_lan_endp>"

/*---------------------------------------------------------------------------*/
#define KILL_UML_CLOWNIX      "<kill_uml_clownix>\n"\
                              "  <tid> %d </tid>\n"\
                              "</kill_uml_clownix>"

#define DEL_ALL               "<del_all>\n"\
                              "  <tid> %d </tid>\n"\
                              "</del_all>"
/*---------------------------------------------------------------------------*/
#define LIST_PID              "<list_pid_req>\n"\
                              "  <tid> %d </tid>\n"\
                              "</list_pid_req>"

#define LIST_PID_O            "<list_pid_resp>\n"\
                              "  <tid> %d </tid>\n"\
                              "  <qty> %d </qty>\n"
#define LIST_PID_ITEM         "  <pid> %s %d </pid>\n"
#define LIST_PID_C \
                              "</list_pid_resp>"
/*---------------------------------------------------------------------------*/
#define LIST_COMMANDS         "<list_commands_req>\n"\
                              "  <tid> %d </tid>\n"\
                              "  <is_layout> %d </is_layout>\n"\
                              "</list_commands_req>"
/*---------------------------------------------------------------------------*/
#define LIST_COMMANDS_O       "<list_commands_resp>\n"\
                              "  <tid> %d </tid>\n"\
                              "  <qty> %d </qty>\n"
#define LIST_COMMANDS_ITEM    "<item_list_command_delimiter>%s</item_list_command_delimiter>\n"

#define LIST_COMMANDS_C       "</list_commands_resp>"
/*---------------------------------------------------------------------------*/
#define EVENT_PRINT_SUB       "<event_print_sub>\n"\
                              "  <tid> %d </tid>\n"\
                              "</event_print_sub>"

#define EVENT_PRINT_UNSUB     "<event_print_unsub>\n"\
                              "  <tid> %d </tid>\n"\
                              "</event_print_unsub>"

#define EVENT_PRINT           "<event_print>\n"\
                              "  <tid> %d </tid>\n"\
                              "  <txt> %s </txt>\n"\
                              "</event_print>"
/*---------------------------------------------------------------------------*/
#define EVENT_SYS_SUB         "<event_sys_sub>\n"\
                              "  <tid> %d </tid>\n"\
                              "</event_sys_sub>"

#define EVENT_SYS_UNSUB       "<event_sys_unsub>\n"\
                              "  <tid> %d </tid>\n"\
                              "</event_sys_unsub>"
/*---------------------------------------------------------------------------*/
#define EVENT_SYS_ITEM_Q      "<Qtx>\n"\
                              "  <peak_size> %d </peak_size>\n"\
                              "  <size> %d </size>\n"\
                              "  <llid> %d </llid>\n"\
                              "  <qfd> %d </qfd>\n"\
                              "  <waked_in> %d </waked_in>\n"\
                              "  <waked_out> %d </waked_out>\n"\
                              "  <waked_err> %d </waked_err>\n"\
                              "  <out> %d </out>\n"\
                              "  <in> %d </in>\n"\
                              "  <name> %s </name>\n"\
                              "  <id> %d </id>\n"\
                              "  <type> %d </type>\n"\
                              "</Qtx>"
/*---------------------------------------------------------------------------*/
#define EVENT_SYS_O           "<system_info>\n"\
                              "  <tid> %d </tid>\n"\
                              "  <nb_mallocs> %d </nb_mallocs>\n"

#define EVENT_SYS_M   "<m> %lu </m>\n" 
 
#define EVENT_SYS_FN  "<nb_fds_used> %d </nb_fds_used>\n"
#define EVENT_SYS_FU  "<fd> %lu </fd>\n" 

#define EVENT_SYS_R   "<r> selects: %d \n"\
                      "cur_channels: %d max_channels: %d \n"\
                      "channels_recv: %d  channels_send: %d \n"\
                      "clients: %d\n"\
                      "max_time:%d avg_time: %d above50ms: %d \n"\
                      "above20ms: %d above15ms: %d \n"\
                      "nb_Q_not_empty: %d </r>\n"

#define EVENT_SYS_C \
                      "</system_info>"
/*---------------------------------------------------------------------------*/
#define TOPO_SMALL_EVENT_SUB  "<topo_small_event_sub>\n"\
                              "  <tid> %d </tid>\n"\
                              "</topo_small_event_sub>"

#define TOPO_SMALL_EVENT_UNSUB  "<topo_small_event_unsub>\n"\
                              "  <tid> %d </tid>\n"\
                              "</topo_small_event_unsub>"

#define TOPO_SMALL_EVENT          "<topo_small_event>\n"\
                              "  <tid> %d </tid>\n"\
                              "  <name> %s </name>\n"\
                              "  <param1> %s </param1>\n"\
                              "  <param2> %s </param2>\n"\
                              "  <evt> %d </evt>\n"\
                              "</topo_small_event>"

/*---------------------------------------------------------------------------*/
#define EVENT_TOPO_SUB        "<event_topo_sub>\n"\
                              "  <tid> %d </tid>\n"\
                              "</event_topo_sub>"

#define EVENT_TOPO_UNSUB      "<event_topo_unsub>\n"\
                              "  <tid> %d </tid>\n"\
                              "</event_topo_unsub>"



#define EVENT_TOPO_O          "<event_topo>\n"\
                              "  <tid> %d </tid>\n"\
                              "  version:%s \n"\
                              "  conf_rank:%d \n"\
                              "  network:%s username:%s server_port:%d \n"\
                              "  bin_dir:%s \n"\
                              "  flags_config:%d \n"\
                              "  nb_cnt:%d nb_kvm:%d \n"\
                              "  nb_c2c:%d nb_tap:%d nb_phy:%d \n"\
                              "  nb_a2b:%d nb_nat:%d nb_endp:%d \n"\
                              "  nb_info_phy:%d nb_bridges:%d \n"

#define EVENT_TOPO_C          "</event_topo>\n"\

#define EVENT_TOPO_KVM_O      "<kvm>\n"\
                              "  name: %s \n"\
                              "  vm_id: %d \n"\
                              "  color: %d \n"\
                              "  install_cdrom: %s \n"\
                              "  added_cdrom: %s \n"\
                              "  added_disk: %s \n"\
                              "  linux_kernel: %s \n"\
                              "  rootfs_used: %s \n"\
                              "  rootfs_backing: %s \n"\
                              "  vm_config_flags: %d vm_config_param: %d \n"\
                              "  mem: %d cpu: %d nb_tot_eth: %d "

#define EVENT_TOPO_KVM_C      "</kvm>\n"


#define EVENT_TOPO_CNT_O      "<cnt>\n"\
                              "  brandtype: %s \n"\
                              "  name: %s \n"\
                              "  is_persistent: %d \n"\
                              "  vm_id: %d \n"\
                              "  color: %d \n"\
                              "  image: %s \n"\
                              "  ping_ok: %d \n"\
                              "  nb_tot_eth: %d "

#define EVENT_TOPO_CNT_C      "<startup_env_keyid>%s</startup_env_keyid>\n"\
                              "<vmount_keyid>%s</vmount_keyid>\n"\
                              "</cnt>\n"
                        
#define EVENT_TOPO_C2C        "<c2c>\n"\
                              "  name: %s \n"\
                              "  endp_type: %d \n"\
                              "  dist_cloon: %s \n"\
                              "  lan: %s \n"\
                              "  local_is_master: %d \n"\
                              "  dist_tcp_ip:    %x \n"\
                              "  dist_tcp_port:  %hu \n"\
                              "  loc_udp_ip:     %x \n"\
                              "  dist_udp_ip:    %x \n"\
                              "  loc_udp_port:   %hu \n"\
                              "  dist_udp_port:  %hu \n"\
                              "  tcp_connection: %d \n"\
                              "  udp_connection: %d \n"\
                              "</c2c>\n"

#define EVENT_TOPO_A2B        "<a2b>\n"\
                              "  name:   %s \n"\
                              "  endp_type0: %d \n"\
                              "  endp_type1: %d \n"\
                              "</a2b>\n"

#define EVENT_TOPO_NAT        "<nat>\n"\
                              "  name:   %s \n"\
                              "  endp_type: %d \n"\
                              "</nat>\n"

#define EVENT_TOPO_TAP        "<tap>\n"\
                              "  name:   %s \n"\
                              "  endp_type: %d \n"\
                              "</tap>\n"

#define EVENT_TOPO_PHY        "<phy>\n"\
                              "  name:   %s \n"\
                              "  endp_type: %d \n"\
                              "</phy>\n"

#define EVENT_TOPO_INFO_PHY   "<info_phy>\n"\
                              "  name:%s \n"\
                              "</info_phy>\n"

#define EVENT_TOPO_PCI        "<pci>\n"\
                              "  pci:%s \n"\
                              "  drv:%s \n"\
                              "  unused:%s \n"\
                              "</pci>\n"

#define EVENT_TOPO_BRIDGES_O  "<bridges>\n"\
                              "  br_name: %s \n"\
                              "  nb_ports:%d \n"

#define EVENT_TOPO_BRIDGES_I  "<port_name> %s </port_name>\n"

#define EVENT_TOPO_BRIDGES_C  "</bridges>\n"

#define EVENT_TOPO_LAN        "  <lan> %s </lan>\n"

#define EVENT_TOPO_ENDP_O     "<endp>\n"\
                              "  name:%s num:%d type:%d nb_lan:%d \n"\

#define EVENT_TOPO_ENDP_C     "</endp>\n"\
/*---------------------------------------------------------------------------*/
#define VMCMD            "<vmcmd>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <cmd> %d </cmd>\n"\
                         "  <param> %d </param>\n"\
                         "</vmcmd>"
/*---------------------------------------------------------------------------*/
#define EVT_STATS_ITEM      "<item>\n"\
                            "  ms: %d ptx: %d btx: %d prx: %d brx: %d \n"\
                            "</item>"
/*---------------------------------------------------------------------------*/
#define SUB_EVT_STATS_ENDP  "<sub_evt_stats_endp>\n"\
                            "  <tid> %d </tid>\n"\
                            "  name:%s num:%d sub_on:%d \n"\
                            "</sub_evt_stats_endp>\n"
/*---------------------------------------------------------------------------*/
#define EVT_STATS_ENDP_O    "<evt_stats_endp>\n"\
                            "  <tid> %d </tid>\n"\
                            "  network:%s name:%s num:%d status:%d \n"\
                            "  nb_items:%d \n"
/*---------------------------------------------------------------------------*/
#define EVT_STATS_ENDP_C  "</evt_stats_endp>"
/*---------------------------------------------------------------------------*/
#define SUB_EVT_STATS_SYSINFO "<sub_evt_stats_sysinfo>\n"\
                              "  <tid> %d </tid>\n"\
                              "  <name> %s </name>\n"\
                              "  <sub_on> %d </sub_on>\n"\
                              "</sub_evt_stats_sysinfo>\n"
/*---------------------------------------------------------------------------*/
#define EVT_STATS_SYSINFOO "<evt_stats_sysinfo>\n"\
                           "  <tid> %d </tid>\n"\
                           "  <network_name> %s </network_name>\n"\
                           "  <name> %s </name>\n"\
                           "  <status> %d </status>\n"\
                           "  time_ms: %d uptime: %lu \n"\
                           "  load1: %lu load5: %lu load15: %lu \n"\
                           "  totalram: %lu freeram: %lu \n"\
                           "  cachedram: %lu sharedram: %lu bufferram: %lu \n"\
                           "  totalswap: %lu freeswap: %lu procs: %lu \n"\
                           "  totalhigh: %lu freehigh: %lu mem_unit: %lu \n"\
                           "  process_utime: %lu process_stime: %lu \n"\
                           "  process_cutime: %lu process_cstime: %lu \n"\
                           "  process_rss: %lu \n"


#define EVT_STATS_SYSINFOC "</evt_stats_sysinfo>\n"
/*---------------------------------------------------------------------------*/
#define SYNC_WIRESHARK_REQ "<sync_wireshark_req>\n"\
                           "  <tid> %d </tid>\n"\
                           "  <name> %s </name>\n"\
                           "  <num> %d </num>\n"\
                           "  <cmd> %d </cmd>\n"\
                           "</sync_wireshark_req>\n"

#define SYNC_WIRESHARK_RESP "<sync_wireshark_resp>\n"\
                            "  <tid> %d </tid>\n"\
                            "  <name> %s </name>\n"\
                            "  <num> %d </num>\n"\
                            "  <status> %d </status>\n"\
                            "</sync_wireshark_resp>\n"
/*---------------------------------------------------------------------------*/








