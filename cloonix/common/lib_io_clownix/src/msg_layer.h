/*****************************************************************************/
/*    Copyright (C) 2006-2024 clownix@clownix.net License AGPL-3             */
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
typedef struct t_data_chunk
  {
  struct t_data_chunk *next;
  char *chunk;
  int len;
  int len_done;
  int start_len_done;
  } t_data_chunk;
/*---------------------------------------------------------------------------*/
typedef struct t_data_channel
  {
  t_data_chunk *rx;
  t_data_chunk *tx;
  t_data_chunk *last_tx;
  t_data_chunk *rx_bound_start;
  char *boundary;
  int i_bound_start;
  int llid;
  int fd;
  int is_tcp;
  int  decoding_state;
  unsigned long tot_txq_size;
  t_fd_connect server_connect_callback;
  t_fd_error error_callback;
  t_msg_rx_cb rx_callback;
  } t_data_channel;
/*---------------------------------------------------------------------------*/

typedef enum
{
  rx_type_watch = 421,
  rx_type_listen,
  rx_type_ascii_start,
  rx_type_open_bound_found,
  rx_type_doorways,
} t_rx_type;

t_data_channel *get_dchan(int cidx);
long long  *get_peak_queue_len(int cidx);
void new_rx_to_process(t_data_channel *dchan, int len, char *new_rx);
int tx_try_send_chunk(t_data_channel *dchan, int cidx, 
                      int *correct_send, t_fd_error cb);
void err_dchan_cb(int llid, int err, int from);
void chunk_chain_delete(t_data_channel *dchan);
int get_tot_txq_size(int cidx);
/*---------------------------------------------------------------------------*/








