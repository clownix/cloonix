/*****************************************************************************/
/*    Copyright (C) 2006-2025 clownix@clownix.net License AGPL-3             */
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
void set_bulkvm(int nb, t_slowperiodic *slowperiodic);
void set_bulzip(int nb, t_slowperiodic *slowperiodic);
void set_bulcvm(int nb, t_slowperiodic *slowperiodic);
void canvas_ctx_init(void);
void update_topo_phy(int nb_phy, t_topo_info_phy *phy);
void update_topo_bridges(int nb_bridges, t_topo_bridges *bridges);


