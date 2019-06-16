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
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/un.h>


#include "ioc.h"
#include "clo_tcp.h"
#include "sock_fd.h"
#include "main.h"
#include "machine.h"
#include "utils.h"
#include "bootp_input.h"
#include "packets_io.h"
#include "llid_slirptux.h"
#include "tcp_tux.h"
#include "unix2inet.h"


void packet_output_to_slirptux(int len, char *data);


/*****************************************************************************/
void llid_clo_high_close_rx(t_tcp_id *tcpid)
{
  t_clo *clo;
  unix2inet_close_tcpid(tcpid);
  clo = util_get_fast_clo(tcpid);

  if (clo)
    {
    clo_high_close_tx(&(clo->tcpid), 0);
    }
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void llid_slirptux_tcp_close_llid(int llid)
{
  t_clo *clo = util_get_clo(llid);
  if (clo)
    {
    clo->has_been_closed_from_outside_socket = 1;
    if (clo_high_close_tx(&(clo->tcpid), 0))
      KOUT(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_slirptux_tcp_rx_from_slirptux(int mac_len, char *mac_data)
{
  clo_low_input(mac_len, (u8_t *) mac_data);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_slirptux_tcp_rx_from_llid(int llid, int len, char *rx_buf)
{
  int result;
  t_clo *clo;
  clo = util_get_clo(llid);
  if (clo)
    {
    result = clo_high_data_tx(&(clo->tcpid), len, (u8_t *) rx_buf);
    switch (result)
      {
      case error_none:
        break;
      case error_not_authorized:
        KOUT(" %d", result);
        break;
      case error_not_established:
        KERR(" %d", result);
        break;
      default:
        KOUT(" %d", result);
      }
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int llid_slirptux_tcp_tx_to_slirptux_possible(int llid)
{
  int result = 0;
  t_clo *clo = util_get_clo(llid);
  if (clo)
    result =  clo_high_data_tx_possible(&(clo->tcpid));
  return result;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_clo_low_output(int mac_len, u8_t *mac_data)
{
  packet_output_to_slirptux(mac_len, (char *) mac_data);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
int llid_clo_data_rx_possible(t_tcp_id *tcpid)
{
  int is_blkd, result = 0;
  if ((tcpid->llid == 0) && (tcpid->llid_unlocked == 0))
    KERR("Time chaos, llid not yet established, in-between events!");
  else
    {
    if (msg_exist_channel(get_all_ctx(), tcpid->llid, &is_blkd, __FUNCTION__))
      {
      if ((tcpid->llid_unlocked) &&
          (msg_mngt_get_tx_queue_len(get_all_ctx(), tcpid->llid) < 20000))
        result = 1; 
      else
        KERR("%d", msg_mngt_get_tx_queue_len(get_all_ctx(), tcpid->llid));
      }
    else
      {
      result = -1;
      }
    }
  return result;
}
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
void llid_clo_data_rx(t_tcp_id *tcpid, int len, u8_t *data)
{
  int is_blkd;
  if (msg_exist_channel(get_all_ctx(), tcpid->llid, &is_blkd, __FUNCTION__))
    {
    data_tx(get_all_ctx(), tcpid->llid, len, (char *)data);
    }
  else
    KERR("%d ", len);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void callback_connect(t_tcp_id *tcpid, int llid, int status)
{
  t_clo *clo;
  if (status)
    {
    llid_clo_high_close_rx(tcpid);
    }
  else if (clo_high_synack_tx(tcpid))
    {
    llid_clo_high_close_rx(tcpid);
    KERR("  ");
    }
  else
    {
    clo = util_get_fast_clo(tcpid);
    if (clo)
      {
      util_attach_llid_clo(llid, clo);
      clo->tcpid.llid_unlocked = 1;
      }
    else
      KERR(" ");
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_clo_high_syn_rx(t_tcp_id *tcpid)
{
  tcp_tux_socket_create_and_connect_to_tcp(callback_connect, tcpid);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_clo_high_synack_rx(t_tcp_id *tcpid)
{
  t_clo *clo;
  clo = util_get_fast_clo(tcpid);
  if (!clo)
    KERR(" ");
  else if (unix2inet_ssh_syn_ack_arrival(tcpid))
    {
    KERR(" ");
    llid_clo_high_close_rx(tcpid);
    }
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_rpct_heartbeat(t_all_ctx *all_ctx, void *data)
{
  rpct_heartbeat(all_ctx);
  clownix_timeout_add(all_ctx, 100, timeout_rpct_heartbeat, 
                      NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void timeout_blkd_heartbeat(t_all_ctx *all_ctx, void *data)
{
  blkd_heartbeat(all_ctx);
  clownix_timeout_add(all_ctx, 5, timeout_blkd_heartbeat, 
                      NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void beat_50ms_timeout(t_all_ctx *all_ctx, void *data)
{
  clo_heartbeat_timer();
  clownix_timeout_add(all_ctx, 5, beat_50ms_timeout, 
                      NULL, NULL, NULL);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void llid_slirptux_tcp_init(void)
{
  clownix_timeout_add(get_all_ctx(), 5, beat_50ms_timeout, 
                      NULL, NULL, NULL);
  clownix_timeout_add(get_all_ctx(), 100, timeout_rpct_heartbeat, 
                      NULL, NULL, NULL);
  clownix_timeout_add(get_all_ctx(), 100, timeout_blkd_heartbeat, 
                      NULL, NULL, NULL);
}
/*---------------------------------------------------------------------------*/
