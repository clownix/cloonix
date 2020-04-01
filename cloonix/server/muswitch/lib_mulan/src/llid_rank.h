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
t_llid_rank *llid_rank_get_with_name(char *name, int num, int tidx);
t_llid_rank *llid_rank_get_with_llid(int llid);
t_llid_rank *llid_rank_get_with_prechoice_rank(uint32_t rank);
t_llid_rank *llid_rank_peer_rank_set(int llid, char *name, int num, 
                                     int tidx, uint32_t rank);
int llid_rank_peer_rank_unset(int llid, char *name, uint32_t rank);
void llid_rank_llid_create(int llid, char *name, int num, 
                           int tidx, uint32_t rank);
/*---------------------------------------------------------------------------*/
void llid_rank_sig_disconnect(t_all_ctx *all_ctx, int llid);
void llid_rank_traf_disconnect(t_all_ctx *all_ctx, int llid);
int  llid_rank_traf_connect(t_all_ctx *all_ctx, int llid, 
                            char *name, int num, int tidx);
int get_llid_traf_tab(t_all_ctx *all_ctx, int llid, int32_t **llid_tab);
/*---------------------------------------------------------------------------*/
void traf_chain_insert(int llid);
void traf_chain_extract(int llid);
/*---------------------------------------------------------------------------*/
void init_llid_rank(void);
/*---------------------------------------------------------------------------*/
