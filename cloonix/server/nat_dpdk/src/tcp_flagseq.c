/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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

#define ALLOW_EXPERIMENTAL_API
#include <rte_compat.h>
#include <rte_bus_pci.h>
#include <rte_config.h>
#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ethdev.h>
#include <rte_flow.h>
#include <rte_malloc.h>
#include <rte_mbuf.h>
#include <rte_meter.h>
#include <rte_pci.h>
#include <rte_version.h>
#include <rte_vhost.h>

#include "io_clownix.h"
#include "rpc_clownix.h"
#include "tcp_flagseq.h"
#include "tcp_qstore.h"
#include "txq_dpdk.h"
#include "tcp_llid.h"
#include "tcp.h"
#include "utils.h"
#include "ssh_cisco_dpdk.h"


/****************************************************************************/
static void flagseq_end_of_flow(t_flagseq *cur, int fast)
{
  if (cur->is_ssh_cisco)
    ssh_cisco_dpdk_end_of_flow(cur->sip,cur->dip,cur->sport,cur->dport,fast);
  else
    tcp_end_of_flow(cur->sip, cur->dip, cur->sport, cur->dport, fast);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void flagseq_llid_transmit(t_flagseq *cur, int llid,
                                  int data_len, uint8_t *data)
{
  if (cur->is_ssh_cisco)
    ssh_cisco_dpdk_llid_transmit(llid, data_len, data);
  else
    tcp_llid_transmit(llid, data_len, data);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void update_flagseq_tcp_hdr(t_flagseq *cur,
                                   struct rte_tcp_hdr *tcp_hdr,
                                   int data_len)
{
  uint32_t seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);
  uint8_t  flags = tcp_hdr->tcp_flags;
  uint32_t seq_to_add  =  data_len;
  memcpy(&(cur->tcp_hdr), tcp_hdr, sizeof(struct rte_tcp_hdr));
  if ((flags & RTE_TCP_SYN_FLAG) || (flags & RTE_TCP_FIN_FLAG))
    seq_to_add += 1;
  cur->distant_seq = (seq + seq_to_add);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void transmit_flags_back(t_flagseq *cur, uint8_t flags)
{
  uint8_t *buf;
  struct rte_mbuf *mbuf;
  int32_t recv_ack = 0;
  mbuf    = utils_alloc_pktmbuf(cur->offset + 4);
  buf     = rte_pktmbuf_mtod(mbuf, uint8_t *);
  if (flags & RTE_TCP_ACK_FLAG)
    recv_ack = cur->distant_seq;  
  utils_fill_tcp_packet(buf, 0, cur->dmac, cur->smac, cur->dip, cur->sip, 
                        cur->dport, cur->sport, cur->local_seq,
                        recv_ack, flags, 50000);
  if ((flags & RTE_TCP_SYN_FLAG) || (flags & RTE_TCP_FIN_FLAG))
    cur->local_seq += 1;
  txq_dpdk_enqueue(mbuf, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void try_qstore_dequeue_backup_and_xmit(t_flagseq *cur)
{
  struct rte_mbuf *mbuf;
  uint8_t tcp_flags, *buf;
  uint32_t local_seq = 0;
  int i, data_len;
  
  for (i=0; i< 100; i++)
    {
    mbuf = tcp_qstore_get_backup(cur, i, &data_len, &local_seq);
    if (!mbuf)
      break;
    buf = rte_pktmbuf_mtod(mbuf, uint8_t *);
    tcp_flags = RTE_TCP_PSH_FLAG | RTE_TCP_ACK_FLAG;
    utils_fill_tcp_packet(buf, data_len, cur->dmac, cur->smac,
                          cur->dip, cur->sip, cur->dport, cur->sport,
                          local_seq, cur->distant_seq, tcp_flags, 50000);
    txq_dpdk_enqueue(mbuf, 0);
    if (cur->must_ack)
      cur->must_ack = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void try_qstore_dequeue_and_xmit(t_flagseq *cur)
{
  struct rte_mbuf *mbuf;
  uint16_t distant_rx_win;
  uint8_t tcp_flags, *buf;
  int data_len;
  distant_rx_win = rte_be_to_cpu_16(cur->tcp_hdr.rx_win);
  if (((cur->local_seq - cur->ack_local_seq + 5000) < distant_rx_win) ||
       (cur->is_ssh_cisco_first == 1))
    {
    mbuf = tcp_qstore_dequeue(cur, &data_len, cur->local_seq);
    if (mbuf)
      {
      while (mbuf)
        {
        buf = rte_pktmbuf_mtod(mbuf, uint8_t *);
        tcp_flags = RTE_TCP_PSH_FLAG | RTE_TCP_ACK_FLAG;
        utils_fill_tcp_packet(buf, data_len, cur->dmac, cur->smac,
                              cur->dip, cur->sip, cur->dport, cur->sport,
                              cur->local_seq, cur->distant_seq,
                              tcp_flags, 50000);
        cur->local_seq += data_len;
        if (cur->must_ack)
          cur->must_ack = 0;
        txq_dpdk_enqueue(mbuf, 0);
        if ((cur->local_seq - cur->ack_local_seq + 5000) > distant_rx_win)
          {
          mbuf = NULL;
          }
        else
          {
          mbuf = tcp_qstore_dequeue(cur, &data_len, cur->local_seq);
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
static void syn_done_open_upon_syn_ack(t_flagseq *cur,
                                       struct rte_tcp_hdr *tcp_hdr)
{
  uint8_t flags = tcp_hdr->tcp_flags;
  if ((flags & RTE_TCP_ACK_FLAG) && (flags & RTE_TCP_SYN_FLAG))
    {
    cur->is_ssh_cisco_first = 1;
    cur->open_ok = 1;
    transmit_flags_back(cur, RTE_TCP_ACK_FLAG);
    ssh_cisco_dpdk_syn_ack_ok(cur->sip, cur->dip, cur->sport, cur->dport);
    }
  else
    {
    KERR("TOLOOKINTO %X %X %hu %hu flags:%X",
          cur->sip, cur->dip, cur->sport, cur->dport, flags & 0xFF);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void syn_syn_ack_ack_done_open_upon_ack(t_flagseq *cur,
                                               struct rte_tcp_hdr *tcp_hdr)
{
  uint8_t flags = tcp_hdr->tcp_flags; 
  if (flags & RTE_TCP_ACK_FLAG)
    {
    cur->open_ok = 1;
    }
  else if (flags & RTE_TCP_SYN_FLAG)
    {
    transmit_flags_back(cur, RTE_TCP_SYN_FLAG | RTE_TCP_ACK_FLAG);
    }
  else
    KERR("TOLOOKINTO %X %X %hu %hu flags:%X",
          cur->sip, cur->dip, cur->sport, cur->dport, flags & 0xFF);
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
static void fin_fin_ack_done_close_upon_ack(t_flagseq *cur,
                                            struct rte_tcp_hdr *tcp_hdr)
{
  uint32_t ack_local_seq = rte_be_to_cpu_32(cur->tcp_hdr.recv_ack);
  uint8_t flags = tcp_hdr->tcp_flags; 

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
      KERR("TOLOOKINTO %X %X %hu %hu flags:%X",
            cur->sip, cur->dip, cur->sport, cur->dport, flags & 0xFF);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void reception_of_first_fin_ack(t_flagseq *cur,
                                       uint32_t ack_local_seq,
                                       uint32_t new_distant_seq)
{
  cur->fin_req = 1;
  cur->fin_ack_received = 1;
  cur->local_seq = ack_local_seq;
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
void tcp_flagseq_to_dpdk_data(t_flagseq *cur, int data_len, uint8_t *data)
{
  if ((cur->open_ok == 1) && (cur->close_ok == 0))
    {
    if (cur->fin_req == 0)
      {
      tcp_qstore_enqueue(cur, data_len, data);
      }
    else
      {
      rte_free(data);
      }
    }
  else
    {
    rte_free(data);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_flagseq_to_llid_data(t_flagseq *cur, int llid,
                              struct rte_tcp_hdr *tcp_hdr,
                              int data_len, uint8_t *data)
{
  uint32_t new_distant_seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);
  uint32_t ack_local_seq = rte_be_to_cpu_32(cur->tcp_hdr.recv_ack);
  uint8_t flags = tcp_hdr->tcp_flags;

  if (flags & RTE_TCP_RST_FLAG)
    {
    cur->open_ok = 0;
    cur->close_ok = 1;
    flagseq_end_of_flow(cur, 2);
    tcp_qstore_flush(cur); 
    }
  else if (cur->open_ok == 0)
    {
    update_flagseq_tcp_hdr(cur, tcp_hdr, 0);
    if (cur->is_ssh_cisco == 0)
      syn_syn_ack_ack_done_open_upon_ack(cur, tcp_hdr);
    else
      syn_done_open_upon_syn_ack(cur, tcp_hdr);
    }
  else if (cur->fin_req == 1)
    {
    update_flagseq_tcp_hdr(cur, tcp_hdr, 0);
    fin_fin_ack_done_close_upon_ack(cur, tcp_hdr);
    }
  else if (cur->distant_seq == new_distant_seq)
    {
    if (flags & RTE_TCP_ACK_FLAG)
      {
      cur->ack_local_seq = rte_be_to_cpu_32(cur->tcp_hdr.recv_ack);
      tcp_qstore_flush_backup_seq(cur, cur->ack_local_seq);
      }
    if ((flags & RTE_TCP_FIN_FLAG) && (flags & RTE_TCP_ACK_FLAG))
      {
      reception_of_first_fin_ack(cur, ack_local_seq, new_distant_seq);
      }
    else if (data_len)
      {
      flagseq_llid_transmit(cur, llid, data_len, data);
      cur->must_ack = 1;
      }
    update_flagseq_tcp_hdr(cur, tcp_hdr, data_len);
    }
  else if ((new_distant_seq+1) == cur->distant_seq)
    {
    }
  else
    {
//    KERR("DROP %X %X %hu %hu %X %X %d", cur->sip, cur->dip, cur->sport,
//                                        cur->dport, cur->distant_seq,
//                                        new_distant_seq, data_len);
    transmit_flags_back(cur, RTE_TCP_ACK_FLAG);
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
                             struct rte_tcp_hdr *tcp_hdr,
                             int is_ssh_cisco)
{
  uint32_t seq = 0;
  t_flagseq *cur = (t_flagseq *) rte_malloc(NULL, sizeof(t_flagseq), 0);
  if (cur == NULL)
    KERR(" ");
  else
    {
    memset(cur, 0, sizeof(t_flagseq));
    tcp_qstore_init(cur);
    if (is_ssh_cisco == 0)
      seq = rte_be_to_cpu_32(tcp_hdr->sent_seq);
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
      if (tcp_hdr->tcp_flags != RTE_TCP_SYN_FLAG)
        KERR("%X %X %hu %hu flags:%X", cur->sip, cur->dip,
              cur->sport, cur->dport, tcp_hdr->tcp_flags & 0xFF);
      else
        {
        update_flagseq_tcp_hdr(cur, tcp_hdr, 0);
        transmit_flags_back(cur, RTE_TCP_SYN_FLAG | RTE_TCP_ACK_FLAG);
        }
      }
    else
      {
      transmit_flags_back(cur, RTE_TCP_SYN_FLAG);
      }
    }
  return cur;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void tcp_flagseq_init(void)
{
}
/*--------------------------------------------------------------------------*/


