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
#define DIALOG_WAKE   "wake sock_fd:%d srv_idx:%d cli_idx:%d"
#define DIALOG_KILLED "killed sock_fd:%d srv_idx:%d cli_idx:%d"
#define DIALOG_STATS  "stats sock_fd:%d srv_idx:%d cli_idx:%d "\
                      "txpkts:%d txbytes:%lld rxpkts:%d rxbytes:%lld"

typedef void (*t_err)(int sock_fd, int srv_idx, int cli_idx, char *buf);
typedef void (*t_wake)(int sock_fd, int srv_idx, int cli_idx, char *buf);
typedef void (*t_killed)(int sock_fd, int srv_idx, int cli_idx, char *buf);
typedef void (*t_stats)(int sock_fd, int srv_idx, int cli_idx,
                        int txpkts, long long txbytes,
                        int rxpkts, long long rxbytes, char *buf);

int dialog_send_wake(int to_wr_fd, int sock_fd, int srv_idx, int cli_idx);
int dialog_send_killed(int from_wr_fd, int sock_fd, int srv_idx, int cli_idx);
int dialog_send_stats(int wr_fd, int sock_fd, int srv_idx, int cli_idx,
                      int txpkts, long long txbytes,
                      int rxpkts, long long rxbytes);

void dialog_recv_buf(char *buf, int sock_fd, int srv_idx, int cli_idx,
                   t_err err, t_wake wake, t_killed killed, t_stats stats);

int dialog_recv_fd(int fd, int sock_fd, int srv_idx, int cli_idx,
                 t_err err, t_wake wake, t_killed killed, t_stats stats);

int  dialog_tx_queue_non_empty(int fd);
int  dialog_tx_ready(int fd);
void dialog_close(int fd);
int  dialog_open(int s, t_outflow outflow, t_inflow inflow);
void dialog_init(void);


/*--------------------------------------------------------------------------*/

