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

/*---------------------------------------------------------------------------*/
#ifndef GLOB_COMMON_H
#define GLOB_COMMON_H
/*---------------------------------------------------------------------------*/



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <linux/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <string.h>

#include "pthexec.h"


uint64_t cloonix_get_msec(void);
uint64_t cloonix_get_usec(void);
void cloonix_set_pid(int pid);
int cloonix_get_pid(void);


#define MAX_NAME_LEN 64

#define MAX_PATH_LEN 300

#define MAX_VMOUNT 8
#define MAX_SIZE_VMOUNT (MAX_VMOUNT*MAX_PATH_LEN)

#define KERR(format, a...)                               \
 do {                                                    \
    syslog(LOG_ERR | LOG_USER, "KERR: %s"                 \
    " line:%d " format "\n",   \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    } while (0)

#define KOUT(format, a...)                               \
 do {                                                    \
    syslog(LOG_ERR | LOG_USER, "KERR KILL %s"            \
    " line:%d   " format "\n\n",  \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    exit(-1);                                            \
    } while (0)


#define CLOWNIX_MAX_CHANNELS 10000
#define MAX_SELECT_CHANNELS 2000

enum {
  endp_type_none=13,
  endp_type_eths,
  endp_type_ethv,
  endp_type_taps,
  endp_type_tapv,
  endp_type_nats,
  endp_type_natv,
  endp_type_c2cs,
  endp_type_c2cv,
  endp_type_phyas,
  endp_type_phyav,
  endp_type_phyms,
  endp_type_phymv,
  endp_type_a2b,
};

enum {
  item_type_none=113,
  item_type_keth,
  item_type_ceth,
  item_type_tap,
  item_type_phy,
  item_type_c2c,
  item_type_a2b,
  item_type_nat,
};


typedef void (*t_fd_local_flow_ctrl)(int llid, int stop);
typedef void (*t_fd_dist_flow_ctrl)(int llid, char *name, int num,
                                    int tidx, int rank, int stop);
typedef void (*t_fd_error)(int llid, int err, int from);
typedef int  (*t_fd_event)(int llid, int fd);
typedef void (*t_fd_connect)(int llid, int llid_new);


#define PROXYMARKUP_START "proxymarkup_crun_data_start:"
#define PROXYMARKUP_END ":proxymarkup_crun_data_end"

#define MAGIC_COOKIE "MIT-MAGIC-COOKIE-1"
#define MAGIC_COOKIE_LEN 16
#define X11_DISPLAY_NAME "unix:%d.0"
#define X11_DISPLAY_DIR "/tmp/.X11-unix"
#define X11_DISPLAY_PREFIX "/tmp/.X11-unix/X%d"
#define X11_DISPLAY_OFFSET 142
#define X11_DISPLAY_OFFSET_PORT 6000
#define X11_DISPLAY_IDX_MAX 100
#define X11_DISPLAY_XWY_MIN 50
#define X11_DISPLAY_XWY_MAX 100
#define X11_DISPLAY_XWY_RANK_OFFSET 1000
#define X11_DISPLAY_OFFSET_XEPHYR 243
#define X11_DISPLAY_IDX_XEPHYR_MAX 20



#define HEADER_TAP_MSG 8
#define TRAF_TAP_BUF_LEN 1510
#define TRAF_TAP_MINUS_TCP 1448
#define END_FRAME_ADDED_CHECK_LEN 4


#define MAX_DGRAM_LEN 2048
#define MAX_ICMP_RXTX_LEN 1514
#define MAX_ICMP_RXTX_SIG_LEN 4000

#define NAT_IP_CISCO  "172.17.0.1"
#define NAT_IP_GW     "172.17.0.2"
#define NAT_IP_DNS    "172.17.0.3"
#define NAT_MAC_CISCO "42:0A:0E:13:07:01"
#define NAT_MAC_GW    "42:0A:0E:13:07:02"
#define NAT_MAC_DNS   "42:0A:0E:13:07:03"


#define MAX_VM             1000
#define MAX_OVS_BRIDGES    1500
#define MAX_OVS_PORTS      MAX_VM
#define MAX_PHY            32
#define MAX_ETH_VM         32
#define MAX_COLOR         10

#define MAX_TRAF_ENDPOINT 4

#define MAX_POLAR_COORD 314
#define NODE_DIA 75
#define CNT_DIA 62
#define A2B_DIA 30 
#define VAL_INTF_POS_NODE 0.5 
#define VAL_INTF_POS_A2B 0.7 
#define MAX_HOP_PRINT_LEN 2000
#define DOUT rpct_hop_print

#define FLAG_HOP_ERRDIAG 0x0001
#define FLAG_HOP_SIGDIAG 0x0002
#define FLAG_HOP_POLDIAG 0x0004
#define FLAG_HOP_DOORS   0x0008

#define MAX_DOORWAYS_BUF_LEN 1000000
#define MAX_DOOR_CTRL_LEN 1000


#define MAX_RPC_MSG_LEN 50000
#define MAX_BULK_FILES 100

#define MAX_CLOWNIX_BOUND_LEN      64
#define MIN_CLOWNIX_BOUND_LEN      2

#define MAC_ADDR_LEN 6
#define VM_CONFIG_FLAG_PERSISTENT      0x00001
#define VM_CONFIG_FLAG_PRIVILEGED      0x00002
#define VM_CONFIG_FLAG_I386            0x00004
#define VM_CONFIG_FLAG_FULL_VIRT       0x00008
#define VM_CONFIG_FLAG_BALLOONING      0x00010
#define VM_CONFIG_FLAG_INSTALL_CDROM   0x00020
#define VM_CONFIG_FLAG_NO_REBOOT       0x00040
#define VM_CONFIG_FLAG_ADDED_CDROM     0x00080
#define VM_CONFIG_FLAG_ADDED_DISK      0x00100
#define VM_CONFIG_FLAG_NO_QEMU_GA      0x00200
#define VM_CONFIG_FLAG_NATPLUG         0x00400
#define VM_CONFIG_FLAG_WITH_PXE        0x08000

#define VM_FLAG_DERIVED_BACKING        0x10000
#define VM_FLAG_IS_INSIDE_CLOON      0x20000
#define VM_FLAG_CLOONIX_AGENT_PING_OK  0x80000

#define DIR_UMID "umid"
#define VM_WORKDIR "vm"

#define MSG_DIGEST_LEN 32

#define NO_DEFINED_VALUE "NO_DEFINED_VALUE_ITEM"
#define DTACH_SOCK "dtach"
#define DOORS_CTRL_SOCK "doors_ctrl_sock"
#define SPICE_SOCK "spice_sock"
#define CLOONIX_SWITCH "cloonix_switch"
#define XWY_X11_SOCK "xwy_x11"
#define XWY_TRAFFIC_SOCK "xwy_traf"
#define XWY_CONTROL_SOCK "xwy_ctrl"
#define SUID_POWER_SOCK_DIR "suid_power"

#define CRUN_DIR "crun"
#define NGINX_DIR "nginx"
#define RUN_DIR "run"
#define SNF_DIR "snf"
#define NAT_MAIN_DIR  "nat_m"
#define NAT_PROXY_DIR "nat_p"
#define PROXYMOUS "proxymous"
#define A2B_DIR "a2b"
#define C2C_DIR "c2c"
#define CNT_DIR "cnt"
#define MNT_DIR "mnt"
#define LOG_DIR "log"



#define NOVNC_DISPLAY 733

#define MAX_STATS_ITEMS 30

#define MAX_LAN           (0x5FFF + 2)
#define MAX_FD_QTY         5000
#define MAX_BUF_SIZE       2000
#define MAX_ITEM_LEN       200
#define MAX_TYPE_LEN       32
#define MAX_BIG_BUF        20000 
#define MAX_PRINT_LEN      10000
#define HALF_SEC_BEAT      50 
#define MAX_ARGC_ARGV      100
#define MAX_INTF_MNGT      100
#define SPY_TX "spy_tx"
#define SPY_RX "spy_rx"

#define MAX_LIST_COMMANDS_LEN     256
#define MAX_LIST_COMMANDS_QTY     5000


#define MAX_PEER_MAC 50
                                                                                
#define OVS_BRIDGE_PORT "_k_"
#define OVS_BRIDGE   "_b_"


#define PROXYSHARE_IN "/tmp/cloonix_proxymous"
#define LXSESSION "/usr/bin/cloonix_lxsession.sh"


#define BASE_NAMESPACE "cloonix"
#define PATH_NAMESPACE "/var/run/netns/"

enum{
  doors_type_min = 100,

  doors_type_listen_server,
  doors_type_server,

  doors_type_high_prio_begin,
  doors_type_switch,
  doors_type_high_prio_end,

  doors_type_low_prio_begin,
  doors_type_dbssh,
  doors_type_openssh,
  doors_type_spice,
  doors_type_dbssh_x11_ctrl,
  doors_type_dbssh_x11_traf,
  doors_type_low_prio_end,

  doors_type_xwy_main_traf,
  doors_type_xwy_x11_flow,

  doors_type_max,
};

enum{
  doors_val_min = 200,
  doors_val_c2c,
  doors_val_none,
  doors_val_xwy,
  doors_val_sig,
  doors_val_init_link,
  doors_val_link_ok,
  doors_val_link_ko,
  doors_val_max,
};
/*---------------------------------------------------------------------------*/
typedef struct t_topo_clc
{
  char version[MAX_NAME_LEN];
  char network[MAX_NAME_LEN];
  char username[MAX_NAME_LEN];
  char bin_dir[MAX_PATH_LEN];
  int  server_port;
  int  flags_config;
} t_topo_clc;
/*---------------------------------------------------------------------------*/
typedef struct t_lan_group_item
{
  char lan[MAX_NAME_LEN];
} t_lan_group_item;
/*---------------------------------------------------------------------------*/
typedef struct t_lan_group
{
  int nb_lan;
  t_lan_group_item *lan;
} t_lan_group;
/*---------------------------------------------------------------------------*/
typedef struct t_eth_table
{
  int  endp_type;
  int  randmac;
  unsigned char mac_addr[8];
  char vhost_ifname[MAX_NAME_LEN];
} t_eth_table;
/*---------------------------------------------------------------------------*/
typedef struct t_topo_kvm
{
  char name[MAX_NAME_LEN];
  int  vm_config_flags;
  int  vm_config_param;
  int  cpu;
  int  mem;
  int  nb_tot_eth;
  t_eth_table eth_table[MAX_ETH_VM];
  int  vm_id;
  int  color;
  char linux_kernel[MAX_NAME_LEN];
  char rootfs_input[MAX_PATH_LEN];
  char rootfs_used[MAX_PATH_LEN];
  char rootfs_backing[MAX_PATH_LEN];
  char install_cdrom[MAX_PATH_LEN];
  char added_cdrom[MAX_PATH_LEN];
  char added_disk[MAX_PATH_LEN];
} t_topo_kvm;

/*---------------------------------------------------------------------------*/
typedef struct t_topo_cnt
{
  char brandtype[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int  is_persistent;
  int  is_privileged;
  int  ping_ok;
  int  vm_id;
  int  color;
  int  nb_tot_eth;
  t_eth_table eth_table[MAX_ETH_VM];
  char image[MAX_PATH_LEN];
  char startup_env[MAX_PATH_LEN];
  char vmount[MAX_SIZE_VMOUNT];
} t_topo_cnt;
/*---------------------------------------------------------------------------*/
typedef struct t_topo_c2c
  {
  char name[MAX_NAME_LEN];
  int endp_type;
  char dist_cloon[MAX_NAME_LEN];
  char attlan[MAX_NAME_LEN];
  int local_is_master;
  uint32_t dist_tcp_ip;
  uint16_t dist_tcp_port;
  uint32_t loc_udp_ip;
  uint32_t dist_udp_ip;
  uint16_t loc_udp_port;
  uint16_t dist_udp_port;
  int tcp_connection_peered;
  int udp_connection_peered;
  } t_topo_c2c;
/*---------------------------------------------------------------------------*/
typedef struct t_topo_tap
  {
  char name[MAX_NAME_LEN];
  int endp_type;
  } t_topo_tap;
/*---------------------------------------------------------------------------*/
typedef struct t_topo_phy
  {
  char name[MAX_NAME_LEN];
  int endp_type;
  } t_topo_phy;
/*---------------------------------------------------------------------------*/
typedef struct t_topo_a2b
  {
  char name[MAX_NAME_LEN];
  int endp_type0;
  int endp_type1;
  } t_topo_a2b;
/*---------------------------------------------------------------------------*/
typedef struct t_topo_nat
  {
  char name[MAX_NAME_LEN];
  int endp_type;
  } t_topo_nat;
/*---------------------------------------------------------------------------*/
typedef struct t_topo_endp
{
  char name[MAX_NAME_LEN];
  int  num;
  int  type;
  t_lan_group lan;
} t_topo_endp;
/*---------------------------------------------------------------------------*/
typedef struct t_topo_info_phy
{
  char name[MAX_NAME_LEN];
  char phy_mac[MAX_NAME_LEN];
} t_topo_info_phy;
/*---------------------------------------------------------------------------*/
typedef struct t_topo_bridges
{
  char br[MAX_NAME_LEN];
  int nb_ports;
  char ports[MAX_OVS_PORTS][MAX_NAME_LEN];
} t_topo_bridges;
/*---------------------------------------------------------------------------*/
typedef struct t_topo_info
{
  t_topo_clc clc;

  int conf_rank;
  
  int nb_cnt;
  t_topo_cnt *cnt;

  int nb_kvm;
  t_topo_kvm *kvm;

  int nb_c2c;
  t_topo_c2c *c2c;

  int nb_nat;
  t_topo_nat *nat;

  int nb_tap;
  t_topo_tap *tap;

  int nb_a2b;
  t_topo_a2b *a2b;

  int nb_phy;
  t_topo_phy *phy;  

  int nb_endp;
  t_topo_endp *endp;

  int nb_bridges;
  t_topo_bridges *bridges;

  int nb_info_phy;
  t_topo_info_phy *info_phy;  

} t_topo_info;
/*---------------------------------------------------------------------------*/




enum
  {
  bnd_rpct_min = 0,
  bnd_rpct_sigdiag_msg,
  bnd_rpct_poldiag_msg,
  bnd_rpct_cli_req,
  bnd_rpct_cli_resp,
  bnd_rpct_kil_req,
  bnd_rpct_pid_req,
  bnd_rpct_pid_resp,
  bnd_rpct_hop_evt_sub,
  bnd_rpct_hop_evt_unsub,
  bnd_rpct_hop_evt_msg,
  bnd_rpct_max,
  };

/*---------------------------------------------------------------------------*/
#define MUSIGDIAG_MSG_O  "<sigdiag_msg>\n"\
                         "  <tid> %d </tid>\n"

#define MUSIGDIAG_MSG_I "<sigdiag_msg_delimiter>%s</sigdiag_msg_delimiter>\n"

#define MUSIGDIAG_MSG_C  "</sigdiag_msg>"
/*---------------------------------------------------------------------------*/
#define MUPOLDIAG_MSG_O  "<poldiag_msg>\n"\
                         "  <tid> %d </tid>\n"

#define MUPOLDIAG_MSG_I "<poldiag_msg_delimiter>%s</poldiag_msg_delimiter>\n"

#define MUPOLDIAG_MSG_C  "</poldiag_msg>"
/*---------------------------------------------------------------------------*/
#define HOP_PID_REQ   "<hop_req_pid>\n"\
                       "  <tid> %d </tid>\n"\
                       "  name:%s num:%d \n"\
                       "</hop_req_pid>"
/*---------------------------------------------------------------------------*/
#define HOP_PID_RESP  "<hop_resp_pid>\n"\
                       "  <tid> %d </tid>\n"\
                       "  name:%s num:%d toppid:%d pid:%d \n"\
                       "</hop_resp_pid>"
/*---------------------------------------------------------------------------*/
#define HOP_KIL_REQ   "<hop_req_kil>\n"\
                       "  <tid> %d </tid>\n"\
                       "</hop_req_kil>"
/*---------------------------------------------------------------------------*/
#define HOP_EVT_O "<hop_event_txt>\n"\
                  "  <tid> %d </tid>\n"\
                  "  <flags_hop> %d </flags_hop>\n"

#define HOP_EVT_C "</hop_event_txt>"
/*---------------------------------------------------------------------------*/
#define HOP_FREE_TXT  "  <hop_free_txt_joker>%s</hop_free_txt_joker>\n"
/*---------------------------------------------------------------------------*/
#define HOP_EVT_SUB "<hop_evt_sub>\n"\
                    "  <tid> %d </tid>\n"\
                    "  <flags_hop> %d </flags_hop>\n"\
                    "</hop_evt_sub>"
/*---------------------------------------------------------------------------*/
#define HOP_EVT_UNSUB "<hop_evt_unsub>\n"\
                      "  <tid> %d </tid>\n"\
                      "</hop_evt_unsub>"

/*---------------------------------------------------------------------------*/
void rpct_send_sigdiag_msg(int llid, int tid, char *line);
void rpct_send_poldiag_msg(int llid, int tid, char *line);
void rpct_recv_sigdiag_msg(int llid, int tid, char *line);
void rpct_recv_poldiag_msg(int llid, int tid, char *line);
/*---------------------------------------------------------------------------*/
void rpct_send_pid_req(int llid, int tid, char *name, int num);
void rpct_recv_pid_req(int llid, int tid, char *name, int num);
/*---------------------------------------------------------------------------*/
void rpct_send_kil_req(int llid, int tid);
void rpct_recv_kil_req(int llid, int tid);
/*---------------------------------------------------------------------------*/
void rpct_send_pid_resp(int llid, int tid,
                        char *name, int num, int toppid, int pid);
void rpct_recv_pid_resp(int llid, int tid,
                        char *name, int num, int toppid, int pid);
/*---------------------------------------------------------------------------*/
void rpct_send_hop_sub(int llid, int tid, int flags_hop);
void rpct_recv_hop_sub(int llid, int tid, int flags_hop);
/*---------------------------------------------------------------------------*/
void rpct_send_hop_unsub(int llid, int tid);
void rpct_recv_hop_unsub(int llid, int tid);
/*---------------------------------------------------------------------------*/
void rpct_send_hop_msg(int llid, int tid,
                      int flags_hop, char *txt);
void rpct_recv_hop_msg(int llid, int tid,
                      int flags_hop, char *txt);
/*---------------------------------------------------------------------------*/
void rpct_hop_print_add_sub(int llid, int tid, int flags_hop);
void rpct_hop_print_del_sub(int llid);
void rpct_hop_print(int flags_hop, const char * format, ...);
/*---------------------------------------------------------------------------*/
typedef void (*t_rpct_tx)(int llid, int len, char *buf);
int  rpct_decoder(int llid, int len, char *str_rx);
void rpct_redirect_string_tx(t_rpct_tx rpc_tx);
void rpct_init(t_rpct_tx rpc_tx);
/*---------------------------------------------------------------------------*/
#define DEBUG_LOG_DIALOG  "debug_dialog.log"
#define DEBUG_LOG_OVS_CMD "debug_ovs_cmd.log"
#define DEBUG_LOG_VSWITCH "debug_vswitch.log"
#define DEBUG_LOG_SUID    "debug_suid_power.log"
#define DEBUG_LOG_CRUN    "debug_crun.log"
#define DEBUG_LOG_JSON    "debug_crun_json"
/*---------------------------------------------------------------------------*/
#endif
/*---------------------------------------------------------------------------*/
