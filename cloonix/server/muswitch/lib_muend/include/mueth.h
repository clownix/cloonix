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
typedef void  (*t_change_virtio_queue)(t_all_ctx *all_ctx);
void pool_tx_init(t_tx_sock_async_pool *pool_tx);
void unix_sock_rx_activate(t_all_ctx *all_ctx);
void tx_unix_sock(t_all_ctx *all_ctx, void *elem, int len);
void tx_unix_sock_end(t_all_ctx *all_ctx);
int tx_unix_sock_shaping_overload(t_all_ctx *all_ctx);
void tx_unix_sock_shaping_timer(t_all_ctx *all_ctx);
void tx_unix_sock_shaping_value(t_all_ctx *all_ctx, int kbytes_persec);
int tx_unix_sock_pool_len(t_all_ctx *all_ctx);
void stop_tx_counter_increment(t_all_ctx *all_ctx, int idx);
void mueth_main_endless_loop(t_all_ctx *all_ctx, char *net_name,
                             char *name, int num, char *serv_path,
                             t_get_blkd_from_elem get_blkd_from_elem,
                              t_change_virtio_queue change_virtio_queue);
/*---------------------------------------------------------------------------*/


