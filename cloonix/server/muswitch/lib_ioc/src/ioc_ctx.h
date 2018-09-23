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
#define MAX_TIMEOUT_BEATS 0x3FFFF
#define MIN_TIMEOUT_BEATS 1
#define MAX_EPOLL_EVENTS_PER_RUN 50
/*---------------------------------------------------------------------------*/
#define MAX_IOC_JOBS_MASK (0x1FF)
#define MAX_IOC_JOBS (MAX_IOC_JOBS_MASK + 1)


/*****************************************************************************/
typedef struct t_io_channel
  {
  int fd;
  int is_blkd;
  int red_to_stop_reading;
  int red_to_stop_writing;
  int not_first_time;
  t_fd_event rx_cb;
  t_fd_event tx_cb;
  t_fd_error err_cb;
  struct epoll_event epev;
  } t_io_channel;
/*---------------------------------------------------------------------------*/
typedef struct t_llid_pool
  {
  int fifo_free_index[CLOWNIX_MAX_CHANNELS];
  int read_idx;
  int write_idx;
  int used_idx;
  } t_llid_pool;

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
  int decoding_state;
  int data_len_waited_for;
  unsigned long tot_txq_size;
  t_fd_connect server_connect_callback;
  t_fd_error error_callback;
  t_msg_rx_cb rx_callback;
  } t_data_channel;
/*---------------------------------------------------------------------------*/

typedef struct t_timeout_elem
{
  int inhibited;
  t_fct_timeout cb;
  void *data;
  int ref;
  struct t_timeout_elem *prev;
  struct t_timeout_elem *next;
} t_timeout_elem;
/*---------------------------------------------------------------------------*/

typedef struct t_ioc_ctx
{
  struct timeval g_last_heartbeat;
  int g_channel_modification_occurence;
  int g_epfd;
  int g_slipery_select;
  int g_counter_select_speed;
  t_io_channel g_channel[MAX_SELECT_CHANNELS];
  int g_current_max_channels;
  int g_current_nb_channels;
  t_heartbeat_cb g_channel_beat;
  struct timeval g_current_time;
  int32_t g_llid_2_cidx[CLOWNIX_MAX_CHANNELS];
  int32_t g_cidx_2_llid[MAX_SELECT_CHANNELS];
  t_llid_pool g_llid_pool;
  int g_heartbeat_ms_timeout;
  struct epoll_event g_events[MAX_EPOLL_EVENTS_PER_RUN];
  int g_heart_count;
  t_timeout_elem *g_grape[MAX_TIMEOUT_BEATS]; 
  int g_current_ref;
  long long g_current_beat;
  int g_plugged_elem;
  t_data_channel g_dchan[CLOWNIX_MAX_CHANNELS];
  char *g_first_rx_buf;
  int g_first_rx_buf_max;
  long long  g_peak_queue_len[MAX_SELECT_CHANNELS];
  char g_bigsndbuf[MAX_SIZE_BIGGEST_MSG];
} t_ioc_ctx;

t_all_ctx *ioc_ctx_init(char *name, int num, int max_len_per_read);
