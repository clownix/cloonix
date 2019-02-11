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
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>
#include <syslog.h>
#include <string.h>

long long cloonix_get_msec(void);
long long cloonix_get_usec(void);
void cloonix_set_pid(int pid);
int cloonix_get_pid(void);

#ifndef KERR
#define KERR(format, a...)                               \
 do {                                                    \
    syslog(LOG_ERR | LOG_USER, "%s"                      \
    " line:%d " format "\n",                             \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    printf("%s line:%d " format "\n",                    \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    } while (0)
#endif

#ifndef KOUT
#define KOUT(format, a...)                               \
 do {                                                    \
    syslog(LOG_ERR | LOG_USER, "KILL %s"                 \
    " line:%d   " format "\n\n",                         \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    printf("KILL %s line:%d " format "\n",               \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__), \
     __LINE__, ## a);                                    \
    exit(-1);                                            \
    } while (0)
#endif

#define MAX_NAME_LEN 64
#define MAX_PATH_LEN 256
#define CLOWNIX_MAX_CHANNELS 10000
#define MAX_SELECT_CHANNELS 500

#define HEADER_BLKD_SIZE (4 + 8 + sizeof(long long))
#define PAYLOAD_BLKD_SIZE 2400
#define MAX_TOTAL_BLKD_SIZE (HEADER_BLKD_SIZE+PAYLOAD_BLKD_SIZE)
#define GROUP_BLKD_MAX_SIZE (40 * MAX_TOTAL_BLKD_SIZE)
#define MAX_QEMU_BLKD_IN_GROUP 10
#define MAX_TX_BLKD_QUEUED_BYTES (10 * GROUP_BLKD_MAX_SIZE)
#define MAX_GLOB_BLKD_QUEUED_BYTES (100 * GROUP_BLKD_MAX_SIZE)



#define ETH_ALEN 6

struct ieee80211_hdr {
        unsigned char frame_control[2];
        unsigned char duration_id[2];
        unsigned char addr1[ETH_ALEN];
        unsigned char addr2[ETH_ALEN];
        unsigned char addr3[ETH_ALEN];
        unsigned char seq_ctrl[2];
        unsigned char addr4[ETH_ALEN];
};


enum {
  wlan_header_type_min = 7,
  wlan_header_type_guest_host,
  wlan_header_type_peer_sig_addr,
  wlan_header_type_peer_data,
  wlan_header_type_max,
};

typedef struct t_header_wlan {
  uint8_t wlan_header_type;
  uint8_t fill_align[3];
  uint8_t dst[ETH_ALEN];
  uint8_t src[ETH_ALEN];
  int unused_align;
  int data_len;
  char data[0];
} t_header_wlan;

#define WLAN_HEADER_FORMAT "id %s %d srv %d %d cli %d %d end_wlan_head"

typedef struct t_all_ctx_head
{
  void *ioc_ctx;
  void *blkd_ctx;
  void *rpct_ctx;
} t_all_ctx_head;


enum {
  mutype_none = 0,
  mulan_type,
  endp_type_tap,
  endp_type_wif,
  endp_type_raw,
  endp_type_snf,
  endp_type_c2c,
  endp_type_a2b,
  endp_type_nat,
  endp_type_hsim,
  endp_type_kvm_eth,
  endp_type_kvm_wlan,
};

typedef void (*t_fd_local_flow_ctrl)(void *ptr, int llid, int stop);
typedef void (*t_fd_dist_flow_ctrl)(void *ptr, int llid, char *name, int num, 
                                    int tidx, int rank, int stop);
typedef void (*t_fd_error)(void *ptr, int llid, int err, int from);
typedef int  (*t_fd_event)(void *ptr, int llid, int fd);
typedef void (*t_fd_connect)(void *ptr, int llid, int llid_new);
typedef void (*t_qemu_group)(void *ptr, void *data);

int blkd_channel_create(void *ptr, int fd, 
                        t_fd_event rx, 
                        t_fd_event tx, 
                        t_fd_error err,
                        char *from);
/*--------------------------------------------------------------------------*/

/****************************************************************************/
typedef struct t_blkd_item
{
  char sock[MAX_PATH_LEN];
  char rank_name[MAX_NAME_LEN];
  int  rank_num;
  int  rank_tidx;
  int rank;
  int pid;
  int llid;
  int fd;
  int sel_tx;
  int sel_rx;
  int fifo_tx;
  int fifo_rx;
  int queue_tx;
  int queue_rx;
  int bandwidth_tx;
  int bandwidth_rx;
  int stop_tx;
  int stop_rx;
  int dist_flow_ctrl_tx;
  int dist_flow_ctrl_rx;
  long long drop_tx;
  long long drop_rx;
} t_blkd_item;
/*---------------------------------------------------------------------------*/
typedef struct t_blkd_group
{
  uint32_t volatile count_blkd_tied;
  int len_data_done;
  int len_data_read;
  int len_data_max;
  char *head_data; 
  int qemu_total_payload_len;
  t_qemu_group qemu_group_cb;
  void *data;
} t_blkd_group;
/*--------------------------------------------------------------------------*/
typedef struct t_blkd
{
  uint32_t volatile count_reference;
  t_blkd_group *group;
  int   qemu_group_rank;
  int   llid;
  int   type;
  int   val;
  long long usec;
  int  header_blkd_len;
  int   payload_len;
  char  *header_blkd;
  char  *payload_blkd;
} t_blkd;
/*---------------------------------------------------------------------------*/
typedef struct t_blkd_chain
{
  t_blkd *blkd;
  struct t_blkd_chain *next;
} t_blkd_chain;
/*--------------------------------------------------------------------------*/
typedef void (*t_blkd_rx_cb)(void *ptr, int llid);
/*---------------------------------------------------------------------------*/
int blkd_server_listen(void *ptr, char *sock, t_fd_connect con_cb);
/*---------------------------------------------------------------------------*/
void blkd_server_set_callbacks(void *ptr, int llid, t_blkd_rx_cb rx_cb,
                                                    t_fd_error err_cb);
/*---------------------------------------------------------------------------*/
int blkd_watch_fd(void *ptr, char *strident, int fd, t_blkd_rx_cb rx_cb,
                                                     t_fd_error err_cb);
/*---------------------------------------------------------------------------*/
int blkd_client_connect(void *ptr, char *sock, t_blkd_rx_cb rx_cb,
                                               t_fd_error err_cb);
/*---------------------------------------------------------------------------*/
t_blkd *blkd_get_rx(void *ptr, int llid);
void blkd_free(void *ptr, t_blkd *blkd);
/*---------------------------------------------------------------------------*/
void blkd_put_tx(void *ptr, int nb, int *llid, t_blkd *blkd);
/*---------------------------------------------------------------------------*/
t_blkd *blkd_create_tx_full_copy(int len, char *buf, 
                                 int llid, int type, int val);
/*---------------------------------------------------------------------------*/
t_blkd *blkd_create_tx_qemu_group(t_blkd_group **ptr_group,
                                  t_qemu_group qemu_group_cb,
                                  void *data, 
                                  int len, char *buf,
                                  int llid, int type, int val);
/*---------------------------------------------------------------------------*/
t_blkd *blkd_create_tx_empty(int llid, int type, int val);
/*---------------------------------------------------------------------------*/
int blkd_get_tx_rx_queues(void *ptr, int llid, int *tx_queued, int *rx_queued);
/*---------------------------------------------------------------------------*/
int blkd_delete(void *ptr, int llid);
/*---------------------------------------------------------------------------*/
int blkd_get_our_mutype(void *ptr);
void blkd_set_our_mutype(void *ptr, int mutype);
/*---------------------------------------------------------------------------*/
void blkd_heartbeat(void *ptr);
/*---------------------------------------------------------------------------*/
t_blkd_item *get_llid_blkd_report_item(void *ptr, int llid);
/*---------------------------------------------------------------------------*/
int *get_llid_blkd_list(void *ptr);
int get_llid_blkd_list_max(void *ptr);
/*---------------------------------------------------------------------------*/
void blkd_set_cloonix_llid(void *ptr, int llid);
int blkd_get_cloonix_llid(void *ptr);
/*---------------------------------------------------------------------------*/
void blkd_set_rank(void *ptr, int llid, int rank, 
                   char *name, int num, int tidx);
int blkd_get_rank(void *ptr, int llid, 
                  char *name, int *num, int *tidx);
int blkd_get_llid_with_rank(void *ptr, int rank);
/*---------------------------------------------------------------------------*/
void blkd_stop_tx_counter_increment(void *ptr, int llid);
/*---------------------------------------------------------------------------*/
void blkd_stop_rx_counter_increment(void *ptr, int llid);
/*---------------------------------------------------------------------------*/
void blkd_drop_rx_counter_increment(void *ptr, int llid, int val);
/*---------------------------------------------------------------------------*/
void blkd_tx_local_flow_control(void *ptr, int llid, int stop);
void blkd_rx_local_flow_control(void *ptr, int llid, int stop);
/*---------------------------------------------------------------------------*/
void blkd_rx_dist_flow_control(void *ptr, char *name, int num, int tidx,
                               int rank, int stop);
/*---------------------------------------------------------------------------*/
void blkd_init(void *ptr, t_fd_local_flow_ctrl lfc_tx,  
                          t_fd_local_flow_ctrl lfc_rx,  
                          t_fd_dist_flow_ctrl dfc);
/****************************************************************************/

