/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */

#ifndef GLOB_COMMON_H
#define GLOB_COMMON_H

#include <stdio.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <poll.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#ifdef __FreeBSD__
#include <net/if.h>
#else
#include <linux/if.h>
#endif
#include <sys/socket.h>
#include <syslog.h>
#include <string.h>

#include "pthexec.h"
#include "kerr_gate.h"

uint64_t cloonix_get_msec(void);
uint64_t cloonix_get_usec(void);
uint64_t cloonix_get_epoch_usec(void);
void cloonix_set_pid(int pid);
int cloonix_get_pid(void);

#define CLOONIX_SHARE_DIR "/usr/share/cloonix"

#define MAX_NAME_LEN 64

#define MAX_PATH_LEN 300
#define MAX_NAT_PUBLISHED 16
#include "agent_wire.h"
#define MAX_DOORWAYS_BUF_LEN 50000

#define MAX_VMOUNT 8
#define MAX_SIZE_VMOUNT (MAX_VMOUNT*MAX_PATH_LEN)

char *get_cloonix_name(void) __attribute__((weak));

int name_is_sane(char *name);
int path_is_sane(char *path);

int cloonix_caller_uid(void);
int cloonix_caller_gid(void);
void cloonix_drop_root(void);

unsigned long cloonix_now_ms(void);

#define CLOONIX_NET_NAME \
  ((get_cloonix_name && get_cloonix_name() && get_cloonix_name()[0]) \
   ? get_cloonix_name() : "-")

/* syslog(3) blocks while the log daemon does not read /dev/log, and these */
/* are event loops: the line goes on a non-blocking socket, a full socket */
/* drops it, and the first line that passes says how many were lost. KOUT */
/* waits a second for its last word. Weak: one instance per process. */
void cloonix_log(int wait_ms, const char *fmt, ...)
     __attribute__((weak, format(printf, 2, 3)));
/* Defined by a guest agent only: the line, as written locally, also sent
   to the host for the syslog of the net. */
void cloonix_log_forward(const char *line) __attribute__((weak));
#ifdef __FreeBSD__
void cloonix_log(int wait_ms, const char *fmt, ...)
{
  char line[2048];
  va_list ap;
  (void) wait_ms;
  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  syslog(LOG_ERR | LOG_USER, "%s", line);
  if (cloonix_log_forward)
    cloonix_log_forward(line);
}
#else
void cloonix_log(int wait_ms, const char *fmt, ...)
{
  static int fd = -1;
  static dev_t dev;
  static ino_t ino;
  static unsigned int dropped;
  static long long full_since_ms;
  extern char *program_invocation_short_name;
  struct sockaddr_un addr;
  struct pollfd pfd;
  struct stat st;
  struct timespec ts;
  struct tm tm;
  time_t now = time(NULL);
  char stamp[32], buf[2048], lost[256];
  long long now_ms;
  int len, hlen, i;
  va_list ap;
  localtime_r(&now, &tm);
  strftime(stamp, sizeof(stamp), "%b %e %H:%M:%S", &tm);
  hlen = snprintf(buf, sizeof(buf), "<%d>%s %s[%d]: ", LOG_ERR | LOG_USER,
                  stamp, program_invocation_short_name, (int) getpid());
  va_start(ap, fmt);
  len = hlen + vsnprintf(buf + hlen, sizeof(buf) - hlen, fmt, ap);
  va_end(ap);
  if (len > (int) sizeof(buf) - 1)
    len = sizeof(buf) - 1;
  if (cloonix_log_forward)
    cloonix_log_forward(buf + hlen);
  for (i = 0; i < 2; i++)
    {
    /* A child that closed every fd may have another file on this number. */
    if ((fd >= 0) &&
        ((fstat(fd, &st)) || (st.st_dev != dev) || (st.st_ino != ino)))
      fd = -1;
    if (fd < 0)
      {
      fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
      if (fd < 0)
        return;
      memset(&addr, 0, sizeof(addr));
      addr.sun_family = AF_UNIX;
      strncpy(addr.sun_path, "/dev/log", sizeof(addr.sun_path) - 1);
      if ((connect(fd, (struct sockaddr *) &addr, sizeof(addr))) ||
          (fstat(fd, &st)))
        {
        close(fd);
        fd = -1;
        return;
        }
      dev = st.st_dev;
      ino = st.st_ino;
      }
    if (wait_ms)
      {
      pfd.fd = fd;
      pfd.events = POLLOUT;
      poll(&pfd, 1, wait_ms);
      }
    clock_gettime(CLOCK_MONOTONIC, &ts);
    now_ms = (((long long) ts.tv_sec) * 1000) + (ts.tv_nsec / 1000000);
    if (send(fd, buf, len, MSG_NOSIGNAL | MSG_DONTWAIT) == len)
      {
      if (dropped)
        {
        len = snprintf(lost, sizeof(lost), "<%d>%s %s[%d]: KERR: WARNING %u "
                       "line(s) before this one were dropped: the log daemon "
                       "did not read /dev/log for %lld ms\n",
                       LOG_ERR | LOG_USER, stamp,
                       program_invocation_short_name, (int) getpid(),
                       dropped, now_ms - full_since_ms);
        if (send(fd, lost, len, MSG_NOSIGNAL | MSG_DONTWAIT) == len)
          dropped = 0;
        }
      return;
      }
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == ENOBUFS))
      {
      if (!dropped)
        full_since_ms = now_ms;
      dropped += 1;
      return;
      }
    /* The daemon restarted: once more on a new socket. */
    close(fd);
    fd = -1;
    }
}
#endif

#define KERR(format, a...)                               \
 do {                                                    \
    static t_kerr_gate _kerr_g;                          \
    char _kerr_n[64];                                    \
    if (kerr_gate(&_kerr_g, _kerr_n, sizeof(_kerr_n)))   \
      cloonix_log(0, "KERR: %s %s:%s"                    \
      " line:%d " format "%s\n",                         \
      CLOONIX_NET_NAME,                                  \
      (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
       __func__, __LINE__, ## a, _kerr_n);                         \
    } while (0)

/* A block of lines told as one: its head passes the gate of its site or */
/* not, in passed; the lines after it, KERR_MORE, follow that verdict. */
#define KERR_HEAD(passed, format, a...)                  \
 do {                                                    \
    static t_kerr_gate _kerr_g;                          \
    char _kerr_n[64];                                    \
    passed = kerr_gate(&_kerr_g, _kerr_n, sizeof(_kerr_n)); \
    if (passed)                                          \
      cloonix_log(0, "KERR: %s %s:%s"                    \
      " line:%d " format "%s\n",                         \
      CLOONIX_NET_NAME,                                  \
      (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
       __func__, __LINE__, ## a, _kerr_n);                         \
    } while (0)

#define KERR_MORE(passed, format, a...)                  \
 do {                                                    \
    if (passed)                                          \
      cloonix_log(0, "KERR: %s %s:%s"                    \
      " line:%d " format "\n",                           \
      CLOONIX_NET_NAME,                                  \
      (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
       __func__, __LINE__, ## a);                        \
    } while (0)

#define KOUT(format, a...)                               \
 do {                                                    \
    cloonix_log(1000, "KERR KILL %s %s:%s"               \
    " line:%d " format "\n",                             \
    CLOONIX_NET_NAME,                                    \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __func__, __LINE__, ## a);                                    \
    exit(-1);                                            \
    } while (0)

/* "KERR: <net> <file>:<function> line:<n> <text>". KERR fault, repeats gated;
   KOUT fatal, exits; KTRC trace if CLOONIX_TRACE; KTRC_ON trace behind a flag. */
#define CLOONIX_TRACE 0

#if CLOONIX_TRACE
#define KTRC(format, a...) KERR("TRACE " format, ## a)
#else
#define KTRC(format, a...) do { } while (0)
#endif

#define KTRC_ON(flag, format, a...)                      \
 do {                                                    \
    if (flag)                                            \
      KERR("TRACE " format, ## a);                       \
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

#define HEADER_TAP_MSG 8
#define TRAF_TAP_BUF_LEN 1510
#define TRAF_TAP_MINUS_TCP 1448
#define END_FRAME_ADDED_CHECK_LEN 4

#define MAX_DGRAM_LEN 2048

#define NAT_MAC_CISCO "42:0A:0E:13:07:01"
#define NAT_MAC_GW    "42:0A:0E:13:07:02"
#define NAT_MAC_DNS   "42:0A:0E:13:07:03"

#define MAX_VM             1000
#define MAX_OVS_BRIDGES    1500
#define MAX_OVS_PORTS      MAX_VM
#define MAX_PHY            32
#define MAX_ETH_VM         32

#define MAX_BULK_FILES 100

#define MAX_CLOWNIX_BOUND_LEN      64
#define MIN_CLOWNIX_BOUND_LEN      2

#define VM_CONFIG_FLAG_PERSISTENT      0x00001
#define VM_CONFIG_FLAG_I386            0x00004
#define VM_CONFIG_FLAG_FULL_VIRT       0x00008
#define VM_CONFIG_FLAG_INSTALL_CDROM   0x00020
#define VM_CONFIG_FLAG_NO_REBOOT       0x00040
#define VM_CONFIG_FLAG_ADDED_CDROM     0x00080
#define VM_CONFIG_FLAG_ADDED_DISK      0x00100
#define VM_CONFIG_FLAG_NO_QEMU_GA      0x00200
#define VM_CONFIG_FLAG_WINDOWS         0x00400
/* Boot through OVMF instead of the default seabios. */
#define VM_CONFIG_FLAG_UEFI            0x00800
/* The guest is freebsd: the disc is /dev/cd0 and cd9660, not /dev/sr0 and
   iso9660. */
#define VM_CONFIG_FLAG_FREEBSD         0x01000

#define VM_FLAG_DERIVED_BACKING        0x10000
#define VM_FLAG_IS_INSIDE_CLOON      0x20000
#define VM_FLAG_CLOONIX_AGENT_PING_OK  0x80000

#define DIR_UMID "umid"
#define VM_WORKDIR "vm"

#define CONSOLE_CRUN_REQ "console_crun name=%63s"
#define CONSOLE_CRUN_OK  "console_crun_ok name=%s sock=%s"
#define CONSOLE_CRUN_KO  "console_crun_ko name=%s"

#define MSG_DIGEST_LEN 32

#define DTACH_SOCK "dtach"
#define DOORS_CTRL_SOCK "doors_ctrl_sock"
#define VNC_SOCK "vnc_sock"
#define CLOONIX_SWITCH "cloonix_switch"
#define CONSOLE_SOCK "console_sock"

#define CLOONIX_TOY_INIT "cloonix-init-"
#define SUID_POWER_SOCK_DIR "suid_power"
#define GUI_SERVER_SOCK_DIR  "gui_server"

#define CRUN_DIR "crun"
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

#define MAX_LAN           (0x5FFF + 2)
#define MAX_BIG_BUF        20000 
#define MAX_PRINT_LEN      10000
#define HALF_SEC_BEAT      50 

#define MAX_LIST_COMMANDS_LEN     256
/* a note on an item of the canvas, its end zero included */
#define MAX_NOTE_LEN              101
#define MAX_LIST_COMMANDS_QTY     5000

#define OVS_BRIDGE_PORT "_k_"
#define OVS_PEER_PORT   "itp_"
#define OVS_BRIDGE   "_b_"

#define PROXYSHARE_IN "/tmp/cloonix_proxymous"

#define BASE_NAMESPACE "cloonix"
/* the default airs of the cnt and of the kvm radios, names no item takes */
#define CNTWIFI_LAN "cntwifi"
#define KVMWIFI_LAN "kvmwifi"
/* radios of one cnt or kvm */
#define MAX_VWIF 4
/* the wire num of wlan<i> of a guest; eth<i> is i */
#define WLAN_NUM_BASE 256
#define PATH_NAMESPACE "/var/run/netns/"

#include "glob_wire.h"

typedef struct t_topo_clc
{
  char version[MAX_NAME_LEN];
  char network[MAX_NAME_LEN];
  char username[MAX_NAME_LEN];
  char bin_dir[MAX_PATH_LEN];
  int  server_port;
  int  web_port;
} t_topo_clc;
typedef struct t_lan_group_item
{
  char lan[MAX_NAME_LEN];
} t_lan_group_item;
typedef struct t_lan_group
{
  int nb_lan;
  t_lan_group_item *lan;
} t_lan_group;
typedef struct t_eth_table
{
  int  endp_type;
  int  randmac;
  unsigned char mac_addr[8];
  char vhost_ifname[MAX_NAME_LEN];
} t_eth_table;
typedef struct t_topo_kvm
{
  char name[MAX_NAME_LEN];
  int  vm_config_flags;
  int  cpu;
  int  mem;
  int  nb_tot_nb_vwif;
  int  nb_tot_eth;
  t_eth_table eth_table[MAX_ETH_VM];
  char vwif_air[MAX_VWIF][MAX_NAME_LEN];
  int  vwif_endp_type[MAX_VWIF];   /* eths when its air is captured, ethv not */
  int  vm_id;
  int  color;
  char rootfs_input[MAX_PATH_LEN];
  char rootfs_used[MAX_PATH_LEN];
  char rootfs_backing[MAX_PATH_LEN];
  char install_cdrom[MAX_PATH_LEN];
  char added_cdrom[MAX_PATH_LEN];
  char added_disk[MAX_PATH_LEN];
  int  ping_ok;
} t_topo_kvm;

typedef struct t_topo_cnt
{
  char brandtype[MAX_NAME_LEN];
  char name[MAX_NAME_LEN];
  int  is_persistent;
  int  is_privileged;
  int  ping_ok;
  int  vm_id;
  int  color;
  int  nb_tot_nb_vwif;
  int  nb_tot_eth;
  t_eth_table eth_table[MAX_ETH_VM];
  char vwif_air[MAX_VWIF][MAX_NAME_LEN];
  int  vwif_endp_type[MAX_VWIF];   /* eths when its air is captured, ethv not */
  char image[MAX_PATH_LEN];
  char startup_env[MAX_PATH_LEN];
  char vmount[MAX_SIZE_VMOUNT];
} t_topo_cnt;
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
typedef struct t_topo_tap
  {
  char name[MAX_NAME_LEN];
  int endp_type;
  } t_topo_tap;
typedef struct t_topo_phy
  {
  char name[MAX_NAME_LEN];
  int endp_type;
  } t_topo_phy;
typedef struct t_topo_a2b
  {
  char name[MAX_NAME_LEN];
  int endp_type0;
  int endp_type1;
  } t_topo_a2b;
typedef struct t_topo_nat
  {
  char name[MAX_NAME_LEN];
  int endp_type;
  } t_topo_nat;
typedef struct t_topo_endp
{
  char name[MAX_NAME_LEN];
  int  num;
  int  type;
  t_lan_group lan;
} t_topo_endp;
typedef struct t_topo_info_phy
{
  char name[MAX_NAME_LEN];
  char phy_mac[MAX_NAME_LEN];
} t_topo_info_phy;
typedef struct t_topo_bridges
{
  char br[MAX_NAME_LEN];
  int nb_ports;
  char ports[MAX_OVS_PORTS][MAX_NAME_LEN];
} t_topo_bridges;
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

typedef struct t_rx_pktbuf
{
  char rawbuf[MAX_A2D_LEN];
  int  offset;
  int  paylen;
  int  dido_llid;
  int  vwifi_base;
  int  vwifi_cid;
  int  type;
  int  val;
  char *payload;
} t_rx_pktbuf;

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
  bnd_rpct_max,
  };

void rpct_send_sigdiag_msg(int llid, int tid, char *line);
void rpct_send_poldiag_msg(int llid, int tid, char *line);
void rpct_recv_sigdiag_msg(int llid, int tid, char *line);
void rpct_recv_poldiag_msg(int llid, int tid, char *line);
void rpct_send_pid_req(int llid, int tid, char *name, int num);
void rpct_recv_pid_req(int llid, int tid, char *name, int num);
void rpct_send_kil_req(int llid, int tid);
void rpct_recv_kil_req(int llid, int tid);
void rpct_send_pid_resp(int llid, int tid,
                        char *name, int num, int toppid, int pid);
void rpct_recv_pid_resp(int llid, int tid,
                        char *name, int num, int toppid, int pid);
typedef void (*t_rpct_tx)(int llid, int len, char *buf);
int  rpct_decoder(int llid, int len, char *str_rx);
void rpct_redirect_string_tx(t_rpct_tx rpc_tx);
t_rpct_tx rpct_get_string_tx(void);
void rpct_init(t_rpct_tx rpc_tx);
#define DEBUG_LOG_DIALOG  "debug_dialog.log"
#define DEBUG_LOG_OVS_CMD "debug_ovs_cmd.log"
/* The nat gateway passt is started with, and asked for by the shuttle. */
#define PASST_GW "10.117.0.1"
#define DEBUG_LOG_VSWITCH "debug_vswitch.log"
#define DEBUG_LOG_SUID    "debug_suid_power.log"
#define DEBUG_LOG_MAIN    "debug_main.log"
#define DEBUG_LOG_CRUN    "debug_crun.log"
#define DEBUG_LOG_JSON    "debug_crun_json"
#endif
