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
#define OUR_MAC_GW   "42:CA:FE:13:07:02"
#define OUR_MAC_DNS  "42:CA:FE:13:07:03"
#define OUR_MAC_CISCO "42:CA:FE:13:07:13"

#define MAC_ADDR_LEN 6 
#define MAC_HLEN     14
#define IP_HLEN      20
#define TCP_HLEN     20
#define TCP_WND     (u16_t)50000
#define TCP_WND_MIN (u16_t)2000
#define TCP_MAX_SIZE 1400

#define TH_FIN  0x01
#define TH_SYN  0x02
#define TH_RST  0x04
#define TH_PUSH 0x08
#define TH_ACK  0x10


typedef signed char s8_t;
typedef unsigned char u8_t;
typedef signed short int s16_t;
typedef unsigned short int u16_t;
typedef __s32 s32_t;
typedef __u32 u32_t;
#if __WORDSIZE == 64
typedef __s64 s64_t;
typedef __u64 u64_t;
#else
__extension__ typedef signed long long int s64_t;
__extension__ typedef unsigned long long int u64_t;
#endif
/*---------------------------------------------------------------------------*/

enum
{
  error_none = 0,
  error_not_established = -1,
  error_not_authorized = -2,
};



/*****************************************************************************/
enum {
  state_idle = 0,
  state_created,
  state_first_syn_sent,
  state_established,
  state_fin_wait1,
  state_fin_wait2,
  state_fin_wait_last_ack,
  state_closed,
  state_max,
};
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
typedef struct t_low
{
  u8_t  mac_src[MAC_ADDR_LEN];
  u8_t  mac_dest[MAC_ADDR_LEN];
  u32_t ip_src;
  u32_t ip_dest;
  u16_t tcp_src;
  u16_t tcp_dest;
  u32_t ackno;
  u32_t seqno;
  u8_t  flags;
  u16_t wnd;
  u16_t datalen;
  u16_t tcplen;
  u8_t *data;
} t_low;
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
typedef struct t_tcp_id 
{
  u8_t  local_mac[MAC_ADDR_LEN];
  u8_t  remote_mac[MAC_ADDR_LEN];
  u32_t local_ip;
  u32_t remote_ip;
  u16_t local_port;
  u16_t remote_port;
  int llid;
  int llid_unlocked;
} t_tcp_id;
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
typedef struct t_hdata
{
  int len;
  int max_len;
  int count_50ms;
  int count_stuck;
  int count_tries_tx;
  u32_t seqno;
  u8_t *data;
  struct t_hdata *prev;
  struct t_hdata *next;
} t_hdata;
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
typedef struct t_ldata
{
  t_low *low;
  struct t_ldata *prev;
  struct t_ldata *next;
} t_ldata;
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
typedef struct t_clo
{
  t_tcp_id tcpid;
  int closed_state_count;
  int closed_state_count_initialised;
  int closed_state_count_line;
  int id_tcpid;
  int has_been_closed_from_outside_socket;
  int state;
  int syn_sent_non_activ_count;
  int non_activ_count;
  int tx_repeat_failure_count;
  int read_zero_bytes_occurence;
  int have_to_ack;
  int send_ack_active;
  u32_t ackno_sent;
  u32_t send_unack;
  u32_t send_next;
  u32_t recv_next;
  u16_t loc_wnd;
  u16_t dist_wnd;
  t_hdata *head_hdata;
  t_ldata *head_ldata;
  struct t_clo *prev;
  struct t_clo *next;
} t_clo;
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
typedef void (*t_low_output)(int mac_len, u8_t *mac_data);
typedef int  (*t_high_data_rx_possible)(t_tcp_id *tcpid);
typedef void (*t_high_data_rx)(t_tcp_id *tcpid, int len, u8_t *data);
typedef void (*t_high_syn_rx)(t_tcp_id *tcpid);
typedef void (*t_high_synack_rx)(t_tcp_id *tcpid);
typedef void (*t_high_close_rx)(t_tcp_id *tcpid);
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int util_tcpid_comp(t_tcp_id *a, t_tcp_id *b);
t_clo *util_get_clo(int llid);
void util_attach_llid_clo(int llid, t_clo *clo);
void util_detach_llid_clo(int llid, t_clo *clo);

t_clo *util_get_fast_clo(t_tcp_id *tid);
void clo_low_input(int mac_len, u8_t *mac_data);
int  clo_high_data_tx(t_tcp_id *tcpid, int len, u8_t *data);
int  clo_high_data_tx_possible(t_tcp_id *tcpid);
int  clo_high_syn_tx(t_tcp_id *tcpid);
int  clo_high_synack_tx(t_tcp_id *tcpid);
int  clo_high_close_tx(t_tcp_id *tcpid, int trace);
void clo_heartbeat_timer(void);
void clo_high_free_tcpid(t_tcp_id *tcpid);
/*---------------------------------------------------------------------------*/
t_all_ctx *get_all_ctx(void);
/*---------------------------------------------------------------------------*/
void clo_init(t_all_ctx *all_ctx,
              t_low_output     low_output,
              t_high_data_rx_possible high_data_rx_possible,
              t_high_data_rx   high_data_rx,
              t_high_syn_rx    high_syn_rx,
              t_high_synack_rx high_synack_rx,
              t_high_close_rx  high_close_rx);
/*---------------------------------------------------------------------------*/


