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
#define DOOR_X11_OPEN   "door_x11_open idx_x11=%d port=%d"
#define DOOR_X11_OPENOK "door_x11_open_ok"
#define DOOR_X11_OPENKO "door_x11_open_ko"


/****************************************************************************/
int doorways_header_size(void);
/*--------------------------------------------------------------------------*/
typedef void (*t_doorways_end)(int llid);
typedef void (*t_doorways_llid)(int llid);
typedef void (*t_doorways_rx)(int llid, int tid, int type, int val, 
                              int len, char *buf);
/*--------------------------------------------------------------------------*/
int doorways_sock_server_inet(int ip, int port, char *passwd,
                              t_doorways_llid cb_llid,
                              t_doorways_end cb_end, t_doorways_rx cb_rx_fct);
/*--------------------------------------------------------------------------*/
int doorways_tx_or_rx_still_in_queue(int llid);
/*--------------------------------------------------------------------------*/
int doorways_tx(int llid, int tid, int type, int val, int len, char *buf);
/*---------------------------------------------------------------------------*/
int doorways_sock_client_inet_start(int ip, int port, t_fd_event conn_rx);
/*---------------------------------------------------------------------------*/
void doorways_sock_client_inet_delete(int llid);
/*---------------------------------------------------------------------------*/
int doorways_sock_client_inet_end(int type, int llid, int fd,
                                   char *passwd,
                                   t_doorways_end cb_end,
                                   t_doorways_rx cb_rx);
/*---------------------------------------------------------------------------*/
int doorways_sock_client_inet_end_glib(int type, int fd, char *passwd,
                                        t_doorways_end cb_end,
                                        t_doorways_rx cb_rx,
                                        t_fd_event *rx_glib,
                                        t_fd_event *tx_glib);
/*---------------------------------------------------------------------------*/
void doorways_sock_address_detect(char *doors_client_addr, int *ip, int *port);
/*---------------------------------------------------------------------------*/
void doorways_clean_llid(int llid);
/*---------------------------------------------------------------------------*/
void doorways_sock_init(void);
/*---------------------------------------------------------------------------*/


