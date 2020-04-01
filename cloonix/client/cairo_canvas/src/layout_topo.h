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
void layout_send_layout_node(t_layout_node *layout);
void layout_send_layout_lan(t_layout_lan *layout);
void layout_send_layout_sat(t_layout_sat *layout);
void clean_layout_xml(void);
t_layout_xml *get_current_layout_xml(void);
void layout_round_node_eth_coords(double *x, double *y);
void layout_round_a2b_eth_coords(double *x, double *y);
void layout_set_ready_for_send(void);
int layout_get_ready_for_send(void);
void layout_topo_init(void);
/*---------------------------------------------------------------------------*/



