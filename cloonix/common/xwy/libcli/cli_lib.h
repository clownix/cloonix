/*****************************************************************************/
/*    Copyright (C) 2006-2022 clownix@clownix.net License AGPL-3             */
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
#include <sys/epoll.h>

typedef void (*t_sock_fd_tx)(int llid, int tid, int type, int len, char *buf);
typedef void (*t_sock_fd_ass_open)(int cli_idx);
typedef void (*t_sock_fd_ass_close)(int llid);


void xcli_send_randid_associated_begin(int cli_idx);
void xcli_send_randid_associated_end(int llid, int tid, int type, int cli_idx);

int xcli_get_main_epfd(void);
uint32_t xcli_get_randid(void);

void xcli_send_msg_type_x11_connect_ack(int cli_idx, char *txt);

void xcli_x11_x11_rx(int llid_ass, int tid, int type, int len, char *buf);
void xcli_x11_doors_rx(int cli_idx, int len, char *buf);
void xcli_traf_doors_rx(int llid, int len, char *buf);

void xcli_fct_before_epoll(int epfd);
int xcli_fct_after_epoll(int nb, struct epoll_event *events);
void xcli_init(int epfd, int llid, int tid, int type,
               t_sock_fd_tx soc_tx, t_sock_fd_ass_open sock_fd_ass_open,
               t_sock_fd_ass_close sock_fd_ass_close, int action,
               char *src, char *dst, char *cmd);
void xcli_killed_x11(int cli_idx);

/*--------------------------------------------------------------------------*/
