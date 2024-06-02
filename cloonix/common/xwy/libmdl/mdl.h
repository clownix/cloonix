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
#define MAX_MSG_LEN  1000000
#define MAX_FD_NUM 500
#define MAX_TXT_LEN 300

#define MAX_X11_MSG_LEN 100000


#include <string.h>
#include <syslog.h>
#include <libgen.h>
#include <stdint.h>

#define MAX_EPOLL_EVENTS 10

#define MAX_IDX_X11 10
#define X11_OFFSET_PORT 6000
#define SRV_IDX_MIN 50
#define SRV_IDX_MAX 250

#define MAGIC_COOKIE "MIT-MAGIC-COOKIE-1"
#define MAGIC_COOKIE_LEN 16

#define UNIX_X11_SOCKET_PREFIX "/tmp/.X11-unix/X%d"
#define UNIX_X11_DPYNAME "unix:%d.0"

#define CLOONIX_KIL_REQ "KillYou"
#define CLOONIX_PID_REQ "Your_Pid?"
#define CLOONIX_PID_RESP "My_Pid:%d"

enum
{
  action_bash = 0,
  action_cmd,
  action_dae,
  action_get,
  action_put,
  action_crun,
};

enum
{
  thread_type_min = 57,
  thread_type_tx,
  thread_type_x11,
  thread_type_max,
};

enum
{
  fd_type_min = 107,
  fd_type_pipe_win_chg,
  fd_type_pipe_sig,
  fd_type_fork_pty,
  fd_type_dialog_thread,
  fd_type_listen_inet_srv,
  fd_type_listen_unix_srv,
  fd_type_srv,
  fd_type_srv_ass,
  fd_type_cli,
  fd_type_scp,
  fd_type_pty,
  fd_type_sig,
  fd_type_zero,
  fd_type_one,
  fd_type_win,
  fd_type_tx_epoll,
  fd_type_x11_epoll,
  fd_type_x11_listen,
  fd_type_x11_connect_tst,
  fd_type_x11_connect,
  fd_type_x11_accept,
  fd_type_x11_rd_x11,
  fd_type_x11_rd_soc,
  fd_type_x11_wr_x11,
  fd_type_x11_wr_soc,
  fd_type_x11,
  fd_type_listen_cloon,
  fd_type_cloon,
  fd_type_max, 
};

enum
{
  msg_type_randid = 7,
  msg_type_randid_associated,
  msg_type_randid_associated_ack,
  msg_type_data_pty,
  msg_type_open_bash,
  msg_type_open_cmd,
  msg_type_open_crun,
  msg_type_open_dae,
  msg_type_win_size,
  msg_type_data_cli_pty,
  msg_type_end_cli_pty,
  msg_type_end_cli_pty_ack,
  msg_type_x11_info_flow,
  msg_type_x11_init,
  msg_type_x11_connect,
  msg_type_x11_connect_ack,
  msg_type_scp_open_snd,
  msg_type_scp_open_rcv,
  msg_type_scp_ready_to_snd,
  msg_type_scp_ready_to_rcv,
  msg_type_scp_data,
  msg_type_scp_data_end,
  msg_type_scp_data_begin,
  msg_type_scp_data_end_ack,
};
typedef struct t_msg
{
  uint32_t cafe;
  uint32_t randid;
  uint32_t seqnum_checksum;
  uint32_t type;
  uint32_t len;
  uint8_t buf[MAX_MSG_LEN];
} t_msg;

typedef ssize_t (*t_outflow)(int s, const void *buf, long unsigned int len);
typedef ssize_t (*t_inflow)(int s, void *buf, size_t len);

typedef void (*t_rx_msg_cb)(void *ptr, int llid, int fd, t_msg *msg);
typedef void (*t_rx_err_cb)(void *ptr, int llid, int fd, char *err);


static const int g_msg_header_len = (5*sizeof(uint32_t));

uint16_t mdl_sum_calc(int len, uint8_t *buff);
int  mdl_parse_val(const char *str_val);
int  mdl_ip_string_to_int (unsigned long *inet_addr, char *ip_string);
char *mdl_int_to_ip_string (int addr);

int  mdl_queue_write_msg(int fd_dst, t_msg *msg);
void mdl_prepare_header_msg(uint16_t write_seqnum, t_msg *msg);

void mdl_read_fd(void *ptr, int fd, t_rx_msg_cb rx, t_rx_err_cb err);
void mdl_read_data(void *ptr, int llid, int fd, int len, char *data,
                   t_rx_msg_cb rx, t_rx_err_cb err);

void mdl_open(int s, int type, t_outflow outflow, t_inflow inflow);
void mdl_modify(int s, int type);
int  mdl_exists(int s);
void mdl_close(int s);

int mdl_init(void);

void mdl_get_header_vals(t_msg *msg, uint32_t *randid, int *type, int *from,
                                     int *srv_idx, int *cli_idx);

void mdl_set_header_vals(t_msg *msg, uint32_t randid, int type, int from,
                                     int srv_idx, int cli_idx);


char *mdl_argv_linear(char **argv);
FILE *mdl_argv_popen(char *argv[]);

void mdl_heartbeat(void);

typedef void (*t_xtimeout)(void *data);
void cloonix_timeout_add(int nb_beats, t_xtimeout cb, void *data);
void cloonix_timer_beat(void);
void cloonix_timer_init(void);


