/*****************************************************************************/
/*    Copyright (C) 2006-2018 cloonix@cloonix.net License AGPL-3             */
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
#include <asm/types.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "ioc.h"
#include "clo_tcp.h"
#include "clo_mngt.h"
#include "clo_utils.h"
#include "clo_low.h"
/*---------------------------------------------------------------------------*/

static t_clo *head_clo;
static u32_t g_250ms_count;

static int g_tcp_max_size;

void set_g_tcp_max_size(int max_tcp)
{
  g_tcp_max_size = max_tcp;
}

/****************************************************************************/
static u32_t clo_next_iss(void)
{
  static u32_t iss = 0x100000;
  iss += g_250ms_count;
  return iss;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void update_unack_release_past_hdata(t_clo *clo, u32_t ackno)
{
  t_hdata *next, *cur = clo->head_hdata;
  u32_t add = 0;
  while (cur)
    {
    next = cur->next;
    if (cur->count_250ms != 0)
      {
      if ((cur->seqno + TCP_WND ) < cur->seqno)
        add = 0x10000000;
      if ((ackno+add) >= ((cur->seqno+add) + cur->len))
        util_extract_hdata(&(clo->head_hdata), cur);
      }
    cur = next;
    }
  clo->send_unack = ackno;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
u32_t get_g_250ms_count(void)
{
  return g_250ms_count;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
t_clo *get_head_clo(void)
{
  return head_clo;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_mngt_set_state(t_clo *clo, int state)
{
  printf("STATE: from %s to %s\n", 
         util_state2ascii(clo->state), util_state2ascii(state));
  clo->state = state;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int clo_mngt_get_state(t_clo *clo)
{
  return (clo->state);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_mngt_timer(void)
{
  g_250ms_count++;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_clo *clo_mngt_find(t_tcp_id *tcpid)
{
  t_clo *clo = util_get_fast_clo(tcpid);
  return clo;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void ajust(t_clo *clo, t_low *low, 
                  u32_t *ajusted_send_unack, 
                  u32_t *ajusted_send_next,
                  u32_t *ajusted_recv_next,
                  u32_t *ajusted_seqno,
                  u32_t *ajusted_ackno)
{
  if (clo->recv_next <= clo->recv_next + 2*TCP_WND)
    {
    *ajusted_recv_next = clo->recv_next;
    *ajusted_seqno = low->seqno;
    }
  else
    {
    *ajusted_recv_next = clo->recv_next + 0x10000000;
    *ajusted_seqno = low->seqno + 0x10000000;
    }
  if (clo->send_unack <= clo->send_next)
    {
    *ajusted_send_unack = clo->send_unack;
    *ajusted_send_next = clo->send_next;
    *ajusted_ackno = low->ackno;
    }
  else
    {
    *ajusted_send_unack = clo->send_unack + 0x10000000;
    *ajusted_send_next = clo->send_next + 0x10000000;
    *ajusted_ackno = low->ackno + 0x10000000;
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
int clo_mngt_low_input(t_clo *clo, t_low *low, int *inserted)
{
  int result = -1;
  int adjusted_tcplen;
  u32_t adjusted_send_unack, adjusted_send_next, adjusted_recv_next,
        adjusted_seqno, adjusted_ackno;
  *inserted = 0;
  if ((clo->state == state_created) && 
      (low->flags == TH_SYN))
    {
    clo->recv_next = low->seqno + low->tcplen;
    clo->dist_wnd = low->wnd;
    result = 0;
    }
  else if ((clo->state == state_first_syn_sent) &&
           ((low->flags & (TH_SYN | TH_ACK)) == (TH_SYN | TH_ACK)))
    {
    clo->recv_next = low->seqno + low->tcplen;
    clo->dist_wnd = low->wnd;
    update_unack_release_past_hdata(clo, low->ackno);
    clo->have_to_ack = 1;
    result = 0;
    }
  else
    {
    ajust(clo, low, &adjusted_send_unack, &adjusted_send_next,
          &adjusted_recv_next, &adjusted_seqno, &adjusted_ackno); 
    if ((low->flags & TH_FIN) ==  TH_FIN)
      adjusted_tcplen = low->tcplen + 1;
    else
      adjusted_tcplen = low->tcplen;
    if (((adjusted_recv_next <= adjusted_seqno) &&
         (adjusted_seqno <= adjusted_recv_next + 2*TCP_WND)) ||
        ((adjusted_recv_next <= adjusted_seqno + adjusted_tcplen) &&
         adjusted_seqno+adjusted_tcplen-1 <= adjusted_recv_next+2*TCP_WND)) 
      {
      if (low->flags & TH_ACK)
        {
        if ((adjusted_send_unack <= adjusted_ackno) && 
          (adjusted_ackno <= adjusted_send_next))
          update_unack_release_past_hdata(clo, low->ackno);
        }
      if (low->datalen)
        {
        if ((clo->state == state_established) ||
            (clo->state == state_fin_wait1) || 
            (clo->state == state_fin_wait_last_ack) || 
            (clo->state == state_fin_wait2))
          {
          if (low->seqno == clo->recv_next)
            {
            util_insert_ldata(&(clo->head_ldata), low);
            *inserted = 1;
            clo->have_to_ack = 1;
            clo->recv_next = low->seqno + adjusted_tcplen;
            clo->dist_wnd = low->wnd;
            }
          else
            {
            KERR("DROP bad seq rx%d  %d  %d", 
                  low->seqno, clo->recv_next, low->datalen);
            }
          }
        else
          KERR("DROP TOLOOKINTO %d", low->datalen);
        }
      result = 0;
      }
    else 
      {
      KERR("%d %d %d %d", 
           (adjusted_recv_next <= adjusted_seqno),
           (adjusted_seqno <= adjusted_recv_next + 2*TCP_WND),
           (adjusted_recv_next <= adjusted_seqno + adjusted_tcplen),
           (adjusted_seqno+adjusted_tcplen-1 <= adjusted_recv_next+2*TCP_WND));
      KERR("%d %d %d %d", adjusted_seqno, adjusted_recv_next, 
                          2*TCP_WND, adjusted_tcplen);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_mngt_high_input(t_clo *clo, int hlen, u8_t *hdata)
{
  int len, len_to_insert = hlen;
  u8_t *data_to_insert = hdata;
  u8_t *data;
  
  while (len_to_insert)
    {
    data = (u8_t *) malloc(g_tcp_max_size);
    memset(data, 0, g_tcp_max_size);
    if (len_to_insert > g_tcp_max_size)
      len = g_tcp_max_size;
    else 
      len = len_to_insert;
    memcpy(data, data_to_insert, len);
    util_insert_hdata(&(clo->head_hdata), g_tcp_max_size, len, data);
    len_to_insert -= len;
    data_to_insert += len;
    if (len_to_insert < 0)
      KOUT(" %d", len_to_insert);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
t_clo *clo_mngt_create_tcp(t_tcp_id *tcpid)
{
  t_clo *clo;
  clo = util_insert_clo(&head_clo, tcpid);
  clo_mngt_set_state(clo, state_created);
  clo->send_unack = clo_next_iss();
  clo->send_next = clo->send_unack;
  clo->loc_wnd = TCP_WND;
  return clo;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int clo_mngt_delete_tcp(t_clo *clo)
{
  int result = 0;
  clo_mngt_set_state(clo, state_closed);
  if (util_extract_clo(&head_clo, clo))
    {
    result = -1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void clo_mngt_get_ackno_seqno_wnd(t_clo *clo, u32_t *ackno, u32_t *seqno, 
                                  u16_t *loc_wnd, u16_t *dist_wnd)
{
  *ackno = clo->recv_next;
  *seqno = clo->send_next;
  *loc_wnd = clo->loc_wnd;
  *dist_wnd = clo->dist_wnd;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_mngt_adjust_send_next(t_clo *clo, u32_t seqno, int len)
{
  if (seqno != clo->send_next)
    KOUT(" %d %d ", seqno, clo->send_next);
  clo->send_next = seqno + len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int clo_mngt_adjust_loc_wnd(t_clo *clo, u16_t loc_wnd)
{
  int result = 0;
  if (loc_wnd != clo->loc_wnd)
    result = 1;
  clo->loc_wnd = loc_wnd;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int compute_ldata_len(t_clo *clo)
{
  t_ldata *cur = clo->head_ldata;
  int len = 0;
  while (cur)
    {
    len += cur->low->datalen;
    cur = cur->next;
    }
  return len;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int clo_mngt_get_new_rxdata(t_clo *clo, int *len, u8_t **data)
{
  int len_done = 0, result = -1;
  t_ldata *next, *cur = clo->head_ldata;
  *len = compute_ldata_len(clo);
  if (*len)
    {
    clo->non_activ_count = 0;
    *data = (u8_t *) malloc(*len);
    memset(*data, 0, *len);
    while (cur)
      {
      next = cur->next;
      memcpy(*data + len_done, cur->low->data, cur->low->datalen);
      len_done += cur->low->datalen;
      util_extract_ldata(&(clo->head_ldata), cur);
      cur = next;
      }
    result = 0;
    }
  if (clo->head_ldata)
    KOUT(" ");
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int  clo_mngt_authorised_to_send_nexttx(t_clo *clo)
{
  int result = 0;
  int diff;
  diff = (clo->send_next - clo->send_unack);
  if (diff < 0)
    KOUT(" %d %d", clo->send_unack, clo->send_next); 
  diff += g_tcp_max_size + 1;
  if (diff < clo->dist_wnd)
    result = 1;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_mngt_get_txdata(t_clo *clo, t_hdata *hd, u32_t *seqno, 
                        int *len, u8_t **data)
{
  *len = hd->len;
  if (!(*len))
    KOUT(" ");
  *data = (u8_t *) malloc(hd->len);
  memcpy(*data, hd->data, hd->len);
  if (hd->count_250ms == 0)
    hd->seqno = clo->send_next;
  hd->count_250ms = g_250ms_count;
  hd->count_tries_tx += 1;
  if (hd->count_tries_tx > clo->tx_repeat_failure_count)
    clo->tx_repeat_failure_count = hd->count_tries_tx;
  *seqno = hd->seqno;
  clo->non_activ_count = 0;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_mngt_init(void)
{
  head_clo = NULL;
  g_250ms_count = 0;
  g_tcp_max_size = TCP_MAX_SIZE;
}
/*---------------------------------------------------------------------------*/
