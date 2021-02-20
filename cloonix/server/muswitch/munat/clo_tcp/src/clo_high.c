/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
#include "unix2inet.h"
/*---------------------------------------------------------------------------*/

static t_all_ctx *g_all_ctx;
/*****************************************************************************/
static t_low_output     fct_low_output;
static t_high_data_rx_possible fct_high_data_rx_possible;
static t_high_data_rx   fct_high_data_rx;
static t_high_syn_rx    fct_high_syn_rx;
static t_high_synack_rx fct_high_synack_rx;
static t_high_close_rx  fct_high_close_rx;
/*---------------------------------------------------------------------------*/
enum {
  num_fct_none = 0,
  num_fct_high_close_rx,
  num_fct_high_close_rx_send_rst,
  num_fct_send_finack,
  num_fct_max,
};
/*---------------------------------------------------------------------------*/
typedef struct t_timer_fct 
{
  int num_fct;
  int id_tcpid;
  t_tcp_id tcpid;
  int len;
  u8_t *data;
} t_timer_fct; 
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void ackno_send(t_clo *clo)
{
  u32_t ackno, seqno;
  u16_t loc_wnd, dist_wnd;
  clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
  if (clo->ackno_sent < ackno)
    util_send_ack(&(clo->tcpid), ackno, seqno, loc_wnd);
  clo->ackno_sent = ackno;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_send_finack_state_fin(t_clo *clo, int line)
{
  u32_t ackno, seqno;
  u16_t loc_wnd, dist_wnd;
  int state = clo_mngt_get_state(clo);
  if (clo->fin_ack_has_been_sent == 0)
    {
    clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
    if (clo->fin_use_stored_vals)
      util_send_finack(&(clo->tcpid), clo->fin_ackno, clo->fin_seqno, loc_wnd);
    else
      util_send_finack(&(clo->tcpid), ackno, seqno, loc_wnd);
    clo->fin_ack_has_been_sent = 1; 
    clo_mngt_adjust_send_next(clo, seqno, 1);
    if (clo->has_been_closed_from_outside_socket == 0)
      {
      if (state != state_established)
        KERR("%d LLID%d FINACK MISS %d %d %s", line, clo->tcpid.history_llid,
             clo->tcpid.local_port & 0xFFFF, clo->tcpid.remote_port & 0xFFFF,
             util_state2ascii(clo->state));
      clo_mngt_set_state(clo, state_fin_wait_last_ack);
      }
    }
  else
    {
    KERR("%d LLID%d FINACK MISS %d %d %s", line, clo->tcpid.history_llid,
          clo->tcpid.local_port & 0xFFFF, clo->tcpid.remote_port & 0xFFFF,
          util_state2ascii(clo->state));
    }
  unix2inet_finack_state(&(clo->tcpid), line);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void init_closed_state_count_if_not_done(t_clo *clo, int val, int line)
{
  if (!clo->closed_state_count_initialised)
    {
    clo->closed_state_count_line = line;
    clo->closed_state_count_initialised = 1;
    clo->closed_state_count = val;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_send_reset_state_closed(t_clo *clo)
{
  u32_t ackno, seqno;
  u16_t loc_wnd, dist_wnd;
  clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
  util_send_reset(&(clo->tcpid), ackno, seqno, loc_wnd, __FUNCTION__, __LINE__);
  clo_mngt_set_state(clo, state_closed);
  init_closed_state_count_if_not_done(clo, 1, __LINE__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_send_data_to_low(t_clo *clo, t_hdata *cur, int adjust)
{
  int len;
  u32_t ackno, seqno, unused_seqno;
  u16_t loc_wnd, dist_wnd;
  u8_t *data;
  if (clo_mngt_get_state(clo) == state_established)
    {
    clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &unused_seqno,
                                 &loc_wnd, &dist_wnd);
    if (!clo_mngt_get_txdata(clo, cur, &seqno, &len, &data))
      {
      if (clo->must_call_fin != 1)
        {
        util_send_data(&(clo->tcpid), ackno, seqno, loc_wnd, len, data);
        if (adjust)
          clo_mngt_adjust_send_next(clo, seqno, len);
        clo->ackno_sent = ackno;
        }
      else
        KERR("LLID%d CONNECTION %d %d %s", clo->tcpid.history_llid,
              clo->tcpid.local_port & 0xFFFF, clo->tcpid.remote_port & 0xFFFF,
              util_state2ascii(clo->state));
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int try_send_data2low(t_clo *clo)
{
  t_hdata *next, *cur = clo->head_hdata;
  int result = -1;
  if (cur->count_50ms == 0)
    {
    while (cur)
      {
      next = cur->next;
      if (!clo_mngt_authorised_to_send_nexttx(clo))
        {
        cur->count_stuck += 1;
        if (cur->count_stuck > 100)
          {
          KERR("LLID%d CONNECTION %d %d %s", clo->tcpid.history_llid,
                clo->tcpid.local_port & 0xFFFF, clo->tcpid.remote_port & 0xFFFF,
                util_state2ascii(clo->state));
          local_send_reset_state_closed(clo);
          }
        break;
        }
      cur->count_stuck = 0;
      local_send_data_to_low(clo, cur, 1);
      cur = next;
      }
    }
  else
    {
    while (cur)
      {
      next = cur->next;
      if (cur->count_50ms != 0)
        {
        if (!clo_mngt_authorised_to_send_nexttx(clo))
          {
          cur->count_stuck += 1;
          if (cur->count_stuck > 1000)
            {
            KERR("LLID%d CONNECTION %d %d %s", clo->tcpid.history_llid,
                                        clo->tcpid.local_port & 0xFFFF,
                                        clo->tcpid.remote_port & 0xFFFF,
                                        util_state2ascii(clo->state));
            local_send_reset_state_closed(clo);
            }
          break;
          }
        cur->count_stuck = 0;
        local_send_data_to_low(clo, cur, 0);
        }
      cur = next;
      }
    }
  if (clo->head_hdata == NULL)
    result = 0;
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_fct_call(t_all_ctx *all_ctx, void *param)
{
  t_timer_fct *fct = (t_timer_fct *) param;
  t_clo *clo = clo_mngt_find(&(fct->tcpid));
  int do_not_delete_param = 0;
  u32_t ackno, seqno;
  u16_t loc_wnd, dist_wnd;
  switch(fct->num_fct)
    {

    case num_fct_high_close_rx:
      fct_high_close_rx(&(fct->tcpid));
      break;

    case num_fct_high_close_rx_send_rst:
      fct_high_close_rx(&(fct->tcpid));
      if ((clo) && (clo->id_tcpid == fct->id_tcpid))
        {
        clo_mngt_get_ackno_seqno_wnd(clo,&ackno,&seqno,&loc_wnd,&dist_wnd);
        util_send_reset(&(clo->tcpid),ackno,seqno,loc_wnd,
                        __FUNCTION__,__LINE__);
        }
      break;

    case num_fct_send_finack:
      if ((clo) && (clo->id_tcpid == fct->id_tcpid))
        {
        if (clo->head_hdata)
          {
          do_not_delete_param = 1; 
          clownix_timeout_add(get_all_ctx(),5,timer_fct_call,param,NULL,NULL);
          }
        else
          {
          local_send_finack_state_fin(clo, __LINE__);
          }
        }
      break;
    default:
      KOUT(" ");
    }
  if (do_not_delete_param == 0)
    {
    free(fct->data);  
    free(fct);  
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void async_fct_call(int num_fct, int id, t_tcp_id *tcpid, 
                           int len, u8_t *data)
{
  t_timer_fct *fct; 
  fct = (t_timer_fct *) malloc(sizeof(t_timer_fct));
  memset(fct, 0, sizeof(t_timer_fct));
  fct->num_fct = num_fct;
  memcpy(&(fct->tcpid), tcpid, sizeof(t_tcp_id));
  fct->id_tcpid = id;
  fct->len = len;
  fct->data = data;
  if (num_fct == num_fct_send_finack)
    clownix_timeout_add(get_all_ctx(),1,timer_fct_call,(void *)fct,NULL,NULL);
  else if (num_fct == num_fct_high_close_rx)
    clownix_timeout_add(get_all_ctx(),3,timer_fct_call,(void *)fct,NULL,NULL);
  else if (num_fct == num_fct_high_close_rx_send_rst)
    clownix_timeout_add(get_all_ctx(),100,timer_fct_call,(void *)fct,NULL,NULL);
  else
    clownix_timeout_add(get_all_ctx(),1,timer_fct_call,(void *)fct,NULL,NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void break_of_com_kill_both_sides(t_clo *clo, int line)
{
  KERR("TRAP BREAK %d", line);
  async_fct_call(num_fct_high_close_rx_send_rst, 
                 clo->id_tcpid, &(clo->tcpid), 0, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_rx_data_purge(t_clo *clo)
{
  int len, res;
  u8_t *data;
  if (clo->head_ldata)
    {
    res = fct_high_data_rx_possible(&(clo->tcpid));
    if (res == 1)
      {
      while(clo->head_ldata)
        {
        if (clo_mngt_get_new_rxdata(clo, &len, &data))
          {
          KERR("LLID%d", clo->tcpid.history_llid);
          break;
          }
        else
          fct_high_data_rx(&(clo->tcpid), len, data);
        }
      }
    else
      {
      KERR("LLID%d %d", clo->tcpid.history_llid, res);
      while(clo->head_ldata)
        clo_mngt_get_new_rxdata(clo, &len, &data);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int  clo_high_data_tx_possible(t_tcp_id *tcpid)
{
  int result = 0;
  t_clo *clo = clo_mngt_find(tcpid);
  if (clo)
    {
    if (clo_mngt_authorised_to_send_nexttx(clo))
      result = 1;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int clo_high_data_tx(t_tcp_id *tcpid, int hlen, u8_t *hdata)
{
  int is_blkd,  state, llid, result = error_not_established;
  t_clo *clo = clo_mngt_find(tcpid);
  if (!clo)
    KOUT(" ");
  llid = clo->tcpid.llid;
  state = clo_mngt_get_state(clo);
  if ((state != state_established) && 
      (state != state_fin_wait_last_ack) && 
      (state != state_closed))
    {
    KERR("%d", state);
    }
  else
    {
    if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS) ||
        (!msg_exist_channel(get_all_ctx(), llid, &is_blkd, __FUNCTION__))) 
      KERR("%d", llid);
    else
      {
      result = error_none;
      clo_mngt_high_input(clo, hlen, hdata);
      if (state != state_closed)
        {
        try_send_data2low(clo);
        }
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int clo_high_syn_tx(t_tcp_id *tcpid)
{
  int result = -1;
  t_clo *clo;
  u32_t ackno, seqno;
  u16_t loc_wnd, dist_wnd;
  clo = clo_mngt_create_tcp(tcpid);
  if (clo)
    {
    clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
    util_send_syn(tcpid, ackno, seqno, loc_wnd);
    clo_mngt_adjust_send_next(clo, seqno, 1);
    clo_mngt_set_state(clo, state_first_syn_sent);
//    util_trace_tcpid(tcpid, (char *) __FUNCTION__);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void reset_rx_close_all(t_clo *clo)
{
  int is_blkd, llid;
  llid = clo->tcpid.llid;
  if (llid)
    {
    if ((llid <= 0) || (llid >= CLOWNIX_MAX_CHANNELS))
      KERR("%d", llid);
    if (msg_exist_channel(get_all_ctx(), llid, &is_blkd, __FUNCTION__))
      msg_delete_channel(get_all_ctx(), llid);
    if (clo->tcpid.llid)
      util_detach_llid_clo(clo->tcpid.llid, clo);
    }
  clo_mngt_set_state(clo, state_closed);
  util_purge_hdata(clo);
  util_purge_ldata(clo);
  init_closed_state_count_if_not_done(clo, 50, __LINE__);
  clo->reset_rx_close_all_done = 1;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_high_free_tcpid(t_tcp_id *tcpid)
{
  t_clo *clo;
  int state;
  clo = clo_mngt_find(tcpid);
  if (clo)
    {
    state = clo_mngt_get_state(clo);
    KERR("LLID%d %d %s", clo->tcpid.history_llid,
                         clo->has_been_closed_from_outside_socket,
                         util_state2ascii(state));
    if ((clo->head_hdata) && (clo->tcpid.llid_unlocked) &&
        ((state == state_established) || (state == state_fin_wait_last_ack)))
      {
      if (try_send_data2low(clo))
        KERR("LLID%d FAIL PURGE", clo->tcpid.history_llid);
      }
    clo_mngt_set_state(clo, state_closed);
    if (util_purge_hdata(clo))
      KERR("LLID%d", clo->tcpid.history_llid);
    if (util_purge_ldata(clo))
      KERR("LLID%d", clo->tcpid.history_llid);
    init_closed_state_count_if_not_done(clo, 50, __LINE__);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int clo_high_close_tx(t_tcp_id *tcpid)
{
  int state, result = 0;
  t_clo *clo;
  clo = clo_mngt_find(tcpid);
  if (clo == NULL)
    KERR(" ");
  else
    {
    state = clo_mngt_get_state(clo);
    if ((clo->head_hdata) && (clo->tcpid.llid_unlocked))
      {
      if (try_send_data2low(clo))
        {
        result = -1;
        clo->try_send_data2low += 1;
        if (clo->try_send_data2low > 3)
          init_closed_state_count_if_not_done(clo, 50, __LINE__);
        return result;
        }
      }
    switch(state)
      {
      case state_established:
      case state_first_syn_sent:
      case state_closed:
      case state_fin_wait_last_ack:
        if (clo->has_been_closed_from_outside_socket == 1)
          clo->must_call_fin = 1;
        if (util_purge_hdata(clo))
          {
          KERR("LLID%d FAIL PURGE out_close:%d %s", clo->tcpid.history_llid,
               clo->has_been_closed_from_outside_socket, util_state2ascii(state));
          result = -1;
          }
        if (util_purge_ldata(clo))
          {
          KERR("LLID%d FAIL PURGE out_close:%d %s", clo->tcpid.history_llid,
               clo->has_been_closed_from_outside_socket, util_state2ascii(state));
          result = -1;
          }
        init_closed_state_count_if_not_done(clo, 50, __LINE__);
        break;
      case state_created:
        KERR("LLID%d %d %s", clo->tcpid.history_llid,
                             clo->has_been_closed_from_outside_socket,
                             util_state2ascii(state));
        break;
      default:
        KOUT(" %d", state);
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timer_delayed_close(t_all_ctx *all_ctx, void *data)
{
  t_tcp_id *tcpid = (t_tcp_id *) data;
  t_clo *clo = clo_mngt_find(tcpid);
  if (!clo)
    {
    free(data);
    }
  else
    {
    if (!clo_high_close_tx(tcpid))
      free(data);
    else
      {
      clownix_timeout_add(get_all_ctx(), 100, timer_delayed_close,
                      (void *)data, NULL, NULL);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void  clo_delayed_high_close_tx(t_tcp_id *tcpid)
{
  t_tcp_id *tmp_tcpid = (t_tcp_id *) malloc(sizeof(t_tcp_id));
  memcpy(tmp_tcpid, tcpid, sizeof(t_tcp_id)); 
  clownix_timeout_add(get_all_ctx(), 100, timer_delayed_close, 
                      (void *)tmp_tcpid, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int  clo_high_synack_tx(t_tcp_id *tcpid)
{
  int result = -1;
  u32_t ackno, seqno;
  u16_t loc_wnd, dist_wnd;
  t_clo *clo = clo_mngt_find(tcpid);
  if (clo)
    {
    clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
    util_send_synack(tcpid, ackno, seqno, loc_wnd);
    clo_mngt_adjust_send_next(clo, seqno, 1);
    clo_mngt_set_state(clo, state_established);
//    util_trace_tcpid(tcpid, (char *) __FUNCTION__);
    result = 0;
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void non_existing_tcp_low_input(t_tcp_id *tcpid, t_low *low, 
                                       int *inserted)
{
  t_clo *clo;
  u32_t ackno, seqno;
  u16_t loc_wnd, dist_wnd;
  *inserted = 0;
  if (low->flags == TH_SYN)
    {
    clo = clo_mngt_create_tcp(tcpid);
    if (!clo)
      KOUT(" ");
    if (!clo_mngt_low_input(clo, low, inserted))
      {
      fct_high_syn_rx(&(clo->tcpid));
      }
    else
      {
      KERR("%d %d", tcpid->local_port & 0xFFFF, tcpid->remote_port & 0xFFFF);
      clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
      util_send_reset(tcpid, ackno, seqno, loc_wnd, __FUNCTION__, __LINE__);
      clo_mngt_set_state(clo, state_closed);
      init_closed_state_count_if_not_done(clo, 50, __LINE__);
      }
    }
  else if ((low->flags & (TH_SYN | TH_ACK)) == (TH_SYN | TH_ACK))
    {
    KERR("%d %d %X %d", tcpid->local_port & 0xFFFF,
                        tcpid->remote_port & 0xFFFF,
                        low->flags, low->tcplen);
    }
  else if ((low->flags & (TH_FIN | TH_ACK)) == (TH_FIN | TH_ACK))
    {
    KERR("%d %d %X %d", tcpid->local_port & 0xFFFF,
                        tcpid->remote_port & 0xFFFF,
                        low->flags, low->tcplen);
    ackno = low->seqno + low->tcplen+1;
    seqno = low->ackno;
    util_send_ack(tcpid, ackno, seqno, TCP_WND);
    }
  else if ((low->flags & TH_FIN) ==  TH_FIN)
    {
    KERR("%d %d %X %d", tcpid->local_port & 0xFFFF,
                        tcpid->remote_port & 0xFFFF,
                        low->flags, low->tcplen);
    ackno = low->seqno + low->tcplen+1;
    seqno = low->ackno;
    util_send_reset(tcpid, ackno, seqno, TCP_WND, __FUNCTION__, __LINE__);
    }
  else
    {
    KERR("%d %d %X %d", tcpid->local_port & 0xFFFF,
                        tcpid->remote_port & 0xFFFF,
                        low->flags, low->tcplen);
    ackno = low->seqno + low->tcplen;
    seqno = low->ackno;
    util_send_reset(tcpid, ackno, seqno, TCP_WND, __FUNCTION__, __LINE__);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int existing_tcp_low_input(t_clo *clo, t_low *low)
{
  u32_t ackno, seqno;
  u16_t loc_wnd, dist_wnd;
  int result = -1;
  int state = clo_mngt_get_state(clo);
  switch (state)
    {
    case state_created:
      if (low->flags == TH_SYN)
        {
        fct_high_syn_rx(&(clo->tcpid));
        result = 0;
        }
      else
        {
        KERR("%p", clo);
        clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
        util_send_reset(&(clo->tcpid),ackno,seqno,loc_wnd,
                        __FUNCTION__,__LINE__);
        clo_mngt_set_state(clo, state_closed);
        init_closed_state_count_if_not_done(clo, 50, __LINE__);
        }
      break;
    case state_first_syn_sent:
      if ((low->flags & (TH_SYN | TH_ACK)) == (TH_SYN | TH_ACK))
        {
        clo_mngt_set_state(clo, state_established);
        fct_high_synack_rx(&(clo->tcpid));
        result = 0;
        }
      else
        {
        KERR("%p", clo);
        clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
        util_send_reset(&(clo->tcpid), ackno, seqno, loc_wnd,
                        __FUNCTION__,__LINE__);
        async_fct_call(num_fct_high_close_rx, clo->id_tcpid, &(clo->tcpid),
                       0, NULL);
        clo_mngt_set_state(clo, state_closed);
        init_closed_state_count_if_not_done(clo, 50, __LINE__);
        }
      break;

    case state_established:
      if ((low->flags & TH_FIN) ==  TH_FIN)
        {
        clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
        clo_mngt_adjust_recv_next(clo, ackno, 1);
        clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
        if (clo->has_been_closed_from_outside_socket == 1)
          {
          ackno_send(clo);
          clo_mngt_get_ackno_seqno_wnd(clo,&ackno,&seqno,&loc_wnd,&dist_wnd);
          ackno_send(clo);
          clo_mngt_set_state(clo, state_fin_wait_last_ack);
          init_closed_state_count_if_not_done(clo, 50, __LINE__);
          util_purge_hdata(clo);
          }
        else
          {
          util_purge_hdata(clo);
          local_rx_data_purge(clo);
          clo->fin_use_stored_vals = 1;
          clo->fin_ackno = ackno;
          clo->fin_seqno = seqno;
          local_send_finack_state_fin(clo, __LINE__);
          }
        }
      result = 0;
      break;

    case state_fin_wait_last_ack:
        if ((low->flags & TH_ACK) ==  TH_ACK)
          {
          init_closed_state_count_if_not_done(clo, 50, __LINE__);
          }
        else
          KERR("LLID%d %X", clo->tcpid.history_llid, low->flags);
      result = 0;
      break;

    case state_closed:
      break;

    default:
      KOUT(" %d", state);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_low_input(int mac_len, u8_t *mac_data)
{
  int res, len, inserted = 0;
  u8_t *data;
  t_tcp_id tcpid;
  t_low *low;
  t_clo *clo;
  memset(&tcpid, 0, sizeof(t_tcp_id));
  if (!tcp_packet2low(mac_len, mac_data, &low))
    {
    util_low2tcpid(&tcpid, low);
//    util_trace_tcpid(&tcpid, (char *) __FUNCTION__);
    clo = clo_mngt_find(&tcpid);
    if (!clo)
      {
      non_existing_tcp_low_input(&tcpid, low, &inserted);
      }
    else
      {
      clo->non_activ_count_rx = 0;
      if ((low->flags & TH_RST) ==  TH_RST)
        {
        if (clo->reset_rx_close_all_done == 0)
          reset_rx_close_all(clo);
        }
      else if (!clo_mngt_low_input(clo, low, &inserted))
        {
        if (existing_tcp_low_input(clo, low))
          KERR("LLID%d", clo->tcpid.history_llid);
        else if (clo->tcpid.llid_unlocked)
          {
          if (inserted)
            {
            clo_mngt_adjust_loc_wnd(clo, TCP_WND);
            if (!clo_mngt_get_new_rxdata(clo, &len, &data))
              {
              res = fct_high_data_rx_possible(&(clo->tcpid));
              if (res == 1)
                fct_high_data_rx(&(clo->tcpid), len, data);
              else if (res == -1)
                clo_delayed_high_close_tx(&(clo->tcpid));
              else if (res != -2)
                KERR("LLID%d %d", clo->tcpid.history_llid, res);
              }
            }
          }
        }
      else
        KERR("LLID%d", clo->tcpid.history_llid);
      }
    if (!inserted)
      {
      free(low->data);
      free(low);
      }
    }
  else
    {
    KERR("BIG BUG");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_low_output(int mac_len, u8_t *mac_data)
{
  fct_low_output(mac_len, mac_data);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void receive_all_local_data(void)
{
  int len, res;
  u8_t *data;
  t_clo *cur = get_head_clo();
  while (cur)
    {
    while (cur->head_ldata)
      {
      if (!clo_mngt_get_new_rxdata(cur, &len, &data))
        {
        res = fct_high_data_rx_possible(&(cur->tcpid));
        if (res == 1)
          fct_high_data_rx(&(cur->tcpid), len, data);
        else
          KERR("LLID%d %d", cur->tcpid.history_llid, res);
        }
      else
        KERR("LLID%d", cur->tcpid.history_llid);
      }
    cur = cur->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_all_data_to_send(void)
{
  int state;
  t_clo *cur = get_head_clo();
  while (cur)
    {
    state = clo_mngt_get_state(cur);
    if ((cur->head_hdata) && (cur->tcpid.llid_unlocked))
      {
      if (state == state_established)
        {
        ackno_send(cur);
        try_send_data2low(cur);
        }
      else
        {
        util_purge_hdata(cur);
        }
      }
    if(cur->must_call_fin == 1)
      {
      cur->must_call_fin = 0;
      if (cur->fin_ack_has_been_sent == 0)
        async_fct_call(num_fct_send_finack, cur->id_tcpid, &(cur->tcpid), 0, NULL);
      }
    cur = cur->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void clo_clean_closed(void)
{
  t_clo *next, *cur = get_head_clo();
  t_clo tmp;
  while (cur)
    {
    next = cur->next;
    if (cur->closed_state_count)
      {
      cur->closed_state_count -= 1;
      if (cur->closed_state_count == 0) 
        {
        if (cur->head_hdata)
          KERR("LLID%d %d %d %s", cur->tcpid.history_llid,
                                  cur->tcpid.local_port & 0xFFFF,
                                  cur->tcpid.remote_port & 0xFFFF,
                                  util_state2ascii(cur->state));
        if (cur->head_ldata)
          KERR("LLID%d %d %d %s", cur->tcpid.history_llid,
                                  cur->tcpid.local_port & 0xFFFF,
                                  cur->tcpid.remote_port & 0xFFFF,
                                  util_state2ascii(cur->state));
        memcpy(&tmp, cur, sizeof(t_clo));
        if (clo_mngt_delete_tcp(cur))
          {
          async_fct_call(num_fct_high_close_rx_send_rst, 
                         tmp.id_tcpid, &(tmp.tcpid), 0, NULL);
          }
        else
          {
          async_fct_call(num_fct_high_close_rx,
                         tmp.id_tcpid, &(tmp.tcpid), 0, NULL);
          }
        }
      }
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_wnd_modif(void)
{
  int res;
  t_clo *clo = get_head_clo();
  while (clo)
    {  
    if (clo->loc_wnd == 0)
      {
      res = fct_high_data_rx_possible(&(clo->tcpid));
      if (res == 1)
        clo_mngt_adjust_loc_wnd(clo, TCP_WND);
      else  
        {
        KERR("LLID%d %d", clo->tcpid.history_llid, res);
        break_of_com_kill_both_sides(clo, __LINE__);
        }
      }
    if (clo_mngt_get_state(clo) == state_established)
      {
      ackno_send(clo);
      }
    clo = clo->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void non_activ_count_inc(void)
{
  t_clo *next, *cur = get_head_clo();
  while (cur)
    {
    next = cur->next;
    cur->non_activ_count_rx += 1;
    cur->non_activ_count_tx += 1;
    if (clo_mngt_get_state(cur) == state_first_syn_sent)
      cur->syn_sent_non_activ_count += 1;
    else
      cur->syn_sent_non_activ_count = 0;
    if (cur->non_activ_count_rx == 1000) 
      {
      if (cur->state != state_established)
        {
        KERR("LLID%d %d %d %s", cur->tcpid.history_llid,
                              cur->tcpid.local_port & 0xFFFF,
                              cur->tcpid.remote_port & 0xFFFF,
                              util_state2ascii(cur->state));
        local_send_reset_state_closed(cur);
        }
      else
        KERR("LLID%d %d %d %s", cur->tcpid.history_llid,
                              cur->tcpid.local_port & 0xFFFF,
                              cur->tcpid.remote_port & 0xFFFF,
                              util_state2ascii(cur->state));
      }
    if (cur->non_activ_count_tx == 1000)
      {
      if (cur->state != state_established)
        {
        KERR("LLID%d %d %d %s", cur->tcpid.history_llid,
                              cur->tcpid.local_port & 0xFFFF,
                              cur->tcpid.remote_port & 0xFFFF,
                              util_state2ascii(cur->state));
        local_send_reset_state_closed(cur);
        }
      else
        KERR("LLID%d %d %d %s", cur->tcpid.history_llid,
                              cur->tcpid.local_port & 0xFFFF,
                              cur->tcpid.remote_port & 0xFFFF,
                              util_state2ascii(cur->state));
      }
    if (cur->non_activ_count_rx == 2000)
      {
      KERR("LLID%d %d %d %s", cur->tcpid.history_llid,
                            cur->tcpid.local_port & 0xFFFF,
                            cur->tcpid.remote_port & 0xFFFF,
                            util_state2ascii(cur->state));
      local_send_reset_state_closed(cur);
      }

    if (cur->non_activ_count_tx == 2000)
      {
      KERR("LLID%d %d %d %s", cur->tcpid.history_llid,
                            cur->tcpid.local_port & 0xFFFF,
                            cur->tcpid.remote_port & 0xFFFF,
                            util_state2ascii(cur->state));
      local_send_reset_state_closed(cur);
      }
    if (cur->syn_sent_non_activ_count > 500)
      {
      KERR("LLID%d %d %d %s", cur->tcpid.history_llid,
                            cur->tcpid.local_port & 0xFFFF,
                            cur->tcpid.remote_port & 0xFFFF,
                            util_state2ascii(cur->state));
      local_send_reset_state_closed(cur);
      }
    if (cur->tx_repeat_failure_count > 20)
      {
      KERR("LLID%d %d %d %s", cur->tcpid.history_llid,
                            cur->tcpid.local_port & 0xFFFF,
                            cur->tcpid.remote_port & 0xFFFF,
                            util_state2ascii(cur->state));
      local_send_reset_state_closed(cur);
      }
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_heartbeat_timer(void)
{
  clo_mngt_timer();
  send_wnd_modif();
  receive_all_local_data();
  send_all_data_to_send();
  non_activ_count_inc();
  clo_clean_closed();
}
/*---------------------------------------------------------------------------*/



/*****************************************************************************/
t_all_ctx *get_all_ctx(void)
{
  return g_all_ctx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_send_reset_state_closed(t_clo *clo)
{
  local_send_reset_state_closed(clo);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_init(t_all_ctx *all_ctx,
              t_low_output low_output,
              t_high_data_rx_possible high_data_rx_possible,
              t_high_data_rx high_data_rx,
              t_high_syn_rx high_syn_rx,
              t_high_synack_rx high_synack_rx,
              t_high_close_rx high_close_rx)
{
  g_all_ctx = all_ctx;
  fct_low_output     =  low_output;
  fct_high_data_rx_possible = high_data_rx_possible;
  fct_high_data_rx   =  high_data_rx;
  fct_high_syn_rx    =  high_syn_rx;
  fct_high_synack_rx =  high_synack_rx;
  fct_high_close_rx  =  high_close_rx;
  clo_mngt_init();
  util_init();
}
/*---------------------------------------------------------------------------*/

