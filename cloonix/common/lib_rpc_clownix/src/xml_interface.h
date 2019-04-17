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
#define MAX_CLOWNIX_BOUND_LEN      64
#define MIN_CLOWNIX_BOUND_LEN      2


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
#define MUCLI_DIALOG_REQ_O "<mucli_req_dialog>\n"\
                           "  <tid> %d </tid>\n"\
                           "  <name> %s </name>\n"\
                           "  <eth> %d </eth>\n"

#define MUCLI_DIALOG_REQ_I "  <mucli_dialog_req_bound>%s</mucli_dialog_req_bound>\n"

#define MUCLI_DIALOG_REQ_C "</mucli_req_dialog>"
/*---------------------------------------------------------------------------*/
#define MUCLI_DIALOG_RESP_O "<mucli_resp_dialog>\n"\
                            "  <tid> %d </tid>\n"\
                            "  <name> %s </name>\n"\
                            "  <eth> %d </eth>\n"\
                            "  <status> %d </status>\n"

#define MUCLI_DIALOG_RESP_I "  <mucli_dialog_resp_bound>%s</mucli_dialog_resp_bound>\n"

#define MUCLI_DIALOG_RESP_C "</mucli_resp_dialog>"
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
                         "  <work_dir> %s </work_dir>\n"\
                         "  <bulk_dir> %s </bulk_dir>\n"\
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
#define ADD_VM_O         "<add_vm>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <vm_config_flags> %d </vm_config_flags>\n"\
                         "  <cpu> %d </cpu>\n"\
                         "  <mem> %d </mem>\n"\
                         "  <nb_dpdk> %d </nb_dpdk>\n"\
                         "  <nb_eth> %d </nb_eth>\n"\
                         "  <nb_wlan> %d </nb_wlan>"

#define ADD_VM_ETH_PARAMS "<eth_params>\n"\
                          "  <mac> %02X %02X %02X %02X %02X %02X </mac>\n"\
                          "</eth_params>"

#define ADD_VM_C         "  <linux_kernel> %s </linux_kernel>\n"\
                         "  <rootfs_input> %s </rootfs_input>\n"\
                         "  <install_cdrom> %s </install_cdrom>\n"\
                         "  <added_cdrom> %s </added_cdrom>\n"\
                         "  <added_disk> %s </added_disk>\n"\
                         "  <p9_host_share> %s </p9_host_share>\n"\
                         "</add_vm>"

#define SAV_VM           "<sav_vm>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "  <save_type> %d </save_type>\n"\
                         "  <sav_rootfs_path> %s </sav_rootfs_path>\n"\
                         "</sav_vm>"

#define SAV_VM_ALL       "<sav_vm_all>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <save_type> %d </save_type>\n"\
                         "  <sav_rootfs_path> %s </sav_rootfs_path>\n"\
                         "</sav_vm_all>"


#define ADD_SAT_C2C      "<sat_c2c>\n"\
                         "  <tid> %d </tid>\n"\
                         "  name:%s \n"\
                         "  c2c_slave:%s ip_slave:%d \n"\
                         "  port_slave:%d passwd_slave:%s \n"\
                         "</sat_c2c>\n"

#define ADD_SAT_NON_C2C "<sat_non_c2c>\n"\
                         "  <tid> %d </tid>\n"\
                        "  name:%s type:%d \n"\
                        "</sat_non_c2c>\n"

#define DEL_SAT          "<del_sat>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <name> %s </name>\n"\
                         "</del_sat>"

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
#define BLKD_REPORTS_SUB "<blkd_reports_sub>\n"\
                         "  <tid> %d </tid>\n"\
                         "  <sub> %d </sub>\n"\
                         "</blkd_reports_sub>"

#define BLKD_REPORTS_O "<blkd_reports>\n"\
                       "  <tid> %d </tid>\n"\
                       "  <nb_reports> %d </nb_reports>\n"


#define BLKD_REPORTS_C "</blkd_reports>"
/*---------------------------------------------------------------------------*/
#define TOPO_SMALL_EVENT_SUB      "<topo_small_event_sub>\n"\
                              "  <tid> %d </tid>\n"\
                              "</topo_small_event_sub>"

#define TOPO_SMALL_EVENT_UNSUB    "<topo_small_event_unsub>\n"\
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
                              "  network:%s username:%s server_port:%d \n"\
                              "  work_dir:%s bulk_dir:%s bin_dir:%s \n"\
                              "  flags_config:%d \n"\
                              "  nb_kvm:%d nb_c2c:%d nb_snf:%d \n"\
                              "  nb_sat:%d nb_endp:%d \n"

#define EVENT_TOPO_C          "</event_topo>\n"\

#define EVENT_TOPO_KVM_O      "<kvm>\n"\
                              "  name: %s \n"\
                              "  install_cdrom: %s \n"\
                              "  added_cdrom: %s \n"\
                              "  added_disk: %s \n"\
                              "  p9_host_share: %s \n"\
                              "  linux_kernel: %s \n"\
                              "  rootfs_used: %s \n"\
                              "  rootfs_backing: %s \n"\
                              "  vm_id: %d vm_config_flags: %d \n"\
                              "  nb_dpdk: %d nb_eth: %d nb_wlan: %d \n"\
                              "  mem: %d cpu: %d \n"

#define EVENT_TOPO_KVM_C      "</kvm>\n"
                        
#define EVENT_TOPO_C2C        "<c2c>\n"\
                              "  name:%s \n"\
                              "  master:%s slave:%s local_is_master:%d \n"\
                              "  peered:%d ip_slave:%x port_slave%d \n"\
                              "</c2c>\n"

#define EVENT_TOPO_SNF        "<snf>\n"\
                              "  name:%s \n"\
                              "  recpath:%s capture_on:%d \n"\
                              "</snf>\n"

#define EVENT_TOPO_SAT        "<sat>\n"\
                              "  name:%s \n"\
                              "  type:%d \n"\
                              "</sat>\n"

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
#define QMP_ALL_SUB "<qmpy_all_sub>\n"\
                    "  <tid>  %d </tid>\n"\
                    "</qmpy_all_sub>"
/*---------------------------------------------------------------------------*/
#define QMP_SUB   "<qmpy_sub>\n"\
                  "  <tid>  %d </tid>\n"\
                  "  <name> %s </name>\n"\
                  "</qmpy_sub>"
/*---------------------------------------------------------------------------*/
#define QMP_REQ_O "<qmpy_req>\n"\
                  "  <tid>  %d </tid>\n"\
                  "  <name> %s </name>\n"

#define QMP_REQ_C "</qmpy_req>"
/*---------------------------------------------------------------------------*/
#define QMP_MSG_O "<qmpy_msg>\n"\
                  "  <tid>  %d </tid>\n"\
                  "  <name> %s </name>\n"\
                  "  <status>  %d </status>\n"

#define QMP_MSG_C "</qmpy_msg>"
/*---------------------------------------------------------------------------*/
#define QMP_LINE  "<msgqmp_req_boundyzzy>%s</msgqmp_req_boundyzzy>\n"\
/*---------------------------------------------------------------------------*/








