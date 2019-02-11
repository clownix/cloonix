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
struct t_machine;
#define MAX_HASH_IDX 0xFF+2 

enum
{
  is_udp_sock = 1,
  is_tcp_sock,
};

enum
{
  connect_state_min = 0,
  connect_state_before_first_try,
  connect_state_after_first_try,
  connect_state_connected_ok,
  connect_state_max,
};

struct t_machine_sock;

/*****************************************************************************/
typedef struct t_input_msg_rx
{
  short sport;
  short dport;
  int ack;
  int seq;
  char flags;
  int len;
  char *data;
  char header_len;
  short win;
  short urgent;
  short checksum;

  struct t_machine_sock *ms;
  struct sockaddr_in addr;
  struct sockaddr_un addr_unix;
  int fd;
  int connect_state;
  int count_try_connect;
  long long timer_repeat_try_connect_abs_beat;
  int timer_repeat_try_connect_ref;
} t_input_msg_rx;
/*---------------------------------------------------------------------------*/


typedef struct t_not_acked_push_ack_sent
{
  int nb_of_time_sent;
  int len;
  char *data;
  int ack_waited_for;
  long long time_100th_of_millisec;
  struct t_machine_sock *ms;
  struct t_not_acked_push_ack_sent *prev;
  struct t_not_acked_push_ack_sent *next;
} t_not_acked_push_ack_sent;

/*--------------------------------------------------------------------------*/
typedef struct t_machine_sock
{
  int is_type_sock;
  char lip_replacement[MAX_NAME_LEN];
  char lip[MAX_NAME_LEN];
  int  lip_int;
  short lport;
  char fip[MAX_NAME_LEN];
  int  fip_int;
  short fport;
  int fd;
  int llid;
  int lseq;
  int nextfseq;
  int reference_fseq;
  int fseq;
  int fack;
  char fflags;
  int state;
  int count_not_acked_push_ack;
  int data_len;
  char *data;
  int data_waiting_len;
  char *data_waiting;
  long long timer_repeat_abs_beat;
  int timer_repeat_ref;
  long long timer_differed_abs_beat;
  int timer_differed_ref;
  long long timer_killer_abs_beat;
  int timer_killer_ref;
  t_input_msg_rx *msg_rx;
  t_not_acked_push_ack_sent *head_not_acked_push_ack_sent;
  struct t_machine *machine;
  struct t_machine_sock *fast_sock_prev;
  struct t_machine_sock *fast_sock_next;
  struct t_machine_sock *prev;
  struct t_machine_sock *next;
} t_machine_sock;
/*--------------------------------------------------------------------------*/
typedef struct t_machine
{
  char name[MAX_NAME_LEN];
  char mac[MAX_NAME_LEN];
  char ip[MAX_NAME_LEN];
  int  active_machine;
  long long tot_rx_pkt;
  long long last_tot_rx_pkt;
  int  icmp_seq_num;
  long long timer_icmp_abs_beat;
  int timer_icmp_ref;
  long long timer_monitor_abs_beat;
  int timer_monitor_ref;
  t_machine_sock *head_udp;
  t_machine_sock *hash_head[MAX_HASH_IDX];
  struct t_machine *prev;
  struct t_machine *next;
} t_machine;
/*--------------------------------------------------------------------------*/
t_machine *look_for_machine_with_name(char *name);
t_machine *look_for_machine_with_mac(char *mac);
t_machine *look_for_machine_with_ip(char *ip);

int add_machine(char *mac);
int del_machine(char *mac);
void machine_init(void);
/*--------------------------------------------------------------------------*/
t_machine_sock *get_llid_to_socket(int llid);
/*--------------------------------------------------------------------------*/
t_machine_sock *machine_get_udp_sock(t_machine *machine, 
                                     char *lip, short lport,
                                     char *fip, short fport);
/*--------------------------------------------------------------------------*/
void free_machine_sock(t_machine_sock *so);
/*--------------------------------------------------------------------------*/
void machine_boop_alloc(t_machine *machine, char *alloc_ip);
/*--------------------------------------------------------------------------*/
t_machine *get_head_machine(void);
/*--------------------------------------------------------------------------*/
void delete_machine_sock_llid(t_machine_sock *ms);
/*--------------------------------------------------------------------------*/
void add_not_acked_push_ack_sent (t_machine_sock *ms, int ack_waited_for,
                                  int len_data, char *data);
/*--------------------------------------------------------------------------*/
void del_not_acked_push_ack_sent (t_machine_sock *ms,
                                  t_not_acked_push_ack_sent *item);
/*--------------------------------------------------------------------------*/
int not_acked_push_ack_repeat_has_occured(t_machine_sock *ms);
/*--------------------------------------------------------------------------*/
void ack_for_push_ack_has_arrived(t_machine_sock *ms, int ack);
/*--------------------------------------------------------------------------*/
void del_whole_not_acked_push_ack_sent_chain(t_machine_sock *ms);
/*--------------------------------------------------------------------------*/
void tx_push_ack_queue_process(t_machine_sock *ms);
/*--------------------------------------------------------------------------*/
int nb_push_ack_stored(t_machine_sock *ms);
/*--------------------------------------------------------------------------*/
void set_tcp_new_state(t_machine_sock *ms, int new_state);
/*--------------------------------------------------------------------------*/


















