/*****************************************************************************/
/*    Copyright (C) 2006-2021 clownix@clownix.net License AGPL-3             */
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
void channel_beat_set (t_all_ctx *all_ctx, t_heartbeat_cb beat);
/*---------------------------------------------------------------------------*/
int channel_delete(t_all_ctx *all_ctx, int llid);
/*---------------------------------------------------------------------------*/
void channel_loop(t_all_ctx *all_ctx);
/*---------------------------------------------------------------------------*/
void channel_init(t_all_ctx *all_ctx);
/*---------------------------------------------------------------------------*/
int channel_check_llid(t_all_ctx *all_ctx, int llid, const char *fct);
/*---------------------------------------------------------------------------*/
void channel_rx_local_flow_ctrl(void *ptr, int llid, int stop);
void channel_tx_local_flow_ctrl(void *ptr, int llid, int stop);
/*---------------------------------------------------------------------------*/



