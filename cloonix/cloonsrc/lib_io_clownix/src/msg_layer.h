/* Copyright (C) 2006-2026 clownix@clownix.net  SPDX-License-Identifier: AGPL-3.0-or-later */
typedef struct t_data_chunk
  {
  struct t_data_chunk *next;
  char *chunk;
  int len;
  int len_done;
  int start_len_done;
  } t_data_chunk;
typedef struct t_data_channel
  {
  t_data_chunk *rx;
  t_data_chunk *tx;
  t_data_chunk *last_tx;
  t_data_chunk *rx_bound_start;
  char *boundary;
  uint32_t total_length;
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

enum
{
  rx_type_watch = 421,
  rx_type_proxy_traf_tcp_start,
  rx_type_proxy_traf_unix_start,
  rx_type_proxy_sig_start,
  rx_type_proxy_sig_header_ok,
  rx_type_listen,
  rx_type_ascii_start,
  rx_type_open_bound_found,
  rx_type_doorways,
} ;

t_data_channel *get_dchan(int cidx);
long long  *get_peak_queue_len(int cidx);
void new_rx_to_process(t_data_channel *dchan, int len, char *new_rx);
int tx_try_send_chunk(t_data_channel *dchan, int cidx, 
                      int *correct_send, t_fd_error cb);
void err_dchan_cb(int llid, int err, int from);
void chunk_chain_delete(t_data_channel *dchan);
int get_tot_txq_size(int cidx);

