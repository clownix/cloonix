/*****************************************************************************/
/*    Copyright (C) 2006-2020 clownix@clownix.net License AGPL-3             */
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
int llid_traf_chain_get_llid(char *name, int num);
int llid_traf_chain_exists(char *name, int num);
int llid_traf_chain_insert(int llid, char *wlan, char *name, int num, int tidx);
void llid_traf_chain_extract_by_llid(int llid);
int llid_traf_chain_extract_by_name(char *name, int num, int tidx);
int llid_traf_get_tab(t_all_ctx *all_ctx, int llid, int32_t **llid_tab);
void llid_traf_endp_rx(int llid, int len);
void llid_traf_endp_tx(int llid, int len);
void llid_traf_init(t_all_ctx *all_ctx);
/*---------------------------------------------------------------------------*/


