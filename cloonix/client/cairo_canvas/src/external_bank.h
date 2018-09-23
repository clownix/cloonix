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
void topo_add_cr_item_to_canvas(t_bank_item *bitem, t_bank_item *bnode);
void topo_remove_cr_item(t_bank_item *bitem);
void topo_cr_item_text( t_bank_item *bitem, double x, double y, char *name);
void topo_get_matrix_transform_point(t_bank_item *bitem, double *x, double *y);
void topo_cr_item_set_z(t_bank_item *bitem);
void topo_get_absolute_coords(t_bank_item *bitem);
void modif_position_eth(t_bank_item *bitem, double xi, double yi);















