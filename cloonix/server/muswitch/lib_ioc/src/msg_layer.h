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
typedef enum
{
  rx_type_watch = 421,
  rx_type_listen,
  rx_type_ascii_start,
  rx_type_open_bound_found,
  rx_type_rawdata,
} t_rx_type;

t_data_channel *get_dchan(t_ioc_ctx *ioc_ctx, int cidx);
void new_rx_to_process(t_all_ctx *all_ctx, t_data_channel *dchan, 
                       int len, char *new_rx);
void err_dchan_cb(void *ptr, int llid, int err, int from);
void chunk_chain_delete(t_data_channel *dchan);









