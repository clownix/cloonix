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
void layout_llid_destroy(int llid);
void layout_topo_init(void);
void get_layout_main_params(int *go, int *width, int *height,
                            int *cx, int *cy, int *cw, int *ch);
t_layout_xml *get_layout_xml_chain(void);
void layout_add_vm(char *name, int llid);
void layout_add_sat(char *name, int llid);
void layout_add_lan(char *name, int llid);
void layout_del_vm(char *name);
void layout_del_sat(char *name);
void layout_del_lan(char *name);
int layout_node_solve(double x, double y);
int layout_a2b_solve(double x, double y);
/*---------------------------------------------------------------------------*/



