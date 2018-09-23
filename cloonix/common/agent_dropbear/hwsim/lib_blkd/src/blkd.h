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
#define MASK_RX_BLKD_POOL 0x3FFF
#define MASK_TX_BLKD_POOL 0x7FFF
#define STORED_SLOTS 30
#define STORED_SLOTS_IN_HALF_SEC 10
#define STORED_SLOTS_IN_ONE_SEC 20
/*--------------------------------------------------------------------------*/
typedef struct t_blkd_record
{
  int len_to_do;
  int len_done;
  t_blkd *blkd;
} t_blkd_record;
/*--------------------------------------------------------------------------*/
typedef struct t_blkd_fifo_rx
{
  uint32_t volatile circ_lock;
  uint32_t put;
  uint32_t get;
  uint32_t volatile qty;
  unsigned long rx_queued_bytes;
  int max_qty;
  int dist_flow_control_on;
  int dist_flow_control_count;
  int current_slot;
  int slot_sel[STORED_SLOTS];
  int slot_qty[STORED_SLOTS];
  int slot_queue[STORED_SLOTS];
  int slot_bandwidth[STORED_SLOTS];
  int slot_stop[STORED_SLOTS];
  int slot_dist_flow_ctrl[STORED_SLOTS];
  t_blkd_record rec[MASK_RX_BLKD_POOL + 1];
} t_blkd_fifo_rx;
/*--------------------------------------------------------------------------*/
typedef struct t_blkd_fifo_tx
{
  uint32_t volatile circ_lock;
  uint32_t put;
  uint32_t get;
  uint32_t volatile qty;
  unsigned long tx_queued_bytes;
  int max_qty;
  int dist_flow_control_on;
  int dist_flow_control_count;
  int current_slot;
  int slot_sel[STORED_SLOTS];
  int slot_qty[STORED_SLOTS];
  int slot_queue[STORED_SLOTS];
  int slot_bandwidth[STORED_SLOTS];
  int slot_stop[STORED_SLOTS];
  int slot_dist_flow_ctrl[STORED_SLOTS];
  t_blkd_record rec[MASK_TX_BLKD_POOL + 1];
} t_blkd_fifo_tx;
/*--------------------------------------------------------------------------*/
typedef struct t_llid_blkd
{
  char name[MAX_NAME_LEN];
  char sock[MAX_PATH_LEN];
  char sock_type[MAX_NAME_LEN];
  int llid;
  int fd;
  int heartbeat;
  t_blkd_item report_item;
  t_fd_connect blkd_connect_event;
  t_fd_error blkd_error_event;
  t_blkd_rx_cb blkd_rx_ready;
  t_blkd_fifo_rx fifo_rx;
  t_blkd_fifo_tx fifo_tx;
} t_llid_blkd;
/*--------------------------------------------------------------------------*/
typedef struct t_blkd_ctx
{
  char g_name[MAX_NAME_LEN];
  int  g_num;
  int  g_pid;
  int  g_our_mutype;
  int  g_cloonix_llid;
  t_fd_local_flow_ctrl local_flow_ctrl_tx;
  t_fd_local_flow_ctrl local_flow_ctrl_rx;
  t_fd_dist_flow_ctrl dist_flow_ctrl;
  char *g_buf_tx;
  t_llid_blkd *llid2blkd[CLOWNIX_MAX_CHANNELS];
  int llid_list[MAX_SELECT_CHANNELS];
  int llid_list_max;
}t_blkd_ctx;
/*---------------------------------------------------------------------------*/
void blkd_header_rx_extract(t_blkd_record *rec);
void blkd_header_tx_setup(t_blkd_record *rec);
/*---------------------------------------------------------------------------*/
int blkd_fd_event_rx(void *ptr, int llid, int fd,
                                t_blkd_fifo_rx *fifo_rx,
                                t_blkd_rx_cb rx_ready_cb);
/*--------------------------------------------------------------------------*/
int blkd_fd_event_tx(void *ptr, int fd, t_blkd_fifo_tx *fifo_tx);
void blkd_fd_event_purge_tx(void *ptr, t_blkd_fifo_tx *pool);
/*--------------------------------------------------------------------------*/
t_blkd *blkd_rx_fifo_get(t_blkd_fifo_rx *fifo_rx);
/*--------------------------------------------------------------------------*/
int blkd_tx_fifo_alloc(void *ptr, t_blkd_fifo_tx *fifo_tx, t_blkd *blkd);
/*--------------------------------------------------------------------------*/
int blkd_get_max_rx_flow_llid(void *ptr, t_blkd_fifo_rx **pool);

void blkd_tx_init(t_blkd_fifo_tx *fifo_tx);
void blkd_rx_init(t_blkd_fifo_rx *fifo_rx);
t_blkd_ctx *get_blkd_ctx(void *ptr);
/*--------------------------------------------------------------------------*/



