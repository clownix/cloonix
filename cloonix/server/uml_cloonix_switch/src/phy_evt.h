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
int phy_evt_update_lan_add(t_phy *cur, int llid, int tid);
void phy_evt_del_inside(t_phy *cur);
void phy_evt_change_phy_topo(void);
void phy_evt_change_pci_topo(void);
int  phy_evt_update_eth_type(int llid, int tid, int is_add, int eth_type,
                              char *name, char *lan);
void phy_evt_lan_add_done(int eth_type, char *lan);
void phy_evt_lan_del_done(int eth_type, char *lan);
void phy_evt_init(void);
/*--------------------------------------------------------------------------*/
