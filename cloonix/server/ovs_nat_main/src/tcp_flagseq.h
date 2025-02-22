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

/*--------------------------------------------------------------------------*/
typedef struct t_qstore
{
  int data_len;
  uint8_t *data;
  uint32_t local_seq;
  struct t_qstore *next;
} t_qstore;
/*--------------------------------------------------------------------------*/
typedef struct t_flagseq
{
  uint64_t total_enqueue;
  uint64_t total_dequeue;
  uint64_t total_stopped;
  uint32_t sip;
  uint32_t dip;
  uint16_t sport;
  uint16_t dport;
  uint8_t  smac[6];
  uint8_t  dmac[6];
  int      is_ssh_cisco;
  int      is_ssh_cisco_first;
  uint32_t offset;
  uint32_t post_chk;
  uint32_t local_seq;
  uint32_t prev_distant_seq;
  uint32_t distant_seq;
  uint32_t ack_local_seq;
  uint32_t ack_local_seq_prev;
  uint32_t ack_local_seq_count;
  int      must_ack;
  int      open_ok;
  int      fin_req;
  int      close_ok;
  int      llid_was_cutoff;
  int      fin_ack_received;
  int      fin_ack_transmited;
  int      reset_decrementer;
  struct t_qstore *head_qstore;
  struct t_qstore *tail_qstore;
  int   nb_qstore;
  struct t_qstore *head_qstore_backup;
  struct t_qstore *tail_qstore_backup;
  int   nb_qstore_backup;
  uint8_t tcp[TCP_HEADER_LEN];
} t_flagseq;
/*--------------------------------------------------------------------------*/

void tcp_flagseq_init(void);
void tcp_flagseq_llid_was_cutoff(t_flagseq *cur);

void tcp_flagseq_heartbeat(t_flagseq *flagseq);

void tcp_flagseq_to_tap(t_flagseq *flagseq, int data_len, uint8_t *data);

void tcp_flagseq_to_llid_data(t_flagseq *flagseq, int llid, 
                              uint8_t *tcp, int data_len, uint8_t *data);

void tcp_flagseq_end(t_flagseq *flagseq);

t_flagseq *tcp_flagseq_begin(uint32_t sip,   uint32_t dip,
                             uint16_t sport, uint16_t dport,
                             uint8_t *smac,  uint8_t *dmac,
                             uint8_t *tcp, int is_ssh_cisco);

/*--------------------------------------------------------------------------*/
