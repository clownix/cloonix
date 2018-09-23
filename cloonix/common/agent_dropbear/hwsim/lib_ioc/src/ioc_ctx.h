/****************************************************************************/
#define MAX_TIMEOUT_BEATS 0x3FFFF
#define MIN_TIMEOUT_BEATS 1
#define MAX_EPOLL_EVENTS_PER_RUN 50
/*---------------------------------------------------------------------------*/


/*****************************************************************************/
typedef struct t_io_channel
  {
  int is_blkd;
  int fd;
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
  int fifo_free_index[MAX_CHANNELS];
  int read_idx;
  int write_idx;
  int used_idx;
  } t_llid_pool;
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
  t_io_channel g_channel[MAX_SIM_CHANNELS];
  int g_current_max_channels;
  int g_current_nb_channels;
  t_heartbeat_cb g_channel_beat;
  struct timeval g_current_time;
  int g_llid_2_cidx[MAX_CHANNELS];
  int g_cidx_2_llid[MAX_SIM_CHANNELS];
  t_llid_pool g_llid_pool;
  int g_heartbeat_ms_timeout;
  struct epoll_event g_events[MAX_EPOLL_EVENTS_PER_RUN];
  int g_heart_count;
  t_timeout_elem *g_grape[MAX_TIMEOUT_BEATS]; 
  int g_current_ref;
  long long g_current_beat;
  int g_plugged_elem;
  char *g_first_rx_buf;
  int g_first_rx_buf_max;
} t_ioc_ctx;

t_all_ctx *ioc_ctx_init(char *name);
