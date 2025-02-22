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
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>


#include "io_clownix.h"
#include "rpc_clownix.h"
#include "rxtx.h"
#include "utils.h"
#include "tcp.h"
#include "tcp_flagseq.h"
#include "tcp_qstore.h"
#include "ssh_cisco_nat.h"


/*****************************************************************************/
static void tcp_llid_transmit(int llid, int len, uint8_t *data)
{
  if (msg_exist_channel(llid))
    proxy_traf_unix_tx(llid, len, (char *) data);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void flagseq_end_of_flow(t_flagseq *cur, int fast)
{
  if (cur->is_ssh_cisco)
    ssh_cisco_nat_end_of_flow(cur->sip,cur->dip,cur->sport,cur->dport,fast);
  else
    tcp_end_of_flow(cur->sip, cur->dip, cur->sport, cur->dport, fast);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void flagseq_llid_transmit(t_flagseq *cur, int llid,
                                  int data_len, uint8_t *data)
{
  if (cur->is_ssh_cisco)
    ssh_cisco_nat_llid_transmit(llid, data_len, data);
  else
    tcp_llid_transmit(llid, data_len, data);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_flagseq_tcp_hdr(t_flagseq *cur, uint8_t *tcp, int data_len)
{
  uint32_t seq;
  uint8_t  flags;
  uint32_t seq_to_add = data_len;
  seq = (tcp[4]<<24) + (tcp[5]<<16) + (tcp[6]<<8) + tcp[7];
  flags = tcp[13];
  memcpy(cur->tcp, tcp, TCP_HEADER_LEN);
  if ((flags & RTE_TCP_SYN_FLAG) || (flags & RTE_TCP_FIN_FLAG))
    seq_to_add += 1;
  cur->prev_distant_seq = cur->distant_seq;
  cur->distant_seq = (seq + seq_to_add);

  //if (cur->prev_distant_seq != cur->distant_seq)
  //  KERR("NEWDISTANTSEQ: %u %d", cur->distant_seq, data_len);

}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void transmit_flags_back(t_flagseq *cur, uint8_t flags)
{
  int32_t recv_ack = 0;
  uint8_t buf[MAX_TAP_BUF_LEN];
  if (flags & RTE_TCP_ACK_FLAG)
    recv_ack = cur->distant_seq;  
  utils_fill_tcp_packet(buf, 0, cur->dmac, cur->smac, cur->dip, cur->sip, 
                        cur->dport, cur->sport, cur->local_seq,
                        recv_ack, flags, 50000);
  if ((flags & RTE_TCP_SYN_FLAG) || (flags & RTE_TCP_FIN_FLAG))
    cur->local_seq += 1;
  rxtx_tx_enqueue(cur->offset+cur->post_chk, buf);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void try_qstore_dequeue_backup_and_xmit(t_flagseq *cur)
{
  uint8_t tcp_flags, *buf;
  uint32_t local_seq = 0;
  int i, data_len;
  
  for (i=0; i< 100; i++)
    {
    buf = tcp_qstore_get_backup(cur, i, &data_len, &local_seq);
    if (!buf)
      break;
    tcp_flags = RTE_TCP_PSH_FLAG | RTE_TCP_ACK_FLAG;
    utils_fill_tcp_packet(buf, data_len, cur->dmac, cur->smac,
                          cur->dip, cur->sip, cur->dport, cur->sport,
                          local_seq, cur->distant_seq, tcp_flags, 50000);
    rxtx_tx_enqueue(cur->offset+cur->post_chk+data_len, buf);
    if (cur->must_ack)
      cur->must_ack = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void try_qstore_dequeue_and_xmit(t_flagseq *cur)
{
  uint16_t distant_rx_win;
  uint8_t tcp_flags, *buf;
  int data_len;
  distant_rx_win = (cur->tcp[14]<<8) + cur->tcp[15];

  if (((cur->local_seq - cur->ack_local_seq + 5000) < distant_rx_win) ||
       (cur->is_ssh_cisco_first == 1))
    {
    buf = tcp_qstore_dequeue(cur, &data_len, cur->local_seq);
    if (buf)
      {
      while (buf)
        {
        tcp_flags = RTE_TCP_PSH_FLAG | RTE_TCP_ACK_FLAG;
        utils_fill_tcp_packet(buf, data_len, cur->dmac, cur->smac,
                              cur->dip, cur->sip, cur->dport, cur->sport,
                              cur->local_seq, cur->distant_seq,
                              tcp_flags, 50000);
        cur->local_seq += data_len;
        if (cur->must_ack)
          cur->must_ack = 0;
        rxtx_tx_enqueue(cur->offset+cur->post_chk+data_len, buf);
        utils_free(buf);
        if ((cur->local_seq - cur->ack_local_seq + 5000) > distant_rx_win)
          {
          buf = NULL;
          }
        else
          {
          buf = tcp_qstore_dequeue(cur, &data_len, cur->local_seq);
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void check_stall_of_ack(t_flagseq *cur)
{
  if (cur->ack_local_seq_prev == cur->ack_local_seq)
    {
    if (cur->local_seq != cur->ack_local_seq)
      {
      cur->ack_local_seq_count += 1;
      if (cur->ack_local_seq_count > 500)
        {
        cur->ack_local_seq_count = 0;
        try_qstore_dequeue_backup_and_xmit(cur);
        }
      }
    else
      cur->ack_local_seq_count = 0;
    }
  else
    {
    cur->ack_local_seq_prev = cur->ack_local_seq;
    cur->ack_local_seq_count = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void syn_done_open_upon_syn_ack(t_flagseq *cur, uint8_t *tcp)
{
  uint8_t flags = tcp[13];
  if ((flags & RTE_TCP_ACK_FLAG) && (flags & RTE_TCP_SYN_FLAG))
    {
    cur->is_ssh_cisco_first = 1;
    cur->open_ok = 1;
    transmit_flags_back(cur, RTE_TCP_ACK_FLAG);
    ssh_cisco_nat_syn_ack_ok(cur->sip, cur->dip, cur->sport, cur->dport);
    }
  else
    {
    KERR("TOLOOKINTO %X %X %hu %hu flags:%hhX",
          cur->sip, cur->dip, cur->sport, cur->dport, flags);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void syn_syn_ack_ack_done_open_upon_ack(t_flagseq *cur, uint8_t *tcp)
{
  uint8_t flags = tcp[13];
  if (flags & RTE_TCP_ACK_FLAG)
    {
    cur->open_ok = 1;
    }
  else if (flags & RTE_TCP_SYN_FLAG)
    {
    transmit_flags_back(cur, RTE_TCP_SYN_FLAG | RTE_TCP_ACK_FLAG);
    }
  else
    KERR("TOLOOKINTO %X %X %hu %hu flags:%hhX",
          cur->sip, cur->dip, cur->sport, cur->dport, flags);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void repeat_fin_ack_tx(t_flagseq *cur, uint8_t flags)
{
  if (cur->fin_ack_transmited < 4)
    {
    cur->fin_ack_transmited += 1;
    cur->local_seq -= 1;
    transmit_flags_back(cur, RTE_TCP_ACK_FLAG | RTE_TCP_FIN_FLAG);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void fin_fin_ack_done_close_upon_ack(t_flagseq *cur, uint8_t *tcp)
{
  uint32_t recv_ack, ack_local_seq;
  uint8_t flags = tcp[13];
  recv_ack = (tcp[8]<<24) + (tcp[9]<<16) + (tcp[10]<<8) + tcp[11];
  ack_local_seq = recv_ack;

  if (cur->fin_ack_received == 0)
    {
    if (flags & RTE_TCP_FIN_FLAG)
      {
      cur->reset_decrementer = 0;
      cur->fin_ack_received = 1;
      transmit_flags_back(cur, RTE_TCP_ACK_FLAG);
      flagseq_end_of_flow(cur, 1);
      }
    else
      {
      repeat_fin_ack_tx(cur, flags);
      }
    }
  else
    {
    if (flags & RTE_TCP_ACK_FLAG)
      {
      if (cur->local_seq == ack_local_seq)  
        {
        cur->open_ok = 0;
        cur->close_ok = 1;
        transmit_flags_back(cur, RTE_TCP_ACK_FLAG);
        flagseq_end_of_flow(cur, 1);
        }
      else
        {
        repeat_fin_ack_tx(cur, flags);
        }
      }
    else
      {
      KERR("TOLOOKINTO %X %X %hu %hu flags:%hhX",
            cur->sip, cur->dip, cur->sport, cur->dport, flags);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void reception_of_first_fin_ack(t_flagseq *cur,
                                       uint32_t new_distant_seq)
{
  cur->fin_req = 1;
  cur->fin_ack_received = 1;
  cur->distant_seq = new_distant_seq;
  flagseq_end_of_flow(cur, 0);
  tcp_qstore_flush(cur);
  transmit_flags_back(cur, RTE_TCP_ACK_FLAG | RTE_TCP_FIN_FLAG);
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
void tcp_flagseq_llid_was_cutoff(t_flagseq *cur)
{
  if (cur->fin_req == 0)
    cur->llid_was_cutoff = 1;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_flagseq_to_tap(t_flagseq *cur, int data_len, uint8_t *data)
{
  if ((cur->open_ok == 1) && (cur->close_ok == 0))
    {
    if (cur->fin_req == 0)
      {
      tcp_qstore_enqueue(cur, data_len, data);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_flagseq_to_llid_data(t_flagseq *cur, int llid, uint8_t *tcp,
                              int data_len, uint8_t *data)
{
  uint32_t sent_seq, recv_ack, new_distant_seq;
  uint8_t flags = tcp[13];
  sent_seq =(tcp[4]<<24) + (tcp[5]<<16) + (tcp[6]<<8) + tcp[7]; 
  new_distant_seq = sent_seq;

  if (flags & RTE_TCP_RST_FLAG)
    {
    cur->open_ok = 0;
    cur->close_ok = 1;
    flagseq_end_of_flow(cur, 2);
    tcp_qstore_flush(cur); 
    }
  else if (cur->open_ok == 0)
    {
    update_flagseq_tcp_hdr(cur, tcp, 0);
    if (cur->is_ssh_cisco == 0)
      syn_syn_ack_ack_done_open_upon_ack(cur, tcp);
    else
      syn_done_open_upon_syn_ack(cur, tcp);
    }
  else if (cur->fin_req == 1)
    {
    update_flagseq_tcp_hdr(cur, tcp, 0);
    fin_fin_ack_done_close_upon_ack(cur, tcp);
    }
  else
    {
    if (flags & RTE_TCP_ACK_FLAG)
      {
      recv_ack = (tcp[8]<<24) + (tcp[9]<<16) + (tcp[10]<<8) + tcp[11];
      cur->ack_local_seq = recv_ack;
      tcp_qstore_flush_backup_seq(cur, cur->ack_local_seq);
      }
    if (cur->distant_seq == new_distant_seq)
      {
      update_flagseq_tcp_hdr(cur, tcp, data_len);
      if ((flags & RTE_TCP_FIN_FLAG) && (flags & RTE_TCP_ACK_FLAG))
        {
        reception_of_first_fin_ack(cur, new_distant_seq);
        }
      else if (data_len)
        {
        flagseq_llid_transmit(cur, llid, data_len, data);
        cur->must_ack = 1;
        }
      }
    else if ((new_distant_seq+1) == cur->distant_seq)
      {
      KERR("TOOLOOK %X %X %hu %hu %X %X %d", cur->sip, cur->dip,
                       cur->sport, cur->dport, cur->distant_seq,
                       new_distant_seq, data_len);

      update_flagseq_tcp_hdr(cur, tcp, data_len);
      if ((flags & RTE_TCP_FIN_FLAG) && (flags & RTE_TCP_ACK_FLAG))
        {
        reception_of_first_fin_ack(cur, new_distant_seq);
        }
      else if (data_len)
        {
        flagseq_llid_transmit(cur, llid, data_len, data);
        cur->must_ack = 1;
        }

      }
    else
      {
      KERR("ERROR DROP %X %X %hu %hu %X %X %d", cur->sip, cur->dip,
                               cur->sport, cur->dport, cur->distant_seq,
                               new_distant_seq, data_len);
      transmit_flags_back(cur, RTE_TCP_ACK_FLAG);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_flagseq_heartbeat(t_flagseq *cur)
{
  if ((cur->open_ok == 1) && (cur->close_ok == 0))
    {
    if (cur->reset_decrementer)
      {
      cur->reset_decrementer -= 1;
      if (cur->reset_decrementer == 0)
        {
        transmit_flags_back(cur, RTE_TCP_RST_FLAG);
        }
      }
    check_stall_of_ack(cur);
    try_qstore_dequeue_and_xmit(cur);
    if ((cur->llid_was_cutoff) && (cur->fin_req == 0))
      {
      if (tcp_qstore_qty(cur) == 0)
        {
        cur->fin_req = 1;
        cur->reset_decrementer = RESET_DESTRUCTION_DECREMENTER;
        transmit_flags_back(cur, RTE_TCP_ACK_FLAG | RTE_TCP_FIN_FLAG);
        }
      }
    if ((cur->fin_req == 0) && (cur->must_ack))
      {
      cur->must_ack = 0;
      transmit_flags_back(cur, RTE_TCP_ACK_FLAG);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_flagseq_end(t_flagseq *cur)
{
  cur->open_ok = 0;
  cur->close_ok = 1;
  tcp_qstore_flush(cur); 
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
t_flagseq *tcp_flagseq_begin(uint32_t sip,   uint32_t dip,
                             uint16_t sport, uint16_t dport,
                             uint8_t *smac,  uint8_t *dmac,
                             uint8_t *tcp, int is_ssh_cisco)
{
  uint32_t seq = 0;
  t_flagseq *cur = (t_flagseq *) utils_malloc(sizeof(t_flagseq));
  if (cur == NULL)
    KOUT("ERROR MALLOC");
  memset(cur, 0, sizeof(t_flagseq));
  tcp_qstore_init(cur);
  if (is_ssh_cisco == 0)
    seq =(tcp[4]<<24) + (tcp[5]<<16) + (tcp[6]<<8) + tcp[7]; 
  cur->sip   = sip; 
  cur->dip   = dip; 
  cur->sport = sport; 
  cur->dport = dport; 
  cur->is_ssh_cisco = is_ssh_cisco;
  cur->local_seq = 1;
  cur->distant_seq = seq;
  memcpy(cur->smac, smac, 6);
  memcpy(cur->dmac, dmac, 6);
  if (is_ssh_cisco == 0)
    {
    if (tcp[13] != RTE_TCP_SYN_FLAG)
      KERR("ERROR %X %X %hu %hu flags:%hhX", cur->sip, cur->dip,
            cur->sport, cur->dport, tcp[13]);
    else
      {
      update_flagseq_tcp_hdr(cur, tcp, 0);
      transmit_flags_back(cur, RTE_TCP_SYN_FLAG | RTE_TCP_ACK_FLAG);
      }
    }
  else
    {
    transmit_flags_back(cur, RTE_TCP_SYN_FLAG);
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_flagseq_init(void)
{
}
/*--------------------------------------------------------------------------*/


