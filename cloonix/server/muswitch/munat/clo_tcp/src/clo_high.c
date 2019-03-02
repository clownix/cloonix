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
static void ackno_send(t_clo *clo, int force)
{
  u32_t ackno, seqno;
  u16_t loc_wnd, dist_wnd;
  clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
  if ((force) || (ackno != clo->ackno_sent))
    {
    util_send_ack(&(clo->tcpid), ackno, seqno, loc_wnd);
    clo->ackno_sent = ackno;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_ack_cb(long delta_ns, void *data)
{
  t_tcp_id *time_tcpid = (t_tcp_id *) data;
  t_clo *clo = util_get_fast_clo(time_tcpid);
  if (clo)
    {
    ackno_send(clo, 0);
    clo->send_ack_active = 0;
    }
  free(data);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int timed_send_ack(t_clo *clo)
{
  int result = 0;
  t_tcp_id *time_tcpid;
  if (clo->send_ack_active == 0)
    {
    result = 1;
    time_tcpid = (t_tcp_id *) malloc(sizeof(t_tcp_id));
    memcpy(time_tcpid, &(clo->tcpid), sizeof(t_tcp_id));
    clo->send_ack_active = 1;
    clownix_real_timer_add(0, 1, send_ack_cb, (void *) time_tcpid, NULL);
    }
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_send_finack_state_fin(t_clo *clo)
{
  u32_t ackno, seqno;
  u16_t loc_wnd, dist_wnd;
  if (clo->has_been_closed_from_outside_socket == 1)
    {
    clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
    util_send_finack(&(clo->tcpid), ackno, seqno, loc_wnd);
    clo_mngt_adjust_send_next(clo, seqno, 1);
    clo_mngt_set_state(clo, state_fin_wait1);
    }
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
  init_closed_state_count_if_not_done(clo, 20, __LINE__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void local_send_data_to_low(t_clo *clo, t_hdata *cur, int adjust)
{
  int len;
  u32_t ackno, seqno, unused_seqno;
  u16_t loc_wnd, dist_wnd;
  u8_t *data;
  if(clo_mngt_get_state(clo) == state_established)
    {
    clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &unused_seqno,
                                 &loc_wnd, &dist_wnd);
    clo_mngt_get_txdata(clo, cur, &seqno, &len, &data);
    util_send_data(&(clo->tcpid), ackno, seqno, loc_wnd, len, data);
    if (adjust)
      clo_mngt_adjust_send_next(clo, seqno, len);
    clo->ackno_sent = ackno;
    if (clo->have_to_ack > 0)
      clo->have_to_ack -= 1;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void try_send_data2low_timer(t_clo *clo)
{
  u32_t current_250ms = get_g_250ms_count();
  t_hdata *cur = clo->head_hdata;
  while (cur)
    {
    if ((cur->count_250ms) &&
        (current_250ms - cur->count_250ms >= 2))
      {
      local_send_data_to_low(clo, cur, 0);
      }
    else if ((!cur->count_250ms) &&
             (clo_mngt_authorised_to_send_nexttx(clo)))
      {
      local_send_data_to_low(clo, cur, 1);
      }
    cur = cur->next;
    }
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
      else
        KERR(" ");
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
          local_send_finack_state_fin(clo);
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
    clownix_timeout_add(get_all_ctx(),5,timer_fct_call,(void *)fct,NULL,NULL);
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
  t_ldata *next, *cur = clo->head_ldata;
  res = fct_high_data_rx_possible(&(clo->tcpid));
  if (res == 1)
    {
    if (!clo_mngt_get_new_rxdata(clo, &len, &data))
      {
      fct_high_data_rx(&(clo->tcpid), len, data);
      }
    }
  cur = clo->head_ldata;
  if (res != -1)
    {
    if (cur)
      KERR(" TOLOOKINTO %d", res);
    }
  while (cur)
    {
    next = cur->next;
    util_extract_ldata(&(clo->head_ldata), cur);
    cur = next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void try_send_data2low(t_clo *clo)
{
  t_hdata *cur = clo->head_hdata;
  while (cur)
    {
    if (!cur->count_250ms) 
      {
      if (!clo_mngt_authorised_to_send_nexttx(clo))
        {
        break;
        }
      local_send_data_to_low(clo, cur, 1);
      }
    else
      {
      if (!clo_mngt_authorised_to_send_nexttx(clo))
        {
        KERR(" ");
        break;
        }
      }
    cur = cur->next;
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
  int result = error_not_established;
  t_clo *clo = clo_mngt_find(tcpid);
  int state;
  if (!clo)
    KOUT(" ");
  state = clo_mngt_get_state(clo);
  if ((state != state_established) && 
      (state != state_fin_wait1) && 
      (state != state_fin_wait_last_ack) && 
      (state != state_fin_wait2) &&
      (state != state_closed))
    {
    KERR("%d", state);
    }
  else
    {
    result = error_none;
    clo_mngt_high_input(clo, hlen, hdata);
    if (state != state_closed)
      {
      if (!clo_mngt_authorised_to_send_nexttx(clo))
        {
        result = error_not_authorized;
        }
      else
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
  clo = clo_mngt_find(tcpid);
  if (!clo)
    {
    clo = clo_mngt_create_tcp(tcpid);
    clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
    util_send_syn(tcpid, ackno, seqno, loc_wnd);
    clo_mngt_adjust_send_next(clo, seqno, 1);
    clo_mngt_set_state(clo, state_first_syn_sent);
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
  if (msg_exist_channel(get_all_ctx(), llid, &is_blkd, __FUNCTION__))
    msg_delete_channel(get_all_ctx(), llid);
  if (clo->tcpid.llid)
    util_detach_llid_clo(clo->tcpid.llid, clo);
  clo_mngt_set_state(clo, state_closed);
  util_silent_purge_hdata_and_ldata(clo);
  init_closed_state_count_if_not_done(clo, 1, __LINE__);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void clo_high_free_tcpid(t_tcp_id *tcpid)
{
  t_clo *clo;
  clo = clo_mngt_find(tcpid);
  if (clo)
    {
    clo_mngt_set_state(clo, state_closed);
    util_silent_purge_hdata_and_ldata(clo);
    init_closed_state_count_if_not_done(clo, 1, __LINE__);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int clo_high_close_tx(t_tcp_id *tcpid)
{
  int is_blkd, llid, state, result = -1;
  t_clo *clo;
  clo = clo_mngt_find(tcpid);
  if (clo)
    {
    result = 0;
    llid = clo->tcpid.llid;
    if (msg_exist_channel(get_all_ctx(), llid, &is_blkd, __FUNCTION__))
      msg_delete_channel(get_all_ctx(), llid);
    if (clo->tcpid.llid)
      util_detach_llid_clo(clo->tcpid.llid, clo);
    local_rx_data_purge(clo);
    state = clo_mngt_get_state(clo);
    switch(state)
      {
      case state_established:
      case state_first_syn_sent:
        if (clo->has_been_closed_from_outside_socket == 1)
          {
          async_fct_call(num_fct_send_finack, clo->id_tcpid, &(clo->tcpid), 
                         0, NULL);
          }
        else
          local_send_finack_state_fin(clo);
        break;
      case state_created:
        local_send_reset_state_closed(clo);
        break;
      case state_fin_wait1:
      case state_fin_wait2:
        init_closed_state_count_if_not_done(clo, 20, __LINE__);
        break;
      case state_closed:
      case state_fin_wait_last_ack:
        clo_mngt_set_state(clo, state_closed);
        util_silent_purge_hdata_and_ldata(clo);
        init_closed_state_count_if_not_done(clo, 4, __LINE__);
        break;
      default:
        KOUT(" %d", state);
      }
    }
  return result;
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
    if (!clo_mngt_low_input(clo, low, inserted))
      fct_high_syn_rx(&(clo->tcpid));
    else
      {
      KERR(" ");
      clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
      util_send_reset(tcpid, ackno, seqno, loc_wnd, __FUNCTION__, __LINE__);
      clo_mngt_set_state(clo, state_closed);
      init_closed_state_count_if_not_done(clo, 20, __LINE__);
      }
    }
  else
    {
    if ((!(low->flags & TH_RST)) && (low->tcplen != 0))
      {
      if ((!(low->flags & TH_FIN)) && (low->tcplen != 1))
        {
        KERR("%X  %d ", low->flags, low->tcplen);
        if ((low->flags & TH_FIN) ==  TH_FIN)
          ackno = low->seqno + low->tcplen+1;
        else
          ackno = low->seqno + low->tcplen;
        seqno = low->ackno;
        util_send_reset(tcpid, ackno, seqno, TCP_WND, __FUNCTION__, __LINE__);
        }
      }
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
        init_closed_state_count_if_not_done(clo, 20, __LINE__);
        }
      break;
    case state_first_syn_sent:
      if ((low->flags & (TH_SYN | TH_ACK)) == (TH_SYN | TH_ACK))
        {
        clo_mngt_set_state(clo, state_established);
        fct_high_synack_rx(&(clo->tcpid));
//        async_fct_call(num_fct_high_synack_rx, clo->id_tcpid, &(clo->tcpid), 
//                       0, NULL);
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
        init_closed_state_count_if_not_done(clo, 20, __LINE__);
        }
      break;

    case state_established:
      if ((low->flags & TH_FIN) ==  TH_FIN)
        {
        local_rx_data_purge(clo);
        if (clo->has_been_closed_from_outside_socket == 1)
          clo->has_been_closed_from_outside_socket = 0; 
        clo_mngt_get_ackno_seqno_wnd(clo, &ackno, &seqno, &loc_wnd, &dist_wnd);
        util_send_finack(&(clo->tcpid), ackno, seqno, loc_wnd);
        clo_mngt_adjust_send_next(clo, seqno, 1);
        clo_mngt_set_state(clo, state_fin_wait_last_ack);
        }
      result = 0;
      break;

    case state_fin_wait1:

      if ((low->flags & TH_ACK) ==  TH_ACK)
        {
        clo_mngt_set_state(clo, state_fin_wait2);
        if ((low->flags & TH_FIN) ==  TH_FIN)
          {
          if (clo->head_hdata)
            KERR("%p", clo);
          ackno_send(clo, 1);
          async_fct_call(num_fct_high_close_rx, clo->id_tcpid, &(clo->tcpid),
                         0, NULL);
          clo_mngt_set_state(clo, state_closed);
          }
        }
      break;

    case state_fin_wait2:

      if ((low->flags & TH_FIN) ==  TH_FIN)
        {
        if (clo->head_hdata)
          KERR("%p", clo);
        ackno_send(clo, 1);
        async_fct_call(num_fct_high_close_rx, clo->id_tcpid, &(clo->tcpid), 
                       0, NULL);
        clo_mngt_set_state(clo, state_closed);
        }
      break;

    case state_fin_wait_last_ack:
        if ((low->flags & TH_ACK) ==  TH_ACK)
          {
          async_fct_call(num_fct_high_close_rx, clo->id_tcpid, &(clo->tcpid), 
                         0, NULL);
          }
        clo_mngt_set_state(clo, state_closed);
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
  int res, change = 0, len, inserted = 0;
  u8_t *data;
  t_tcp_id tcpid;
  t_low *low;
  t_clo *clo;
  memset(&tcpid, 0, sizeof(t_tcp_id));
  if (!tcp_packet2low(mac_len, mac_data, &low))
    {
    util_low2tcpid(&tcpid, low);
    clo = clo_mngt_find(&tcpid);
    if (!clo)
      {
      non_existing_tcp_low_input(&tcpid, low, &inserted);
      }
    else
      {
      if ((low->flags & TH_RST) ==  TH_RST)
        {
        reset_rx_close_all(clo);
        }
      else if (!clo_mngt_low_input(clo, low, &inserted))
        {
        if (!existing_tcp_low_input(clo, low))
          {
          if (inserted)
            {
            res = fct_high_data_rx_possible(&(clo->tcpid));
            if (res == -1)
              break_of_com_kill_both_sides(clo, __LINE__);
            if (res == 1)
              {
              change = clo_mngt_adjust_loc_wnd(clo, TCP_WND);
              if (!clo_mngt_get_new_rxdata(clo, &len, &data))
                {
                fct_high_data_rx(&(clo->tcpid), len, data);
                }
              }
            else if (res == 0)
              {
              if (clo->tcpid.llid_unlocked)
                change = clo_mngt_adjust_loc_wnd(clo, TCP_WND_MIN);
              }
            }
          if ((change || clo->have_to_ack) &&
              (clo_mngt_get_state(clo) == state_established))
            {
            if (timed_send_ack(clo))
              {
              if (clo->have_to_ack > 0)
                clo->have_to_ack -= 1;
              }
            }
          }
        }
      else
        KERR("%p", clo);
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
    if (cur->head_ldata)
      {
      res = fct_high_data_rx_possible(&(cur->tcpid));
      if (res == -1)
        break_of_com_kill_both_sides(cur, __LINE__);
      if (res == 1)
        {
        if (!clo_mngt_get_new_rxdata(cur, &len, &data))
          {
          fct_high_data_rx(&(cur->tcpid), len, data);
          }
        }
      }
    cur = cur->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void send_all_data_to_send(void)
{
  t_clo *cur = get_head_clo();
  while (cur)
    {
    if (cur->head_hdata) 
      try_send_data2low_timer(cur);
    cur = cur->next;
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void clo_clean_closed(void)
{
  t_clo *next, *cur = get_head_clo();
  while (cur)
    {
    next = cur->next;
    if (cur->closed_state_count)
      {
      cur->closed_state_count -= 1;
      if (cur->closed_state_count == 0) 
        {
        if (clo_mngt_delete_tcp(cur))
          {
          async_fct_call(num_fct_high_close_rx_send_rst, 
                         cur->id_tcpid, &(cur->tcpid), 0, NULL);
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
  int res, change = 0;
  t_clo *clo = get_head_clo();
  while (clo)
    {  
    if (clo->loc_wnd == 0)
      {
      res = fct_high_data_rx_possible(&(clo->tcpid));
      if (res == -1)
        break_of_com_kill_both_sides(clo, __LINE__);
      if (res == 1)
        change = clo_mngt_adjust_loc_wnd(clo, TCP_WND);
      else  
        KERR("%d", clo_mngt_get_state(clo));
      }
    if ((change || clo->have_to_ack) &&
        (clo_mngt_get_state(clo) == state_established))
      {
      ackno_send(clo, 1);
      if (clo->have_to_ack > 0)
        clo->have_to_ack -= 1;
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
    cur->non_activ_count += 1;
    if (clo_mngt_get_state(cur) == state_first_syn_sent)
      cur->syn_sent_non_activ_count += 1;
    else
      cur->syn_sent_non_activ_count = 0;
    if (cur->non_activ_count == 400) 
      {
      KERR("TOLOOKINTO Connection lasting more than 100 sec");
      KERR("%X %X %d %d", cur->tcpid.local_ip, cur->tcpid.remote_ip,
                          cur->tcpid.local_port, cur->tcpid.remote_port);
      }
    if (cur->non_activ_count > 500000) 
      {
      KERR(" ");
      clo_high_close_tx(&(cur->tcpid));
      }
    if (cur->syn_sent_non_activ_count > 10)
      {
      KERR(" ");
      clo_high_close_tx(&(cur->tcpid));
      }
    if (cur->tx_repeat_failure_count > 10)
      {
      KERR(" ");
      clo_high_close_tx(&(cur->tcpid));
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
}
/*---------------------------------------------------------------------------*/

